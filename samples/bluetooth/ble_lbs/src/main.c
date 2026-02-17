/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <ble_gap.h>
#include <nrf_soc.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/services/ble_lbs.h>
#include <bm/bluetooth/services/ble_dis.h>
#include <bm/bm_buttons.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <board-config.h>

LOG_MODULE_REGISTER(app, CONFIG_SAMPLE_BLE_LBS_LOG_LEVEL);

BLE_ADV_DEF(ble_adv); /* BLE advertising instance */
BLE_LBS_DEF(ble_lbs); /* BLE LED Button Service instance */

/* Device information service is single-instance */

static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	uint32_t nrf_err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;
		nrf_err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (nrf_err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Peer disconnected");
		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}
		break;

	case BLE_GAP_EVT_AUTH_STATUS:
		LOG_INF("Authentication status: %#x",
		       evt->evt.gap_evt.params.auth_status.auth_status);
		break;

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		/* Pairing not supported */
		nrf_err = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle,
						      BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL,
						      NULL);
		if (nrf_err) {
			LOG_ERR("Failed to reply with Security params, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		LOG_INF("BLE_GATTS_EVT_SYS_ATTR_MISSING");
		/* No system attributes have been stored */
		nrf_err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (nrf_err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", nrf_err);
		}
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

static void ble_adv_evt_handler(struct ble_adv *adv, const struct ble_adv_evt *adv_evt)
{
	switch (adv_evt->evt_type) {
	case BLE_ADV_EVT_ERROR:
		LOG_ERR("Advertising error %#x", adv_evt->error.reason);
		break;
	default:
		break;
	}
}

static void button_handler(uint8_t pin, enum bm_buttons_evt_type action)
{
	LOG_INF("Button event callback: %d, %d", pin, action);
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
			LOG_INF("Received LED ON!");
		} else {
			led_off();
			LOG_INF("Received LED OFF!");
		}
		break;
	default:
		break;
	}
}

int main(void)
{
	int err;
	uint32_t nrf_err;
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
		.sec_mode = BLE_LBS_CONFIG_SEC_MODE_DEFAULT,
	};
	struct ble_dis_config dis_config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
	};

	LOG_INF("BLE LBS sample started");

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
		LOG_ERR("Failed to initialize buttons, err: %d", err);
		goto idle;
	}

	err = bm_buttons_enable();
	if (err) {
		LOG_ERR("Failed to enable button detection, err: %d", err);
		goto idle;
	}

	nrf_err = ble_lbs_init(&ble_lbs, &lbs_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to setup LED Button Service, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_dis_init(&dis_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize device information service, nrf_error %#x", nrf_err);
		goto idle;
	}

	/* Adding the LBS UUID to the scan response data. */
	ble_uuid_t adv_uuid_list[] = {
		{ .uuid = BLE_UUID_LBS_SERVICE, .type = ble_lbs.uuid_type },
	};
	ble_adv_config.sr_data.uuid_lists.complete.uuid = &adv_uuid_list[0];
	ble_adv_config.sr_data.uuid_lists.complete.len = ARRAY_SIZE(adv_uuid_list);

	LOG_INF("Services initialized");

	nrf_err = ble_adv_init(&ble_adv, &ble_adv_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize BLE advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (nrf_err) {
		LOG_ERR("Failed to start advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Advertising as %s", CONFIG_BLE_ADV_NAME);

idle:
	while (true) {
		log_flush();

		k_cpu_idle();
	}

	return 0;
}
