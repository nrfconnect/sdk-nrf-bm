/*
 * Copyright (c) 2014-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <nrf_soc.h>
#include <hal/nrf_gpio.h>
#include <ble.h>
#include <bm/bm_buttons.h>

#include <bm/bluetooth/ble_scan.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/ble_conn_state.h>
#include <bm/bluetooth/peer_manager/peer_manager.h>
#include <bm/bluetooth/peer_manager/peer_manager_handler.h>
#include <bm/bluetooth/peer_manager/peer_manager_types.h>
#include <bm/bluetooth/peer_manager/nrf_ble_lesc.h>

#include <bm/bluetooth/ble_gq.h>
#include <bm/bluetooth/ble_conn_state.h>
#include <bm/bluetooth/services/ble_hrs_client.h>
#include <bm/bluetooth/services/uuid.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_BLE_HRS_CENTRAL_SAMPLE_LOG_LEVEL);

/* Perform bonding. */
#define SEC_PARAM_BOND 1
/* Man In The Middle protection not required. */
#define SEC_PARAM_MITM 0
/* LE Secure Connections enabled. */
#define SEC_PARAM_LESC 1
/* Keypress notifications not enabled. */
#define SEC_PARAM_KEYPRESS 0
/* No I/O capabilities. */
#define SEC_PARAM_IO_CAPABILITIES  BLE_GAP_IO_CAPS_NONE
/* Out Of Band data not available. */
#define SEC_PARAM_OOB 0
/* Minimum encryption key size in octets. */
#define SEC_PARAM_MIN_KEY_SIZE 7
/* Maximum encryption key size in octets. */
#define SEC_PARAM_MAX_KEY_SIZE 16

/* Macro to unpack 16bit unsigned UUID from octet stream. */
#define UUID16_EXTRACT(DST, SRC)                                                                   \
	do {                                                                                       \
		(*(DST)) = (SRC)[1];                                                               \
		(*(DST)) <<= 8;                                                                    \
		(*(DST)) |= (SRC)[0];                                                              \
	} while (0)

/* Structure used to identify the heart rate client module. */
BLE_HRS_CLIENT_DEF(ble_hrs_client);
/* Gatt queue instance. */
BLE_GQ_DEF(ble_gq);
/* DB discovery module instance. */
BLE_DB_DISCOVERY_DEF(ble_db_disc);
/* Scanning module instance. */
BLE_SCAN_DEF(ble_scan);

/* Current connection handle. */
static uint16_t conn_handle;
/* True if allow list has been temporarily disabled. */
static bool allow_list_disabled;

#if defined(CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_ADDR)
uint8_t target_periph_addr[BLE_GAP_ADDR_LEN] = {
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 40) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 32) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 24) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 16) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 8) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR) & 0xff,
};
#endif /* CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_ADDR */

static uint32_t scan_start(bool erase_bonds);

static void db_disc_handler(struct ble_db_discovery *db_discovery,
			    struct ble_db_discovery_evt *evt)
{
	ble_hrs_on_db_disc_evt(&ble_hrs_client, evt);
}

static void pm_evt_handler(const struct pm_evt *evt)
{
	pm_handler_on_pm_evt(evt);
	pm_handler_disconnect_on_sec_failure(evt);
	pm_handler_flash_clean(evt);

	switch (evt->evt_id) {
	case PM_EVT_PEERS_DELETE_SUCCEEDED:
		scan_start(false);
		break;

	default:
		break;
	}
}

static void on_ble_evt(const ble_evt_t *ble_evt, void *ctx)
{
	uint32_t nrf_err;
	const ble_gap_evt_t *gap_evt = &ble_evt->evt.gap_evt;

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Connected");
		conn_handle = ble_evt->evt.gap_evt.conn_handle;

		nrf_err = ble_db_discovery_start(&ble_db_disc, ble_evt->evt.gap_evt.conn_handle);
		if (nrf_err != 0) {
			LOG_ERR("db discovery start failed, nrf_error %#x", nrf_err);
		}

		if (ble_conn_state_central_conn_count() < CONFIG_NRF_SDH_BLE_CENTRAL_LINK_COUNT) {
			scan_start(false);
		}

		nrf_gpio_pin_write(BOARD_PIN_LED_1, BOARD_LED_ACTIVE_STATE);

		break;

	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Disconnected, reason %#x", gap_evt->params.disconnected.reason);

		if (ble_conn_state_central_conn_count() < CONFIG_NRF_SDH_BLE_CENTRAL_LINK_COUNT) {
			scan_start(false);
		}

		nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);

		break;

	case BLE_GAP_EVT_TIMEOUT:
		if (gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN) {
			LOG_INF("Connection Request timed out");
		}
		break;

	case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
		LOG_INF("ble gap event connection parameter update request");
		nrf_err = sd_ble_gap_conn_param_update(
			gap_evt->conn_handle,
			&gap_evt->params.conn_param_update_request.conn_params);
		if (nrf_err) {
			LOG_ERR("Failed to update connection params, nrf_error %#x", nrf_err);
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

static uint32_t peer_manager_init(void)
{
	uint32_t nrf_err;
	ble_gap_sec_params_t sec_param = {
		.bond = SEC_PARAM_BOND,
		.mitm = SEC_PARAM_MITM,
		.lesc = SEC_PARAM_LESC,
		.keypress = SEC_PARAM_KEYPRESS,
		.io_caps = SEC_PARAM_IO_CAPABILITIES,
		.oob = SEC_PARAM_OOB,
		.min_key_size = SEC_PARAM_MIN_KEY_SIZE,
		.max_key_size = SEC_PARAM_MAX_KEY_SIZE,
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
static uint32_t delete_bonds(void)
{
	uint32_t nrf_err;

	LOG_INF("Erase bonds!");

	nrf_err = pm_peers_delete();
	if (nrf_err) {
		LOG_ERR("Failed to delete bonds, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static void allow_list_disable(void)
{
	if (!allow_list_disabled) {
		LOG_INF("allow list temporarily disabled.");
		allow_list_disabled = true;
		ble_scan_stop(&ble_scan);
		scan_start(false);
	}
}

static void button_handler_allow_list_off(uint8_t pin, uint8_t action)
{
	LOG_INF("Button allow list off");
	allow_list_disable();
}

static void button_handler_disconnect(uint8_t pin, uint8_t action)
{
	LOG_INF("Button disconnect");

	uint32_t nrf_err = sd_ble_gap_disconnect(conn_handle,
						 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
	if (nrf_err != 0) {
		LOG_ERR("ble gap disconnect failed, nrf_error %#x", nrf_err);
	}
}

static void hrs_c_evt_handler(struct ble_hrs_client *hrs, struct ble_hrs_client_evt *evt)
{

	uint32_t nrf_err;

	switch (evt->evt_type) {
	case BLE_HRS_CLIENT_EVT_DISCOVERY_COMPLETE:
		LOG_INF("Heart rate service discovered.");

		nrf_err = ble_hrs_client_handles_assign(hrs, evt->conn_handle,
							 &evt->params.peer_db);
		if (nrf_err != 0) {
			LOG_ERR("ble_hrs_client_handles_assign failed, nrf_error %#x", nrf_err);
		}

		/* Heart rate service discovered. Enable notification of Heart Rate Measurement. */
		nrf_err = ble_hrs_client_hrm_notif_enable(hrs);
		if (nrf_err != 0) {
			LOG_ERR("ble_hrs_client_hrm_notif_enable failed, nrf_error %#x", nrf_err);
		}

		break;

	case BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION:
		LOG_INF("Heart Rate = %d.", evt->params.hrm.hr_value);
		if (evt->params.hrm.rr_intervals_cnt != 0) {
			uint32_t rr_avg = 0;

			for (uint32_t i = 0; i < evt->params.hrm.rr_intervals_cnt; i++) {
				rr_avg += evt->params.hrm.rr_intervals[i];
			}
			rr_avg = rr_avg / evt->params.hrm.rr_intervals_cnt;
			LOG_INF("rr_interval (avg) = %d.", rr_avg);
		}
		break;

	default:
		LOG_WRN("Unhandled hrs event %d", evt->evt_type);
		break;
	}
}

static uint32_t hrs_c_init(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client_config hrs_client_cfg = {
		.evt_handler = hrs_c_evt_handler,
		.gatt_queue = &ble_gq,
		.db_discovery = &ble_db_disc
	};

	nrf_err = ble_hrs_client_init(&ble_hrs_client, &hrs_client_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to init HRS client, nrf_error %#x", nrf_err);
	}

	return nrf_err;
}

static uint32_t db_discovery_init(void)
{
	uint32_t nrf_err;
	struct ble_db_discovery_config db_init = {0};

	db_init.evt_handler = db_disc_handler;
	db_init.gatt_queue = &ble_gq;

	nrf_err = ble_db_discovery_init(&ble_db_disc, &db_init);
	if (nrf_err) {
		LOG_ERR("db discovery init failed, nrf_error %#x", nrf_err);
	}

	return nrf_err;
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
	uint16_t peers[8];
	uint32_t peer_cnt;

	memset(peers, PM_PEER_ID_INVALID, sizeof(peers));
	peer_cnt = (sizeof(peers) / sizeof(*peers));

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

	ble_gap_addr_t allow_list_addrs[8] = {0};
	ble_gap_irk_t allow_list_irks[8] = {0};

	uint32_t addr_cnt = (sizeof(allow_list_addrs) / sizeof(ble_gap_addr_t));
	uint32_t irk_cnt = (sizeof(allow_list_irks) / sizeof(ble_gap_irk_t));

	nrf_err = allow_list_load();
	if (nrf_err) {
		return nrf_err;
	}

	nrf_err = pm_allow_list_get(allow_list_addrs, &addr_cnt, allow_list_irks, &irk_cnt);
	if (nrf_err) {
		return nrf_err;
	}

	if (((addr_cnt == 0) && (irk_cnt == 0)) || (allow_list_disabled)) {
		/* Don't use allow list.*/
		nrf_err = ble_scan_params_set(&ble_scan, NULL);
		if (nrf_err) {
			return nrf_err;
		}
	}

	return NRF_SUCCESS;
}

static uint32_t scan_start(bool erase_bonds)
{
	uint32_t nrf_err;

	if (erase_bonds) {
		/* Scan is started by the PM_EVT_PEERS_DELETE_SUCCEEDED event. */
		delete_bonds();
	} else {
		nrf_err = ble_scan_start(&ble_scan);
		if (nrf_err) {
			LOG_ERR("ble_scan_start failed, nrf_error %#x", nrf_err);
			return nrf_err;
		}
	}

	return NRF_SUCCESS;
}

static void conn_params_evt_handler(const struct ble_conn_params_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED:
		LOG_INF("GATT ATT MTU on connection 0x%x changed to %d.", evt->conn_handle,
			evt->att_mtu);
		break;

	case BLE_CONN_PARAMS_EVT_DATA_LENGTH_UPDATED:
		LOG_INF("Data length for connection 0x%x updated to %d.", evt->conn_handle,
			evt->data_length.rx);
		break;

	default:
		LOG_WRN("unhandled conn params event %d", evt->evt_type);
		break;
	}
}

static void scan_evt_handler(const struct ble_scan_evt *scan_evt)
{
	uint32_t nrf_err;

	switch (scan_evt->evt_type) {
	case BLE_SCAN_EVT_NOT_FOUND:
		/* ignore */
		break;

	case BLE_SCAN_EVT_ALLOW_LIST_REQUEST:
		on_allow_list_req();
		allow_list_disabled = false;
		LOG_INF("allow list request.");
		break;

	case BLE_SCAN_EVT_CONNECTING_ERROR:
		nrf_err = scan_evt->params.connecting_err.reason;
		LOG_INF("Scan connecting error");
		break;

	case BLE_SCAN_EVT_SCAN_TIMEOUT:
		LOG_INF("Scan timed out.");
		scan_start(false);
		break;

	case BLE_SCAN_EVT_FILTER_MATCH:
		LOG_INF("Scan filter match");
		break;

	case BLE_SCAN_EVT_ALLOW_LIST_ADV_REPORT:
		LOG_INF("allow list advertise report.");
		break;

	case BLE_SCAN_EVT_CONNECTED: {
		const ble_gap_evt_connected_t *p_connected = scan_evt->params.connected.connected;

		LOG_INF("Connecting to target %02x%02x%02x%02x%02x%02x",
			p_connected->peer_addr.addr[0], p_connected->peer_addr.addr[1],
			p_connected->peer_addr.addr[2], p_connected->peer_addr.addr[3],
			p_connected->peer_addr.addr[4], p_connected->peer_addr.addr[5]);
	} break;

	default:
		LOG_WRN("Unhandled scan event %d", scan_evt->evt_type);
		break;
	}
}

static uint32_t gatt_init(void)
{
	uint32_t nrf_err = ble_conn_params_evt_handler_set(conn_params_evt_handler);

	if (nrf_err) {
		LOG_ERR("ble_conn_params_evt_handler_set failed, nrf_error %#x", nrf_err);
	}

	return nrf_err;
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

	nrf_err = ble_scan_init(&ble_scan, &scan_cfg);
	if (nrf_err) {
		LOG_ERR("nrf_ble_scan_init failed, nrf_error %#x", nrf_err);
	}

	ble_uuid_t uuid = {
		.uuid = BLE_UUID_HEART_RATE_SERVICE,
		.type = BLE_UUID_TYPE_BLE,
	};

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_UUID_FILTER, &uuid);
	if (nrf_err) {
		LOG_ERR("nrf_ble_scan_filter_add uuid failed, nrf_error %#x", nrf_err);
	}

#if defined(CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_NAME)
		nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_NAME_FILTER,
					      CONFIG_SAMPLE_TARGET_PERIPHERAL_NAME);
		if (nrf_err) {
			LOG_ERR("nrf_ble_scan_filter_add name failed, nrf_error %#x", nrf_err);
		}
#endif /* CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_NAME */

#if defined(CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_ADDR)
		nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_ADDR_FILTER, target_periph_addr);
		if (nrf_err) {
			LOG_ERR("nrf_ble_scan_filter_add address failed, nrf_error %#x", nrf_err);
		}
#endif /* CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_ADDR */

	nrf_err = ble_scan_filters_enable(&ble_scan, BLE_SCAN_UUID_FILTER |
						     BLE_SCAN_NAME_FILTER |
						     BLE_SCAN_ADDR_FILTER, false);
	if (nrf_err) {
		LOG_ERR("Failed to enable scan filters, nrf_error %#x", nrf_err);
	}

	return NRF_SUCCESS;
}

int main(void)
{
	int err;
	uint32_t nrf_err;
	static struct bm_buttons_config configs[] = {
		{
			.pin_number = BOARD_PIN_BTN_0,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler_allow_list_off,
		},
		{
			.pin_number = BOARD_PIN_BTN_1,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler_disconnect,
		},
	};

	LOG_INF("BLE HRS Central sample started.");

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_1);

	err = bm_buttons_init(configs, ARRAY_SIZE(configs), BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		LOG_ERR("Failed to initialize buttons, err %d", err);
		goto idle;
	}

	err = bm_buttons_enable();
	if (err) {
		LOG_ERR("Failed to enable buttons, err %d", err);
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

	nrf_err = gatt_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize gatt, nrf_error %#x", nrf_err);
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

	nrf_err = hrs_c_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize HRS Client, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = scan_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize scan library, nrf_error %#x", nrf_err);
		goto idle;
	}

	scan_start(erase_bonds);

	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
	LOG_INF("BLE HRS sample initialized");

	while (true) {
#if defined(CONFIG_PM_LESC)
		(void)nrf_ble_lesc_request_handler();
#endif
idle:
		log_flush();

		k_cpu_idle();
	}
}
