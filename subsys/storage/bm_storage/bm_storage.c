/*
 * Copyright (c) 2016-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <sys/types.h>
#include <bm/storage/bm_storage.h>
#include <bm/storage/bm_storage_backend.h>

__weak int bm_storage_backend_uninit(struct bm_storage *storage)
{
	return -ENOTSUP;
}

__weak int bm_storage_backend_erase(const struct bm_storage *storage, uint32_t addr,
					uint32_t len, void *ctx)
{
	return -ENOTSUP;
}

__weak bool bm_storage_backend_is_busy(const struct bm_storage *storage)
{
	return false;
}

static inline bool is_within_bounds(off_t addr, size_t len, off_t boundary_start,
				    size_t boundary_size)
{
	return (addr >= boundary_start && (addr < (boundary_start + boundary_size)) &&
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

	if (bm_storage_info.program_unit == 0) {
		return -EIO;
	}

	storage->nvm_info = &bm_storage_info;
	storage->evt_handler = config->evt_handler;
	storage->start_addr = config->start_addr;
	storage->end_addr = config->end_addr;

	err = bm_storage_backend_init(storage);
	if (err) {
		return err;
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

	err = bm_storage_backend_uninit(storage);

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
		return -EFAULT;
	}

	return bm_storage_backend_read(storage, src, dest, len);
}

int bm_storage_write(const struct bm_storage *storage, uint32_t dest, const void *src,
		     uint32_t len, void *ctx)
{
	if (!storage || !src) {
		return -EFAULT;
	}

	if (!storage->initialized) {
		return -EPERM;
	}

	if (len == 0 || len % storage->nvm_info->program_unit != 0) {
		return -EINVAL;
	}

	if (!is_within_bounds(dest, len, storage->start_addr,
		storage->end_addr - storage->start_addr)) {
		return -EFAULT;
	}

	return bm_storage_backend_write(storage, dest, src, len, ctx);
}

int bm_storage_erase(const struct bm_storage *storage, uint32_t addr, uint32_t len, void *ctx)
{
	if (!storage) {
		return -EFAULT;
	}

	if (!storage->initialized) {
		return -EPERM;
	}

	if (storage->nvm_info->no_explicit_erase) {
		return -ENOTSUP;
	}

	if (storage->nvm_info->erase_unit == 0) {
		return -EIO;
	}

	if (len == 0 || len % storage->nvm_info->erase_unit != 0) {
		return -EINVAL;
	}

	if (!is_within_bounds(addr, len, storage->start_addr,
			      storage->end_addr - storage->start_addr)) {
		return -EFAULT;
	}

	return bm_storage_backend_erase(storage, addr, len, ctx);
}

bool bm_storage_is_busy(const struct bm_storage *storage)
{
	if (!storage) {
		return true;
	}

	if (!storage->initialized) {
		return true;
	}

	return bm_storage_backend_is_busy(storage);
}
