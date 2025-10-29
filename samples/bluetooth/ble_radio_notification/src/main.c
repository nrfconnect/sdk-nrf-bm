/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/ble_radio_notification.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <hal/nrf_gpio.h>
#include <board-config.h>

LOG_MODULE_REGISTER(app, CONFIG_BLE_RADIO_NOTIFICATION_SAMPLE_LOG_LEVEL);

/* BLE advertising instance */
BLE_ADV_DEF(ble_adv);

static void led_init(void)
{
	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
}

static void led_set(bool state)
{
	nrf_gpio_pin_write(BOARD_PIN_LED_0,
			   (state ? BOARD_LED_ACTIVE_STATE : !BOARD_LED_ACTIVE_STATE));
}

static void ble_adv_evt_handler(struct ble_adv *adv, const struct ble_adv_evt *adv_evt)
{
	switch (adv_evt->evt_type) {
	case BLE_ADV_EVT_ERROR:
		LOG_ERR("Advertising error %d", adv_evt->error.reason);
		break;
	default:
		break;
	}
}

static void on_conn_params_evt(const struct ble_conn_params_evt *evt)
{
	uint32_t err;

	switch (evt->id) {
	case BLE_CONN_PARAMS_EVT_REJECTED:
		err = sd_ble_gap_disconnect(evt->conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
		if (err) {
			LOG_ERR("Disconnect failed on conn params update rejection, nrf_error %#x",
				err);
		} else {
			LOG_INF("Disconnected from peer, unacceptable conn params");
		}
		break;

	default:
		break;
	}
}

static void ble_radio_notification_evt_handler(bool radio_active)
{
	led_set(radio_active);
}

int main(void)
{
	int err;
	uint32_t nrf_err;

	struct ble_adv_config ble_adv_cfg = {
		.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
		.evt_handler = ble_adv_evt_handler,
		.adv_data = {
			.name_type = BLE_ADV_DATA_FULL_NAME,
			.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		},

	};

	LOG_INF("BLE Radio Notification sample started");

	led_init();

	err = nrf_sdh_enable_request();
	if (err) {
		LOG_ERR("Failed to enable SoftDevice, err %d", err);
		goto idle;
	}

	LOG_INF("SoftDevice enabled");

	nrf_err = ble_radio_notification_init(CONFIG_BLE_RADIO_NOTIFICATION_DISTANCE_US,
					  ble_radio_notification_evt_handler);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to enable BLE, nrf_error %#x", nrf_err);
		goto idle;
	}

	err = nrf_sdh_ble_enable(CONFIG_NRF_SDH_BLE_CONN_TAG);
	if (err) {
		LOG_ERR("Failed to enable BLE, err %d", err);
		goto idle;
	}

	LOG_INF("Bluetooth enabled");

	err = ble_conn_params_evt_handler_set(on_conn_params_evt);
	if (err) {
		LOG_ERR("Failed to setup conn param event handler, err %d", err);
		goto idle;
	}

	err = ble_adv_init(&ble_adv, &ble_adv_cfg);
	if (err) {
		LOG_ERR("Failed to initialize advertising, err %d", err);
		goto idle;
	}

	err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (err) {
		LOG_ERR("Failed to start advertising, err %d", err);
		goto idle;
	}

	LOG_INF("Advertising as %s", CONFIG_BLE_ADV_NAME);

idle:
	while (true) {
		while (LOG_PROCESS()) {
		}

		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();
	}

	return 0;
}
