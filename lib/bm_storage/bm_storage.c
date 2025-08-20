/*
 * Copyright (c) 2016-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <sys/types.h>
#include <nrf_error.h>
#include <bm_storage.h>

static inline bool is_within_bounds(off_t addr, size_t len, off_t boundary_start,
				    size_t boundary_size)
{
	return (addr >= boundary_start && (addr < (boundary_start + boundary_size)) &&
		(len <= (boundary_start + boundary_size - addr)));
}

uint32_t bm_storage_init(struct bm_storage *storage)
{
	uint32_t err;

	if (!storage) {
		return NRF_ERROR_NULL;
	}

	storage->nvm_info = &bm_storage_info;

	err = bm_storage_api.init(storage);
	if (err != NRF_SUCCESS) {
		return err;
	}

	storage->api = &bm_storage_api;
}

uint32_t bm_storage_uninit(struct bm_storage *storage)
{
	uint32_t err;

	if (!storage) {
		return NRF_ERROR_NULL;
	}

	if (!storage->api) {
		return NRF_ERROR_INVALID_STATE;
	}

	err = (storage->api)->uninit(storage);

	if (err == NRF_SUCCESS) {
		storage->api = NULL;
		storage->nvm_info = NULL;
	}

	return err;
}

uint32_t bm_storage_read(const struct bm_storage *storage, uint32_t src, void *dest, uint32_t len)
{
	if (!storage || !dest) {
		return NRF_ERROR_NULL;
	}

	if (!storage->api || !storage->nvm_info) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (len == 0) {
		return NRF_ERROR_INVALID_LENGTH;
	}

	if (!is_within_bounds(src, len, storage->start_addr,
		storage->end_addr - storage->start_addr)) {
		return NRF_ERROR_INVALID_ADDR;
	}

	return (storage->api)->read(storage, src, dest, len);
}

uint32_t bm_storage_write(const struct bm_storage *storage, uint32_t dest, const void *src,
			  uint32_t len, void *ctx)
{

	if (!storage || !src) {
		return NRF_ERROR_NULL;
	}

	if (!storage->api || !storage->nvm_info) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (len == 0 || len % storage->nvm_info->program_unit != 0) {
		return NRF_ERROR_INVALID_LENGTH;
	}

	if (!is_within_bounds(dest, len, storage->start_addr,
		storage->end_addr - storage->start_addr)) {
		return NRF_ERROR_INVALID_ADDR;
	}

	return (storage->api)->write(storage, dest, src, len, ctx);
}

uint32_t bm_storage_erase(const struct bm_storage *storage, uint32_t addr, uint32_t len, void *ctx)
{
	if (!storage) {
		return NRF_ERROR_NULL;
	}

	if (!storage->api || !storage->nvm_info) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (len == 0 || len % storage->nvm_info->erase_unit != 0) {
		return NRF_ERROR_INVALID_LENGTH;
	}

	if (!is_within_bounds(addr, len, storage->start_addr,
			      storage->end_addr - storage->start_addr)) {
		return NRF_ERROR_INVALID_ADDR;
	}

	return (storage->api)->erase(storage, addr, len, ctx);
}

bool bm_storage_is_busy(const struct bm_storage *storage)
{
	if (!storage) {
		return true;
	}

	if (!storage->api) {
		return true;
	}

	return (storage->api)->is_busy(storage);
}
