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
#include <nrf.h>
#include <ble.h>
#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/ble_conn_params.h>
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

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <board-config.h>

LOG_MODULE_REGISTER(app, CONFIG_APP_BLE_CGMS_LOG_LEVEL);

#ifndef IS_BIT_SET
#define IS_BIT_SET(value, bit) ((((value) >> (bit)) & (0x1)) != 0)
#endif

enum led_indicate {
	LED_INDICATE_IDLE = 1,
	LED_INDICATE_ADVERTISING,
	LED_INDICATE_ADVERTISING_WHITELIST,
	LED_INDICATE_ADVERTISING_SLOW,
	LED_INDICATE_ADVERTISING_DIRECTED,
	LED_INDICATE_CONNECTED,
};

/** Battery timer. */
static struct bm_timer battery_timer;
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

/** Battery Level sensor simulator state. */
static struct sensorsim_state battery_sim_state;

static uint16_t current_time_offset;
static uint16_t glucose_concentration = CONFIG_APP_GLUCOSE_CONCENTRATION_MIN;

static uint8_t qwr_mem[CONFIG_APP_QWR_MEM_BUFF_SIZE];

/** @brief Function for performing battery measurement and updating the Battery Level characteristic
 *         in Battery Service.
 */
static void battery_level_update(void)
{
	int err;
	uint32_t nrf_err;
	uint32_t battery_level;

	err = (uint8_t)sensorsim_measure(&battery_sim_state, &battery_level);
	if (err) {
		LOG_ERR("Sensorsim measure failed, err %d", err);
	}

	nrf_err = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	if (nrf_err) {
		/* Ignore if not in a connection or notifications disabled in CCCD. */
		if (nrf_err != NRF_ERROR_NOT_FOUND && nrf_err != NRF_ERROR_INVALID_STATE) {
			LOG_ERR("Failed to update battery level, nrf_error %#x", nrf_err);
		}
	}
}

/**
 * @brief Function for handling the Battery measurement timer timeout.
 *
 * @details This function will be called each time the battery level measurement timer expires.
 *
 * @param[in] context Pointer used for passing some arbitrary information (context) from the
 *                    bm_timer_start() call to the timeout handler.
 */
static void battery_level_meas_timeout_handler(void *context)
{
	ARG_UNUSED(context);
	battery_level_update();
}

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
		current_time_offset += CONFIG_APP_GLUCOSE_MEAS_INTERVAL;
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

	err = bm_timer_init(&battery_timer, BM_TIMER_MODE_REPEATED,
			      battery_level_meas_timeout_handler);
	if (err) {
		LOG_ERR("Failed to initialize battery timer, err %d", err);
		return -1;
	}

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
			cgms->comm_interval = CONFIG_APP_GLUCOSE_MEAS_INTERVAL;
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
		.mem_buffer.len = CONFIG_APP_QWR_MEM_BUFF_SIZE,
		.mem_buffer.p_mem = qwr_mem,
		.evt_handler = qwr_evt_handler,
	};

	struct ble_dis_config dis_config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
	};

	nrf_err = ble_qwr_init(&ble_qwr, &qwr_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize QWR service, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	/* Initialize Glucose Service */
	ble_cgms.comm_interval = CONFIG_APP_GLUCOSE_MEAS_INTERVAL;

	nrf_err = ble_cgms_init(&ble_cgms, &cgms_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize CGMS service, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	ble_cgms.comm_interval = CONFIG_APP_GLUCOSE_MEAS_INTERVAL;

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

/** @brief Function for initializing the sensor simulators. */
static int sensor_simulator_init(void)
{
	int err;
	/** Battery Level sensor simulator configuration. */
	static struct sensorsim_cfg battery_sim_cfg = {
		.min = CONFIG_APP_BATTERY_LEVEL_MIN,
		.max = CONFIG_APP_BATTERY_LEVEL_MAX,
		.incr = CONFIG_APP_BATTERY_LEVEL_INCREMENT,
		.start_at_max = true,
	};

	err = sensorsim_init(&battery_sim_state, &battery_sim_cfg);
	if (err) {
		LOG_ERR("Sensorsim init failed, err %d", err);
		return err;
	}

	return 0;
}

/** @brief Function for starting application timers. */
static int application_timers_start(void)
{
	int err;

	/* Start application timers. */
	err = bm_timer_start(&battery_timer,
		BM_TIMER_MS_TO_TICKS(CONFIG_APP_BATTERY_LEVEL_MEAS_INTERVAL_MS),
		NULL);
	if (err) {
		LOG_ERR("Failed to start app timer, err %d", err);
		return err;
	}

	return 0;
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

static void led_indication_set(enum led_indicate led_indicate)
{
	nrf_gpio_pin_write(BOARD_PIN_LED_0, IS_BIT_SET(led_indicate, 0) == BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_1, IS_BIT_SET(led_indicate, 1) == BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_2, IS_BIT_SET(led_indicate, 2) == BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_3, IS_BIT_SET(led_indicate, 3) == BOARD_LED_ACTIVE_STATE);
}

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{

	uint32_t nrf_err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Connected");
		led_indication_set(LED_INDICATE_CONNECTED);

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
		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}

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
	case BLE_GAP_EVT_AUTH_STATUS:
		LOG_INF("Authentication status: %#x",
		       evt->evt.gap_evt.params.auth_status.auth_status);
		break;
	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		break;
	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		LOG_INF("BLE_GATTS_EVT_SYS_ATTR_MISSING");
		/* No system attributes have been stored */
		nrf_err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (nrf_err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", nrf_err);
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

/**
 * @brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt Advertising event.
 */
static void ble_adv_evt_handler(struct ble_adv *adv, const struct ble_adv_evt *adv_evt)
{
	switch (adv_evt->evt_type) {
	case BLE_ADV_EVT_ERROR:
		LOG_ERR("BLE advertising error, %#x", adv_evt->error.reason);
		__ASSERT(false, "BLE advertising error %#x", adv_evt->error.reason);
		break;
	case BLE_ADV_EVT_DIRECTED_HIGH_DUTY:
		led_indication_set(LED_INDICATE_ADVERTISING_DIRECTED);
		break;
	case BLE_ADV_EVT_FAST:
		led_indication_set(LED_INDICATE_ADVERTISING);
		break;
	case BLE_ADV_EVT_SLOW:
		led_indication_set(LED_INDICATE_ADVERTISING_SLOW);
		break;
	case BLE_ADV_EVT_FAST_WHITELIST:
		led_indication_set(LED_INDICATE_ADVERTISING_WHITELIST);
		break;
	case BLE_ADV_EVT_SLOW_WHITELIST:
		led_indication_set(LED_INDICATE_ADVERTISING_WHITELIST);
		break;
	case BLE_ADV_EVT_IDLE:
		led_indication_set(LED_INDICATE_IDLE);
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

/**
 * @brief Function for handling events from the BSP module.
 *
 * @param[in] event Event generated by button press.
 */
static void button_handler(uint8_t pin, uint8_t action)
{
	if (action != BM_BUTTONS_PRESS) {
		return;
	}

	switch (pin) {
	case BOARD_PIN_BTN_0:
		LOG_INF("Sleep mode not supported");
		break;

	case BOARD_PIN_BTN_1:
		LOG_INF("Increase GL Concentration");
		glucose_concentration += CONFIG_APP_GLUCOSE_CONCENTRATION_INC;
		if (glucose_concentration > CONFIG_APP_GLUCOSE_CONCENTRATION_MAX) {
			glucose_concentration = CONFIG_APP_GLUCOSE_CONCENTRATION_MIN;
		}
		break;

	case BOARD_PIN_BTN_3:
		LOG_INF("Decrease GL Concentration");
		glucose_concentration -= CONFIG_APP_GLUCOSE_CONCENTRATION_DEC;
		if (glucose_concentration < CONFIG_APP_GLUCOSE_CONCENTRATION_MIN) {
			glucose_concentration = CONFIG_APP_GLUCOSE_CONCENTRATION_MAX;
		}
		break;

	default:
		break;
	}
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

struct bm_buttons_config btn_configs[4] = {
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

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_1);
	nrf_gpio_cfg_output(BOARD_PIN_LED_2);
	nrf_gpio_cfg_output(BOARD_PIN_LED_3);

	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_2, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_3, !BOARD_LED_ACTIVE_STATE);

	return 0;
}

/** @brief Function for application main entry. */
int main(void)
{
	int err;
	uint32_t nrf_err;
	bool erase_bonds;

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
	(void)sensor_simulator_init();

	nrf_err = ble_conn_params_evt_handler_set(on_conn_params_evt);
	if (nrf_err) {
		LOG_ERR("Failed to setup conn param event handler, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Continuous Glucose Monitoring sample started.");
	err = application_timers_start();
	if (err) {
		goto idle;
	}

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (nrf_err) {
		LOG_ERR("Failed to start advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Advertising as %s", CONFIG_BLE_ADV_NAME);

idle:
	/* Enter main loop. */
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

/** @} */
