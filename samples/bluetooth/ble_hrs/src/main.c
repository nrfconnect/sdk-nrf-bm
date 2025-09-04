/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <ble_adv.h>
#include <ble_conn_params.h>
#include <ble_gap.h>
#include <nrf_soc.h>
#include <bluetooth/services/uuid.h>
#include <bluetooth/services/ble_bas.h>
#include <bluetooth/services/ble_dis.h>
#include <bluetooth/services/ble_hrs.h>
#include <bm_timer.h>
#include <sensorsim.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <bluetooth/peer_manager/peer_manager.h>

LOG_MODULE_REGISTER(app, CONFIG_BLE_HRS_SAMPLE_LOG_LEVEL);

#define CONN_TAG 1

/* Perform bonding. */
#define SEC_PARAM_BOND                      1
/* Man In The Middle protection not required. */
#define SEC_PARAM_MITM                      0
/* LE Secure Connections enabled. */
#define SEC_PARAM_LESC                      1
/* Keypress notifications not enabled. */
#define SEC_PARAM_KEYPRESS                  0
/* No I/O capabilities. */
#define SEC_PARAM_IO_CAPABILITIES           BLE_GAP_IO_CAPS_NONE
/* Out Of Band data not available. */
#define SEC_PARAM_OOB                       0
/* Minimum encryption key size. */
#define SEC_PARAM_MIN_KEY_SIZE              7
/* Maximum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE              16

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

void battery_level_meas_timeout_handler(void *context)
{
	int err;
	uint32_t battery_level;

	ARG_UNUSED(context);

	err = sensorsim_measure(&battery_sim_state, &battery_level);
	if (err) {
		LOG_ERR("Failed to get battery measurement, err %d", err);
		return;
	}

	err = ble_bas_battery_level_update(&ble_bas, conn_handle, (uint8_t)battery_level);
	if (err) {
		/* Ignore if not in a connection or notifications disabled in CCCD. */
		if (err != -ENOTCONN && err != -EPIPE) {
			LOG_ERR("Failed to update battery level, err %d", err);
		}
	}
}

static void heart_rate_meas_timeout_handler(void *context)
{
	static uint32_t cnt;
	int err;
	uint32_t heart_rate;

	ARG_UNUSED(context);

	err = sensorsim_measure(&heart_rate_sim_state, &heart_rate);
	if (err) {
		LOG_ERR("Failed to get heart rate measurement, err %d", err);
		return;
	}

	err = ble_hrs_heart_rate_measurement_send(&ble_hrs, (uint16_t)heart_rate);
	if (err) {
		/* Ignore if not in a connection or notifications disabled in CCCD. */
		if (err != -ENOTCONN && err != -EPIPE) {
			LOG_ERR("Failed to update heart rate measurement, err %d", err);
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

		err = ble_hrs_rr_interval_add(&ble_hrs, (uint16_t)rr_interval);
		if (err) {
			LOG_ERR("Failed to add RR interval, err %d", err);
		}
	}
}

static void sensor_contact_detected_timeout_handler(void *context)
{
	static bool sim_sensor_contact_detected;
	int err;

	ARG_UNUSED(context);

	sim_sensor_contact_detected = !sim_sensor_contact_detected;
	err = ble_hrs_sensor_contact_detected_update(&ble_hrs, sim_sensor_contact_detected);
	if (err) {
		LOG_ERR("Failed to update sensor contact detected state, err %d", err);
	}
}

static void simulated_meas_init(void)
{
	int err;
	const struct sensorsim_cfg battery_sim_cfg = {
		.min = CONFIG_BATTERY_LEVEL_MIN, .max = CONFIG_BATTERY_LEVEL_MAX,
		.incr = CONFIG_BATTERY_LEVEL_INCREMENT, .start_at_max = true,
	};
	const struct sensorsim_cfg heart_rate_sim_cfg = {
		.min = CONFIG_HEART_RATE_MIN, .max = CONFIG_HEART_RATE_MAX,
		.incr = CONFIG_HEART_RATE_INCREMENT, .start_at_max = false,
	};
	const struct sensorsim_cfg rr_interval_sim_cfg = {
		.min = CONFIG_RR_INTERVAL_MIN, .max = CONFIG_RR_INTERVAL_MAX,
		.incr = CONFIG_RR_INTERVAL_INCREMENT, .start_at_max = false,
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
			       BM_TIMER_MS_TO_TICKS(CONFIG_BATTERY_LEVEL_MEAS_INTERVAL), NULL);
	(void)bm_timer_start(&heart_rate_timer,
			       BM_TIMER_MS_TO_TICKS(CONFIG_HEART_RATE_MEAS_INTERVAL), NULL);
	(void)bm_timer_start(&rr_interval_timer,
			       BM_TIMER_MS_TO_TICKS(CONFIG_RR_INTERVAL_MEAS_INTERVAL), NULL);
	(void)bm_timer_start(&sensor_contact_timer,
			       BM_TIMER_MS_TO_TICKS(CONFIG_SENSOR_CONTACT_DETECTED_INTERVAL),
						      NULL);
}

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	int err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;
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
		LOG_INF("System attribute missing event");
		/* No system attributes have been stored */
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", err);
		}
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, 0);

void on_conn_params_evt(const struct ble_conn_params_evt *evt)
{
	int err;

	switch (evt->id) {
	case BLE_CONN_PARAMS_EVT_REJECTED:
		err = sd_ble_gap_disconnect(evt->conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
		if (err) {
			LOG_ERR("Disconnect failed on conn params update rejection, nrf_error %#x",
			       err);
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
		LOG_ERR("Advertising error %d", adv_evt->error.reason);
		break;
	default:
		break;
	}
}

static void ble_bas_evt_handler(struct ble_bas *bas, const struct ble_bas_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_BAS_EVT_NOTIFICATION_ENABLED:
		/* ignore */
		break;

	case BLE_BAS_EVT_NOTIFICATION_DISABLED:
		/* ignore */
		break;

	default:
		break;
	}
}

static void ble_hrs_evt_handler(struct ble_hrs *hrs, const struct ble_hrs_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_HRS_EVT_NOTIFICATION_ENABLED:
		/* ignore */
		break;

	case BLE_HRS_EVT_NOTIFICATION_DISABLED:
		/* ignore */
		break;

	default:
		break;
	}
}

static void delete_bonds(void)
{
	uint32_t err;

	printk("Erase bonds!\n");

	err = pm_peers_delete();
	if (err) {
		printk("Failed to delete peers, err %d\n", err);
	}
}

static void advertising_start(bool erase_bonds)
{
	int err;

	if (erase_bonds) {
		delete_bonds();
	} else {
		err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
		if (err) {
			printk("Failed to start advertising, err %d\n", err);
		}
	}
}

static void pm_evt_handler(pm_evt_t const *p_evt)
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

static void peer_manager_init(void)
{
	ble_gap_sec_params_t sec_param;
	int err;

	err = pm_init();
	if (err) {
		return;
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
		printk("pm_sec_params_set() failed, err: %d\n", err);
		return;
	}

	err = pm_register(pm_evt_handler);
	if (err) {
		printk("pm_register() failed, err: %d\n", err);
		return;
	}
}

int main(void)
{
	int err;
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
		.battery_level = CONFIG_BATTERY_LEVEL_MAX,
	};
	struct ble_hrs_config hrs_cfg = {
		.evt_handler = ble_hrs_evt_handler,
		.is_sensor_contact_supported = true,
		.body_sensor_location = &body_sensor_location,
	};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_cfg.batt_rd_sec);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_cfg.cccd_wr_sec);

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&hrs_cfg.hrm_cccd_wr_sec);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&hrs_cfg.bsl_rd_sec);

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

	peer_manager_init();

	err = ble_hrs_init(&ble_hrs, &hrs_cfg);
	if (err) {
		LOG_ERR("Failed to initialize heart rate service, err %d", err);
		goto idle;
	}

	err = ble_bas_init(&ble_bas, &bas_cfg);
	if (err) {
		LOG_ERR("Failed to initialize battery service, err %d", err);
		goto idle;
	}

	err = ble_dis_init();
	if (err) {
		LOG_ERR("Failed to initialize device information service, err %d", err);
		goto idle;
	}

	LOG_INF("Services initialized");

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

	simulated_meas_start();

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
