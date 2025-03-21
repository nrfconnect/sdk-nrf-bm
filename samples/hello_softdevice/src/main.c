/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <nrf_sdh_soc.h>
#include <zephyr/kernel.h> /* k_busy_wait() */
#include <zephyr/sys_clock.h> /* USEC_PER_SEC */
#include <zephyr/sys/printk.h>

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	printk("BLE event %d\n", evt->header.evt_id);
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, 0);

static void on_soc_evt(uint32_t evt, void *ctx)
{
	printk("SoC event\n");
}
NRF_SDH_SOC_OBSERVER(sdh_soc, on_soc_evt, NULL, 0);

static void on_state_change(enum nrf_sdh_state_evt state, void *ctx)
{
	printk("SoftDevice state has changed to %d\n", state);
}
NRF_SDH_STATE_EVT_OBSERVER(sdh_state, on_state_change, NULL, 0);

int main(void)
{
	int err;

	printk("Hello World! %s\n", CONFIG_BOARD_TARGET);

	err = nrf_sdh_enable_request();
	if (err) {
		printk("Failed to enable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice enabled\n");

	err = nrf_sdh_ble_enable(CONFIG_NRF_SDH_BLE_CONN_TAG);
	if (err) {
		printk("Failed to enable BLE, err %d\n", err);
		return -1;
	}

	printk("Bluetooth is enabled!\n");

	k_busy_wait(2 * USEC_PER_SEC);

	err = nrf_sdh_disable_request();
	if (err) {
		printk("Failed to disable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice disabled\n");
	printk("Bye\n");

	return 0;
}
