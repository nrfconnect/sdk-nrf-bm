/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <ble_adv.h>
#include <ble_gap.h>
#include <nrf_soc.h>
#include <bluetooth/services/ble_lbs.h>
#include <bluetooth/services/ble_dis.h>
#include <zephyr/sys/printk.h>

#include <lite_buttons.h>

#define CONN_TAG 1

#if defined(CONFIG_SOC_SERIES_NRF52X)
#define PIN_BTN_0 NRF_PIN_PORT_TO_PIN_NUMBER(11, 0)
#define PIN_LED_0 NRF_PIN_PORT_TO_PIN_NUMBER(13, 0)
#define LED_ACTIVE_STATE 0 /* GPIO_ACTIVE_LOW */
#elif defined(CONFIG_SOC_SERIES_NRF54LX)
#define PIN_BTN_0 NRF_PIN_PORT_TO_PIN_NUMBER(13, 1)
#define PIN_LED_0 NRF_PIN_PORT_TO_PIN_NUMBER(9, 2)
#define LED_ACTIVE_STATE 1 /* GPIO_ACTIVE_HIGH */
#endif

BLE_ADV_DEF(ble_adv); /* BLE advertising instance */
BLE_LBS_DEF(ble_lbs); /* BLE LED Button Service instance */

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
			printk("Failed to set system attributes, nrf_error %#x\n", err);
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

static void ble_adv_evt_handler(enum ble_adv_evt evt)
{
	/* ignore */
}

static void ble_adv_error_handler(uint32_t error)
{
	printk("Advertising error %d\n", error);
}

static void button_handler(uint8_t pin, enum lite_buttons_event_type action)
{
	printk("Button event callback: %d, %d\n", pin, action);
	ble_lbs_on_button_change(&ble_lbs, conn_handle, action);
}

static void led_on(void)
{
	nrf_gpio_pin_write(PIN_LED_0, LED_ACTIVE_STATE ? 1 : 0);
}

static void led_off(void)
{
	nrf_gpio_pin_write(PIN_LED_0, LED_ACTIVE_STATE ? 0 : 1);
}

static void led_init(void)
{
	nrf_gpio_cfg_output(PIN_LED_0);
	led_off();
}

static void led_write_handler(uint16_t conn_handle, struct ble_lbs *lbs, uint8_t value)
{
	if (value) {
		led_on();
		printk("Received LED ON!\n");
	} else {
		led_off();
		printk("Received LED OFF!\n");
	}
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
		},
	};
	struct ble_lbs_config lbs_cfg = {
		.led_write_handler = led_write_handler,
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

	led_init();

	err = lite_buttons_init(
		&(struct lite_buttons_config){
			.pin_number = PIN_BTN_0,
			.active_state = LITE_BUTTONS_ACTIVE_LOW,
			.pull_config = LITE_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		1,
		LITE_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		printk("lite_buttons_init error: %d\n", err);
		return -1;
	}

	err = lite_buttons_enable();
	if (err) {
		printk("lite_buttons_enable error: %d\n", err);
		return -1;
	}

	err = ble_lbs_init(&ble_lbs, &lbs_cfg);
	if (err) {
		printk("Failed to setup LED Button Service, err %d\n", err);
		return -1;
	}

	printk("LBS initialized\n");

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
	}

	return 0;
}
