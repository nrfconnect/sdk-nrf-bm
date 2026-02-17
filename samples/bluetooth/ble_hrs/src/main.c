/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <ble_gap.h>
#include <nrf_soc.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/peer_manager/nrf_ble_lesc.h>
#include <bm/bluetooth/peer_manager/peer_manager.h>
#include <bm/bluetooth/peer_manager/peer_manager_handler.h>
#include <bm/bluetooth/services/uuid.h>
#include <bm/bluetooth/services/ble_bas.h>
#include <bm/bluetooth/services/ble_dis.h>
#include <bm/bluetooth/services/ble_hrs.h>
#include <bm/bm_buttons.h>
#include <bm/bm_timer.h>
#include <bm/sensorsim.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <board-config.h>

LOG_MODULE_REGISTER(app, CONFIG_SAMPLE_BLE_HRS_LOG_LEVEL);

#define CONN_TAG 1

/* Perform bonding. */
#define SEC_PARAM_BOND 1
/* Man In The Middle protection not required. */
#define SEC_PARAM_MITM 0
/* LE Secure Connections enabled. */
#define SEC_PARAM_LESC 1
/* Keypress notifications not enabled. */
#define SEC_PARAM_KEYPRESS 0
/* No I/O capabilities. */
#define SEC_PARAM_IO_CAPABILITIES BLE_GAP_IO_CAPS_DISPLAY_ONLY
/* Out Of Band data not available. */
#define SEC_PARAM_OOB 0
/* Minimum encryption key size. */
#define SEC_PARAM_MIN_KEY_SIZE 7
/* Maximum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE 16

BLE_ADV_DEF(ble_adv); /* BLE advertising instance */
BLE_BAS_DEF(ble_bas); /* BLE battery service instance */
BLE_HRS_DEF(ble_hrs); /* BLE heart rate service instance */

/** Handle of the current connection. */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

/**
 * Flag for enabling and disabling the registration of new RR interval measurements.
 * The purpose of disabling this is just to test sending HRM without RR interval data.
 */
static bool rr_interval_enabled = true;

static struct sensorsim_state battery_sim_state;
static struct sensorsim_state heart_rate_sim_state;
static struct sensorsim_state rr_interval_sim_state;

static struct bm_timer battery_timer;
static struct bm_timer heart_rate_timer;
static struct bm_timer rr_interval_timer;
static struct bm_timer sensor_contact_timer;

static bool hrs_notif_enabled;
static bool bas_notif_enabled;

void battery_level_meas_timeout_handler(void *context)
{
	int err;
	uint32_t nrf_err;
	uint32_t battery_level;

	ARG_UNUSED(context);

	err = sensorsim_measure(&battery_sim_state, &battery_level);
	if (err) {
		LOG_ERR("Failed to get battery measurement, err %d", err);
		return;
	}

	if (!bas_notif_enabled) {
		return;
	}

	nrf_err = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	if (nrf_err) {
		/* Ignore if not in a connection or notifications disabled in CCCD. */
		if (nrf_err != NRF_ERROR_NOT_FOUND && nrf_err != NRF_ERROR_INVALID_STATE) {
			LOG_ERR("Failed to update battery level, nrf_error %#x", nrf_err);
		}
	}
}

static void heart_rate_meas_timeout_handler(void *context)
{
	static uint32_t cnt;
	int err;
	uint32_t nrf_err;
	uint32_t heart_rate;

	ARG_UNUSED(context);

	err = sensorsim_measure(&heart_rate_sim_state, &heart_rate);
	if (err) {
		LOG_ERR("Failed to get heart rate measurement, err %d", err);
		return;
	}

	if (!hrs_notif_enabled) {
		return;
	}

	nrf_err = ble_hrs_heart_rate_measurement_send(&ble_hrs, (uint16_t)heart_rate);
	if (nrf_err) {
		/* Ignore if not in a connection or notifications disabled in CCCD. */
		if (nrf_err != NRF_ERROR_NOT_FOUND && nrf_err != NRF_ERROR_INVALID_STATE) {
			LOG_ERR("Failed to update heart rate measurement, nrf_error %#x", nrf_err);
		}
	}

	/* Disable RR Interval recording every third heart rate measurement.
	 * NOTE: An application will normally not do this. It is done here just for
	 * testing generation of messages without RR Interval measurements.
	 */
	rr_interval_enabled = ((cnt++ % 3) != 0);
}

static void rr_interval_timeout_handler(void *context)
{
	int err;
	uint32_t nrf_err;
	uint32_t rr_interval;

	ARG_UNUSED(context);

	if (!rr_interval_enabled) {
		return;
	}

	for (int i = 0; i < 3; i++) {
		err = sensorsim_measure(&rr_interval_sim_state, &rr_interval);
		if (err) {
			LOG_ERR("Failed to get RR interval measurement, err %d", err);
			break;
		}

		nrf_err = ble_hrs_rr_interval_add(&ble_hrs, (uint16_t)rr_interval);
		if (nrf_err) {
			LOG_ERR("Failed to add RR interval, nrf_error %#x", nrf_err);
		}
	}
}

static void sensor_contact_detected_timeout_handler(void *context)
{
	static bool sim_sensor_contact_detected;
	uint32_t nrf_err;

	ARG_UNUSED(context);

	sim_sensor_contact_detected = !sim_sensor_contact_detected;
	nrf_err = ble_hrs_sensor_contact_detected_update(&ble_hrs, sim_sensor_contact_detected);
	if (nrf_err) {
		LOG_ERR("Failed to update sensor contact detected state, nrf_error %#x", nrf_err);
	}
}

static void simulated_meas_init(void)
{
	int err;
	const struct sensorsim_cfg battery_sim_cfg = {
		.min = CONFIG_SAMPLE_BATTERY_LEVEL_MIN, .max = CONFIG_SAMPLE_BATTERY_LEVEL_MAX,
		.incr = CONFIG_SAMPLE_BATTERY_LEVEL_INCREMENT, .start_at_max = true,
	};
	const struct sensorsim_cfg heart_rate_sim_cfg = {
		.min = CONFIG_SAMPLE_HEART_RATE_MIN, .max = CONFIG_SAMPLE_HEART_RATE_MAX,
		.incr = CONFIG_SAMPLE_HEART_RATE_INCREMENT, .start_at_max = false,
	};
	const struct sensorsim_cfg rr_interval_sim_cfg = {
		.min = CONFIG_SAMPLE_RR_INTERVAL_MIN, .max = CONFIG_SAMPLE_RR_INTERVAL_MAX,
		.incr = CONFIG_SAMPLE_RR_INTERVAL_INCREMENT, .start_at_max = false,
	};

	err = sensorsim_init(&battery_sim_state, &battery_sim_cfg);
	if (err) {
		LOG_ERR("Failed to initialize battery simulator, err %d", err);
	}
	err = sensorsim_init(&heart_rate_sim_state, &heart_rate_sim_cfg);
	if (err) {
		LOG_ERR("Failed to initialize heart rate simulator, err %d", err);
	}
	err = sensorsim_init(&rr_interval_sim_state, &rr_interval_sim_cfg);
	if (err) {
		LOG_ERR("Failed to initialize RR interval simulator, err %d", err);
	}

	err = bm_timer_init(&battery_timer, BM_TIMER_MODE_REPEATED,
			      battery_level_meas_timeout_handler);
	if (err) {
		LOG_ERR("Failed to initialize battery measurement timer, err %d", err);
	}
	err = bm_timer_init(&heart_rate_timer, BM_TIMER_MODE_REPEATED,
			      heart_rate_meas_timeout_handler);
	if (err) {
		LOG_ERR("Failed to initialize heart rate measurement timer, err %d", err);
	}
	err = bm_timer_init(&rr_interval_timer, BM_TIMER_MODE_REPEATED,
			      rr_interval_timeout_handler);
	if (err) {
		LOG_ERR("Failed to initialize RR interval measurement timer, err %d", err);
	}
	err = bm_timer_init(&sensor_contact_timer, BM_TIMER_MODE_REPEATED,
			      sensor_contact_detected_timeout_handler);
	if (err) {
		LOG_ERR("Failed to initialize sensor contact measurement timer, err %d", err);
	}
}

static void simulated_meas_start(void)
{
	(void)bm_timer_start(&battery_timer,
			     BM_TIMER_MS_TO_TICKS(CONFIG_SAMPLE_BATTERY_LEVEL_MEAS_INTERVAL), NULL);
	(void)bm_timer_start(&heart_rate_timer,
			     BM_TIMER_MS_TO_TICKS(CONFIG_SAMPLE_HEART_RATE_MEAS_INTERVAL), NULL);
	(void)bm_timer_start(&rr_interval_timer,
			     BM_TIMER_MS_TO_TICKS(CONFIG_SAMPLE_RR_INTERVAL_MEAS_INTERVAL), NULL);
	(void)bm_timer_start(&sensor_contact_timer,
			     BM_TIMER_MS_TO_TICKS(CONFIG_SAMPLE_SENSOR_CONTACT_DETECTED_INTERVAL),
			     NULL);
}

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;
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
		LOG_INF("BLE_GAP_EVT_SEC_PARAMS_REQUEST");
		break;

	case BLE_GAP_EVT_PASSKEY_DISPLAY:
		LOG_INF("Passkey: %.*s", BLE_GAP_PASSKEY_LEN,
			evt->evt.gap_evt.params.passkey_display.passkey);
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

void on_conn_params_evt(const struct ble_conn_params_evt *evt)
{
	uint32_t nrf_err;

	switch (evt->evt_type) {
	case BLE_CONN_PARAMS_EVT_REJECTED:
		nrf_err = sd_ble_gap_disconnect(evt->conn_handle,
						BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
		if (nrf_err) {
			LOG_ERR("Disconnect failed on conn params update rejection, nrf_error %#x",
				nrf_err);
		} else {
			LOG_ERR("Disconnected from peer, unacceptable conn params");
		}
		break;

	case BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED:
		ble_hrs_conn_params_evt(&ble_hrs, evt);
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

static void ble_bas_evt_handler(struct ble_bas *bas, const struct ble_bas_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_BAS_EVT_NOTIFICATION_ENABLED:
		bas_notif_enabled = true;
		break;
	case BLE_BAS_EVT_NOTIFICATION_DISABLED:
		bas_notif_enabled = false;
		break;
	default:
		break;
	}
}

static void ble_hrs_evt_handler(struct ble_hrs *hrs, const struct ble_hrs_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_HRS_EVT_NOTIFICATION_ENABLED:
		hrs_notif_enabled = true;
		break;
	case BLE_HRS_EVT_NOTIFICATION_DISABLED:
		hrs_notif_enabled = false;
		break;
	default:
		break;
	}
}

static int buttons_init(bool *erase_bonds)
{
	int err;
	const struct bm_buttons_config cfg[] = {
		{
			.pin_number = BOARD_PIN_BTN_1,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = NULL,
		},
	};

	err = bm_buttons_init(cfg, ARRAY_SIZE(cfg), BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		return err;
	}

	err = bm_buttons_enable();
	if (err) {
		return err;
	}

	if (erase_bonds != NULL) {
		*erase_bonds = bm_buttons_is_pressed(BOARD_PIN_BTN_1);
	}

	return 0;
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

static void advertising_start(bool erase_bonds)
{
	uint32_t nrf_err;

	if (erase_bonds) {
		delete_bonds();
	} else {
		nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
		if (nrf_err) {
			LOG_ERR("Failed to start advertising, nrf_error %#x", nrf_err);
		} else {
			LOG_INF("Advertising as %s", CONFIG_BLE_ADV_NAME);
		}
	}
}

static void pm_evt_handler(const struct pm_evt *p_evt)
{
	pm_handler_on_pm_evt(p_evt);
	pm_handler_disconnect_on_sec_failure(p_evt);
	pm_handler_flash_clean(p_evt);

	switch (p_evt->evt_id) {
	case PM_EVT_PEERS_DELETE_SUCCEEDED:
		advertising_start(false);
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

int main(void)
{
	int err;
	uint32_t nrf_err;
	bool erase_bonds = false;
	uint8_t body_sensor_location = BLE_HRS_BODY_SENSOR_LOCATION_FINGER;
	ble_uuid_t adv_uuid_list[] = {
		{ .uuid = BLE_UUID_HEART_RATE_SERVICE, .type = BLE_UUID_TYPE_BLE },
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
	struct ble_bas_config bas_cfg = {
		.evt_handler = ble_bas_evt_handler,
		.can_notify = true,
		.battery_level = CONFIG_SAMPLE_BATTERY_LEVEL_MAX,
		.sec_mode = BLE_BAS_CONFIG_SEC_MODE_DEFAULT,
	};
	struct ble_dis_config dis_config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
	};
	struct ble_hrs_config hrs_cfg = {
		.evt_handler = ble_hrs_evt_handler,
		.is_sensor_contact_supported = true,
		.body_sensor_location = &body_sensor_location,
		.sec_mode = BLE_HRS_CONFIG_SEC_MODE_DEFAULT,
	};

	LOG_INF("BLE HRS sample started");

	simulated_meas_init();

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

	nrf_err = peer_manager_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize Peer Manager, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Peer Manager initialized");

	nrf_err = ble_hrs_init(&ble_hrs, &hrs_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize heart rate service, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_bas_init(&ble_bas, &bas_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize battery service, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_dis_init(&dis_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize device information service, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Services initialized");

	nrf_err = ble_conn_params_evt_handler_set(on_conn_params_evt);
	if (nrf_err) {
		LOG_ERR("Failed to setup conn param event handler, nrf_error %#x", nrf_err);
		goto idle;
	}

	err = buttons_init(&erase_bonds);
	if (err) {
		LOG_ERR("Failed to initialize buttons, err %d", err);
		goto idle;
	}

	nrf_err = ble_adv_init(&ble_adv, &ble_adv_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	simulated_meas_start();

	advertising_start(erase_bonds);

idle:
	while (true) {
		(void)nrf_ble_lesc_request_handler();

		log_flush();

		k_cpu_idle();
	}

	return 0;
}
