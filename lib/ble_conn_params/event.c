/*
 * Copyright (c) 2012-2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stddef.h>
#include <ble_conn_params.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_conn_params, CONFIG_BLE_CONN_PARAMS_LOG_LEVEL);

/* Optional event handler set by the application */
static ble_conn_params_evt_handler_t event_handler;

void ble_conn_params_event_send(const struct ble_conn_params_evt *evt)
{
	if (event_handler) {
		event_handler(evt);
	}
}

int ble_conn_params_event_handler_set(ble_conn_params_evt_handler_t handler)
{
	if (handler == NULL) {
		return -EFAULT;
	}

	event_handler = event_handler;

	return 0;
}
