/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <hal/nrf_gpio.h>
#include <nrf_error.h>
#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <nrf_soc.h>
#include <bm_timer.h>
#include <bm_buttons.h>
#include <ble_adv.h>
#include <ble_gap.h>
#include <ble_types.h>
#include <ble_qwr.h>
#include <bluetooth/services/common.h>
#include <bluetooth/services/uuid.h>
#include <bluetooth/services/ble_bms.h>
#include <bluetooth/services/ble_dis.h>
#include <bluetooth/peer_manager/nrf_ble_lesc.h>
#include <bluetooth/peer_manager/peer_manager.h>
#include <bluetooth/peer_manager/peer_manager_handler.h>
#include <event_scheduler.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/toolchain.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <board-config.h>

/* FIFO for keeping track of keystrokes that can not be sent immediately. */
RING_BUF_DECLARE(peers_to_delete_on_disconnect,
		 CONFIG_BLE_BMS_PEERS_TO_DELETE_ON_DISCONNECT_MAX * sizeof(pm_peer_id_t));

LOG_MODULE_REGISTER(app, CONFIG_BLE_BMS_SAMPLE_LOG_LEVEL);

/* Perform bonding. */
#define SEC_PARAM_BOND 1
/* Man In The Middle protection not required. */
#define SEC_PARAM_MITM 0
/* LE Secure Connections enabled. */
#define SEC_PARAM_LESC 1
/* Keypress notifications not enabled. */
#define SEC_PARAM_KEYPRESS 1
/* No I/O capabilities. */
#define SEC_PARAM_IO_CAPABILITIES BLE_GAP_IO_CAPS_DISPLAY_YESNO
/* Out Of Band data not available. */
#define SEC_PARAM_OOB 0
/* Minimum encryption key size. */
#define SEC_PARAM_MIN_KEY_SIZE 7
/* Maximum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE 16

/* BLE Advertising library instance */
BLE_ADV_DEF(ble_adv);
/* BLE QWR instance */
BLE_QWR_DEF(ble_qwr);
/* BLE Bond Management Service instance */
BLE_BMS_DEF(ble_bms);

/* BLE Connection handle */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
/* Peer ID */
static pm_peer_id_t peer_id;
/* Flag for ongoing authentication request */
static bool auth_key_request;

/* Write buffer for the Queued Write module. */
static uint8_t qwr_mem[CONFIG_QWR_MEM_BUFF_SIZE];

/* Forward declaration */
static void identities_set(pm_peer_id_list_skip_t skip);

static void delete_disconnected_bonds(void);

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	uint32_t nrf_err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected, conn handle %d", evt->evt.gap_evt.conn_handle);
		conn_handle = evt->evt.gap_evt.conn_handle;

		nrf_err = ble_qwr_conn_handle_assign(&ble_qwr, conn_handle);
		if (nrf_err) {
			LOG_ERR("Failed to assign qwr handle, nrf_error %#x", nrf_err);
			return;
		}
		nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);
		nrf_gpio_pin_write(BOARD_PIN_LED_1, BOARD_LED_ACTIVE_STATE);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Peer disconnected, reason %d",
			evt->evt.gap_evt.params.disconnected.reason);

		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}
		delete_disconnected_bonds();
		nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
		break;

	case BLE_GAP_EVT_PASSKEY_DISPLAY:
		LOG_INF("Passkey: %.*s", BLE_GAP_PASSKEY_LEN,
			evt->evt.gap_evt.params.passkey_display.passkey);
		if (evt->evt.gap_evt.params.passkey_display.match_request) {
			LOG_INF("Pairing request, press button 0 to accept or button 1 to reject.");
			auth_key_request = true;
		}
		break;

	case BLE_GAP_EVT_AUTH_KEY_REQUEST:
		LOG_INF("Pairing request, press button 0 to accept or button 1 to reject.");
		auth_key_request = true;
		break;

	default:
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, 0);

static void ble_adv_evt_handler(struct ble_adv *ble_adv, const struct ble_adv_evt *evt)
{
	uint32_t err;
	ble_gap_addr_t *peer_addr;
	ble_gap_addr_t whitelist_addrs[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	ble_gap_irk_t whitelist_irks[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	uint32_t addr_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
	uint32_t irk_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

	switch (evt->evt_type) {
	case BLE_ADV_EVT_ERROR:
		LOG_ERR("Advertising error %d", evt->error.reason);
		break;
	case BLE_ADV_EVT_DIRECTED_HIGH_DUTY:
	case BLE_ADV_EVT_DIRECTED:
	case BLE_ADV_EVT_FAST:
	case BLE_ADV_EVT_SLOW:
	case BLE_ADV_EVT_FAST_WHITELIST:
	case BLE_ADV_EVT_SLOW_WHITELIST:
		nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
		break;
	case BLE_ADV_EVT_IDLE:
		nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);
		break;
	case BLE_ADV_EVT_WHITELIST_REQUEST:
		err = pm_whitelist_get(whitelist_addrs, &addr_cnt,
						whitelist_irks, &irk_cnt);
		if (err) {
			LOG_ERR("Failed to get whitelist, nrf_error %#x", err);
		}
		LOG_DBG("pm_whitelist_get returns %d addr in whitelist and %d irk whitelist",
				addr_cnt, irk_cnt);

		/* Set the correct identities list
		 * (no excluding peers with no Central Address Resolution).
		 */
		identities_set(PM_PEER_ID_LIST_SKIP_NO_IRK);

		err = ble_adv_whitelist_reply(ble_adv, whitelist_addrs, addr_cnt, whitelist_irks,
					      irk_cnt);
		if (err) {
			LOG_ERR("Failed to set whitelist, nrf_error %#x", err);
		}
		break;

	case BLE_ADV_EVT_PEER_ADDR_REQUEST:
		pm_peer_data_bonding_t peer_bonding_data;

		/* Only Give peer address if we have a handle to the bonded peer. */
		if (peer_id != PM_PEER_ID_INVALID) {
			err = pm_peer_data_bonding_load(peer_id, &peer_bonding_data);
			if (err != NRF_ERROR_NOT_FOUND) {
				if (err) {
					LOG_ERR("Failed to load bonding data, nrf_error %#x", err);
				}

				/* Manipulate identities to exclude peers with no
				 * Central Address Resolution.
				 */
				identities_set(PM_PEER_ID_LIST_SKIP_ALL);

				peer_addr = &(peer_bonding_data.peer_ble_id.id_addr_info);
				err = ble_adv_peer_addr_reply(ble_adv, peer_addr);
				if (err) {
					LOG_ERR("Failed to reply peer address, nrf_error %#x", err);
				}
			}
		}
		break;
	default:
		break;
	}
}


static void num_comp_reply(uint16_t conn_handle, bool accept)
{
	uint8_t key_type;
	uint32_t err;

	if (accept) {
		LOG_INF("Numeric Match. Conn handle: %d", conn_handle);
		key_type = BLE_GAP_AUTH_KEY_TYPE_PASSKEY;
	} else {
		LOG_INF("Numeric REJECT. Conn handle: %d", conn_handle);
		key_type = BLE_GAP_AUTH_KEY_TYPE_NONE;
	}

	err = sd_ble_gap_auth_key_reply(conn_handle, key_type, NULL);
	if (err != NRF_SUCCESS) {
		LOG_ERR("Failed to reply auth request, err %d", err);
	}
}

static void button_handler(uint8_t pin, uint8_t action)
{
	if (conn_handle == BLE_CONN_HANDLE_INVALID) {
		return;
	}

	if (auth_key_request) {
		switch (pin) {
		case BOARD_PIN_BTN_0:
			if (action == BM_BUTTONS_PRESS) {
				num_comp_reply(conn_handle, true);
			} else {
				auth_key_request = false;
			}
			break;
		case BOARD_PIN_BTN_1:
			if (action == BM_BUTTONS_PRESS) {
				num_comp_reply(conn_handle, false);
			} else {
				auth_key_request = false;
			}
			break;
		}
		return;
	}
}

static void whitelist_set(pm_peer_id_list_skip_t skip)
{
	uint32_t err;
	pm_peer_id_t peer_ids[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	uint32_t peer_id_count = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

	err = pm_peer_id_list(peer_ids, &peer_id_count, PM_PEER_ID_INVALID, skip);
	if (err) {
		LOG_ERR("Failed to get peer id list, err %d", err);
	}

	LOG_INF("Whitelisted peers: %d, max %d",
		peer_id_count, BLE_GAP_WHITELIST_ADDR_MAX_COUNT);

	err = pm_whitelist_set(peer_ids, peer_id_count);
	if (err) {
		LOG_ERR("Failed to set whitelist, err %d", err);
	}
}

static void identities_set(pm_peer_id_list_skip_t skip)
{
	uint32_t err;
	pm_peer_id_t peer_ids[BLE_GAP_DEVICE_IDENTITIES_MAX_COUNT];
	uint32_t peer_id_count = BLE_GAP_DEVICE_IDENTITIES_MAX_COUNT;

	err = pm_peer_id_list(peer_ids, &peer_id_count, PM_PEER_ID_INVALID, skip);
	if (err) {
		LOG_ERR("Failed to get peer id list, err %d", err);
	}

	err = pm_device_identities_list_set(peer_ids, peer_id_count);
	if (err) {
		LOG_ERR("Failed to set peer manager identity list, err %d", err);
	}
}

static void delete_bonds(void)
{
	uint32_t err;

	LOG_INF("Erasing bonds");

	err = pm_peers_delete();
	if (err) {
		LOG_ERR("Failed to delete peers, err %d", err);
	}
}

static void bond_delete(pm_peer_id_t peer_id, void *ctx)
{
	uint32_t err;
	uint16_t peer_conn_handle;

	LOG_DBG("Attempting to delete bond.");
	if (peer_id != PM_PEER_ID_INVALID) {

		err = pm_conn_handle_get(peer_id, &peer_conn_handle);
		if (err != NRF_SUCCESS) {
			LOG_ERR("Failed to get connection handle for peer %d, nrf_error %#x",
				peer_id, err);
		}

		if (peer_conn_handle == conn_handle) {
			NRFX_CRITICAL_SECTION_ENTER();
			(void)ring_buf_put(&peers_to_delete_on_disconnect,
					   (void *)&peer_id, sizeof(pm_peer_id_t));
			NRFX_CRITICAL_SECTION_EXIT();
			return;
		}

		err = pm_peer_delete(peer_id);
		if (err != NRF_SUCCESS) {
			LOG_ERR("Failed to delete peer, nrf_error %#x", err);
		}
	}

}

static void delete_disconnected_bonds(void)
{
	pm_peer_id_t peer_id;
	uint32_t err;
	bool peer_to_delete;

	while (!ring_buf_is_empty(&peers_to_delete_on_disconnect)) {
		NRFX_CRITICAL_SECTION_ENTER();
		if (!ring_buf_is_empty(&peers_to_delete_on_disconnect)) {
			(void)ring_buf_get(&peers_to_delete_on_disconnect,
					   (uint8_t *)&peer_id, sizeof(pm_peer_id_t));
			peer_to_delete = true;
		}
		NRFX_CRITICAL_SECTION_EXIT();

		if (!peer_to_delete) {
			return;
		}

		LOG_INF("delete bond, peer id %d", peer_id);
		err = pm_peer_delete(peer_id);
		if (err != NRF_SUCCESS) {
			LOG_ERR("Failed to delete peer, nrf_error %#x", err);
		}
	}
}

static void delete_requesting_bond(const struct ble_bms *bms)
{
	uint32_t err;
	pm_peer_id_t peer_id;

	LOG_INF("Client requested that bond to current device deleted");
	err = pm_peer_id_get(bms->conn_handle, &peer_id);
	if (err != NRF_SUCCESS) {
		LOG_ERR("Failed to get peer id, nrf_error %#x", err);
		return;
	}

	LOG_INF("Adding peer id %d to list to delete", peer_id);

	NRFX_CRITICAL_SECTION_ENTER();
	(void)ring_buf_put(&peers_to_delete_on_disconnect, (void *)&peer_id, sizeof(pm_peer_id_t));
	NRFX_CRITICAL_SECTION_EXIT();
}

static void delete_all_bonds(const struct ble_bms *bms)
{
	uint32_t err;
	pm_peer_id_t peer_id;

	LOG_INF("Client requested that all bonds be deleted");

	peer_id = pm_next_peer_id_get(PM_PEER_ID_INVALID);
	while (peer_id != PM_PEER_ID_INVALID) {
		bond_delete(peer_id, NULL);

		peer_id = pm_next_peer_id_get(peer_id);
	}
}

static void delete_all_except_requesting_bond(const struct ble_bms *bms)
{
	uint32_t err;
	uint16_t peer_conn_handle;
	pm_peer_id_t peer_id;

	LOG_INF("Client requested that all bonds except current bond be deleted");

	peer_id = pm_next_peer_id_get(PM_PEER_ID_INVALID);
	while (peer_id != PM_PEER_ID_INVALID) {
		err = pm_conn_handle_get(peer_id, &peer_conn_handle);
		if (err != NRF_SUCCESS) {
			LOG_ERR("Failed to get connection handle, err %#x", err);
		}

		/* Do nothing if this is our own bond. */
		if (peer_conn_handle != bms->conn_handle) {
			if (peer_conn_handle == conn_handle) {
				NRFX_CRITICAL_SECTION_ENTER();
				(void)ring_buf_put(&peers_to_delete_on_disconnect,
						   (void *)&peer_id, sizeof(pm_peer_id_t));
				NRFX_CRITICAL_SECTION_EXIT();
			} else {
				bond_delete(peer_id, NULL);
			}

		}

		peer_id = pm_next_peer_id_get(peer_id);
	}
}

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
		    (memcmp(auth_code, evt->auth_code.code, auth_code_len) != 0)) {
			is_authorized = false;
		}
	#endif
		err = ble_bms_auth_response(&ble_bms, is_authorized);
		if (err) {
			LOG_ERR("BMS auth response failed, err %#x", err);
		}
		break;
	case BLE_BMS_EVT_BOND_DELETE_REQUESTING:
		delete_requesting_bond(bms);
		break;
	case BLE_BMS_EVT_BOND_DELETE_ALL:
		delete_all_bonds(bms);
		break;
	case BLE_BMS_EVT_BOND_DELETE_ALL_EXCEPT_REQUESTING:
		delete_all_except_requesting_bond(bms);
		break;
	}
}

static int advertising_start(bool erase_bonds)
{
	int err = 0;

	if (erase_bonds) {
		delete_bonds();
	} else {
		whitelist_set(PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);

		err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
		if (err) {
			LOG_ERR("Failed to start advertising, err %d", err);
		}
	}

	return err;
}

static void pm_evt_handler(pm_evt_t const *evt)
{
	pm_handler_on_pm_evt(evt);
	pm_handler_disconnect_on_sec_failure(evt);
	pm_handler_flash_clean(evt);

	switch (evt->evt_id) {
	case PM_EVT_CONN_SEC_SUCCEEDED:
		peer_id = evt->peer_id;
		break;

	case PM_EVT_PEERS_DELETE_SUCCEEDED:
		advertising_start(false);
		break;

	case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
		if (evt->params.peer_data_update_succeeded.flash_changed &&
		    (evt->params.peer_data_update_succeeded.data_id == PM_PEER_DATA_ID_BONDING)) {
			LOG_INF("New bond, add the peer to the whitelist if possible");
			/* Note: You should check on what kind of whitelist policy your
			 * application should use.
			 */

			whitelist_set(PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);
		}
		break;

	default:
		break;
	}
}

static int peer_manager_init(void)
{
	ble_gap_sec_params_t sec_param;
	int err;

	err = pm_init();
	if (err) {
		return -EFAULT;
	}

	memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

	/* Security parameters to be used for all security procedures. */
	sec_param = (ble_gap_sec_params_t) {
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

	err = pm_sec_params_set(&sec_param);
	if (err) {
		LOG_ERR("pm_sec_params_set() failed, err: %d", err);
		return -EFAULT;
	}

	err = pm_register(pm_evt_handler);
	if (err) {
		LOG_ERR("pm_register() failed, err: %d", err);
		return -EFAULT;
	}

	return 0;
}

uint16_t ble_qwr_evt_handler(struct ble_qwr *qwr, const struct ble_qwr_evt *qwr_evt)
{
	switch (qwr_evt->evt_type) {
	case BLE_QWR_EVT_ERROR:
		LOG_ERR("QWR error event, nrf_error 0x%x", qwr_evt->error.reason);
		break;
	case BLE_QWR_EVT_EXECUTE_WRITE:
		LOG_INF("QWR execute write event");
		break;
	case BLE_QWR_EVT_AUTH_REQUEST:
		LOG_INF("QWR auth request event");
		break;
	}

	return ble_bms_on_qwr_evt(&ble_bms, qwr, qwr_evt);
}

int main(void)
{
	int err;
	ble_uuid_t adv_uuid_list[] = {
		{ .uuid = BLE_UUID_BMS_SERVICE, .type = BLE_UUID_TYPE_BLE },
	};

	struct ble_adv_config ble_adv_cfg = {
		.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
		.evt_handler = ble_adv_evt_handler,
		.adv_data = {
			.name_type = BLE_ADV_DATA_FULL_NAME,
			.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,

		},
		.sr_data.uuid_lists.complete = {
			.uuid = &adv_uuid_list[0],
			.len = ARRAY_SIZE(adv_uuid_list),
		}
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
		.ctrlpt_sec = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM,
		.feature_sec = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM,
	};

	struct ble_qwr_config qwr_config = {
		.evt_handler = ble_qwr_evt_handler,
		.mem_buffer.len   = ARRAY_SIZE(qwr_mem),
		.mem_buffer.p_mem = qwr_mem,
	};

	struct bm_buttons_config configs[4] = {
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
		{
			.pin_number = BOARD_PIN_BTN_3,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
	};

	LOG_INF("BLE BMS sample started");

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_1);
	nrf_gpio_cfg_output(BOARD_PIN_LED_3);

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

	LOG_INF("Bluetooth is enabled!");

	err = peer_manager_init();
	if (err) {
		LOG_ERR("Failed to initialize Peer Manager, err %d", err);
		goto idle;
	}

	err = ble_qwr_init(&ble_qwr, &qwr_config);
	if (err) {
		LOG_ERR("ble_qwr_init failed, err %d", err);
		goto idle;
	}

	err = ble_dis_init();
	if (err) {
		LOG_ERR("Failed to initialize device information service, err %d", err);
		goto idle;
	}

	err = ble_bms_init(&ble_bms, &bms_cfg);
	if (err) {
		LOG_ERR("Failed to initialize BMS service, err %d", err);
		goto idle;
	}

	err = sd_ble_gap_appearance_set(BLE_APPEARANCE_UNKNOWN);
	if (err) {
		LOG_ERR("Failed to sd_ble_gap_appearance_set, err %d", err);
		goto idle;
	}

	err = ble_adv_init(&ble_adv, &ble_adv_cfg);
	if (err) {
		LOG_ERR("Failed to initialize BLE advertising, err %d", err);
		goto idle;
	}

	err = advertising_start(erase_bonds);
	if (err) {
		LOG_ERR("Failed to start advertising, err %d", err);
		goto idle;
	}

	LOG_INF("Advertising as %s", CONFIG_BLE_ADV_NAME);

idle:
	while (true) {
		(void)nrf_ble_lesc_request_handler();

		while (LOG_PROCESS()) {
			/* Empty. */
		}

		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();
	}

	return 0;
}
