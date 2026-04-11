/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/logging/log.h>
#include <bootutil/bootutil_public.h>
#include <assert.h>

#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <img_mgmt.h>

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

#define CHUNK_SZ (CONFIG_MCUMGR_GRP_IMG_IMAGE_BUFFER_CHUNK_SZ)
/* Allow writing multiple chunks at a time. Though, limit the number so that we clear up space for
 * new data from BLE.
 */
#define STORAGE_MAX_WRITE_SIZE (CHUNK_SZ * CONFIG_MCUMGR_GRP_IMG_IMAGE_BUFFER_WRITE_CHUNKS_MAX)
#define PKTBUF_SZ (CONFIG_MCUMGR_GRP_IMG_IMAGE_BUFFER_SZ)

#if defined(BM_STORAGE_BACKEND_SD)
BUILD_ASSERT((PKTBUF_SZ % CHUNK_SZ) == 0);
BUILD_ASSERT((CHUNK_SZ % RRAMC_WRITE_BLOCK_SIZE) == 0);
#endif

RING_BUF_DECLARE(ring_buf, PKTBUF_SZ);

/* Write offset. */
static int write_offset;
/* Write operation ongoing. */
static bool ongoing;
/* Last data in image is received. */
static bool last_data;

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
		.is_write_padded = 1
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
	int err = 0;
	int rb_size;
	uint8_t *rb_data;

	if (!ongoing && ((ring_buf_size_get(&ring_buf) >= CHUNK_SZ) || last_data)) {
		rb_size = ring_buf_get_claim(&ring_buf, &rb_data, STORAGE_MAX_WRITE_SIZE);

		if (!last_data) {
			/* Make sure we are writing full chuncks until end of image. */
			rb_size = (rb_size / CHUNK_SZ) * CHUNK_SZ;
		}

		err = bm_storage_write(&s0_storage, write_offset,
				       rb_data, rb_size, NULL);
		if (err) {
			LOG_ERR("Write request failed at offset %x", write_offset);
			return IMG_MGMT_ERR_INVALID_IMAGE_TOO_LARGE;
		}

		ongoing = true;
		write_offset += rb_size;
	}

	return IMG_MGMT_ERR_OK;
}

static void bm_storage_evt_handler_writes(struct bm_storage_evt *evt)
{
	ring_buf_get_finish(&ring_buf, evt->len);
	ongoing = false;

	(void)claim_and_write();
}

static void storage_init(void)
{
	static int storage_initialized;

	if (!storage_initialized) {
		bm_storage_init(&s0_storage, &s0_config);
		bm_storage_init(&s1_storage, &s1_config);
		storage_initialized = 1;
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
	int rb_freespace;

	storage_init();
	if (offset == 0) {
		/* New image. */
		write_offset = 0;
	}

	rb_freespace = ring_buf_space_get(&ring_buf);
	if (rb_freespace < num_bytes) {
		LOG_DBG("busy");
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
