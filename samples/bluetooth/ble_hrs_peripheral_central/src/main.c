/*
 * Copyright (c) 2014-2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <string.h>
#include <ble.h>
#include <bm/bm_buttons.h>
#include <hal/nrf_gpio.h>

#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/ble_gq.h>
#include <bm/bluetooth/ble_scan.h>
#include <bm/bluetooth/peer_manager/nrf_ble_lesc.h>
#include <bm/bluetooth/peer_manager/peer_manager.h>
#include <bm/bluetooth/peer_manager/peer_manager_handler.h>
#include <bm/bluetooth/peer_manager/peer_manager_types.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>

#include <bm/bluetooth/services/ble_bas.h>
#include <bm/bluetooth/services/ble_bas_client.h>
#include <bm/bluetooth/services/ble_hrs.h>
#include <bm/bluetooth/services/ble_hrs_client.h>
#include <bm/bluetooth/services/uuid.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_BLE_HRS_PERIPHERAL_CENTRAL_LOG_LEVEL);

/* Advertising instance. */
BLE_ADV_DEF(ble_adv);
/* Battery service instance. */
BLE_BAS_DEF(ble_bas);
/* Battery service client instance. */
BLE_BAS_CLIENT_DEF(ble_bas_client);
/* Database discovery instance. */
BLE_DB_DISCOVERY_DEF(ble_db_disc);
/* GATT queue instance. */
BLE_GQ_DEF(ble_gq);
/* Heart rate service instance. */
BLE_HRS_DEF(ble_hrs);
/* Heart rate service client instance. */
BLE_HRS_CLIENT_DEF(ble_hrs_client);
/* Scanning instance. */
BLE_SCAN_DEF(ble_scan);

/* Current connection handle for peer that acts as a central. */
static uint16_t conn_handle_central = BLE_CONN_HANDLE_INVALID;
/* Current connection handle for peer that acts as a peripheral. */
static uint16_t conn_handle_peripheral = BLE_CONN_HANDLE_INVALID;
/* True if allow list has been temporarily disabled. */
static bool allow_list_disabled;
/* Authentication key request queue. */
static uint16_t auth_key_requests[CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT];
/* Number of authentication key requests in queue. */
static uint16_t auth_key_requests_num;

#if defined(CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_ADDR)
static const uint8_t target_periph_addr[BLE_GAP_ADDR_LEN] = {
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 8) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 16) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 24) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 32) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 40) & 0xff,
};
#endif /* CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_ADDR */

static void delete_bonds(void)
{
	uint32_t nrf_err;

	LOG_INF("Erase bonds!");

	nrf_err = pm_peers_delete();
	if (nrf_err) {
		LOG_ERR("Failed to delete bonds, nrf_error %#x", nrf_err);
	}
}

static void peer_list_get(uint16_t *peers, uint32_t *size)
{
	uint16_t peer_id;
	uint32_t peers_to_copy;

	peers_to_copy = (*size < BLE_GAP_WHITELIST_ADDR_MAX_COUNT)
				? *size
				: BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

	peer_id = pm_next_peer_id_get(PM_PEER_ID_INVALID);
	*size = 0;

	while ((peer_id != PM_PEER_ID_INVALID) && (peers_to_copy--)) {
		peers[(*size)++] = peer_id;
		peer_id = pm_next_peer_id_get(peer_id);
	}
}

static uint32_t allow_list_load(void)
{
	uint32_t nrf_err;
	uint16_t peers[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	uint32_t peer_cnt;

	memset(peers, PM_PEER_ID_INVALID, sizeof(peers));
	peer_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

	peer_list_get(peers, &peer_cnt);

	nrf_err = pm_allow_list_set(peers, peer_cnt);
	if (nrf_err) {
		return nrf_err;
	}

	nrf_err = pm_device_identities_list_set(peers, peer_cnt);
	if (nrf_err != NRF_ERROR_NOT_SUPPORTED) {
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static uint32_t on_allow_list_req(void)
{
	uint32_t nrf_err;

	ble_gap_addr_t allow_list_addrs[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	ble_gap_irk_t allow_list_irks[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	uint32_t addr_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
	uint32_t irk_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

	nrf_err = allow_list_load();
	if (nrf_err) {
		LOG_ERR("Failed to delete bonds, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	nrf_err = pm_allow_list_get(allow_list_addrs, &addr_cnt, allow_list_irks, &irk_cnt);
	if (nrf_err) {
		return nrf_err;
	}

	if (((addr_cnt == 0) && (irk_cnt == 0)) || (allow_list_disabled)) {
		/* Don't use allow list. */
		nrf_err = ble_scan_params_set(&ble_scan, NULL);
		if (nrf_err) {
			return nrf_err;
		}
	}

	return NRF_SUCCESS;
}

static void scan_start(void)
{
	uint32_t nrf_err;

	nrf_err = ble_scan_start(&ble_scan);
	if (nrf_err) {
		LOG_ERR("Failed to start scanning, nrf_error %#x", nrf_err);
	} else {
		LOG_INF("Scanning");
	}
}

static void advertising_start(void)
{
	uint32_t nrf_err;

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (nrf_err) {
		LOG_ERR("Failed to start advertising, nrf_error %#x", nrf_err);
	} else {
		LOG_INF("Advertising as %s", CONFIG_SAMPLE_BLE_DEVICE_NAME);
	}
}

static void allow_list_disable(void)
{
	if (!allow_list_disabled) {
		LOG_INF("allow list temporarily disabled.");
		allow_list_disabled = true;
		scan_start();
	}
}

static void on_ble_evt(const ble_evt_t *ble_evt, void *ctx)
{
	uint32_t nrf_err;
	const ble_gap_evt_t *const gap_evt = &ble_evt->evt.gap_evt;

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Connected");
		if (gap_evt->params.connected.role == BLE_GAP_ROLE_CENTRAL) {
			conn_handle_peripheral = gap_evt->conn_handle;
			nrf_gpio_pin_write(BOARD_PIN_LED_1, BOARD_LED_ACTIVE_STATE);

			nrf_err = ble_db_discovery_start(&ble_db_disc,
							 ble_evt->evt.gap_evt.conn_handle);
			if (nrf_err) {
				LOG_ERR("db discovery start failed, nrf_error %#x", nrf_err);
			}
		} else if (gap_evt->params.connected.role == BLE_GAP_ROLE_PERIPH) {
			conn_handle_central = gap_evt->conn_handle;
			nrf_gpio_pin_write(BOARD_PIN_LED_2, BOARD_LED_ACTIVE_STATE);
		}
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Disconnected conn_handle %#x, reason %#x",
			gap_evt->conn_handle, gap_evt->params.disconnected.reason);

		if (gap_evt->conn_handle == conn_handle_peripheral) {
			conn_handle_peripheral = BLE_CONN_HANDLE_INVALID;
			nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
			scan_start();
		} else if (gap_evt->conn_handle == conn_handle_central) {
			conn_handle_central = BLE_CONN_HANDLE_INVALID;
			nrf_gpio_pin_write(BOARD_PIN_LED_2, !BOARD_LED_ACTIVE_STATE);
			advertising_start();
		}
		break;

	case BLE_GAP_EVT_PASSKEY_DISPLAY:
		LOG_INF("Passkey: %.*s", BLE_GAP_PASSKEY_LEN,
			gap_evt->params.passkey_display.passkey);
		if (gap_evt->params.passkey_display.match_request) {
			LOG_INF("Pairing request, press button 0 to accept or button 1 to reject.");
			if (auth_key_requests_num < ARRAY_SIZE(auth_key_requests)) {
				auth_key_requests[auth_key_requests_num++] = gap_evt->conn_handle;
			}
		}
		break;

	case BLE_GAP_EVT_TIMEOUT:
		if (gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN) {
			LOG_INF("Connection Request timed out");
		}
		break;

	case BLE_GATTC_EVT_TIMEOUT:
		LOG_INF("GATT Client Timeout.");
		nrf_err = sd_ble_gap_disconnect(ble_evt->evt.gattc_evt.conn_handle,
						BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (nrf_err) {
			LOG_ERR("Failed to disconnect, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_GATTS_EVT_TIMEOUT:
		LOG_INF("GATT Server Timeout.");
		nrf_err = sd_ble_gap_disconnect(ble_evt->evt.gatts_evt.conn_handle,
						BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (nrf_err) {
			LOG_ERR("Failed to disconnect, nrf_error %#x", nrf_err);
		}
		break;

	default:
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

static void db_disc_evt_handler(struct ble_db_discovery *db_discovery,
				struct ble_db_discovery_evt *evt)
{
	ble_hrs_on_db_disc_evt(&ble_hrs_client, evt);
	ble_bas_on_db_disc_evt(&ble_bas_client, evt);
}

static void num_comp_reply(uint16_t conn_handle, bool accept)
{
	uint32_t nrf_err;
	uint8_t key_type;

	if (accept) {
		LOG_INF("Numeric Match. Conn handle %d", conn_handle);
		key_type = BLE_GAP_AUTH_KEY_TYPE_PASSKEY;
	} else {
		LOG_INF("Numeric REJECT. Conn handle %d", conn_handle);
		key_type = BLE_GAP_AUTH_KEY_TYPE_NONE;
	}

	nrf_err = sd_ble_gap_auth_key_reply(conn_handle, key_type, NULL);
	if (nrf_err) {
		LOG_ERR("Failed to reply auth request, nrf_error %#x", nrf_err);
	}
}

static void button_handler(uint8_t pin, uint8_t action)
{
	uint32_t nrf_err;
	uint16_t conn_handle_disconnect = BLE_CONN_HANDLE_INVALID;

	if (action != BM_BUTTONS_PRESS) {
		return;
	}

	/* Handle pairing match request yes/no. */
	if ((auth_key_requests_num > 0) && (pin == BOARD_PIN_BTN_0 || pin == BOARD_PIN_BTN_1)) {
		num_comp_reply(auth_key_requests[0], (pin == BOARD_PIN_BTN_0));

		/* Move to next auth key request in the array (if any). */
		auth_key_requests_num = MAX(0, auth_key_requests_num - 1);
		memmove(&auth_key_requests[0], &auth_key_requests[1], auth_key_requests_num);
		return;
	}

	/* Handle regular button functionality. */
	switch (pin) {
	case BOARD_PIN_BTN_0:
		LOG_INF("Button allow list off");
		allow_list_disable();
		break;
	case BOARD_PIN_BTN_1:
		if (conn_handle_peripheral != BLE_CONN_HANDLE_INVALID) {
			LOG_INF("Button disconnect peripheral");
			conn_handle_disconnect = conn_handle_peripheral;
		}
		break;
	case BOARD_PIN_BTN_2:
		if (conn_handle_central != BLE_CONN_HANDLE_INVALID) {
			LOG_INF("Button disconnect central");
			conn_handle_disconnect = conn_handle_central;
		}
		break;
	default:
		break;
	}

	if (conn_handle_disconnect != BLE_CONN_HANDLE_INVALID) {
		nrf_err = sd_ble_gap_disconnect(conn_handle_disconnect,
						BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (nrf_err) {
			LOG_ERR("ble gap disconnect failed, nrf_error %#x", nrf_err);
		}
	}
}

static void pm_evt_handler(const struct pm_evt *evt)
{
	pm_handler_on_pm_evt(evt);
	pm_handler_disconnect_on_sec_failure(evt);
	pm_handler_flash_clean(evt);

	switch (evt->evt_id) {
	case PM_EVT_PEERS_DELETE_SUCCEEDED:
		scan_start();
		advertising_start();
		break;

	default:
		break;
	}
}

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

static void hrs_client_evt_handler(struct ble_hrs_client *hrs, const struct ble_hrs_client_evt *evt)
{

	uint32_t nrf_err;

	switch (evt->evt_type) {
	case BLE_HRS_CLIENT_EVT_DISCOVERY_COMPLETE:
		LOG_INF("Heart rate service discovered");

		nrf_err = ble_hrs_client_handles_assign(hrs, evt->conn_handle,
							&evt->discovery_complete.handles);
		if (nrf_err) {
			LOG_ERR("ble_hrs_client_handles_assign failed, nrf_error %#x", nrf_err);
		}

		/* Heart rate service discovered. Enable notification of Heart Rate Measurement. */
		nrf_err = ble_hrs_client_hrm_notif_enable(hrs);
		if (nrf_err) {
			LOG_ERR("ble_hrs_client_hrm_notif_enable failed, nrf_error %#x", nrf_err);
		}

		break;

	case BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION:
		LOG_INF("Heart Rate = %d.", evt->hrm_notification.hr_value);
		if (evt->hrm_notification.rr_intervals_cnt != 0) {
			uint32_t rr_avg = 0;

			for (uint32_t i = 0; i < evt->hrm_notification.rr_intervals_cnt; i++) {
				rr_avg += evt->hrm_notification.rr_intervals[i];

				nrf_err = ble_hrs_rr_interval_add(
					&ble_hrs, evt->hrm_notification.rr_intervals[i]);
				if (nrf_err) {
					LOG_INF("Failed to add RR interval, nrf_error %#x",
						nrf_err);
				}
			}
			rr_avg = rr_avg / evt->hrm_notification.rr_intervals_cnt;
			LOG_INF("rr_interval (avg) = %d.", rr_avg);
		}
		nrf_err = ble_hrs_heart_rate_measurement_send(&ble_hrs,
							      evt->hrm_notification.hr_value);
		if (nrf_err) {
			/* Ignore if not connected, or CCCD not written/configured by peer. */
			if (nrf_err != BLE_ERROR_INVALID_CONN_HANDLE &&
			    nrf_err != NRF_ERROR_INVALID_STATE &&
			    nrf_err != BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
				LOG_ERR("Failed to send heart rate measurement, nrf_error %#x",
					nrf_err);
			}
		}
		break;
	case BLE_HRS_CLIENT_EVT_BSL_UPDATE:
		ble_hrs_body_sensor_location_set(&ble_hrs, evt->bsl_update.body_sensor_location);
		break;

	default:
		LOG_WRN("Unhandled hrs event %d", evt->evt_type);
		break;
	}
}

static void hrs_evt_handler(struct ble_hrs *hrs, const struct ble_hrs_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_HRS_EVT_NOTIFICATION_ENABLED:
		LOG_INF("HRM notifications enabled for connection %#x", evt->conn_handle);
		break;
	case BLE_HRS_EVT_NOTIFICATION_DISABLED:
		LOG_INF("HRM notifications disabled for connection %#x", evt->conn_handle);
		break;
	case BLE_HRS_EVT_ERROR:
		LOG_ERR("HRS error event, nrf_error %#x", evt->error.reason);
		break;
	default:
		break;
	}
}

static void bas_client_evt_handler(struct ble_bas_client *bas, const struct ble_bas_client_evt *evt)
{
	uint32_t nrf_err;

	switch (evt->evt_type) {
	case BLE_BAS_CLIENT_EVT_DISCOVERY_COMPLETE:
		LOG_INF("Battery service discovered");
		nrf_err = ble_bas_client_handles_assign(bas, evt->conn_handle,
							&evt->discovery_complete.handles);
		if (nrf_err) {
			LOG_ERR("ble_bas_client_handles_assign failed, nrf_error %#x", nrf_err);
		}

		/* Battery service discovered. Enable notification of battery level. */
		nrf_err = ble_bas_client_bl_notif_enable(bas);
		if (nrf_err) {
			LOG_ERR("ble_bas_client_bl_notif_enable failed, nrf_error %#x", nrf_err);
		}
		break;
	case BLE_BAS_CLIENT_EVT_BATT_NOTIFICATION:
		LOG_INF("Battery Level = %d", evt->battery.level);

		nrf_err = ble_bas_battery_level_update(&ble_bas, conn_handle_central,
						       evt->battery.level);
		if (nrf_err) {
			/* Ignore if not connected, or CCCD not written/configured by peer. */
			if (nrf_err != BLE_ERROR_INVALID_CONN_HANDLE &&
			    nrf_err != NRF_ERROR_INVALID_STATE &&
			    nrf_err != BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
				LOG_ERR("Failed to send heart rate measurement, nrf_error %#x",
					nrf_err);
			}
		}
		break;
	default:
		LOG_WRN("Unhandled ble event %d", evt->evt_type);
		break;
	}
}

static void bas_evt_handler(struct ble_bas *bas, const struct ble_bas_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_BAS_EVT_NOTIFICATION_ENABLED:
		LOG_INF("Battery notifications enabled for connection %#x", evt->conn_handle);
		break;
	case BLE_BAS_EVT_NOTIFICATION_DISABLED:
		LOG_INF("Battery notifications disabled for connection %#x", evt->conn_handle);
		break;
	case BLE_BAS_EVT_ERROR:
		LOG_ERR("BAS error event, nrf_error %#x", evt->error.reason);
		break;
	default:
		break;
	}
}

static void scan_evt_handler(const struct ble_scan_evt *scan_evt)
{
	switch (scan_evt->evt_type) {
	case BLE_SCAN_EVT_NOT_FOUND:
		/* Ignore. */
		break;

	case BLE_SCAN_EVT_ALLOW_LIST_REQUEST:
		on_allow_list_req();
		allow_list_disabled = false;
		LOG_INF("allow list request");
		break;

	case BLE_SCAN_EVT_CONNECTING_ERROR:
		LOG_ERR("Failed to connect, nrf_error %#x", scan_evt->connecting_err.reason);
		break;

	case BLE_SCAN_EVT_SCAN_TIMEOUT:
		LOG_INF("Scan timed out");
		scan_start();
		break;

	case BLE_SCAN_EVT_FILTER_MATCH:
		LOG_INF("Scan filter match");
		break;

	case BLE_SCAN_EVT_ALLOW_LIST_ADV_REPORT:
		LOG_INF("allow list advertise report");
		break;

	case BLE_SCAN_EVT_CONNECTED: {
		const ble_gap_addr_t *const peer_addr = &scan_evt->connected.connected->peer_addr;

		LOG_INF("Connecting to target %02X:%02X:%02X:%02X:%02X:%02X",
			peer_addr->addr[5], peer_addr->addr[4], peer_addr->addr[3],
			peer_addr->addr[2], peer_addr->addr[1], peer_addr->addr[0]);
	} break;

	default:
		LOG_WRN("Unhandled scan event %d", scan_evt->evt_type);
		break;
	}
}

static void conn_params_evt_handler(const struct ble_conn_params_evt *evt)
{
	ble_hrs_conn_params_evt(&ble_hrs, evt);
}

static int buttons_leds_init(void)
{
	int err;
	static struct bm_buttons_config btn_cfg[] = {
		{
			.pin_number = BOARD_PIN_BTN_0,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		{
			.pin_number = BOARD_PIN_BTN_1,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		{
			.pin_number = BOARD_PIN_BTN_2,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
	};

	err = bm_buttons_init(btn_cfg, ARRAY_SIZE(btn_cfg), BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		LOG_ERR("Failed to initialize buttons, err %d", err);
		return err;
	}

	err = bm_buttons_enable();
	if (err) {
		LOG_ERR("Failed to enable buttons, err %d", err);
		return err;
	}

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_1);
	nrf_gpio_cfg_output(BOARD_PIN_LED_2);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_2, !BOARD_LED_ACTIVE_STATE);

	return 0;
}

static uint32_t peer_manager_init(void)
{
	uint32_t nrf_err;
	ble_gap_sec_params_t sec_param = {
		.bond = 1,
		.mitm = 0,
		.lesc = 1,
		.keypress = 0,
		.io_caps = BLE_GAP_IO_CAPS_DISPLAY_YESNO,
		.oob = 0,
		.min_key_size = 7,
		.max_key_size = 16,
		.kdist_own.enc = 1,
		.kdist_own.id = 1,
		.kdist_peer.enc = 1,
		.kdist_peer.id = 1,
	};

	nrf_err = pm_init();
	if (nrf_err) {
		LOG_ERR("PM init failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	nrf_err = pm_sec_params_set(&sec_param);
	if (nrf_err) {
		LOG_ERR("Failed to set PM sec params, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	nrf_err = pm_register(pm_evt_handler);
	if (nrf_err) {
		LOG_ERR("PM register failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static uint32_t db_discovery_init(void)
{
	struct ble_db_discovery_config db_cfg = {
		.evt_handler = db_disc_evt_handler,
		.gatt_queue = &ble_gq,
	};

	return ble_db_discovery_init(&ble_db_disc, &db_cfg);
}

static uint32_t hrs_client_init(void)
{
	struct ble_hrs_client_config hrs_client_cfg = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &ble_gq,
		.db_discovery = &ble_db_disc,
	};

	return ble_hrs_client_init(&ble_hrs_client, &hrs_client_cfg);
}

static uint32_t hrs_init(void)
{
	uint8_t body_sensor_location = BLE_HRS_BODY_SENSOR_LOCATION_CHEST;
	struct ble_hrs_config hrs_cfg = {
		.evt_handler = hrs_evt_handler,
		.is_sensor_contact_supported = true,
		.body_sensor_location = &body_sensor_location,
		.sec_mode = BLE_HRS_CONFIG_SEC_MODE_DEFAULT,
	};

	return ble_hrs_init(&ble_hrs, &hrs_cfg);
}

static uint32_t bas_client_init(void)
{
	struct ble_bas_client_config bas_client_cfg = {
		.evt_handler = bas_client_evt_handler,
		.gatt_queue = &ble_gq,
		.db_discovery = &ble_db_disc,
	};

	return ble_bas_client_init(&ble_bas_client, &bas_client_cfg);
}

static uint32_t bas_init(void)
{
	struct ble_bas_config bas_cfg = {
		.evt_handler = bas_evt_handler,
		.can_notify = true,
		.battery_level = 0,
		.sec_mode = BLE_BAS_CONFIG_SEC_MODE_DEFAULT,
	};

	return ble_bas_init(&ble_bas, &bas_cfg);
}

static uint32_t scan_init(void)
{
	uint32_t nrf_err;
	struct ble_scan_config scan_cfg = {
		.scan_params = {
			.active = 0x01,
			.interval = BLE_GAP_SCAN_INTERVAL_US_MIN * 6,
			.window = BLE_GAP_SCAN_WINDOW_US_MIN * 6,
			.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
			.timeout = BLE_GAP_SCAN_TIMEOUT_UNLIMITED,
			.scan_phys = BLE_GAP_PHY_AUTO,
		},
		.conn_params = BLE_SCAN_CONN_PARAMS_DEFAULT,
		.connect_if_match = true,
		.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
		.evt_handler = scan_evt_handler,
	};
	struct ble_scan_filter_data filter_data = {
		.uuid_filter.uuid = {
			.uuid = BLE_UUID_HEART_RATE_SERVICE,
			.type = BLE_UUID_TYPE_BLE,
		},
	};
	uint8_t filter_mode_mask = BLE_SCAN_UUID_FILTER;

	nrf_err = ble_scan_init(&ble_scan, &scan_cfg);
	if (nrf_err) {
		LOG_ERR("ble_scan_init failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_UUID_FILTER, &filter_data);
	if (nrf_err) {
		LOG_ERR("ble_scan_filter_add uuid failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

#if defined(CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_NAME)
	filter_data.name_filter.name = CONFIG_SAMPLE_TARGET_PERIPHERAL_NAME;
	filter_mode_mask |= BLE_SCAN_NAME_FILTER;

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_NAME_FILTER, &filter_data);
	if (nrf_err) {
		LOG_ERR("ble_scan_filter_add name failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}
#endif /* CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_NAME */

#if defined(CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_ADDR)
	filter_data.addr_filter.addr = target_periph_addr;
	filter_mode_mask |= BLE_SCAN_ADDR_FILTER;

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_ADDR_FILTER, &filter_data);
	if (nrf_err) {
		LOG_ERR("ble_scan_filter_add address failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}
#endif /* CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_ADDR */

	nrf_err = ble_scan_filters_enable(&ble_scan, filter_mode_mask, true);
	if (nrf_err) {
		LOG_ERR("Failed to enable scan filters, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static uint32_t adv_init(void)
{
	ble_uuid_t adv_uuid_list[] = {
		{.uuid = BLE_UUID_HEART_RATE_SERVICE, .type = BLE_UUID_TYPE_BLE},
	};
	struct ble_adv_config ble_adv_cfg = {
		.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
		.evt_handler = ble_adv_evt_handler,
		.adv_data = {
			.name_type = BLE_ADV_DATA_FULL_NAME,
			.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		},
		.sr_data.uuid_lists.complete = {
			.len = ARRAY_SIZE(adv_uuid_list),
			.uuid = &adv_uuid_list[0],
		},
	};

	return ble_adv_init(&ble_adv, &ble_adv_cfg);
}

int main(void)
{
	int err;
	uint32_t nrf_err;
	ble_gap_conn_sec_mode_t device_name_write_sec;

	LOG_INF("BLE HRS Peripheral Central sample started");

	err = buttons_leds_init();
	if (err) {
		goto idle;
	}

	const bool erase_bonds = bm_buttons_is_pressed(BOARD_PIN_BTN_1);

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

	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&device_name_write_sec);
	nrf_err = sd_ble_gap_device_name_set(&device_name_write_sec, CONFIG_SAMPLE_BLE_DEVICE_NAME,
					     strlen(CONFIG_SAMPLE_BLE_DEVICE_NAME));
	if (nrf_err) {
		LOG_ERR("Failed to set device name, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_conn_params_evt_handler_set(conn_params_evt_handler);
	if (nrf_err) {
		LOG_ERR("Failed to setup conn params event handler, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = peer_manager_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize peer manager, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = db_discovery_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize db discovery, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = hrs_client_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize HRS client, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = hrs_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize HRS, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = bas_client_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize BAS client, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = bas_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize BAS, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = adv_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = scan_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize scan library, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
	LOG_INF("BLE HRS Peripheral Central sample initialized");

	if (erase_bonds) {
		delete_bonds();
		/* Scan and advertising will be started on a PM_EVT_PEERS_DELETE_SUCCEEDED event. */
	} else {
		scan_start();
		advertising_start();
	}

idle:
	while (true) {
#if defined(CONFIG_PM_LESC)
		(void)nrf_ble_lesc_request_handler();
#endif
		log_flush();

		k_cpu_idle();
	}
}
