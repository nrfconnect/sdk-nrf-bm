/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

 #define DT_DRV_COMPAT zephyr_retained_ram

#include <zephyr/logging/log.h>
#include <bm/settings/bluetooth_name.h>
#include <string.h>
#include <errno.h>
#include <bm/storage/bm_rmem.h>

LOG_MODULE_REGISTER(settings_bluetooth_name_v2, CONFIG_NCS_BM_FLAT_SETTINGS_LOG_LEVEL);

/* External reference to bluetooth_name_val from bluetooth_name.c */
char bluetooth_name_val[16] = {0};

static const char bluetooth_name_key[] = "fw_loader/adv_name";

static int bluetooth_name_value_set(const char *name, const void *value, size_t val_len)
{
	if (name == NULL || value == NULL) {
		return -EINVAL;
	}

	/* Check if the key name matches "fw_loader/adv_name" */
	if (memcmp(name, bluetooth_name_key, sizeof(bluetooth_name_key) - 1) != 0) {
		return -ENOENT;
	}

	/* Check if value length exceeds buffer size (leave room for null terminator) */
	if (val_len > (sizeof(bluetooth_name_val) - 1)) {
		LOG_ERR("Bluetooth name value too long: %zu (max: %zu)", 
			val_len, sizeof(bluetooth_name_val) - 1);
		return -EINVAL;
	}

	/* Copy the value */
	memcpy(bluetooth_name_val, value, val_len);

	/* Ensure null termination */
	bluetooth_name_val[val_len] = '\0';

	LOG_INF("Bluetooth name set to: %s", bluetooth_name_val);

	return 0;
}

int settings_runtime_set(const char *name, const void *data, size_t len)
{
	return bluetooth_name_value_set(name, data, len);
}

int settings_runtime_get(const char *name, void *data, size_t len)
{
	/* TODO: Implement settings_runtime_get */
	(void)name;
	(void)data;
	(void)len;
	return -ENOTSUP;
}

int settings_delete(const char *name)
{
	/* TODO: Implement settings_delete */
	(void)name;
	return -ENOTSUP;
}

int settings_commit(void)
{
	int rc;
	struct bm_retained_clipboard_ctx clipboard_ctx;

	rc = bm_rmem_init_writer(&clipboard_ctx);
	if (rc != 0) {
		LOG_ERR("Failed to initialize retained clipboard writer: %d", rc);
		goto end;
	}

	rc = bm_rmem_write_data(&clipboard_ctx, BM_REM_TLV_TYPE_BLE_NAME, bluetooth_name_val, strlen(bluetooth_name_val));
	if (rc < 0) {
		LOG_ERR("Failed to write BLE name to retained clipboard: %d", rc);
		goto end;
	}

	rc = bm_rmem_write_crc32(&clipboard_ctx);
	if (rc < 0) {
		LOG_ERR("Failed to commit retained clipboard content: %d", rc);
	}

end:
	return rc;
}

int settings_load(void)
{
	/* TODO: Implement settings_load */
	return 0;
}

int settings_save(void)
{
	/* TODO: Implement settings_save */
	return 0;
}
