/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <zephyr/sys/atomic.h>
#include <nrfx_rramc.h>
#include <nrf_error.h>
#include <bm_storage.h>

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

static uint32_t bm_storage_rram_init(struct bm_storage *storage)
{
	uint32_t err;
	nrfx_err_t nrfx_err;

	/* If it's already initialized, return early successfully.
	 * This is to support more than one client initialization.
	 */
	if (state.is_rramc_init) {
		return NRF_SUCCESS;
	}

	/* RRAMC backend must be initialized consistently from one context only.
	 * NRFX does not guarantee thread-safety or re-entrancy.
	 * Once the driver initialized, it will neither be re-initialized nor uninitialized.
	 */
	if (!atomic_cas(&state.operation_ongoing, 0, 1)) {
		return NRF_ERROR_BUSY;
	}

	nrfx_err = nrfx_rramc_init(&rramc_config, NULL);
	if (nrfx_err != NRFX_SUCCESS) {
		err = NRF_ERROR_INTERNAL;
	} else {
		state.is_rramc_init = true;
		err = NRF_SUCCESS;
	}

	atomic_set(&state.operation_ongoing, 0);

	return err;
}

static uint32_t bm_storage_rram_uninit(struct bm_storage *storage)
{
	if (!state.is_rramc_init) {
		return NRF_ERROR_FORBIDDEN;
	}

	return NRF_SUCCESS;
}

static uint32_t bm_storage_rram_read(const struct bm_storage *storage, uint32_t src, void *dest,
				     uint32_t len)
{
	if (!state.is_rramc_init) {
		return NRF_ERROR_FORBIDDEN;
	}

	nrfx_rramc_buffer_read(dest, src, len);

	return NRF_SUCCESS;
}

static uint32_t bm_storage_rram_write(const struct bm_storage *storage, uint32_t dest,
				      const void *src, uint32_t len, void *ctx)
{
	if (!state.is_rramc_init) {
		return NRF_ERROR_FORBIDDEN;
	}

	if (!atomic_cas(&state.operation_ongoing, 0, 1)) {
		return NRF_ERROR_BUSY;
	}

	nrfx_rramc_bytes_write(dest, src, len);

	/* Clear the atomic before sending the event, to allow API calls in the event context. */
	atomic_set(&state.operation_ongoing, 0);

	struct bm_storage_evt evt = {
		.id = BM_STORAGE_EVT_WRITE_RESULT,
		.dispatch_type = BM_STORAGE_EVT_DISPATCH_SYNC,
		.result = NRF_SUCCESS,
		.addr = dest,
		.src = src,
		.len = len,
		.ctx = ctx
	};

	event_send(storage, &evt);

	return NRF_SUCCESS;
}

static uint32_t bm_storage_rram_erase(const struct bm_storage *storage, uint32_t addr, uint32_t len,
				      void *ctx)
{
	/* Do nothing. RRAM does not require erase functionality. */

	return NRF_ERROR_NOT_SUPPORTED;
}

static bool bm_storage_rram_is_busy(const struct bm_storage *storage)
{
	/* Always appear as busy if driver is not initialized. */
	if (!state.is_rramc_init) {
		return true;
	}

	return (atomic_get(&state.operation_ongoing) == 1);
}

const struct bm_storage_api bm_storage_api = {
	.init = bm_storage_rram_init,
	.uninit = bm_storage_rram_uninit,
	.write = bm_storage_rram_write,
	.read = bm_storage_rram_read,
	.erase = bm_storage_rram_erase,
	.is_busy = bm_storage_rram_is_busy
};

const struct bm_storage_info bm_storage_info = {
	.program_unit = RRAMC_WRITE_BLOCK_SIZE,
	.no_explicit_erase = true
};
