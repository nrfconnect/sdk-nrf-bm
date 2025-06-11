/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/crc.h>
#include <zephyr/logging/log.h>
#include <bm_installs.h>
#include <zephyr/storage/flash_map.h>

#if defined(CONFIG_BM_METADATA_WRITE)
#include <zephyr/drivers/flash.h>

static struct flash_area metadata_slot;
static bool setup_finished;
static bool bm_installs_was_valid;
#endif

LOG_MODULE_REGISTER(bm_installs, CONFIG_BM_INSTALL_LOG_LEVEL);

#if defined(CONFIG_SOC_SERIES_NRF54LX)
#define MAX_IMAGE_SIZE (2 * 1024 * 1024)
#else
#define MAX_IMAGE_SIZE (1 * 1024 * 1024)
#endif

BUILD_ASSERT(sizeof(struct bm_installs) == CONFIG_BM_INSTALL_ENTRY_SIZE,
	     "Metadata struct entry size mismatch");

static struct bm_installs bm_installs_data;
static bool bm_installs_valid;
static uint16_t bm_installs_index;

static bool bm_installs_validate(struct bm_installs *data)
{
	uint32_t checksum = 0;
	uint16_t i = 0;
	bool valid = true;

	while (i < CONFIG_BM_INSTALL_IMAGES) {
		if ((data->images[i].start_address + data->images[i].image_size) >
		    MAX_IMAGE_SIZE) {
			return false;
		}

		++i;
	}

#if CONFIG_BM_INSTALL_IMAGES >= 2
	if ((data->images[BM_INSTALLS_IMAGE_INDEX_FIRMWARE_LOADER].start_address +
	     data->images[BM_INSTALLS_IMAGE_INDEX_FIRMWARE_LOADER].image_size) >
	     data->images[BM_INSTALLS_IMAGE_INDEX_SOFTDEVICE].start_address) {
		return false;
	}
#endif

	checksum = crc32_ieee((const uint8_t *)data, (sizeof(struct bm_installs) -
			       sizeof(data->checksum)));

	if (checksum != data->checksum) {
		valid = false;
	}

	return valid;
}

void bm_installs_init(void)
{
	uint32_t read_position = FIXED_PARTITION_OFFSET(metadata_partition);

	bm_installs_index = 0;
	bm_installs_valid = false;
#if defined(CONFIG_BM_METADATA_WRITE)
	bm_installs_was_valid = false;
#endif

	memset(&bm_installs_data, 0, sizeof(bm_installs_data));

	while (bm_installs_index < CONFIG_BM_INSTALL_ENTRIES) {
		memcpy(&bm_installs_data, (void *)read_position, sizeof(bm_installs_data));

		bm_installs_valid = bm_installs_validate(&bm_installs_data);

		if (bm_installs_valid) {
			break;
		}

		read_position += sizeof(bm_installs_data);
		++bm_installs_index;
	}

	if (bm_installs_valid == false) {
		bm_installs_index = 0;
	}

#if defined(CONFIG_BM_METADATA_WRITE)
	bm_installs_was_valid = bm_installs_valid;

	if (setup_finished == false) {
		const struct flash_area *fap;
		int rc;

		rc = flash_area_open(FIXED_PARTITION_ID(slot0_partition), &fap);

		if (rc) {
			return;
		}

		metadata_slot.fa_id = fap->fa_id;
		metadata_slot.fa_dev = fap->fa_dev;
		metadata_slot.fa_off = FIXED_PARTITION_OFFSET(metadata_partition);
		metadata_slot.fa_size = FIXED_PARTITION_SIZE(metadata_partition);
#if CONFIG_FLASH_MAP_LABELS
		metadata_slot.fa_label = fap->fa_label;
#endif

		(void)flash_area_close(fap);
		setup_finished = true;
	}
#endif
}

bool bm_installs_isvalid(void)
{
	return bm_installs_valid;
}

int bm_installs_read(struct bm_installs *data)
{
	if (!bm_installs_valid) {
		return -ENOENT;
	}

	memcpy(data, &bm_installs_data, sizeof(bm_installs_data));

	return 0;
}

#if defined(CONFIG_BM_METADATA_WRITE)
int bm_installs_write(struct bm_installs *data)
{
	off_t index_offset;
	uint16_t index = bm_installs_index;
	int rc = 0;

#if defined(CONFIG_FLASH_HAS_EXPLICIT_ERASE)
	const struct flash_parameters *fparams;
#endif

	if (bm_installs_was_valid) {
		++index;

		if (index >= CONFIG_BM_INSTALL_ENTRIES) {
			index = 0;
		}
	}

#if defined(CONFIG_FLASH_HAS_EXPLICIT_ERASE)
	/* Ensure position to write to is erased, if not increment until there is a safe
	 * position, if there are none, erase the whole sector and start from scratch
	 */
	fparams = flash_get_parameters(metadata_slot.fa_dev);

	if (flash_params_get_erase_cap(fparams) & FLASH_ERASE_C_EXPLICIT) {
		uint8_t unerased_data[sizeof(struct bm_installs)];
		uint8_t empty_data[sizeof(struct bm_installs)] = { 0x00 };

		memset(unerased_data, fparams->erase_value, sizeof(unerased_data));

		while (index < CONFIG_BM_INSTALL_ENTRIES) {
			uint32_t current_index_offset = index * sizeof(struct bm_installs);
			uint32_t current_flash_offset = FIXED_PARTITION_OFFSET(metadata_partition) +
							current_index_offset;

			if (memcmp((void *)current_flash_offset, unerased_data,
				   sizeof(unerased_data)) == 0) {
				break;
			} else if (memcmp((void *)current_flash_offset, empty_data,
					  sizeof(empty_data)) != 0) {
				/* Clear part in case data here is seen as valid */
				rc = flash_area_write(&metadata_slot, current_index_offset,
						      empty_data, sizeof(empty_data));

				if (rc) {
					LOG_ERR("Failed to erase: %d", rc);
					return rc;
				}
			}

			++index;
		}

		if (index == CONFIG_BM_INSTALL_ENTRIES) {
			/* No free spaces, erase whole sector and start again */
			index = 0;
			rc = flash_area_erase(&metadata_slot, 0, FIXED_PARTITION_SIZE(
									metadata_partition));

			if (rc) {
				return rc;
			}
		}
	}
#endif

	index_offset = index * sizeof(bm_installs_data);
	rc = flash_area_write(&metadata_slot, index_offset, data, sizeof(struct bm_installs));

	return rc;
}

int bm_installs_invalidate(void)
{
	off_t index_offset = bm_installs_index * sizeof(bm_installs_data);
	uint8_t empty_data[sizeof(struct bm_installs)] = { 0x00 };
	int rc = 0;
#if defined(CONFIG_FLASH_HAS_EXPLICIT_ERASE)
	bool performed_erase = false;
#endif

	if (!bm_installs_valid) {
		return -ENOENT;
	}

#if defined(CONFIG_FLASH_HAS_EXPLICIT_ERASE)
	if ((index_offset + sizeof(bm_installs_data)) >= FIXED_PARTITION_SIZE(
								metadata_partition)) {
		/* Since this is at end of sector, it would be better to just erase the whole
		 * sector
		 */
		const struct flash_parameters *fparams = flash_get_parameters(
								metadata_slot.fa_dev);

		if (flash_params_get_erase_cap(fparams) & FLASH_ERASE_C_EXPLICIT) {
			rc = flash_area_erase(&metadata_slot, 0, FIXED_PARTITION_SIZE(
									metadata_partition));
			performed_erase = true;
		}
	}

	if (performed_erase == false) {
#endif
		rc = flash_area_write(&metadata_slot, index_offset, empty_data,
				      sizeof(empty_data));
#if defined(CONFIG_FLASH_HAS_EXPLICIT_ERASE)
	}
#endif

	if (rc) {
		return rc;
	}

	bm_installs_valid = false;

	return rc;
}
#endif

int bm_installs_get_image_data(uint8_t image, off_t *start_address, size_t *image_size)
{
	if (!bm_installs_valid) {
		return -ENOENT;
	} else if (image >= BM_INSTALLS_IMAGE_INDEX_COUNT || image >= CONFIG_BM_INSTALL_IMAGES) {
		return -EINVAL;
	}

	if (start_address != NULL) {
		*start_address = bm_installs_data.images[image].start_address;
	}

	if (image_size != NULL) {
		*image_size = bm_installs_data.images[image].image_size;
	}

	return 0;
}
