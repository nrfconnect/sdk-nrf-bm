/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/** @file
 *
 * @defgroup ble_cgms_sample main.c
 * @{
 * @ingroup ble_cgms
 * @brief Continuous Glucose Monitoring Profile Sample
 *
 * This file contains the source code for a sample using the Continuous Glucose Monitoring Service.
 * Bond Management Service, Battery Service, and Device Information Service are also present.
 *
 */

#include <stdint.h>
#include <string.h>
#include <ble.h>
#include <hal/nrf_gpio.h>
#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/peer_manager/nrf_ble_lesc.h>
#include <bm/bluetooth/peer_manager/peer_manager.h>
#include <bm/bluetooth/peer_manager/peer_manager_handler.h>
#include <bm/bluetooth/services/common.h>
#include <bm/bluetooth/services/uuid.h>
#include <bm/bluetooth/services/ble_bas.h>
#include <bm/bluetooth/services/ble_cgms.h>
#include <bm/bluetooth/services/ble_dis.h>
#include <bm/sensorsim.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <nrf_soc.h>
#include <bm/bm_timer.h>
#include <bm/bm_buttons.h>
#include <bm/bluetooth/ble_qwr.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/util.h>

#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_BLE_CGMS_LOG_LEVEL);

/* Perform bonding. */
#define SEC_PARAM_BOND 1
/* Man In The Middle protection not required. */
#define SEC_PARAM_MITM 0
/* LE Secure Connections enabled. */
#define SEC_PARAM_LESC 1
/* Keypress notifications not enabled. */
#define SEC_PARAM_KEYPRESS 0
/* No I/O capabilities. */
#define SEC_PARAM_IO_CAPABILITIES BLE_GAP_IO_CAPS_DISPLAY_YESNO
/* Out Of Band data not available. */
#define SEC_PARAM_OOB 0
/* Minimum encryption key size. */
#define SEC_PARAM_MIN_KEY_SIZE 7
/* Maximum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE 16

/** Glucose measurement timer. */
static struct bm_timer glucose_meas_timer;

/** Battery Service instance. */
BLE_BAS_DEF(ble_bas);
/** CGMS instance. */
BLE_CGMS_DEF(ble_cgms);
/** Context for the Queued Write module. */
BLE_QWR_DEF(ble_qwr);
/** Advertising module instance. */
BLE_ADV_DEF(ble_adv);
/** BLE GATT Queue instance. */
BLE_GQ_DEF(ble_gatt_gueue);

/** Handle of the current connection. */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
/* Peer ID */
static uint16_t peer_id;
/* Flag for ongoing authentication request */
static bool auth_key_request;

static uint16_t current_time_offset;
static uint16_t glucose_concentration = CONFIG_SAMPLE_GLUCOSE_CONCENTRATION_MIN;

static uint8_t qwr_mem[CONFIG_SAMPLE_QWR_MEM_BUFF_SIZE];


/**
 * @brief Function for updating the glucose measurement and updating the glucose characteristic in
 *        Continuous Glucose Monitoring Service.
 */
static void read_glucose_measurement(void)
{
	struct ble_cgms_rec rec = {
		.meas = {
			.glucose_concentration = glucose_concentration,
			.sensor_status_annunciation = {
				.warning = 0,
				.calib_temp = 0,
				.status = 0,
			},
			.flags = 0,
			.time_offset = current_time_offset,
		},
	};

	LOG_INF("Read glucose measurement: %dmg/dL", glucose_concentration);

	ble_cgms_meas_create(&ble_cgms, &rec);
}

/**
 * @brief Function for handling the Glucose measurement timer timeout.
 *
 * @details This function will be called each time the glucose measurement timer expires.
 *
 * @param[in] context Pointer used for passing some arbitrary information (context) from the
 *                    bm_timer_start() call to the timeout handler.
 */
static void glucose_meas_timeout_handler(void *context)
{
	uint32_t nrf_err;

	ARG_UNUSED(context);

	if (ble_cgms.comm_interval != 0) {
		current_time_offset += ble_cgms.comm_interval;
	} else {
		current_time_offset += CONFIG_SAMPLE_GLUCOSE_MEAS_INTERVAL;
	}

	read_glucose_measurement();

	ble_cgms.sensor_status.time_offset = current_time_offset;
	nrf_err = ble_cgms_update_status(&ble_cgms, &ble_cgms.sensor_status);
	if (nrf_err) {
		LOG_ERR("Failed to update BLE CGMS status, nrf_error %#x", nrf_err);
	}
}

/**
 * @brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static int timers_init(void)
{
	int err;

	err = bm_timer_init(&glucose_meas_timer, BM_TIMER_MODE_REPEATED,
			      glucose_meas_timeout_handler);
	if (err) {
		LOG_ERR("Failed to initialize glucose meas timer, err %d", err);
		return -1;
	}

	return 0;
}

/**
 * @brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static uint32_t gap_params_init(void)
{
	uint32_t nrf_err;

	nrf_err = sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_GLUCOSE_METER);
	if (nrf_err) {
		LOG_ERR("Failed to set GAP appearance, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static void cgms_evt_handler(struct ble_cgms *cgms, const struct ble_cgms_evt *evt)
{
	int err;

	switch (evt->evt_type) {
	case BLE_CGMS_EVT_ERROR:
		LOG_ERR("BLE Service error %d", evt->error.reason);
		__ASSERT(false, "BLE Service error %d", evt->error.reason);
		break;
	case BLE_CGMS_EVT_NOTIFICATION_ENABLED:
		break;
	case BLE_CGMS_EVT_NOTIFICATION_DISABLED:
		break;

	case BLE_CGMS_EVT_START_SESSION:
		LOG_INF("CGMS Start Session");

		/* Reset measurement time offset. */
		current_time_offset = 0;

		err = bm_timer_start(&glucose_meas_timer,
				       BM_TIMER_MS_TO_TICKS(cgms->comm_interval * 60000), NULL);
		if (err) {
			LOG_ERR("Failed to start glucose meas timer, err %d", err);
		}
		break;

	case BLE_CGMS_EVT_STOP_SESSION:
		LOG_INF("CGMS Stop Session");
		err = bm_timer_stop(&glucose_meas_timer);
		if (err) {
			LOG_ERR("Failed to stop glucose meas timer, err %d", err);
		}
		break;

	case BLE_CGMS_EVT_WRITE_COMM_INTERVAL:
		LOG_INF("CGMS change communication interval");
		if (cgms->comm_interval == 0xFF) {
			cgms->comm_interval = CONFIG_SAMPLE_GLUCOSE_MEAS_INTERVAL;
		}
		err = bm_timer_stop(&glucose_meas_timer);
		if (err) {
			LOG_ERR("Failed to stop glucose meas timer, err %d", err);
		}

		if (cgms->comm_interval != 0) {
			err = bm_timer_start(&glucose_meas_timer,
					       BM_TIMER_MS_TO_TICKS(cgms->comm_interval * 60000),
					       NULL);
			if (err) {
				LOG_ERR("Failed to start glucose meas timer, err %d", err);
			}
		}
		break;

	default:
		break;
	}
}

uint16_t qwr_evt_handler(struct ble_qwr *qwr, const struct ble_qwr_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_QWR_EVT_ERROR:
		LOG_ERR("BLE QWR error, %d", evt->error.reason);
		__ASSERT(false, "BLE QWR error %d", evt->error.reason);
		break;
	case BLE_QWR_EVT_EXECUTE_WRITE:
	case BLE_QWR_EVT_AUTH_REQUEST:
		return BLE_QWR_REJ_REQUEST_ERR_CODE;
	}

	return 0;
}

/**
 * @brief Function for initializing the services that will be used by the application.
 *
 * @details Initialize the Glucose, Battery, and Device Information services.
 */
static uint32_t services_init(void)
{
	uint32_t nrf_err;
	struct ble_cgms_config cgms_config = {
		.evt_handler = cgms_evt_handler,
		.gatt_queue = &ble_gatt_gueue,
		.initial_run_time = 20,
		.initial_sensor_status = {
			.time_offset = 0x00,
			.status.status = BLE_CGMS_STATUS_SESSION_STOPPED,
		},
		.feature = {
			.feature = BLE_CGMS_FEAT_MULTIPLE_SESSIONS_SUPPORTED,
			.type = BLE_CGMS_MEAS_TYPE_VEN_BLOOD,
			.sample_location = BLE_CGMS_MEAS_LOC_AST,
		},
		.sec_mode = BLE_CGMS_CONFIG_SEC_MODE_DEFAULT,
	};
	struct ble_bas_config bas_config = {
		.evt_handler = NULL,
		.can_notify = true,
		.report_ref = NULL,
		.battery_level = 100,
		.sec_mode = BLE_BAS_CONFIG_SEC_MODE_DEFAULT,
	};

	struct ble_qwr_config qwr_config = {
		.mem_buffer.len = CONFIG_SAMPLE_QWR_MEM_BUFF_SIZE,
		.mem_buffer.p_mem = qwr_mem,
		.evt_handler = qwr_evt_handler,
	};

	struct ble_dis_config dis_config = {
		.sec_mode.device_info_char.read = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM,
	};

	nrf_err = ble_qwr_init(&ble_qwr, &qwr_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize QWR service, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	/* Initialize Glucose Service */
	ble_cgms.comm_interval = CONFIG_SAMPLE_GLUCOSE_MEAS_INTERVAL;

	nrf_err = ble_cgms_init(&ble_cgms, &cgms_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize CGMS service, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	ble_cgms.comm_interval = CONFIG_SAMPLE_GLUCOSE_MEAS_INTERVAL;

	/* Add a basic battery measurement with only mandatory fields */

	nrf_err = ble_bas_init(&ble_bas, &bas_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize BAS service, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	/* Initialize Device Information Service. */
	nrf_err = ble_dis_init(&dis_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize DIS service, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}


/**
 * @brief Function for handling the Connection Parameter events.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail configuration parameter, but instead we use the
 *                event handler mechanism to demonstrate its use.
 *
 * @param[in] evt Event received from the Connection Parameters Module.
 */
void on_conn_params_evt(const struct ble_conn_params_evt *evt)
{
	uint32_t nrf_err;

	switch (evt->evt_type) {
	case BLE_CONN_PARAMS_EVT_REJECTED:
		nrf_err = sd_ble_gap_disconnect(conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
		if (nrf_err) {
			LOG_ERR("Failed to disconnect BLE GAP, nrf_error %#x", nrf_err);
		}
		LOG_ERR("Disconnected from peer, unacceptable conn params");
		break;
	default:
		break;
	}
}

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{

	uint32_t nrf_err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Connected");
		nrf_gpio_pin_write(BOARD_PIN_LED_1, BOARD_LED_ACTIVE_STATE);

		conn_handle = evt->evt.gap_evt.conn_handle;
		nrf_err = ble_qwr_conn_handle_assign(&ble_qwr, conn_handle);
		if (nrf_err) {
			LOG_ERR("Failed to assign BLE QWR conn handle, nrf_error %#x", nrf_err);
		}

		nrf_err = ble_cgms_conn_handle_assign(&ble_cgms, conn_handle);
		if (nrf_err) {
			LOG_ERR("Failed to assign BLE CGMS conn handle, nrf_error %#x", nrf_err);
		}

		nrf_err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (nrf_err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", nrf_err);
		}

		break;
	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Disconnected");
		nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}

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
	case BLE_GATTC_EVT_TIMEOUT:
		/* Disconnect on GATT Client timeout event. */
		LOG_DBG("GATT Client Timeout.");
		nrf_err = sd_ble_gap_disconnect(evt->evt.gattc_evt.conn_handle,
						BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (nrf_err) {
			LOG_ERR("Failed to disconnect GAP, nrf_error %#x", nrf_err);
		}
		break;
	case BLE_GATTS_EVT_TIMEOUT:
		/* Disconnect on GATT Server timeout event. */
		LOG_DBG("GATT Server Timeout.");
		nrf_err = sd_ble_gap_disconnect(evt->evt.gatts_evt.conn_handle,
						BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (nrf_err) {
			LOG_ERR("Failed to disconnect GAP, nrf_error %#x", nrf_err);
		}
		break;
	}
}

NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

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
		LOG_ERR("Failed to set peer manager identity list, nrf_error %#x", nrf_err);
	}
}

/**
 * @brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 */
static void ble_adv_evt_handler(struct ble_adv *adv, const struct ble_adv_evt *adv_evt)
{
	uint32_t nrf_err;
	ble_gap_addr_t *peer_addr;
	ble_gap_addr_t allow_list_addrs[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	ble_gap_irk_t allow_list_irks[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	uint32_t addr_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
	uint32_t irk_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

	switch (adv_evt->evt_type) {
	case BLE_ADV_EVT_ERROR:
		LOG_ERR("BLE advertising error, %#x", adv_evt->error.reason);
		__ASSERT(false, "BLE advertising error %#x", adv_evt->error.reason);
		break;
	case BLE_ADV_EVT_DIRECTED_HIGH_DUTY:
	case BLE_ADV_EVT_DIRECTED:
	case BLE_ADV_EVT_FAST:
	case BLE_ADV_EVT_SLOW:
	case BLE_ADV_EVT_FAST_ALLOW_LIST:
	case BLE_ADV_EVT_SLOW_ALLOW_LIST:
		LOG_DBG("Started advertising, adv_evt_type %d", adv_evt->evt_type);
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

		nrf_err = ble_adv_allow_list_reply(adv, allow_list_addrs, addr_cnt,
						   allow_list_irks, irk_cnt);
		if (nrf_err) {
			LOG_ERR("Failed to set allow list, nrf_error %#x", nrf_err);
		}
		break;
	case BLE_ADV_EVT_PEER_ADDR_REQUEST:
		struct pm_peer_data_bonding peer_bonding_data;

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
				nrf_err = ble_adv_peer_addr_reply(adv, peer_addr);
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

/**
 * @brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static int ble_stack_init(void)
{
	int err;

	err = nrf_sdh_enable_request();
	if (err) {
		LOG_ERR("Failed to enable SoftDevice, err %d", err);
		return err;
	}

	LOG_INF("SoftDevice enabled");

	/* Enable BLE stack. */
	err = nrf_sdh_ble_enable(CONFIG_NRF_SDH_BLE_CONN_TAG);
	if (err) {
		LOG_ERR("Failed to enable BLE, err %d", err);
		return err;
	}

	LOG_INF("Bluetooth enabled");

	return 0;
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
}

static void button_handler(uint8_t pin, uint8_t action)
{
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

	if (action != BM_BUTTONS_PRESS) {
		return;
	}

	switch (pin) {
	case BOARD_PIN_BTN_2:
		LOG_INF("Decrease GL Concentration");
		glucose_concentration -= CONFIG_SAMPLE_GLUCOSE_CONCENTRATION_DEC;
		if (glucose_concentration < CONFIG_SAMPLE_GLUCOSE_CONCENTRATION_MIN) {
			glucose_concentration = CONFIG_SAMPLE_GLUCOSE_CONCENTRATION_MAX;
		}
		break;
	case BOARD_PIN_BTN_3:
		LOG_INF("Increase GL Concentration");
		glucose_concentration += CONFIG_SAMPLE_GLUCOSE_CONCENTRATION_INC;
		if (glucose_concentration > CONFIG_SAMPLE_GLUCOSE_CONCENTRATION_MAX) {
			glucose_concentration = CONFIG_SAMPLE_GLUCOSE_CONCENTRATION_MIN;
		}
		break;
	default:
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

static void delete_bonds(void)
{
	uint32_t nrf_err;

	LOG_INF("Erasing bonds");

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
			LOG_INF("New bond, add the peer to the allow list if possible");
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

/** @brief Function for initializing the Advertising functionality. */
static uint32_t advertising_init(void)
{
	uint32_t nrf_err;
	uint8_t adv_flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
	ble_uuid_t adv_uuid_list[] = {
		{ .uuid = BLE_UUID_CGM_SERVICE, .type = BLE_UUID_TYPE_BLE },
	};
	struct ble_adv_config config = {
		.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
		.adv_data = {
			.name_type = BLE_ADV_DATA_FULL_NAME,
			.include_appearance = true,
			.flags = adv_flags,
		},
		.sr_data.uuid_lists.complete = {
			.len = ARRAY_SIZE(adv_uuid_list),
			.uuid = &adv_uuid_list[0],
		},

		.evt_handler = ble_adv_evt_handler,
	};

	nrf_err = ble_adv_init(&ble_adv, &config);
	if (nrf_err) {
		LOG_ERR("BLE advertising init failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	ble_adv_conn_cfg_tag_set(&ble_adv, CONFIG_NRF_SDH_BLE_CONN_TAG);

	return NRF_SUCCESS;
}

struct bm_buttons_config btn_configs[] = {
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

/**
 * @brief Function for initializing buttons and leds.
 *
 * @param[out] erase_bonds true if the clear bonding button was pressed to wake the application up.
 */
static int buttons_leds_init(bool *erase_bonds)
{
	int err;

	err = bm_buttons_init(btn_configs, ARRAY_SIZE(btn_configs),
			      BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		LOG_ERR("bm_buttons_init error: %d", err);
		return err;
	}

	err = bm_buttons_enable();
	if (err) {
		LOG_ERR("bm_buttons_enable error: %d", err);
		return err;
	}

	*erase_bonds = bm_buttons_is_pressed(BOARD_PIN_BTN_1);

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_1);

	return 0;
}

/** @brief Function for application main entry. */
int main(void)
{
	int err;
	uint32_t nrf_err;
	bool erase_bonds = false;

	err = timers_init();
	if (err) {
		goto idle;
	}
	err = buttons_leds_init(&erase_bonds);
	if (err) {
		goto idle;
	}

	err = ble_stack_init();
	if (err) {
		goto idle;
	}
	nrf_err = peer_manager_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize Peer Manager, nrf_error %#x", nrf_err);
		goto idle;
	}
	nrf_err = gap_params_init();
	if (nrf_err) {
		goto idle;
	}
	nrf_err = advertising_init();
	if (nrf_err) {
		goto idle;
	}
	nrf_err = services_init();
	if (nrf_err) {
		goto idle;
	}

	nrf_err = ble_conn_params_evt_handler_set(on_conn_params_evt);
	if (nrf_err) {
		LOG_ERR("Failed to setup conn param event handler, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Continuous Glucose Monitoring sample started.");

	nrf_err = advertising_start(erase_bonds);
	if (nrf_err) {
		LOG_ERR("Failed to start advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Advertising as %s", CONFIG_BLE_ADV_NAME);

	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
	LOG_INF("Continuous Glucose Monitoring sample initialized");

idle:
	/* Enter main loop. */
	while (true) {
		(void)nrf_ble_lesc_request_handler();

		log_flush();

		k_cpu_idle();
	}

	return 0;
}

/** @} */
