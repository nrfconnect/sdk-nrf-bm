/*
 * Copyright (c) 2018-2021 mcumgr authors
 *
 * SPDX-License-Identifier: Apache-2.0
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

LOG_MODULE_DECLARE(mcumgr_img_grp, CONFIG_MCUMGR_GRP_IMG_LOG_LEVEL);

#define SLOT0_PARTITION		slot0_partition
#define S0_PARTITION DT_NODELABEL(slot0_partition)
#define S0_START DT_REG_ADDR(S0_PARTITION)
#define S0_SIZE DT_REG_SIZE(S0_PARTITION)
#define WORST_CASE_TAIL_WRITES 2
#define RRAMC_WRITE_BLOCK_SIZE 16

static struct bm_storage s0_storage;

/* SLOT0_PARTITION and SLOT1_PARTITION are not checked because
 * there is not conditional code that depends on them. If they do
 * not exist compilation will fail, but in case if some of other
 * partitions do not exist, code will compile and will not work
 * properly.
 */

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
struct bm_storage_config storage_config = {
	.evt_handler = bm_storage_evt_handler_writes,
	.start_addr = S0_START,
	.end_addr = S0_START+S0_SIZE,
};

static void storage_init(void)
{
	static int storage_initialized;

	if (!storage_initialized) {
		bm_storage_init(&s0_storage, &storage_config);
		storage_initialized = 1;
	}
}

int img_mgmt_vercmp(const struct image_version *a, const struct image_version *b)
{
	if (a->iv_major < b->iv_major) {
		return -1;
	} else if (a->iv_major > b->iv_major) {
		return 1;
	}

	if (a->iv_minor < b->iv_minor) {
		return -1;
	} else if (a->iv_minor > b->iv_minor) {
		return 1;
	}

	if (a->iv_revision < b->iv_revision) {
		return -1;
	} else if (a->iv_revision > b->iv_revision) {
		return 1;
	}

	return 0;
}

int img_mgmt_read(unsigned int offset, void *dst, unsigned int num_bytes)
{
	storage_init();
	return bm_storage_read(&s0_storage, s0_storage.start_addr + offset,
		dst, num_bytes);
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
		LOG_ERR("last size %d pad %d", rb_size, pad);
		rb_size = ring_buf_get_claim(&ring_buf, &rb_data, PKTBUF_SZ);
		ring_buf_get_finish(&ring_buf, rb_size);
		int err = bm_storage_write(&s0_storage, s0_storage.start_addr + write_offset,
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
				s0_storage.start_addr + write_offset, rb_data, rb_size, NULL);
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
		int err = bm_storage_write(&s0_storage, s0_storage.start_addr + write_offset,
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

/**
 * Verifies an upload request and indicates the actions that should be taken
 * during processing of the request.  This is a "read only" function in the
 * sense that it doesn't write anything to flash and doesn't modify any global
 * variables.
 *
 * @param req		The upload request to inspect.
 * @param action	On success, gets populated with information about how to process
 *			the request.
 *
 * @return 0 if processing should occur; A MGMT_ERR code if an error response should be sent
 *	   instead.
 */
int img_mgmt_upload_inspect(const struct img_mgmt_upload_req *req,
			    struct img_mgmt_upload_action *action)
{
	const struct image_header *hdr;
	struct image_version cur_ver;
	int rc;

	memset(action, 0, sizeof(*action));

	if (req->off == SIZE_MAX) {
		/* Request did not include an `off` field. */
		IMG_MGMT_UPLOAD_ACTION_SET_RC_RSN(action, img_mgmt_err_str_hdr_malformed);
		LOG_DBG("Request did not include an `off` field");
		return IMG_MGMT_ERR_INVALID_OFFSET;
	}

	if (req->off == 0) {
		/* First upload chunk. */

		if (req->img_data.len < sizeof(struct image_header)) {
			/*  Image header is the first thing in the image */
			IMG_MGMT_UPLOAD_ACTION_SET_RC_RSN(action, img_mgmt_err_str_hdr_malformed);
			LOG_DBG("Image data too short: %u < %u", req->img_data.len,
				sizeof(struct image_header));
			return IMG_MGMT_ERR_INVALID_IMAGE_HEADER;
		}

		if (req->size == SIZE_MAX) {
			/* Request did not include a `len` field. */
			IMG_MGMT_UPLOAD_ACTION_SET_RC_RSN(action, img_mgmt_err_str_hdr_malformed);
			LOG_DBG("Request did not include a `len` field");
			return IMG_MGMT_ERR_INVALID_LENGTH;
		}

		action->size = req->size;

		hdr = (struct image_header *)req->img_data.value;
		if (hdr->ih_magic != IMAGE_MAGIC) {
			IMG_MGMT_UPLOAD_ACTION_SET_RC_RSN(action, img_mgmt_err_str_magic_mismatch);
			LOG_DBG("Magic mismatch: %08X != %08X", hdr->ih_magic, IMAGE_MAGIC);
			return IMG_MGMT_ERR_INVALID_IMAGE_HEADER_MAGIC;
		}

		if (req->data_sha.len > IMG_MGMT_DATA_SHA_LEN) {
			LOG_DBG("Invalid hash length: %u", req->data_sha.len);
			return IMG_MGMT_ERR_INVALID_HASH;
		}

		/*
		 * If request includes proper data hash we can check whether there is
		 * upload in progress (interrupted due to e.g. link disconnection) with
		 * the same data hash so we can just resume it by simply including
		 * current upload offset in response.
		 */
		if ((req->data_sha.len > 0) && (g_img_mgmt_state.area_id != -1)) {
			if ((g_img_mgmt_state.data_sha_len == req->data_sha.len) &&
			    !memcmp(g_img_mgmt_state.data_sha, req->data_sha.value,
				    req->data_sha.len)) {
				return IMG_MGMT_ERR_OK;
			}
		}
		/* Check that the area is of sufficient size to store the new image */
		if (req->size > S0_SIZE) {
			IMG_MGMT_UPLOAD_ACTION_SET_RC_RSN(action,
				img_mgmt_err_str_image_too_large);
			LOG_DBG("Upload too large for slot: %u > %u", req->size,
				S0_SIZE);
			return IMG_MGMT_ERR_INVALID_IMAGE_TOO_LARGE;
		}

		if (req->upgrade) {
			/* User specified upgrade-only. Make sure new image version is
			 * greater than that of the currently running image.
			 */
			rc = img_mgmt_my_version(&cur_ver);
			if (rc != 0) {
				LOG_DBG("Version get failed: %d", rc);
				return IMG_MGMT_ERR_VERSION_GET_FAILED;
			}

			if (img_mgmt_vercmp(&cur_ver, &hdr->ih_ver) >= 0) {
				IMG_MGMT_UPLOAD_ACTION_SET_RC_RSN(action,
					img_mgmt_err_str_downgrade);
				LOG_DBG("Downgrade: %d.%d.%d.%d, expected: %d.%d.%d.%d",
					cur_ver.iv_major, cur_ver.iv_minor, cur_ver.iv_revision,
					cur_ver.iv_build_num, hdr->ih_ver.iv_major,
					hdr->ih_ver.iv_minor, hdr->ih_ver.iv_revision,
					hdr->ih_ver.iv_build_num);
				return IMG_MGMT_ERR_CURRENT_VERSION_IS_NEWER;
			}
		}

#ifndef CONFIG_IMG_ERASE_PROGRESSIVELY
		rc = img_mgmt_flash_check_empty(action->area_id);
		if (rc < 0) {
			LOG_DBG("Flash check empty failed: %d", rc);
			return rc;
		}

		action->erase = (rc == 0);
#endif
	} else {
		/* Continuation of upload. */
		action->area_id = g_img_mgmt_state.area_id;
		action->size = g_img_mgmt_state.size;

		if (req->off != g_img_mgmt_state.off) {
			/*
			 * Invalid offset. Drop the data, and respond with the offset we're
			 * expecting data for.
			 */
			LOG_DBG("Invalid offset: %08x, expected: %08x", req->off,
				g_img_mgmt_state.off);
			return IMG_MGMT_ERR_OK;
		}

		if ((req->off + req->img_data.len) > action->size) {
			/* Data overrun, the amount of data written would be more than the size
			 * of the image that the client originally sent
			 */
			IMG_MGMT_UPLOAD_ACTION_SET_RC_RSN(action, img_mgmt_err_str_data_overrun);
			LOG_DBG("Data overrun: %u + %u > %llu", req->off, req->img_data.len,
				action->size);
			return IMG_MGMT_ERR_INVALID_IMAGE_DATA_OVERRUN;
		}
	}

	action->write_bytes = req->img_data.len;
	action->proceed = true;
	IMG_MGMT_UPLOAD_ACTION_SET_RC_RSN(action, NULL);

	return IMG_MGMT_ERR_OK;
}
