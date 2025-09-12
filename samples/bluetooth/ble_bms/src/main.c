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
#include <bluetooth/services/uuid.h>
#include <bluetooth/services/ble_bms.h>
#include <bluetooth/services/ble_dis.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <ble_qwr.h>

#include <board-config.h>

#include <bm_buttons.h>

LOG_MODULE_REGISTER(app, CONFIG_BLE_BMS_SAMPLE_LOG_LEVEL);

BLE_ADV_DEF(ble_adv); /* BLE advertising instance */
BLE_BMS_DEF(ble_bms); /* BLE Bond Management Service instance */
/** Context for the Queued Write module. */
BLE_QWR_DEF(ble_qwr);

/* Device information service is single-instance */

static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
static uint8_t qwr_mem[CONFIG_QWR_MEM_BUFF_SIZE];

#ifdef CONFIG_USE_AUTHORIZATION_CODE //TODO
static uint8_t auth_code[] = {'A', 'B', 'C', 'D'}; //0x41, 0x42, 0x43, 0x44
static int auth_code_len = sizeof(m_auth_code);
#endif

/**
 * Bond management service event handler.
 */
void bms_evt_handler(struct ble_bms *bms, struct ble_bms_evt *evt)
{
	uint32_t err;
	bool is_authorized = true;

	switch (evt->evt_type) {
	case BLE_BMS_EVT_ERROR:
		LOG_ERR("BMS error event, err %#x", evt->error.reason);
		break;
	case BLE_BMS_EVT_AUTH:
		LOG_DBG("Authorization request.");
	#if CONFIG_USE_AUTHORIZATION_CODE
		if ((evt->auth_code.len != auth_code_len) ||
			(memcmp(auth_code, evt->auth_code.code, auth_code_len) != 0))
		{
			is_authorized = false;
		}
	#endif
		err = ble_bms_auth_response(&ble_bms, is_authorized);
		if (err) {
			LOG_ERR("BMS auth response failed, err %#x", err);
		}
	}
}

uint16_t qwr_evt_handler(struct ble_qwr *qwr, const struct ble_qwr_evt *evt)
{
	return ble_bms_on_qwr_evt(&ble_bms, qwr, evt);
}


static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	int err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;
		err = ble_qwr_conn_handle_assign(&ble_qwr, conn_handle);
		if (err) {
			LOG_ERR("Failed to assign BLE QWR conn handle, err %d", err);
		}
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", err);
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
		err = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle,
						  BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
		if (err) {
			LOG_ERR("Failed to reply with Security params, nrf_error %#x", err);
		}
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		LOG_INF("BLE_GATTS_EVT_SYS_ATTR_MISSING");
		/* No system attributes have been stored */
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", err);
		}
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, 0);

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

static void button_handler(uint8_t pin, enum bm_buttons_evt_type action)
{
	LOG_INF("Button event callback: %d, %d", pin, action);
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

static void delete_requesting_bond(const struct ble_bms *bms)
{
	LOG_INF("Client requested that bond to current device deleted");
	//ble_conn_state_user_flag_set(p_bms->conn_handle, m_bms_bonds_to_delete, true);
}

static void delete_all_bonds(const struct ble_bms *bms)
{
//	uint32_t err_code;
//	uint16_t conn_handle;

	LOG_INF("Client requested that all bonds be deleted");

//	pm_peer_id_t peer_id = pm_next_peer_id_get(PM_PEER_ID_INVALID);
//	while (peer_id != PM_PEER_ID_INVALID) {
//		err_code = pm_conn_handle_get(peer_id, &conn_handle);
//		APP_ERROR_CHECK(err_code);
//
//		bond_delete(conn_handle, NULL);
//
//		peer_id = pm_next_peer_id_get(peer_id);
//	}
}

static void delete_all_except_requesting_bond(const struct ble_bms *bms)
{
//	uint32_t err_code;
//	uint16_t conn_handle;

	LOG_INF("Client requested that all bonds except current bond be deleted");

//	pm_peer_id_t peer_id = pm_next_peer_id_get(PM_PEER_ID_INVALID);
//	while (peer_id != PM_PEER_ID_INVALID)
//	{
//		err_code = pm_conn_handle_get(peer_id, &conn_handle);
//		APP_ERROR_CHECK(err_code);
//
//		/* Do nothing if this is our own bond. */
//		if (conn_handle != p_bms->conn_handle)
//		{
//		bond_delete(conn_handle, NULL);
//		}
//
//		peer_id = pm_next_peer_id_get(peer_id);
//	}
}

int main(void)
{
	int err;
	ble_uuid_t adv_uuid_list[] = {
		{ .uuid = BLE_UUID_BMS_SERVICE, .type = BLE_UUID_TYPE_BLE },
	};
	struct ble_adv_config ble_adv_config = {
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

	struct ble_bms_config bms_cfg = {
		.evt_handler = bms_evt_handler,
#if USE_AUTHORIZATION_CODE
		.feature.delete_requesting_auth = true,
		.feature.delete_all_auth = true,
		.feature.delete_all_but_requesting_auth = true,
#else
		.feature.delete_requesting = true,
		.feature.delete_all = true,
		.feature.delete_all_but_requesting = true,
#endif

		.qwr = &ble_qwr,
		.bond_callbacks.delete_requesting = delete_requesting_bond,
		.bond_callbacks.delete_all = delete_all_bonds,
		.bond_callbacks.delete_all_except_requesting = delete_all_except_requesting_bond,
	};


	LOG_INF("BLE BMS sample started");

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

	err = ble_dis_init();
	if (err) {
		LOG_ERR("Failed to initialize device information service, err %d", err);
		goto idle;
	}

	struct ble_qwr_config qwr_config = {
		.mem_buffer.len = CONFIG_QWR_MEM_BUFF_SIZE,
		.mem_buffer.p_mem = qwr_mem,
		.evt_handler = qwr_evt_handler,
	};

	err = ble_qwr_init(&ble_qwr, &qwr_config);
	if (err) {
		LOG_ERR("Failed to initialize QWR service, err %d", err);
		goto idle;
	}

	err = ble_bms_init(&ble_bms, &bms_cfg);
	if (err) {
		LOG_ERR("Failed to initialize BMS service, err %d", err);
		goto idle;
	}

	LOG_INF("Services initialized");

	err = ble_adv_init(&ble_adv, &ble_adv_config);
	if (err) {
		LOG_ERR("Failed to initialize BLE advertising, err %d", err);
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
