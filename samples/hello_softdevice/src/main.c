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

#define CONN_TAG 1

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	printk("BLE EVENT %d\n", evt->header.evt_id);
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, 0);

static void on_soc_evt(uint32_t evt, void *ctx)
{
	printk("SoC event!!\n");
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
	uint32_t ram_start;

	printk("Hello World! %s\n", CONFIG_BOARD_TARGET);

	err = nrf_sdh_enable_request();
	if (err) {
		printk("Failed to enable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice enabled\n");

#define DEST 0x3e000
#define word_size(x) ((sizeof((x)) + 3) % sizeof(uint32_t))
	__aligned(4) uint32_t dummy = 0xdeadbeef;

	err = sd_flash_write((void *)DEST, &dummy, word_size(dummy));
	if (err) {
		printk("Error %d", err);
		return -1;
	}

	nrf_sdh_ble_app_ram_start_get(&ram_start);

	err = nrf_sdh_ble_default_cfg_set(CONN_TAG);
	if (err) {
		printk("Failed to setup default configuration, err %d\n", err);
		return -1;
	}

	err = nrf_sdh_ble_enable(ram_start);
	if (err) {
		printk("Failed to enable BLE, err %d\n", err);
		return -1;
	}

	printk("Bluetooth is enabled!\n");

	k_busy_wait(1 * USEC_PER_SEC);

	err = nrf_sdh_disable_request();
	if (err) {
		printk("Failed to disable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice disabled\n");
	printk("Bye\n");

	return 0;
}
