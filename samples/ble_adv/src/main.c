/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <nrf_sdh_soc.h>
#include <ble_adv.h>
#include <nrf_soc.h>
#include <zephyr/sys/printk.h>

#define CONN_TAG 1

BLE_ADV_DEF(ble_adv);

static void ble_adv_evt_handler(enum ble_adv_evt evt)
{
	switch (evt) {
	case BLE_ADV_EVT_IDLE:
		printk("BLE_ADV_EVT_IDLE\n");
		break;
	case BLE_ADV_EVT_DIRECTED_HIGH_DUTY:
		printk("BLE_ADV_EVT_DIRECTED_HIGH_DUTY\n");
		break;
	case BLE_ADV_EVT_DIRECTED:
		printk("BLE_ADV_EVT_DIRECTED\n");
		break;
	case BLE_ADV_EVT_FAST:
		printk("BLE_ADV_EVT_FAST\n");
		break;
	case BLE_ADV_EVT_SLOW:
		printk("BLE_ADV_EVT_SLOW\n");
		break;
	case BLE_ADV_EVT_FAST_WHITELIST:
		printk("BLE_ADV_EVT_FAST_WHITELIST\n");
		break;
	case BLE_ADV_EVT_SLOW_WHITELIST:
		printk("BLE_ADV_EVT_SLOW_WHITELIST\n");
		break;
	case BLE_ADV_EVT_WHITELIST_REQUEST:
		printk("BLE_ADV_EVT_WHITELIST_REQUEST\n");
		break;
	case BLE_ADV_EVT_PEER_ADDR_REQUEST:
		printk("BLE_ADV_EVT_PEER_ADDR_REQUEST\n");
		break;
	}
}

static void ble_adv_error_handler(uint32_t error)
{
	printk("BLE advertising error %d\n", error);
}

int main(void)
{
	int err;
	uint32_t ram_start;
	struct ble_adv_config ble_adv_config = {
		.conn_cfg_tag = CONN_TAG,
		.evt_handler = ble_adv_evt_handler,
		.error_handler = ble_adv_error_handler,
		.adv_data = {
			.name_type = BLE_ADV_DATA_FULL_NAME,
			.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		}
	};

	err = nrf_sdh_enable_request();
	if (err) {
		printk("Failed to enable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice enabled\n");

	err = nrf_sdh_ble_app_ram_start_get(&ram_start);
	if (err) {
		printk("Failed to get application RAM start address, err %d\n", err);
		return -1;
	}

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

	err = ble_adv_init(&ble_adv, &ble_adv_config);
	if (err) {
		printk("Failed to initialize BLE advertising, err %d\n", err);
		return -1;
	}

	printk("Advertising..\n");

	err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (err) {
		printk("Failed to start advertising, err %d\n", err);
		return -1;
	}

	while (true) {
		sd_app_evt_wait();
	}

	return 0;
}
