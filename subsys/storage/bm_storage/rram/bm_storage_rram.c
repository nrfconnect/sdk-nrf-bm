/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <mdk/nrf_peripherals.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <nrfx_rramc.h>
#include <bm/storage/bm_storage.h>

/* RRAM word line size in bytes, derived from hardware definition (in bits). */
#define RRAMC_WRITE_BLOCK_SIZE (RRAMC_NRRAMWORDSIZE / BITS_PER_BYTE)

static nrfx_rramc_config_t rramc_config = NRFX_RRAMC_DEFAULT_CONFIG(RRAMC_WRITE_BLOCK_SIZE);

static const struct bm_storage_info bm_storage_info = {
	.program_unit = RRAMC_WRITE_BLOCK_SIZE,
	.erase_unit = RRAMC_WRITE_BLOCK_SIZE,
	.wear_unit = RRAMC_WRITE_BLOCK_SIZE,
	.erase_value = 0xFF,
	.is_erase_before_write = false,
};

struct bm_storage_rram_state {
	atomic_t refcount;
	bool is_rramc_init;
	atomic_t operation_ongoing;
};

static struct bm_storage_rram_state state;

static uint8_t erase_buf[RRAMC_WRITE_BLOCK_SIZE];

static void event_send(const struct bm_storage *storage, struct bm_storage_evt *evt)
{
	if (!storage->evt_handler) {
		return;
	}

	storage->evt_handler(evt);
}

static int bm_storage_rramc_init(struct bm_storage *storage, const struct bm_storage_config *config)
{
	int err;

	atomic_inc(&state.refcount);

	if (atomic_get(&state.refcount) == 1) {
		atomic_set(&state.operation_ongoing, 0);

		/* First user: initialize hardware */
		err = nrfx_rramc_init(&rramc_config, NULL);
		if (err) {
			atomic_dec(&state.refcount);
			return err;
		}

		state.is_rramc_init = true;
	}

	storage->nvm_info = &bm_storage_info;

	(void)memset(erase_buf, (int)(bm_storage_info.erase_value & 0xFF), sizeof(erase_buf));

	return 0;
}

static int bm_storage_rramc_uninit(struct bm_storage *storage)
{
	if (atomic_get(&state.refcount) == 0) {
		return -EPERM;
	}

	if (atomic_dec(&state.refcount) == 1) {
		/* Last user: uninitialize hardware. */
		if (atomic_get(&state.operation_ongoing) == 1) {
			/* Restore refcount on failure. */
			atomic_inc(&state.refcount);
			return -EBUSY;
		}

		nrfx_rramc_uninit();
		state.is_rramc_init = false;
	}

	return 0;
}

static int bm_storage_rramc_read(const struct bm_storage *storage, uint32_t src, void *dest,
				 uint32_t len)
{
	if (!state.is_rramc_init) {
		return -EPERM;
	}

	(void)nrfx_rramc_buffer_read(dest, src, len);

	return 0;
}

static int bm_storage_rramc_write(const struct bm_storage *storage, uint32_t dest, const void *src,
				  uint32_t len, void *ctx)
{
	if (!state.is_rramc_init) {
		return -EPERM;
	}

	if (!atomic_cas(&state.operation_ongoing, 0, 1)) {
		return -EBUSY;
	}

	nrfx_rramc_bytes_write(dest, src, len);

	/* Clear the atomic before sending the event, to allow API calls in the event context. */
	atomic_set(&state.operation_ongoing, 0);

	struct bm_storage_evt evt = {
		.id = BM_STORAGE_EVT_WRITE_RESULT,
		.is_async = false,
		.result = 0,
		.addr = dest,
		.src = src,
		.len = len,
		.ctx = ctx
	};

	event_send(storage, &evt);

	return 0;
}

static int bm_storage_rramc_erase(const struct bm_storage *storage, uint32_t addr, uint32_t len,
				  void *ctx)
{
	if (!state.is_rramc_init) {
		return -EPERM;
	}

	if (!atomic_cas(&state.operation_ongoing, 0, 1)) {
		return -EBUSY;
	}

	for (uint32_t offset = 0; offset < len; offset += RRAMC_WRITE_BLOCK_SIZE) {
		nrfx_rramc_bytes_write(addr + offset, erase_buf, RRAMC_WRITE_BLOCK_SIZE);
	}

	atomic_set(&state.operation_ongoing, 0);

	struct bm_storage_evt evt = {
		.id = BM_STORAGE_EVT_ERASE_RESULT,
		.is_async = false,
		.result = 0,
		.addr = addr,
		.len = len,
		.ctx = ctx
	};

	event_send(storage, &evt);

	return 0;
}

static bool bm_storage_rramc_is_busy(const struct bm_storage *storage)
{
	/* Always appear as busy if driver is not initialized. */
	if (!state.is_rramc_init) {
		return true;
	}

	return (atomic_get(&state.operation_ongoing) == 1);
}

const struct bm_storage_api bm_storage_rram_api = {
	.init = bm_storage_rramc_init,
	.uninit = bm_storage_rramc_uninit,
	.read = bm_storage_rramc_read,
	.write = bm_storage_rramc_write,
	.erase = bm_storage_rramc_erase,
	.is_busy = bm_storage_rramc_is_busy,
};
