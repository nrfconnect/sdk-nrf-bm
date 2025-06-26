/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <softdevice/ble_gap.h>
#include <softdevice/nrf_soc.h>
#include <bm/sdh/nrf_sdh.h>
#include <bm/sdh/nrf_sdh_ble.h>
#include <bm/ble/services/ble_lbs.h>
#include <bm/ble/services/ble_dis.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/printk.h>

#include <board-config.h>

#include <bm/lib/ble_adv.h>
#include <bm/lib/bm_buttons.h>

BLE_ADV_DEF(ble_adv); /* BLE advertising instance */
BLE_LBS_DEF(ble_lbs); /* BLE LED Button Service instance */

/* Device information service is single-instance */

static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	int err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		printk("Peer connected\n");
		conn_handle = evt->evt.gap_evt.conn_handle;
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			printk("Failed to set system attributes, nrf_error %#x\n", err);
		}
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		printk("Peer disconnected\n");
		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
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
			printk("Failed to reply with Security params, nrf_error %#x\n", err);
		}
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		printk("BLE_GATTS_EVT_SYS_ATTR_MISSING\n");
		/* No system attributes have been stored */
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			printk("Failed to set system attributes, nrf_error %#x\n", err);
		}
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, 0);

static void ble_adv_evt_handler(struct ble_adv *adv, const struct ble_adv_evt *adv_evt)
{
	switch (adv_evt->evt_type) {
	case BLE_ADV_EVT_ERROR:
		printk("Advertising error %d\n", adv_evt->error.reason);
		break;
	default:
		break;
	}
}

static void button_handler(uint8_t pin, enum bm_buttons_evt_type action)
{
	printk("Button event callback: %d, %d\n", pin, action);
	ble_lbs_on_button_change(&ble_lbs, conn_handle, action);
}

static void led_on(void)
{
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
}

static void led_off(void)
{
	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);
}

static void led_init(void)
{
	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	led_off();
}

static void lbs_evt_handler(struct ble_lbs *lbs, const struct ble_lbs_evt *lbs_evt)
{
	switch (lbs_evt->evt_type) {
	case BLE_LBS_EVT_LED_WRITE:
		if (lbs_evt->led_write.value) {
			led_on();
			printk("Received LED ON!\n");
		} else {
			led_off();
			printk("Received LED OFF!\n");
		}
		break;
	default:
		break;
	}
}

int main(void)
{
	int err;
	struct ble_adv_config ble_adv_config = {
		.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
		.evt_handler = ble_adv_evt_handler,
		.adv_data = {
			.name_type = BLE_ADV_DATA_FULL_NAME,
			.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		},
	};
	struct ble_lbs_config lbs_cfg = {
		.evt_handler = lbs_evt_handler,
	};

	printk("BLE LBS sample started\n");

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

	printk("Bluetooth enabled\n");

	led_init();

	err = bm_buttons_init(
		&(struct bm_buttons_config){
			.pin_number = BOARD_PIN_BTN_0,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		1,
		BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		printk("Failed to initialize buttons, err: %d\n", err);
		return -1;
	}

	err = bm_buttons_enable();
	if (err) {
		printk("Failed to enable button detection, err: %d\n", err);
		return -1;
	}

	err = ble_lbs_init(&ble_lbs, &lbs_cfg);
	if (err) {
		printk("Failed to setup LED Button Service, err %d\n", err);
		return -1;
	}

	err = ble_dis_init();
	if (err) {
		printk("Failed to initialize device information service, err %d\n", err);
		return -1;
	}

	/* Adding the LBS UUID to the scan response data. */
	ble_uuid_t adv_uuid_list[] = {
		{ .uuid = BLE_UUID_LBS_SERVICE, .type = ble_lbs.uuid_type },
	};
	ble_adv_config.sr_data.uuid_lists.complete.uuid = &adv_uuid_list[0];
	ble_adv_config.sr_data.uuid_lists.complete.len = ARRAY_SIZE(adv_uuid_list);

	printk("Services initialized\n");

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

	printk("Advertising as %s\n", CONFIG_BLE_ADV_NAME);

	while (true) {
		while (LOG_PROCESS()) {
			/* Empty. */
		}

		sd_app_evt_wait();
	}

	return 0;
}
