/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/util.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/logging/log.h>
#include <bootutil/bootutil_public.h>
#include <assert.h>

#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <bm/mgmt/mcumgr/grp/img_mgmt/img_mgmt.h>

#include <bm/storage/bm_storage.h>

#include <mgmt/mcumgr/grp/img_mgmt/img_mgmt_priv.h>

LOG_MODULE_DECLARE(mcumgr_img_grp_data, CONFIG_MCUMGR_GRP_IMG_LOG_LEVEL);

#define S0_START FIXED_PARTITION_ADDRESS(slot0_partition)
#define S0_SIZE FIXED_PARTITION_SIZE(slot0_partition)

#define S1_START FIXED_PARTITION_ADDRESS(slot1_partition)
#define S1_SIZE FIXED_PARTITION_SIZE(slot1_partition)
#define RRAMC_WRITE_BLOCK_SIZE 16

static struct bm_storage s0_storage;
static struct bm_storage s1_storage;

#if defined(CONFIG_BM_STORAGE_BACKEND_SD) || defined(CONFIG_BM_STORAGE_BACKEND_RRAM)
#define RRAMC_WEAR_UNIT (RRAMC_NRRAMWORDSIZE / BITS_PER_BYTE)
BUILD_ASSERT(IS_ALIGNED(CONFIG_BM_MCUMGR_GRP_IMG_BUFFER_SZ, RRAMC_WEAR_UNIT));
#endif

RING_BUF_DECLARE(ring_buf, CONFIG_BM_MCUMGR_GRP_IMG_BUFFER_SZ);

/* Write offset. */
static unsigned int write_offset;
/* Last data in image is received. */
static volatile bool last_data;
/* Write operation ongoing. */
static atomic_t ongoing;

/* NVM wear unit. */
static unsigned int storage_wear_unit;
/* Allow writing multiple wear units at a time. Though, limit the number so that we clear up space
 * for new data from BLE.
 */
static unsigned int storage_write_size_max;

/* Forward declaration. */
static void bm_storage_evt_handler_writes(struct bm_storage_evt *evt);

static const struct bm_storage_config s0_config = {
#if defined(CONFIG_BM_STORAGE_BACKEND_SD)
	.api = &bm_storage_sd_api,
#else
	.api = &bm_storage_rram_api,
#endif
	.evt_handler = bm_storage_evt_handler_writes,
	.addr = S0_START,
	.size = S0_SIZE,
	.flags = {
		.is_write_padded = 1,
	},
};

static const struct bm_storage_config s1_config = {
#if defined(CONFIG_BM_STORAGE_BACKEND_SD)
	.api = &bm_storage_sd_api,
#else
	.api = &bm_storage_rram_api,
#endif
	.evt_handler = bm_storage_evt_handler_writes,
	.addr = S1_START,
	.size = S1_SIZE,
};

static int claim_and_write(void)
{
	int err;
	int rb_size;
	uint8_t *rb_data;

	if ((ring_buf_size_get(&ring_buf) >= storage_wear_unit) || last_data) {
		if (!atomic_cas(&ongoing, 0, 1)) {
			/* Could not set ongoing, write opertion already in progress. */
			return IMG_MGMT_ERR_OK;
		}
		rb_size = ring_buf_get_claim(&ring_buf, &rb_data, storage_write_size_max);

		if (!last_data) {
			/* Make sure we are writing full wear units until end of image. */
			rb_size = ROUND_DOWN(rb_size, storage_wear_unit);
		}

		err = bm_storage_write(&s0_storage, write_offset, rb_data, rb_size, NULL);
		if (err) {
			LOG_ERR("Write request failed at offset %#x (err %d)", write_offset, err);
			atomic_set(&ongoing, 0);
			ring_buf_get_finish(&ring_buf, 0);
			return IMG_MGMT_ERR_INVALID_IMAGE_TOO_LARGE;
		}
	}

	return IMG_MGMT_ERR_OK;
}

static void bm_storage_evt_handler_writes(struct bm_storage_evt *evt)
{
	ring_buf_get_finish(&ring_buf, evt->len);
	write_offset += evt->len;

	atomic_set(&ongoing, 0);

	(void)claim_and_write();
}

static void storage_init(void)
{
	static int storage_initialized;

	if (!storage_initialized) {
		bm_storage_init(&s0_storage, &s0_config);
		bm_storage_init(&s1_storage, &s1_config);
		storage_initialized = 1;

		const struct bm_storage_info *info = bm_storage_nvm_info_get(&s0_storage);

		storage_wear_unit = info->wear_unit;
		storage_write_size_max = (storage_wear_unit *
					  CONFIG_BM_MCUMGR_GRP_IMG_NVM_WRITE_BLOCKS_MAX);

		if (!IS_ALIGNED(CONFIG_BM_MCUMGR_GRP_IMG_BUFFER_SZ, storage_wear_unit)) {
			LOG_WRN("Image buffer size (%#x) not aligned with NVM wear unit (%#x)",
				CONFIG_BM_MCUMGR_GRP_IMG_BUFFER_SZ, storage_wear_unit);
		}
	}
}

int img_mgmt_read(int slot, unsigned int offset, void *dst, unsigned int num_bytes)
{
	struct bm_storage *slot_p = (slot == 0) ? &s0_storage : &s1_storage;

	storage_init();
	return bm_storage_read(slot_p, offset, dst, num_bytes);
}

int img_mgmt_write_image_data(unsigned int offset, const void *data, unsigned int num_bytes,
			      bool last)
{
	int rb_space;

	storage_init();
	if (offset == 0) {
		/* New image. */
		write_offset = 0;
	}

	if ((offset + num_bytes) > S0_SIZE) {
		return IMG_MGMT_ERR_INVALID_IMAGE_TOO_LARGE;
	}

	rb_space = ring_buf_space_get(&ring_buf);
	if (rb_space < num_bytes) {
		return MGMT_ERR_EBUSY;
	} else {
		ring_buf_put(&ring_buf, data, num_bytes);
	}

	last_data = last;

	return claim_and_write();
}

bool img_mgmt_write_in_progress(void)
{
	return bm_storage_is_busy(&s0_storage);
}
