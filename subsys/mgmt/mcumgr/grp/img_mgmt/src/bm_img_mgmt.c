/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/drivers/flash.h>
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

#define SLOT0_PARTITION		slot0_partition
#define SLOT1_PARTITION		slot1_partition
#define S0_PARTITION DT_NODELABEL(slot0_partition)
#define S0_START DT_REG_ADDR(S0_PARTITION)
#define S0_SIZE DT_REG_SIZE(S0_PARTITION)
#define S1_PARTITION DT_NODELABEL(slot1_partition)
#define S1_START DT_REG_ADDR(S1_PARTITION)
#define S1_SIZE DT_REG_SIZE(S1_PARTITION)
#define WORST_CASE_TAIL_WRITES 2
#define RRAMC_WRITE_BLOCK_SIZE 16

static struct bm_storage s0_storage;
static struct bm_storage s1_storage;

#define CHUNK_SZ (16 * RRAMC_WRITE_BLOCK_SIZE)
#define PKTBUF_SZ (2 * 8 * CHUNK_SZ)

#if defined(BM_STORAGE_BACKEND_SD)
#define QUEUE_THRESHOLD (CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE - WORST_CASE_TAIL_WRITES)
BUILD_ASSERT(CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE > WORST_CASE_TAIL_WRITES);
BUILD_ASSERT(((PKTBUF_SZ%CHUNK_SZ) == 0) &&
	(PKTBUF_SZ >= (CHUNK_SZ * CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE)));
#else
#define QUEUE_THRESHOLD 2
#endif

RING_BUF_DECLARE(ring_buf, PKTBUF_SZ);

static uint8_t ongoing;
static int write_offset;

static void bm_storage_evt_handler_writes(struct bm_storage_evt *evt)
{
	if (ongoing) {
		ongoing--;
	}
}

struct bm_storage_config s0_config = {
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

struct bm_storage_config s1_config = {
#if defined(CONFIG_BM_STORAGE_BACKEND_SD)
	.api = &bm_storage_sd_api,
#else
	.api = &bm_storage_rram_api,
#endif
	.evt_handler = bm_storage_evt_handler_writes,
	.addr = S1_START,
	.size = S1_SIZE,
};

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
	storage_init();
	if (slot == 0) {
		return bm_storage_read(&s0_storage, offset,
			dst, num_bytes);
	} else {
		return bm_storage_read(&s1_storage, offset,
			dst, num_bytes);
	}
}

int img_mgmt_write_image_data(unsigned int offset, const void *data, unsigned int num_bytes,
			      bool last)
{
	int rc = IMG_MGMT_ERR_OK;
	uint8_t *rb_data;

	if (offset == 0) {
		storage_init();
	}
	int rb_freespace = ring_buf_space_get(&ring_buf);

	if ((rb_freespace - ongoing * CHUNK_SZ) < num_bytes) {
		rc = IMG_MGMT_ERR_BUSY;
	} else {
		ring_buf_put(&ring_buf, data, num_bytes);
	}

	if (last) {
		int rb_size = ring_buf_size_get(&ring_buf);
		int pad = CHUNK_SZ - (rb_size % CHUNK_SZ);

		ring_buf_put(&ring_buf, data, pad);
		rb_size = ring_buf_get_claim(&ring_buf, &rb_data, PKTBUF_SZ);
		ring_buf_get_finish(&ring_buf, rb_size);
		int err = bm_storage_write(&s0_storage, write_offset,
			rb_data, rb_size, NULL);

		if (err) {
			rc = IMG_MGMT_ERR_INVALID_IMAGE_TOO_LARGE;
			goto out;
		}
		write_offset += rb_size;
		if (ring_buf_size_get(&ring_buf)) {
			rb_size = ring_buf_get_claim(&ring_buf, &rb_data, PKTBUF_SZ);
			ring_buf_get_finish(&ring_buf, rb_size);
			int err = bm_storage_write(&s0_storage,
				write_offset, rb_data, rb_size, NULL);
			if (err) {
				rc = IMG_MGMT_ERR_INVALID_IMAGE_TOO_LARGE;
				goto out;
			}
			write_offset += rb_size;
		}
	}
	while (ongoing < QUEUE_THRESHOLD && ring_buf_size_get(&ring_buf) >= CHUNK_SZ) {
		int rb_size = ring_buf_get_claim(&ring_buf, &rb_data, CHUNK_SZ);

		ring_buf_get_finish(&ring_buf, rb_size);
		int err = bm_storage_write(&s0_storage, write_offset,
			rb_data, rb_size, NULL);

		if (err) {
			rc = IMG_MGMT_ERR_INVALID_IMAGE_TOO_LARGE;
			goto out;
		}
		ongoing++;
		write_offset += rb_size;
	}
out:
	return rc;
}
