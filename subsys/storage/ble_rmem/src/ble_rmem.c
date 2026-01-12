/*
 * Copyright (c) 2025 - 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <errno.h>
#include <bm/storage/bm_rmem.h>
#include <bm/storage/ble_rmem.h>

/**
 * @file ble_rmem.c
 * @brief Implementation of the BLE advertising name set-up handling.
 *
 * This file implements the settings handler for the BLE advertising name.
 * It is used to set and get the BLE advertising name from the retention memory.
 * API which mimics the setting subsystem is implemented here for compatibility with
 * the mcumgr mgmt grp settings commands (Only bare-minimal functionality is implemented here)
 */


LOG_MODULE_REGISTER(ble_rmem, CONFIG_BM_FLAT_SETTINGS_LOG_LEVEL);

struct {
	char name[BLE_RMEM_MAX_NAME_SIZE];
	size_t size;
} static bluetooth_name_val = {
	.name = {0},
	.size = 0,
};

static const char bluetooth_name_key[] = "fw_loader/adv_name";

static int bluetooth_name_value_set(const char *name, const void *value, size_t val_len)
{
	size_t name_len;
	size_t key_len;

	if (name == NULL || value == NULL) {
		return -EFAULT;
	}

	name_len = strlen(name);
	key_len = sizeof(bluetooth_name_key) - 1;

	if (name_len != key_len) {
		return -ENOENT;
	}

	/* Check if the key name matches "fw_loader/adv_name" */
	if (memcmp(name, bluetooth_name_key, key_len) != 0) {
		return -ENOENT;
	}

	/* Check if value length exceeds buffer size */
	if (val_len > sizeof(bluetooth_name_val.name)) {
		LOG_ERR("Bluetooth name value too long: %zu (max: %zu)",
			val_len, sizeof(bluetooth_name_val.name));
		return -EINVAL;
	}

	/* Copy the value */
	memcpy(bluetooth_name_val.name, value, val_len);

	/* Set the size */
	bluetooth_name_val.size = val_len;

	LOG_INF("Bluetooth name set to: %.*s", (int)bluetooth_name_val.size,
		bluetooth_name_val.name);

	return 0;
}

size_t ble_rmem_adv_name_get(struct bm_retained_clipboard_ctx *ctx, char const **name)
{
	int err;
	struct bm_rmem_data_desc desc;

	if (name == NULL || ctx == NULL) {
		return 0;
	}

	desc.type = BM_RMEM_TLV_TYPE_BLE_NAME;

	err = bm_rmem_data_get(ctx, &desc);
	if (err < 0) {
		*name = NULL;
		return 0;
	}

	*name = (const char *)desc.data;
	return desc.len;
}

int settings_runtime_set(const char *name, const void *data, size_t len)
{
	return bluetooth_name_value_set(name, data, len);
}

int settings_runtime_get(const char *name, void *data, size_t len)
{
	/* Intentionally noop */
	(void)name;
	(void)data;
	(void)len;
	return -ENOTSUP;
}

int settings_delete(const char *name)
{
	/* Intentionally noop */
	(void)name;
	return -ENOTSUP;
}

int settings_save(void)
{
	int rc;
	struct bm_retained_clipboard_ctx clipboard_ctx;

	rc = bm_rmem_init_empty(&clipboard_ctx);
	if (rc != 0) {
		LOG_ERR("Failed to initialize retained clipboard writer: %d", rc);
		goto end;
	}

	rc = bm_rmem_data_write(&clipboard_ctx, BM_RMEM_TLV_TYPE_BLE_NAME, bluetooth_name_val.name,
				bluetooth_name_val.size);
	if (rc < 0) {
		LOG_ERR("Failed to write BLE name to retained clipboard: %d", rc);
		goto end;
	}

	rc = bm_rmem_crc32_write(&clipboard_ctx);
	if (rc < 0) {
		LOG_ERR("Failed to commit retained clipboard content: %d", rc);
		goto end;
	}

	LOG_INF("clipboard content exported");
end:
	return rc;
}

int settings_load(void)
{
	/* Intentionally noop */
	return 0;
}

int settings_commit(void)
{
	/* Intentionally noop */
	return 0;
}

int settings_save_subtree(const char *subtree, const void *data, size_t len)
{
	/* Intentionally noop */
	(void)subtree;
	(void)data;
	(void)len;
	return -ENOTSUP;
}
