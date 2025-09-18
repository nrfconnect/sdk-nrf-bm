/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <settings/bluetooth_name.h>

LOG_MODULE_REGISTER(settings_bluetooth_name, CONFIG_SETTINGS_LOG_LEVEL);

static char bluetooth_name_val[32] = "";

static int bluetooth_name_handle_set(const char *name, size_t len, settings_read_cb read_cb,
				     void *cb_arg);
static int bluetooth_name_handle_export(int (*cb)(const char *name, const void *value,
					size_t val_len));
static int bluetooth_name_handle_commit(void);
static int bluetooth_name_handle_get(const char *name, char *val, int val_len_max);

SETTINGS_STATIC_HANDLER_DEFINE(bluetooth_name, "bluetooth_name", bluetooth_name_handle_get,
			       bluetooth_name_handle_set, bluetooth_name_handle_commit,
			       bluetooth_name_handle_export);

static int bluetooth_name_handle_set(const char *name, size_t len, settings_read_cb read_cb,
				     void *cb_arg)
{
	const char *next;
	size_t name_len;
	int rc;

	name_len = settings_name_next(name, &next);

	if (!next) {
		if (!strncmp(name, "name", name_len)) {
			if (len > (sizeof(bluetooth_name_val) - 1)) {
				LOG_ERR("Invalid settings value for bluetooth_name/name");
				return -EINVAL;
			}

			rc = read_cb(cb_arg, bluetooth_name_val, sizeof(bluetooth_name_val));

			if (rc < 0) {
				return rc;
			} else if (rc > 0) {
				LOG_ERR("Config set to %s", bluetooth_name_val);
			}

			return 0;
		}
	}

	return -ENOENT;
}

static int bluetooth_name_handle_export(int (*cb)(const char *name, const void *value,
					size_t val_len))
{
	(void)cb("bluetooth_name/name", bluetooth_name_val, strlen(bluetooth_name_val) + 1);
	LOG_ERR("export_done");
	return 0;
}

static int bluetooth_name_handle_commit(void)
{
	LOG_ERR("loading_done");
	return 0;
}

static int bluetooth_name_handle_get(const char *name, char *val, int val_len_max)
{
	const char *next;

	if (name == NULL || val == NULL) {
		return -EINVAL;
	}

	if (settings_name_steq(name, "name", &next) && !next) {
		val_len_max = MIN(val_len_max, strlen(bluetooth_name_val));
		memcpy(val, bluetooth_name_val, val_len_max);
		return val_len_max;
	}

	return -ENOENT;
}

const char *bluetooth_name_value_get(void)
{
	return bluetooth_name_val;
}
