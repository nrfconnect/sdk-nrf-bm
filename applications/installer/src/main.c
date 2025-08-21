/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/reboot.h>
#include <bm_installs.h>

LOG_MODULE_REGISTER(installer, CONFIG_INSTALLER_LOG_LEVEL);

#define IMAGE_DATA_SIZE 64
#define IMAGE_DATA_ARRAY_SIZE 32
#define EXPECTED_HEADER { 0x92, 0x11, 0xf2, 0xe9 }
#define PROCESS_SECTOR_SIZE 4096

extern uintptr_t _flash_used;
static const uint8_t expected_header[] = EXPECTED_HEADER;

struct bm_installs_update_image {
	off_t start_address; /* Metadata */
	size_t image_size; /* Metadata - also need to erase the end of this section */
	off_t data_offset_address; /* Position of data from end of image */
	size_t data_image_size; /* Length of data */
	uint32_t data_checksum; /* CRC32 of data */
	uint8_t image_id; /* Metadata */
	uint8_t padding[3];
} __packed;

struct bm_installs_update {
	uint8_t header[sizeof(expected_header)];
	struct bm_installs_update_image images[CONFIG_BM_INSTALL_IMAGES];
	uint32_t checksum;
} __packed;

/* Data after end of installer image containing update images */
static const struct bm_installs_update *update_data;

int main(void)
{
	int rc;
	uint32_t checksum_calculated;
	uint32_t checksum_expected;
	uint32_t installer_data_size;
	uint8_t i = 0;
	bool upgrade_ok = true;
	struct bm_installs replacement_metadata;
	struct flash_area fa_installer = {
		.fa_id = 1,
		.fa_off = CONFIG_FLASH_LOAD_OFFSET,
		.fa_size = PROCESS_SECTOR_SIZE,
		.fa_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller)),
	};
#if defined(CONFIG_FLASH_HAS_EXPLICIT_ERASE)
	static uint8_t erase_buffer[PROCESS_SECTOR_SIZE];
	const struct flash_parameters *fparams;
#else
	static uint8_t write_buffer[CONFIG_ROM_START_OFFSET] = { 0x00 };
#endif

	update_data = (struct bm_installs_update *)((int)&_flash_used + CONFIG_FLASH_LOAD_OFFSET);

	if (update_data == NULL) {
		LOG_ERR("Installer data is NULL");
		goto erase_header;
	}

	if (memcmp(update_data->header, expected_header, sizeof(expected_header)) != 0) {
		LOG_ERR("Header mismatch, data not valid");
		goto erase_header;
	}

	installer_data_size = sizeof(struct bm_installs_update) - sizeof(update_data->checksum);
	checksum_calculated = crc32_ieee((void *)update_data, installer_data_size);
	memcpy(&checksum_expected, &update_data->checksum, sizeof(checksum_expected));

	LOG_DBG("Checksum - calculated: %u, expected: %u", checksum_calculated, checksum_expected);

	if (checksum_calculated == checksum_expected) {
		while (i < CONFIG_BM_INSTALL_IMAGES) {
			checksum_calculated = crc32_ieee((void *)((uint32_t)update_data +
						   sizeof(struct bm_installs_update) +
						   update_data->images[i].data_offset_address),
						   update_data->images[i].data_image_size);
			memcpy(&checksum_expected, &update_data->images[i].data_checksum,
			       sizeof(checksum_expected));

			LOG_DBG("Image %d:", i);
			LOG_DBG("\tStart address: 0x%lx:", update_data->images[i].start_address);
			LOG_DBG("\tSize: 0x%x:", update_data->images[i].image_size);
			LOG_DBG("\tData address: 0x%lx:",
				update_data->images[i].data_offset_address);
			LOG_DBG("\tData size: 0x%x:", update_data->images[i].data_image_size);
			LOG_DBG("\tImage ID: 0x%x:", update_data->images[i].image_id);
			LOG_DBG("\tImage checksum (calculated): %u", checksum_calculated);
			LOG_DBG("\tImage checksum (expected): %u", checksum_expected);

			if (checksum_calculated != checksum_expected) {
				upgrade_ok = false;
			}

			++i;
		}
	} else {
		upgrade_ok = false;
	}

	if (!upgrade_ok) {
		/* Update data is bad, erase own header and reboot */
		LOG_ERR("Upgrade data bad");
		goto erase_header;
	}

	LOG_DBG("Upgrade data OK");

#if defined(CONFIG_FLASH_HAS_EXPLICIT_ERASE)
	fparams = flash_get_parameters(DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller)));
	memset(erase_buffer, fparams->erase_value, sizeof(erase_buffer));
#endif

	i = 0;

	bm_installs_init();

	if (bm_installs_is_valid()) {
		rc = bm_installs_invalidate();

		if (rc) {
			LOG_ERR("Metadata invalidation failed: %d", rc);
		} else {
			LOG_DBG("Metadata invalidation OK");
		}
	}

	memset(&replacement_metadata.padding, 0xff, BM_INSTALLS_PADDING_SIZE);

	while (i < CONFIG_BM_INSTALL_IMAGES) {
		uint32_t pos = 0;
		uint32_t read_pos = (uint32_t)update_data + sizeof(struct bm_installs_update) +
				    update_data->images[i].data_offset_address;
		uint32_t write_pos = update_data->images[i].start_address;
		struct flash_area fa = {
			.fa_id = 1,
			.fa_off = write_pos,
			.fa_size = update_data->images[i].image_size,
			.fa_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller)),
		};

		LOG_DBG("Start update of image %d...", i);

		replacement_metadata.images[i].start_address =
							update_data->images[i].start_address;
		replacement_metadata.images[i].image_size = update_data->images[i].image_size;

		while (pos < update_data->images[i].data_image_size) {
			uint32_t process_size = update_data->images[i].data_image_size - pos;

			if (process_size > PROCESS_SECTOR_SIZE) {
				process_size = PROCESS_SECTOR_SIZE;
			}

			LOG_DBG("Write to: %p, read from: %p, size: %d", (void *)write_pos,
				(void *)read_pos, process_size);

			if (memcmp((void *)write_pos, (void *)read_pos, process_size)) {
#if defined(CONFIG_FLASH_HAS_EXPLICIT_ERASE)
				if (memcmp((void *)write_pos, erase_buffer, process_size)) {
					rc = flash_area_erase(&fa, pos, PROCESS_SECTOR_SIZE);

					if (rc) {
						LOG_ERR("Erase failed: %d, at: 0x%x, size: %d",
							rc, pos, PROCESS_SECTOR_SIZE);
						goto erase_header;
					} else {
						LOG_DBG("Erase OK at: 0x%x, size: %d", pos,
							PROCESS_SECTOR_SIZE);
					}
				}
#endif

				rc = flash_area_write(&fa, pos, (void *)read_pos, process_size);

				if (rc) {
					LOG_ERR("Write failed: %d, at: 0x%x, size: %d", rc, pos,
						process_size);
					goto erase_header;
				} else {
					LOG_DBG("Write OK at: 0x%x, size: %d", pos, process_size);
				}
			}

			pos += process_size;
			read_pos += process_size;
			write_pos += process_size;
		}

		++i;
	}

	/* Write new metadata, after updating checksum */
	replacement_metadata.checksum = crc32_ieee((const uint8_t *)&replacement_metadata,
						   (sizeof(struct bm_installs) -
						    sizeof(replacement_metadata.checksum)));
	rc = bm_installs_write(&replacement_metadata);

	if (rc) {
		LOG_ERR("Metadata update failed: %d", rc);
	} else {
		LOG_DBG("Metadata update OK");
	}

erase_header:
	/* Erase header of installer image to boot back into firmware loader image */
#if defined(CONFIG_FLASH_HAS_EXPLICIT_ERASE)
	memset(erase_buffer, 0, sizeof(erase_buffer));
	rc = flash_area_write(&fa_installer, 0, erase_buffer, CONFIG_ROM_START_OFFSET);
#else
	rc = flash_area_write(&fa_installer, 0, write_buffer, CONFIG_ROM_START_OFFSET);
#endif

	if (rc) {
		LOG_ERR("Clear installer header failed: %d", rc);
	} else {
		LOG_ERR("Clear installer header OK");
	}

	while (LOG_PROCESS()) {
	}

	sys_reboot(SYS_REBOOT_WARM);

	return 0;
}
