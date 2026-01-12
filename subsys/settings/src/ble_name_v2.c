/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <bm/settings/bluetooth_name.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_REGISTER(settings_bluetooth_name_v2, CONFIG_SETTINGS_LOG_LEVEL);

/* External reference to bluetooth_name_val from bluetooth_name.c */
extern char bluetooth_name_val[16];

static const char bluetooth_name_key[] = "fw_loader/adv_name";

int bluetooth_name_value_set(const char *name, const void *value, size_t val_len)
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
