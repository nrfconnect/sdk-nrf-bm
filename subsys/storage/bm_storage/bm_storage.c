/*
 * Copyright (c) 2016-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <sys/types.h>
#include <bm/storage/bm_storage.h>

static bool is_within_bounds(off_t addr, size_t len, off_t boundary_start, size_t boundary_size)
{
	return (addr >= boundary_start &&
			(addr < (boundary_start + boundary_size)) &&
			(len <= (boundary_start + boundary_size - addr)));
}

int bm_storage_init(struct bm_storage *storage, const struct bm_storage_config *config)
{
	int err;

	if (!storage || !config) {
		return -EFAULT;
	}

	if (storage->initialized) {
		return -EPERM;
	}
	if (!config->api) {
		return -EFAULT;
	}

	storage->api = config->api;
	storage->evt_handler = config->evt_handler;
	storage->start_addr = config->start_addr;
	storage->end_addr = config->end_addr;
	storage->flags.is_wear_aligned = config->flags.is_wear_aligned;
	storage->flags.is_write_padded = config->flags.is_write_padded;

	err = storage->api->init(storage, config);
	if (err) {
		return err;
	}

	__ASSERT(storage->nvm_info, "NVM info not provided by backend");

	if (storage->nvm_info->is_erase_before_write) {
		__ASSERT(storage->nvm_info->erase_unit == storage->nvm_info->wear_unit,
			"erase_unit != wear_unit but is_erase_before_write is set");
	}

	if (storage->flags.is_wear_aligned &&
	    storage->nvm_info->is_erase_before_write) {
		return -EINVAL;
	}

	storage->initialized = true;

	return 0;
}

int bm_storage_uninit(struct bm_storage *storage)
{
	int err;

	if (!storage) {
		return -EFAULT;
	}

	if (!storage->initialized) {
		return -EPERM;
	}

	err = storage->api->uninit(storage);

	if (err == 0) {
		/* Prevent further operations */
		storage->initialized = false;
	}

	return err;
}

int bm_storage_read(const struct bm_storage *storage, uint32_t src, void *dest, uint32_t len)
{
	if (!storage || !dest) {
		return -EFAULT;
	}

	if (!storage->initialized) {
		return -EPERM;
	}

	if (len == 0) {
		return -EINVAL;
	}

	if (!is_within_bounds(src, len, storage->start_addr,
			      storage->end_addr - storage->start_addr)) {
		return -EINVAL;
	}

	return storage->api->read(storage, src, dest, len);
}

int bm_storage_write(const struct bm_storage *storage, uint32_t dest, const void *src,
		     uint32_t len, void *ctx)
{
	uint32_t write_align;

	if (!storage || !src) {
		return -EFAULT;
	}

	if (!storage->initialized) {
		return -EPERM;
	}

	if (storage->flags.is_wear_aligned) {
		write_align = storage->nvm_info->wear_unit;
	} else {
		write_align = storage->nvm_info->program_unit;
	}

	if (!IS_ALIGNED(dest, write_align)) {
		return -EINVAL;
	}

	if (!storage->flags.is_write_padded && !IS_ALIGNED(len, write_align)) {
		return -EINVAL;
	}

	if (!is_within_bounds(dest, len, storage->start_addr,
			      storage->end_addr - storage->start_addr)) {
		return -EINVAL;
	}

	return storage->api->write(storage, dest, src, len, ctx);
}

int bm_storage_erase(const struct bm_storage *storage, uint32_t addr, uint32_t len, void *ctx)
{
	uint32_t erase_align;

	if (!storage) {
		return -EFAULT;
	}

	if (!storage->initialized) {
		return -EPERM;
	}

	if (storage->nvm_info->is_erase_before_write) {
		erase_align = storage->nvm_info->erase_unit;
	} else {
		/* Erase operation is simulated, enforce wear-unit alignment if configured */
		if (storage->flags.is_wear_aligned) {
			erase_align = storage->nvm_info->wear_unit;
		} else {
			erase_align = storage->nvm_info->program_unit;
		}
	}

	if (!IS_ALIGNED(addr, erase_align) ||
	    !IS_ALIGNED(len,  erase_align)) {
		return -EINVAL;
	}

	if (!is_within_bounds(addr, len, storage->start_addr,
			      storage->end_addr - storage->start_addr)) {
		return -EINVAL;
	}

	return storage->api->erase(storage, addr, len, ctx);
}

bool bm_storage_is_busy(const struct bm_storage *storage)
{
	if (!storage) {
		return false;
	}

	if (!storage->initialized) {
		return false;
	}

	return storage->api->is_busy(storage);
}

const struct bm_storage_info *bm_storage_nvm_info_get(const struct bm_storage *storage)
{
	if (!storage || !storage->initialized) {
		return NULL;
	}

	return storage->nvm_info;
}
