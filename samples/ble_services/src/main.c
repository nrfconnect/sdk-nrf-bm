/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <nrf_sdh_soc.h>
#include <event_scheduler.h>
#include <ble_adv.h>
#include <ble_gap.h>
#include <bluetooth/services/ble_bas.h>
#include <bluetooth/services/ble_dis.h>
#include <zephyr/kernel.h> /* k_busy_wait() */
#include <zephyr/sys_clock.h> /* USEC_PER_SEC */
#include <zephyr/sys/printk.h>

#define CONN_TAG 1

BLE_ADV_DEF(ble_adv); /* BLE advertising instance */
BLE_BAS_DEF(ble_bas); /* BLE battery service instance */

/* Device information service is single-instance */

static uint16_t conn_handle;

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	int err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		printk("Peer connected\n");
		conn_handle = evt->evt.gap_evt.conn_handle;
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			printk("Failed to set system attributes, nrf_error %d", err);
		}
		break;

	case BLE_GAP_EVT_AUTH_STATUS:
		printk("Authentication status: %#x\n",
		       evt->evt.gap_evt.params.auth_status.auth_status);
		break;

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		/* Pairing not supported */
		err = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle,
						  BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
		if (err) {
			printk("Failed to reply with Security params, nrf_error %d\n", err);
		}
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		printk("BLE_GATTS_EVT_SYS_ATTR_MISSING\n");
		/* No system attributes have been stored */
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			printk("Failed to set system attributes, nrf_error %d", err);
		}
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, 0);

static void ble_adv_evt_handler(enum ble_adv_evt evt)
{
	/* ignore */
}

static void ble_adv_error_handler(uint32_t error)
{
	printk("Advertising error %d\n", error);
}

static void ble_bas_evt_handler(struct ble_bas *bas, const struct ble_bas_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_BAS_EVT_NOTIFICATION_ENABLED:
		printk("Battery level notifications enabled\n");
		break;
	case BLE_BAS_EVT_NOTIFICATION_DISABLED:
		printk("Battery level notifications disabled\n");
		break;
	}
}

int main(void)
{
	int err;
	uint32_t ram_start;
	uint8_t battery_level = 77;
	struct ble_adv_config ble_adv_config = {
		.conn_cfg_tag = CONN_TAG,
		.evt_handler = ble_adv_evt_handler,
		.error_handler = ble_adv_error_handler,
		.adv_data = {
			.name_type = BLE_ADV_DATA_FULL_NAME,
			.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		},
	};
	struct ble_bas_config bas_cfg = {
		.evt_handler = ble_bas_evt_handler,
		.can_notify = true,
		.battery_level = 77,
	};

	err = nrf_sdh_enable_request();
	if (err) {
		printk("Failed to enable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice enabled\n");

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

	/** TODO: Kconfig? */
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_cfg.batt_rd_sec);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_cfg.cccd_wr_sec);

	err = ble_bas_init(&ble_bas, &bas_cfg);
	if (err) {
		printk("Failed to setup battery service, err %d\n", err);
		return -1;
	}

	printk("BAS initialized");

	err = ble_dis_init();
	if (err) {
		printk("Failed to initialize device information service, err %d\n", err);
		return -1;
	}

	err = ble_adv_init(&ble_adv, &ble_adv_config);
	if (err) {
		printk("Failed to initialize BLE advertising, err %d\n", err);
		return -1;
	}

	err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (err) {
		printk("Failed to start advertising, err %d\n", err);
		return -1;
	}

	while (true) {
		sd_app_evt_wait();
		k_busy_wait(1 * USEC_PER_SEC);
		ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level++ % 100);
		event_scheduler_process();
	}

	err = nrf_sdh_disable_request();
	if (err) {
		printk("Failed to disable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice disabled\n");

	return 0;
}
