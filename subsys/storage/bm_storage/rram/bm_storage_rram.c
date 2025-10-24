/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <stdint.h>
#include <zephyr/sys/atomic.h>
#include <nrfx_rramc.h>
#include <bm/storage/bm_storage.h>
#include <bm/storage/bm_storage_backend.h>

/* 128-bit word line. This is the optimal size to fully utilize RRAM 128-bit word line with ECC
 * (error correction code) and minimize ECC updates overhead, due to these updates happening
 * per-line.
 */
#define RRAMC_WRITE_BLOCK_SIZE 16

static nrfx_rramc_config_t rramc_config = NRFX_RRAMC_DEFAULT_CONFIG(RRAMC_WRITE_BLOCK_SIZE);

struct bm_storage_rram_state {
	bool is_rramc_init;
	atomic_t operation_ongoing;
};

static struct bm_storage_rram_state state;

static void event_send(const struct bm_storage *storage, struct bm_storage_evt *evt)
{
	if (!storage->evt_handler) {
		return;
	}

	storage->evt_handler(evt);
}

int bm_storage_backend_init(struct bm_storage *storage)
{
	int err;
	nrfx_err_t nrfx_err;

	/* If it's already initialized, return early successfully.
	 * This is to support more than one client initialization.
	 */
	if (state.is_rramc_init) {
		return 0;
	}

	/* RRAMC backend must be initialized consistently from one context only.
	 * NRFX does not guarantee thread-safety or re-entrancy.
	 * Once the driver initialized, it will neither be re-initialized nor uninitialized.
	 */
	if (!atomic_cas(&state.operation_ongoing, 0, 1)) {
		return -EBUSY;
	}

	nrfx_err = nrfx_rramc_init(&rramc_config, NULL);
	if (nrfx_err != NRFX_SUCCESS) {
		err = -EIO;
	} else {
		state.is_rramc_init = true;
		err = 0;
	}

	atomic_set(&state.operation_ongoing, 0);

	return err;
}

int bm_storage_backend_read(const struct bm_storage *storage, uint32_t src, void *dest,
			    uint32_t len)
{
	if (!state.is_rramc_init) {
		return -EPERM;
	}

	nrfx_rramc_buffer_read(dest, src, len);

	return 0;
}

int bm_storage_backend_write(const struct bm_storage *storage, uint32_t dest,
			     const void *src, uint32_t len, void *ctx)
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
		.dispatch_type = BM_STORAGE_EVT_DISPATCH_SYNC,
		.result = 0,
		.addr = dest,
		.src = src,
		.len = len,
		.ctx = ctx
	};

	event_send(storage, &evt);

	return 0;
}

bool bm_storage_backend_is_busy(const struct bm_storage *storage)
{
	/* Always appear as busy if driver is not initialized. */
	if (!state.is_rramc_init) {
		return true;
	}

	return (atomic_get(&state.operation_ongoing) == 1);
}

const struct bm_storage_info bm_storage_info = {
	.program_unit = RRAMC_WRITE_BLOCK_SIZE,
	.no_explicit_erase = true
};
