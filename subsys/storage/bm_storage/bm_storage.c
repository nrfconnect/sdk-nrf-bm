/*
 * Copyright (c) 2016-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/toolchain.h>
#include <sys/types.h>
#include <bm/storage/bm_storage.h>

TOOLCHAIN_DISABLE_WARNING("-Wdeprecated-declarations");

static bool is_within_bounds(const struct bm_storage *storage, uint32_t addr, uint32_t len)
{
	if (!storage->flags.has_absolute_addressing) {
		addr += storage->addr;
	}

	return ((addr >= storage->addr) &&			 /* after/at the start address */
		(addr + len <= storage->addr + storage->size) && /* no larger than the partition */
		(addr + len > addr));				 /* no overflow */
}

int bm_storage_init(struct bm_storage *storage, const struct bm_storage_config *config)
{
	int err;

	if (!storage || !config) {
		return -EFAULT;
	}

	if (storage->flags.is_initialized) {
		return -EPERM;
	}

	if (!config->api) {
		return -EFAULT;
	}

	storage->api = config->api;
	storage->evt_handler = config->evt_handler;

	storage->flags.pad_write_operations = config->flags.pad_write_operations;

	if (config->start_addr || config->end_addr) {
		storage->addr = config->start_addr;
		storage->size = config->end_addr - config->start_addr;
		storage->flags.has_absolute_addressing = true;
	} else {
		storage->addr = config->addr;
		storage->size = config->size;
		storage->flags.has_absolute_addressing = false;
	}

	err = storage->api->init(storage, config);
	if (err) {
		return err;
	}

	__ASSERT(storage->nvm_info, "NVM info not provided by backend");


	storage->flags.is_initialized = true;

	return 0;
}

int bm_storage_uninit(struct bm_storage *storage)
{
	int err;

	if (!storage) {
		return -EFAULT;
	}

	if (!storage->flags.is_initialized) {
		return -EPERM;
	}

	err = storage->api->uninit(storage);

	if (err == 0) {
		/* Prevent further operations */
		storage->flags.is_initialized = false;
	}

	return err;
}

int bm_storage_read(const struct bm_storage *storage, uint32_t src, void *dest, uint32_t len)
{
	if (!storage || !dest) {
		return -EFAULT;
	}

	if (!storage->flags.is_initialized) {
		return -EPERM;
	}

	if (len == 0) {
		return -EINVAL;
	}

	if (!is_within_bounds(storage, src, len)) {
		return -EINVAL;
	}

	return storage->api->read(storage, src, dest, len);
}

int bm_storage_write(const struct bm_storage *storage, uint32_t dest, const void *src,
		     uint32_t len, void *ctx)
{
	if (!storage || !src) {
		return -EFAULT;
	}

	if (!storage->flags.is_initialized) {
		return -EPERM;
	}

	if (!IS_ALIGNED(dest, storage->nvm_info->program_unit)) {
		return -EINVAL;
	}

	if (!IS_ALIGNED(len, storage->nvm_info->program_unit) &&
	    !storage->flags.pad_write_operations) {
		return -EINVAL;
	}

	if (!is_within_bounds(storage, dest, len)) {
		return -EINVAL;
	}

	return storage->api->write(storage, dest, src, len, ctx);
}

int bm_storage_erase(const struct bm_storage *storage, uint32_t addr, uint32_t len, void *ctx)
{
	if (!storage) {
		return -EFAULT;
	}

	if (!storage->flags.is_initialized) {
		return -EPERM;
	}

	if (!IS_ALIGNED(addr, storage->nvm_info->erase_unit) ||
	    !IS_ALIGNED(len,  storage->nvm_info->erase_unit)) {
		return -EINVAL;
	}

	if (!is_within_bounds(storage, addr, len)) {
		return -EINVAL;
	}

	return storage->api->erase(storage, addr, len, ctx);
}

bool bm_storage_is_busy(const struct bm_storage *storage)
{
	if (!storage) {
		return false;
	}

	if (!storage->flags.is_initialized) {
		return false;
	}

	return storage->api->is_busy(storage);
}

TOOLCHAIN_ENABLE_WARNING("-Wdeprecated-declarations");
