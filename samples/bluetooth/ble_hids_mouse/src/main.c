/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <stdint.h>
#include <nrf_soc.h>
#include <ble_gap.h>
#include <ble_types.h>
#include <hal/nrf_gpio.h>
#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/ble_qwr.h>
#include <bm/bluetooth/services/common.h>
#include <bm/bluetooth/services/uuid.h>
#include <bm/bluetooth/services/ble_bas.h>
#include <bm/bluetooth/services/ble_dis.h>
#include <bm/bluetooth/services/ble_hids.h>
#include <bm/bluetooth/peer_manager/nrf_ble_lesc.h>
#include <bm/bluetooth/peer_manager/peer_manager.h>
#include <bm/bluetooth/peer_manager/peer_manager_handler.h>

#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/bm_scheduler.h>
#include <bm/bm_buttons.h>
#include <bm/bm_timer.h>
#include <bm/sensorsim.h>

#include <zephyr/kernel.h>
#include <zephyr/toolchain.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_BLE_HIDS_MOUSE_LOG_LEVEL);

#define BASE_USB_HID_SPEC_VERSION 0x0101

/**< Number of pixels by which the cursor is moved each time a button is pushed. */
#define MOVEMENT_SPEED 5
/**< Number of input reports in this application. */
#define INPUT_REPORT_COUNT 3
/**< Length of Mouse Input Report containing button data. */
#define INPUT_REP_BUTTONS_LEN 3
/**< Length of Mouse Input Report containing movement data. */
#define INPUT_REP_MOVEMENT_LEN 3
/**< Length of Mouse Input Report containing media player data. */
#define INPUT_REP_MEDIA_PLAYER_LEN 1
/**< Index of Mouse Input Report containing button data. */
#define INPUT_REP_BUTTONS_INDEX 0
/**< Index of Mouse Input Report containing movement data. */
#define INPUT_REP_MOVEMENT_INDEX 1
/**< Index of Mouse Input Report containing media player data. */
#define INPUT_REP_MPLAYER_INDEX 2
/**< Id of reference to Mouse Input Report containing button data. */
#define INPUT_REP_REF_BUTTONS_ID 1
/**< Id of reference to Mouse Input Report containing movement data. */
#define INPUT_REP_REF_MOVEMENT_ID 2
/**< Id of reference to Mouse Input Report containing media player data. */
#define INPUT_REP_REF_MPLAYER_ID 3

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

/* HID service instance */
BLE_HIDS_DEF(ble_hids);
/* BLE Advertising library instance */
BLE_ADV_DEF(ble_adv);
/* BLE BAS instance */
BLE_BAS_DEF(ble_bas);
/* BLE QWR instance */
BLE_QWR_DEF(ble_qwr);

/* BLE Connection handle */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

static uint16_t peer_id;

static void identities_set(enum pm_peer_id_list_skip skip);

static bool boot_mode;
static bool auth_key_request;

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	uint32_t nrf_err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;

		nrf_err = ble_qwr_conn_handle_assign(&ble_qwr, conn_handle);
		if (nrf_err) {
			LOG_ERR("Failed to assign qwr handle, nrf_error %#x", nrf_err);
		}
		nrf_gpio_pin_write(BOARD_PIN_LED_1, BOARD_LED_ACTIVE_STATE);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Peer disconnected");
		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}
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
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

static void ble_adv_evt_handler(struct ble_adv *ble_adv, const struct ble_adv_evt *evt)
{
	uint32_t nrf_err;
	ble_gap_addr_t *peer_addr;
	struct pm_peer_data_bonding peer_bonding_data;
	ble_gap_addr_t allow_list_addrs[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	ble_gap_irk_t allow_list_irks[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	uint32_t addr_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
	uint32_t irk_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

	switch (evt->evt_type) {
	case BLE_ADV_EVT_ERROR:
		LOG_ERR("Advertising error %#x", evt->error.reason);
		break;
	case BLE_ADV_EVT_DIRECTED_HIGH_DUTY:
	case BLE_ADV_EVT_DIRECTED:
	case BLE_ADV_EVT_FAST:
	case BLE_ADV_EVT_SLOW:
	case BLE_ADV_EVT_FAST_ALLOW_LIST:
	case BLE_ADV_EVT_SLOW_ALLOW_LIST:
		LOG_DBG("Started advertising, adv_evt_type %d", evt->evt_type);
		break;
	case BLE_ADV_EVT_IDLE:
		LOG_DBG("Advertising idle");
		break;
	case BLE_ADV_EVT_ALLOW_LIST_REQUEST:
		nrf_err = pm_allow_list_get(allow_list_addrs, &addr_cnt, allow_list_irks, &irk_cnt);
		if (nrf_err) {
			LOG_ERR("Failed to get allow list, nrf_error %#x", nrf_err);
		}
		LOG_DBG("pm_allow_list_get returns %d addr in allow list and %d irk allow list",
				addr_cnt, irk_cnt);

		/* Set the correct identities list
		 * (no excluding peers with no Central Address Resolution).
		 */
		identities_set(PM_PEER_ID_LIST_SKIP_NO_IRK);

		nrf_err = ble_adv_allow_list_reply(ble_adv, allow_list_addrs, addr_cnt,
						   allow_list_irks, irk_cnt);
		if (nrf_err) {
			LOG_ERR("Failed to set allow list, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_ADV_EVT_PEER_ADDR_REQUEST:
		/* Only Give peer address if we have a handle to the bonded peer. */
		if (peer_id != PM_PEER_ID_INVALID) {
			nrf_err = pm_peer_data_bonding_load(peer_id, &peer_bonding_data);
			if (nrf_err != NRF_ERROR_NOT_FOUND) {
				if (nrf_err) {
					LOG_ERR("Failed to load bonding data, nrf_error %#x",
						nrf_err);
				}

				/* Manipulate identities to exclude peers with no
				 * Central Address Resolution.
				 */
				identities_set(PM_PEER_ID_LIST_SKIP_ALL);

				peer_addr = &(peer_bonding_data.peer_ble_id.id_addr_info);
				nrf_err = ble_adv_peer_addr_reply(ble_adv, peer_addr);
				if (nrf_err) {
					LOG_ERR("Failed to reply peer address, nrf_error %#x",
						nrf_err);
				}
			}
		}
		break;
	default:
		break;
	}
}

static void on_hids_evt(struct ble_hids *hids, const struct ble_hids_evt *hids_evt)
{
	switch (hids_evt->evt_type) {
	case BLE_HIDS_EVT_HOST_SUSP:
		LOG_DBG("Host suspended event");
		break;
	case BLE_HIDS_EVT_HOST_EXIT_SUSP:
		LOG_DBG("Exit suspended event");
		break;
	case BLE_HIDS_EVT_NOTIF_ENABLED:
		LOG_DBG("Notifications enabled event");
		break;
	case BLE_HIDS_EVT_NOTIF_DISABLED:
		LOG_DBG("Notifications disabled event");
		break;
	case BLE_HIDS_EVT_REP_CHAR_WRITE:
		LOG_DBG("Report characteristic write event");
		break;
	case BLE_HIDS_EVT_BOOT_MODE_ENTERED:
		LOG_DBG("Entered boot mode");
		boot_mode = true;
		break;
	case BLE_HIDS_EVT_REPORT_MODE_ENTERED:
		LOG_DBG("Entered report mode");
		boot_mode = false;
		break;
	case BLE_HIDS_EVT_REPORT_READ:
		LOG_DBG("Read report event");
		break;
	default:
		break;
	}
}

static uint32_t hids_init(void)
{
	struct ble_hids_report_config *input_report;

	static struct ble_hids_report_config input_report_array[3];
	static uint8_t report_map_data[] = {
		0x05, 0x01, /* Usage Page (Generic Desktop) */
		0x09, 0x02, /* Usage (Mouse) */

		0xA1, 0x01, /* Collection (Application) */

		/* Report ID 1: Mouse buttons + scroll/pan */
		0x85, 0x01, /* Report Id 1 */
		0x09, 0x01, /* Usage (Pointer) */
		0xA1, 0x00, /* Collection (Physical) */
		0x95, 0x05, /* Report Count (3) */
		0x75, 0x01, /* Report Size (1) */
		0x05, 0x09, /* Usage Page (Buttons) */
		0x19, 0x01, /* Usage Minimum (01) */
		0x29, 0x05, /* Usage Maximum (05) */
		0x15, 0x00, /* Logical Minimum (0) */
		0x25, 0x01, /* Logical Maximum (1) */
		0x81, 0x02, /* Input (Data, Variable, Absolute) */
		0x95, 0x01, /* Report Count (1) */
		0x75, 0x03, /* Report Size (3) */
		0x81, 0x01, /* Input (Constant) for padding */
		0x75, 0x08, /* Report Size (8) */
		0x95, 0x01, /* Report Count (1) */
		0x05, 0x01, /* Usage Page (Generic Desktop) */
		0x09, 0x38, /* Usage (Wheel) */
		0x15, 0x81, /* Logical Minimum (-127) */
		0x25, 0x7F, /* Logical Maximum (127) */
		0x81, 0x06, /* Input (Data, Variable, Relative) */
		0x05, 0x0C, /* Usage Page (Consumer) */
		0x0A, 0x38, 0x02, /* Usage (AC Pan) */
		0x95, 0x01, /* Report Count (1) */
		0x81, 0x06, /* Input (Data,Value,Relative,Bit Field) */
		0xC0, /* End Collection (Physical) */

		/* Report ID 2: Mouse motion */
		0x85, 0x02, /* Report Id 2 */
		0x09, 0x01, /* Usage (Pointer) */
		0xA1, 0x00, /* Collection (Physical) */
		0x75, 0x0C, /* Report Size (12) */
		0x95, 0x02, /* Report Count (2) */
		0x05, 0x01, /* Usage Page (Generic Desktop) */
		0x09, 0x30, /* Usage (X) */
		0x09, 0x31, /* Usage (Y) */
		0x16, 0x01, 0xF8, /* Logical maximum (2047) */
		0x26, 0xFF, 0x07, /* Logical minimum (-2047) */
		0x81, 0x06, /* Input (Data, Variable, Relative) */
		0xC0, /* End Collection (Physical) */
		0xC0, /* End Collection (Application) */

		/* Report ID 3: Advanced buttons */
		0x05, 0x0C, /* Usage Page (Consumer) */
		0x09, 0x01, /* Usage (Consumer Control) */
		0xA1, 0x01, /* Collection (Application) */
		0x85, 0x03, /* Report Id (3) */
		0x15, 0x00, /* Logical minimum (0) */
		0x25, 0x01, /* Logical maximum (1) */
		0x75, 0x01, /* Report Size (1) */
		0x95, 0x01, /* Report Count (1) */

		0x09, 0xCD, /* Usage (Play/Pause) */
		0x81, 0x06, /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x83, 0x01, /* Usage (Consumer Control Configuration) */
		0x81, 0x06, /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xB5, /* Usage (Scan Next Track) */
		0x81, 0x06, /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xB6, /* Usage (Scan Previous Track) */
		0x81, 0x06, /* Input (Data,Value,Relative,Bit Field) */

		0x09, 0xEA, /* Usage (Volume Down) */
		0x81, 0x06, /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xE9, /* Usage (Volume Up) */
		0x81, 0x06, /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x25, 0x02, /* Usage (AC Forward) */
		0x81, 0x06, /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x24, 0x02, /* Usage (AC Back) */
		0x81, 0x06, /* Input (Data,Value,Relative,Bit Field) */
		0xC0 /* End Collection */
	};

	memset((void *)input_report_array, 0, sizeof(input_report_array));

	/* Initialize HID Service */
	input_report = &input_report_array[INPUT_REP_BUTTONS_INDEX];
	input_report->len = INPUT_REP_BUTTONS_LEN;
	input_report->report_id = INPUT_REP_REF_BUTTONS_ID;
	input_report->report_type = BLE_HIDS_REPORT_TYPE_INPUT;

	input_report->sec_mode.read = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	input_report->sec_mode.write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	input_report->sec_mode.cccd_write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;

	input_report = &input_report_array[INPUT_REP_MOVEMENT_INDEX];
	input_report->len = INPUT_REP_MOVEMENT_LEN;
	input_report->report_id = INPUT_REP_REF_MOVEMENT_ID;
	input_report->report_type = BLE_HIDS_REPORT_TYPE_INPUT;

	input_report->sec_mode.read = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	input_report->sec_mode.write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	input_report->sec_mode.cccd_write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;

	input_report = &input_report_array[INPUT_REP_MPLAYER_INDEX];
	input_report->len = INPUT_REP_MEDIA_PLAYER_LEN;
	input_report->report_id = INPUT_REP_REF_MPLAYER_ID;
	input_report->report_type = BLE_HIDS_REPORT_TYPE_INPUT;

	input_report->sec_mode.read = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	input_report->sec_mode.write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	input_report->sec_mode.cccd_write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;

	struct ble_hids_config hids_config = {
		.evt_handler = on_hids_evt,
		.input_report = input_report_array,
		.output_report = NULL,
		.feature_report = NULL,
		.input_report_count = ARRAY_SIZE(input_report_array),
		.output_report_count = 0,
		.feature_report_count = 0,
		.hid_information = {
			.bcd_hid = BASE_USB_HID_SPEC_VERSION,
			.b_country_code = 0,
			.flags = {
				.remote_wake = 1,
				.normally_connectable = 1,
			},
		},
		.report_map = {
			.data = report_map_data,
			.len = sizeof(report_map_data),
		},
		.included_services_count = 0,
		.included_services_array = NULL,
		.sec_mode = BLE_HIDS_CONFIG_SEC_MODE_DEFAULT_MOUSE,
	};

	return ble_hids_init(&ble_hids, &hids_config);
}

static void mouse_movement_send(struct ble_hids *hids, int16_t delta_x, int16_t delta_y)
{
	uint32_t nrf_err;
	uint8_t buffer[INPUT_REP_MOVEMENT_LEN];
	struct ble_hids_boot_mouse_input_report boot_mouse_inp_rep = {
		.buttons = 0x00,
	};
	struct ble_hids_input_report inp_rep = {
		.report_index = INPUT_REP_MOVEMENT_INDEX,
		.data = buffer,
		.len = INPUT_REP_MOVEMENT_LEN,
	};

	if (boot_mode) {
		boot_mouse_inp_rep.delta_x = MIN(delta_x, 0x00ff);
		boot_mouse_inp_rep.delta_y = MIN(delta_y, 0x00ff);

		nrf_err = ble_hids_boot_mouse_inp_rep_send(hids, conn_handle, &boot_mouse_inp_rep);
	} else {
		delta_x = MIN(delta_x, 0x00ff);
		delta_y = MIN(delta_y, 0x0fff);

		buffer[0] = delta_x & 0x00ff;
		buffer[1] = ((delta_y & 0x000f) << 4) | ((delta_x & 0x0f00) >> 8);
		buffer[2] = (delta_y & 0x0ff0) >> 4;

		nrf_err = ble_hids_inp_rep_send(hids, conn_handle, &inp_rep);
	}

	if (nrf_err && nrf_err != BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
		LOG_ERR("Failed to send input report, nrf_error %#x", nrf_err);
	}
}

static void num_comp_reply(uint16_t conn_handle, bool accept)
{
	uint8_t key_type;
	uint32_t nrf_err;

	if (accept) {
		LOG_INF("Numeric Match. Conn handle: %d", conn_handle);
		key_type = BLE_GAP_AUTH_KEY_TYPE_PASSKEY;
	} else {
		LOG_INF("Numeric REJECT. Conn handle: %d", conn_handle);
		key_type = BLE_GAP_AUTH_KEY_TYPE_NONE;
	}

	nrf_err = sd_ble_gap_auth_key_reply(conn_handle, key_type, NULL);
	if (nrf_err) {
		LOG_ERR("Failed to reply auth request, nrf_error %#x", nrf_err);
	}

	auth_key_request = false;
}

static void button_handler(uint8_t pin, uint8_t action)
{
	if (conn_handle == BLE_CONN_HANDLE_INVALID || action != BM_BUTTONS_PRESS) {
		return;
	}

	if (auth_key_request) {
		switch (pin) {
		case BOARD_PIN_BTN_0:
			num_comp_reply(conn_handle, true);
			break;
		case BOARD_PIN_BTN_1:
			num_comp_reply(conn_handle, false);
			break;
		}
		return;
	}

	switch (pin) {
	case BOARD_PIN_BTN_0:
		mouse_movement_send(&ble_hids, -MOVEMENT_SPEED, 0);
		break;
	case BOARD_PIN_BTN_1:
		mouse_movement_send(&ble_hids, 0, -MOVEMENT_SPEED);
		break;
	case BOARD_PIN_BTN_2:
		mouse_movement_send(&ble_hids, MOVEMENT_SPEED, 0);
		break;
	case BOARD_PIN_BTN_3:
		mouse_movement_send(&ble_hids, 0, MOVEMENT_SPEED);
		break;
	}
}

static void allow_list_set(enum pm_peer_id_list_skip skip)
{
	uint32_t nrf_err;
	uint16_t peer_ids[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	uint32_t peer_id_count = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

	nrf_err = pm_peer_id_list(peer_ids, &peer_id_count, PM_PEER_ID_INVALID, skip);
	if (nrf_err) {
		LOG_ERR("Failed to get peer id list, nrf_error %#x", nrf_err);
	}

	LOG_INF("Number of peers added to the allow list: %d, max %d",
		peer_id_count, BLE_GAP_WHITELIST_ADDR_MAX_COUNT);

	nrf_err = pm_allow_list_set(peer_ids, peer_id_count);
	if (nrf_err) {
		LOG_ERR("Failed to set allow list, nrf_error %#x", nrf_err);
	}
}

static void identities_set(enum pm_peer_id_list_skip skip)
{
	uint32_t nrf_err;
	uint16_t peer_ids[BLE_GAP_DEVICE_IDENTITIES_MAX_COUNT];
	uint32_t peer_id_count = BLE_GAP_DEVICE_IDENTITIES_MAX_COUNT;

	nrf_err = pm_peer_id_list(peer_ids, &peer_id_count, PM_PEER_ID_INVALID, skip);
	if (nrf_err) {
		LOG_ERR("Failed to get peer id list, nrf_error %#x", nrf_err);
	}

	nrf_err = pm_device_identities_list_set(peer_ids, peer_id_count);
	if (nrf_err) {
		LOG_ERR("Failed to set identities list, nrf_error %#x", nrf_err);
	}
}

static void delete_bonds(void)
{
	uint32_t nrf_err;

	LOG_INF("Erase bonds!");

	nrf_err = pm_peers_delete();
	if (nrf_err) {
		LOG_ERR("Failed to delete peers, nrf_error %#x", nrf_err);
	}
}

static uint32_t advertising_start(bool erase_bonds)
{
	uint32_t nrf_err = NRF_SUCCESS;

	if (erase_bonds) {
		delete_bonds();
	} else {
		allow_list_set(PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);

		nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
		if (nrf_err) {
			LOG_ERR("Failed to start advertising, nrf_error %#x", nrf_err);
		}
	}

	return nrf_err;
}

static void pm_evt_handler(const struct pm_evt *evt)
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
			LOG_INF("New Bond, add the peer to the allow list if possible");
			/* Note: You should check on what kind of allow list policy your
			 * application should use.
			 */
			allow_list_set(PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);
		}
		break;

	default:
		break;
	}
}

static uint32_t peer_manager_init(void)
{
	ble_gap_sec_params_t sec_param;
	uint32_t nrf_err;

	nrf_err = pm_init();
	if (nrf_err) {
		return nrf_err;
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

	nrf_err = pm_sec_params_set(&sec_param);
	if (nrf_err) {
		LOG_ERR("pm_sec_params_set() failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	nrf_err = pm_register(pm_evt_handler);
	if (nrf_err) {
		LOG_ERR("pm_register() failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

uint16_t ble_qwr_evt_handler(struct ble_qwr *qwr, const struct ble_qwr_evt *qwr_evt)
{
	switch (qwr_evt->evt_type) {
	case BLE_QWR_EVT_ERROR:
		LOG_ERR("QWR error event, nrf_error %#x", qwr_evt->error.reason);
		break;
	case BLE_QWR_EVT_EXECUTE_WRITE:
		LOG_INF("QWR execute write event");
		break;
	case BLE_QWR_EVT_AUTH_REQUEST:
		LOG_INF("QWR auth request event");
		break;
	}

	return BLE_GATT_STATUS_SUCCESS;
}

int main(void)
{
	int err;
	uint32_t nrf_err;

	ble_uuid_t adv_uuid_list[] = {
		{ .uuid = BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE, .type = BLE_UUID_TYPE_BLE },
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

	struct ble_bas_config bas_config = {
		.evt_handler = NULL,
		.can_notify = true,
		.report_ref = NULL,
		.battery_level = 100,
		.sec_mode = BLE_BAS_CONFIG_SEC_MODE_DEFAULT,
	};

	struct ble_dis_config dis_config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
	};

	struct ble_qwr_config qwr_config = {
		.evt_handler = ble_qwr_evt_handler,
	};

	struct bm_buttons_config configs[] = {
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

	LOG_INF("BLE HIDS Mouse sample started.");

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_1);

	err = bm_buttons_init(configs, ARRAY_SIZE(configs), BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		LOG_ERR("Failed to initialize buttons, err: %d", err);
		goto idle;
	}

	err = bm_buttons_enable();
	if (err) {
		LOG_ERR("Failed to enable buttons, err: %d", err);
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

	LOG_INF("Bluetooth enabled!");

	nrf_err = peer_manager_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize Peer Manager, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_qwr_init(&ble_qwr, &qwr_config);
	if (nrf_err) {
		LOG_ERR("ble_qwr_init failed, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_dis_init(&dis_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize device information service, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_bas_init(&ble_bas, &bas_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize BAS service, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = hids_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize HIDS, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("HIDS initialized");

	nrf_err = ble_adv_init(&ble_adv, &ble_adv_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize BLE advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = advertising_start(erase_bonds);
	if (nrf_err) {
		LOG_ERR("Failed to start advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Advertising as %s", CONFIG_BLE_ADV_NAME);

	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
	LOG_INF("BLE HIDS Mouse sample initialized");

idle:
	while (true) {
		(void)nrf_ble_lesc_request_handler();

		log_flush();

		k_cpu_idle();
	}

	return 0;
}
