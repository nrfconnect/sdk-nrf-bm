/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm_sdh.h>
#include <bm_sdh_ble.h>
#include <ble_adv.h>
#include <ble_conn_params.h>
#include <ble_gap.h>
#include <nrf_soc.h>
#include <bluetooth/services/ble_bas.h>
#include <bluetooth/services/ble_dis.h>
#include <bluetooth/services/ble_hrs.h>
#include <bm_timer.h>
#include <sensorsim.h>
#include <zephyr/sys/printk.h>

#define CONN_TAG 1

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
		printk("Failed to get battery measurement, err %d\n", err);
		return;
	}

	err = ble_bas_battery_level_update(&ble_bas, conn_handle, (uint8_t)battery_level);
	if (err) {
		/* Ignore if not in a connection or notifications disabled in CCCD. */
		if (err != -ENOTCONN && err != -EPIPE) {
			printk("Failed to update battery level, err %d\n", err);
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
		printk("Failed to get heart rate measurement, err %d\n", err);
		return;
	}

	err = ble_hrs_heart_rate_measurement_send(&ble_hrs, (uint16_t)heart_rate);
	if (err) {
		/* Ignore if not in a connection or notifications disabled in CCCD. */
		if (err != -ENOTCONN && err != -EPIPE) {
			printk("Failed to update heart rate measurement, err %d\n", err);
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
			printk("Failed to get RR interval measurement, err %d\n", err);
			break;
		}

		err = ble_hrs_rr_interval_add(&ble_hrs, (uint16_t)rr_interval);
		if (err) {
			printk("Failed to add RR interval, err %d\n", err);
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
		printk("Failed to update sensor contact detected state, err %d\n", err);
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
		printk("Failed to initialize battery simulator, err %d\n", err);
	}
	err = sensorsim_init(&heart_rate_sim_state, &heart_rate_sim_cfg);
	if (err) {
		printk("Failed to initialize heart rate simulator, err %d\n", err);
	}
	err = sensorsim_init(&rr_interval_sim_state, &rr_interval_sim_cfg);
	if (err) {
		printk("Failed to initialize RR interval simulator, err %d\n", err);
	}

	err = bm_timer_init(&battery_timer, BM_TIMER_MODE_REPEATED,
			      battery_level_meas_timeout_handler);
	if (err) {
		printk("Failed to initialize battery measurement timer, err %d\n", err);
	}
	err = bm_timer_init(&heart_rate_timer, BM_TIMER_MODE_REPEATED,
			      heart_rate_meas_timeout_handler);
	if (err) {
		printk("Failed to initialize heart rate measurement timer, err %d\n", err);
	}
	err = bm_timer_init(&rr_interval_timer, BM_TIMER_MODE_REPEATED,
			      rr_interval_timeout_handler);
	if (err) {
		printk("Failed to initialize RR interval measurement timer, err %d\n", err);
	}
	err = bm_timer_init(&sensor_contact_timer, BM_TIMER_MODE_REPEATED,
			      sensor_contact_detected_timeout_handler);
	if (err) {
		printk("Failed to initialize sensor contact measurement timer, err %d\n", err);
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
		printk("Peer connected\n");
		conn_handle = evt->evt.gap_evt.conn_handle;
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			printk("Failed to set system attributes, nrf_error %#x\n", err);
		}
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		printk("Peer disconnected\n");
		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}
		break;

	case BLE_GAP_EVT_AUTH_STATUS:
		printk("Authentication status: %#x\n",
		       evt->evt.gap_evt.params.auth_status.auth_status);
		break;

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		/* Pairing not supported */
		err = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle,
						  BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
		if (err) {
			printk("Failed to reply with Security params, nrf_error %#x\n", err);
		}
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		printk("BLE_GATTS_EVT_SYS_ATTR_MISSING\n");
		/* No system attributes have been stored */
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			printk("Failed to set system attributes, nrf_error %#x\n", err);
		}
		break;
	}
}
BM_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, 0);

void on_conn_params_evt(const struct ble_conn_params_evt *evt)
{
	int err;

	switch (evt->id) {
	case BLE_CONN_PARAMS_EVT_REJECTED:
		err = sd_ble_gap_disconnect(evt->conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
		if (err) {
			printk("Disconnect failed on conn params update rejection, nrf_error %#x\n",
			       err);
		} else {
			printk("Disconnected from peer, unacceptable conn params\n");
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
		printk("Advertising error %d\n", adv_evt->error.reason);
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

int main(void)
{
	int err;
	uint8_t body_sensor_location = BLE_HRS_BODY_SENSOR_LOCATION_FINGER;
	struct ble_adv_config ble_adv_cfg = {
		.conn_cfg_tag = CONFIG_BM_SDH_BLE_CONN_TAG,
		.evt_handler = ble_adv_evt_handler,
		.adv_data = {
			.name_type = BLE_ADV_DATA_FULL_NAME,
			.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
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

	printk("BLE HRS sample started\n");

	simulated_meas_init();

	err = bm_sdh_enable_request();
	if (err) {
		printk("Failed to enable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice enabled\n");

	err = bm_sdh_ble_enable(CONFIG_BM_SDH_BLE_CONN_TAG);
	if (err) {
		printk("Failed to enable BLE, err %d\n", err);
		return -1;
	}

	printk("Bluetooth enabled\n");

	err = ble_hrs_init(&ble_hrs, &hrs_cfg);
	if (err) {
		printk("Failed to initialize heart rate service, err %d\n", err);
		return -1;
	}

	err = ble_bas_init(&ble_bas, &bas_cfg);
	if (err) {
		printk("Failed to initialize battery service, err %d\n", err);
		return -1;
	}

	err = ble_dis_init();
	if (err) {
		printk("Failed to initialize device information service, err %d\n", err);
		return -1;
	}

	printk("Services initialized\n");

	err = ble_conn_params_evt_handler_set(on_conn_params_evt);
	if (err) {
		printk("Failed to setup conn param event handler, err %d\n", err);
		return -1;
	}

	err = ble_adv_init(&ble_adv, &ble_adv_cfg);
	if (err) {
		printk("Failed to initialize advertising, err %d\n", err);
		return -1;
	}

	simulated_meas_start();

	err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (err) {
		printk("Failed to start advertising, err %d\n", err);
		return -1;
	}

	printk("Advertising as %s\n", CONFIG_BLE_ADV_NAME);

	while (true) {
		sd_app_evt_wait();
	}
}
