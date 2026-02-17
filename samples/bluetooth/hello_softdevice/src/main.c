/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>

#include <zephyr/kernel.h> /* k_busy_wait() */
#include <zephyr/sys_clock.h> /* USEC_PER_SEC */
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

LOG_MODULE_REGISTER(app, CONFIG_SAMPLE_BLE_HELLO_SD_LOG_LEVEL);

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	LOG_INF("BLE event %d", evt->header.evt_id);
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

static void on_soc_evt(uint32_t evt, void *ctx)
{
	LOG_INF("SoC event");
}
NRF_SDH_SOC_OBSERVER(sdh_soc, on_soc_evt, NULL, USER_LOW);

static int on_state_change(enum nrf_sdh_state_evt state, void *ctx)
{
	LOG_INF("SoftDevice state %d", state);

	return 0;
}
NRF_SDH_STATE_EVT_OBSERVER(sdh_state, on_state_change, NULL, USER_LOW);

int main(void)
{
	int err;

	LOG_INF("Hello SoftDevice sample started");

	err = nrf_sdh_enable_request();
	if (err) {
		LOG_ERR("Failed to enable SoftDevice, err %d", err);
		goto idle;
	}

	LOG_INF("SoftDevice enabled");

	err = nrf_sdh_ble_enable(CONFIG_NRF_SDH_BLE_CONN_TAG);
	if (err) {
		LOG_ERR("Failed to enable BLE, err %d", err);
		goto idle;
	}

	LOG_INF("Bluetooth enabled");

	log_flush();

	k_busy_wait(2 * USEC_PER_SEC);

	err = nrf_sdh_disable_request();
	if (err) {
		LOG_ERR("Failed to disable SoftDevice, err %d", err);
		goto idle;
	}

	LOG_INF("SoftDevice disabled");
	LOG_INF("Bye");

idle:
	while (true) {
		log_flush();

		k_cpu_idle();
	}

	return 0;
}
