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
 * This file contains the source code for a sample using the Continuous Glucose Monitoring service.
 * Bond Management Service, Battery service and Device Information service is also present.
 *
 */

#include <stdint.h>
#include <string.h>
#include <nrf.h>
#include <ble.h>
#include <ble_adv.h>
#include <ble_conn_params.h>
#include <sensorsim.h>
#include <nrf_sdh.h>
#include <nrf_sdh_soc.h>
#include <nrf_sdh_ble.h>
#include <nrf_soc.h>
#include <lite_timer.h>
#include <lite_buttons.h>
#include <nrf_ble_qwr.h>

#include <bluetooth/services/uuid.h>
#include <bluetooth/services/ble_bas.h>
#include <bluetooth/services/ble_cgms.h>
#include <bluetooth/services/ble_dis.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, CONFIG_BLE_CGMS_SAMPLE_LOG_LEVEL);

/* GPIO configuration, to be moved to separate file. */
#if defined(CONFIG_SOC_SERIES_NRF52X)
#define PIN_BTN_0 NRF_PIN_PORT_TO_PIN_NUMBER(11, 0)
#define PIN_BTN_1 NRF_PIN_PORT_TO_PIN_NUMBER(12, 0)
#define PIN_BTN_2 NRF_PIN_PORT_TO_PIN_NUMBER(24, 0)
#define PIN_BTN_3 NRF_PIN_PORT_TO_PIN_NUMBER(25, 0)
#define PIN_LED_0 NRF_PIN_PORT_TO_PIN_NUMBER(13, 0)
#define PIN_LED_1 NRF_PIN_PORT_TO_PIN_NUMBER(14, 0)
#define PIN_LED_2 NRF_PIN_PORT_TO_PIN_NUMBER(15, 0)
#define PIN_LED_3 NRF_PIN_PORT_TO_PIN_NUMBER(16, 0)
#define LED_ACTIVE_STATE 0 /* GPIO_ACTIVE_LOW */
#elif defined(CONFIG_SOC_SERIES_NRF54LX)
#define PIN_BTN_0 NRF_PIN_PORT_TO_PIN_NUMBER(13, 1)
#define PIN_BTN_1 NRF_PIN_PORT_TO_PIN_NUMBER(9, 1)
#define PIN_BTN_2 NRF_PIN_PORT_TO_PIN_NUMBER(8, 1)
#define PIN_BTN_3 NRF_PIN_PORT_TO_PIN_NUMBER(4, 0)
#define PIN_LED_0 NRF_PIN_PORT_TO_PIN_NUMBER(9, 2)
#define PIN_LED_1 NRF_PIN_PORT_TO_PIN_NUMBER(10, 1)
#define PIN_LED_2 NRF_PIN_PORT_TO_PIN_NUMBER(7, 2)
#define PIN_LED_3 NRF_PIN_PORT_TO_PIN_NUMBER(14, 1)
#define LED_ACTIVE_STATE 1 /* GPIO_ACTIVE_HIGH */
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
static struct lite_timer battery_timer;
/** Glucose measurement timer. */
static struct lite_timer glucose_meas_timer;

/** Battery service instance. */
BLE_BAS_DEF(ble_bas);
/** CGMS instance. */
NRF_BLE_CGMS_DEF(ble_cgms);
/** Context for the Queued Write module. */
NRF_BLE_QWR_DEF(ble_qwr);
/** Advertising module instance. */
BLE_ADV_DEF(ble_adv);
/** BLE GATT Queue instance. */
BLE_GQ_DEF(ble_gatt_gueue);

/** Handle of the current connection. */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

/** Battery Level sensor simulator state. */
static struct sensorsim_state battery_sim_state;

static uint16_t current_time_offset;
static uint16_t glucose_concentration = CONFIG_GLUCOSE_CONCENTRATION_MIN;

static uint8_t qwr_mem[CONFIG_QWR_MEM_BUFF_SIZE];

/**
 * @brief Function for handling advertising errors.
 *
 * @param[in] nrf_error Error code containing information about what went wrong.
 */
static void ble_adv_error_handler(int err)
{
	LOG_ERR("BLE advertising error, %d", err);
	__ASSERT(false, "BLE advertising error %d", err);
}

/**
 * @brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in] nrf_error Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(int nrf_error)
{
	LOG_ERR("BLE QWR error, %d", nrf_error);
	__ASSERT(false, "BLE QWR error %d", nrf_error);
}

/** @brief Function for performing battery measurement and updating the Battery Level characteristic
 *         in Battery Service.
 */
static void battery_level_update(void)
{
	int err;
	uint32_t battery_level;

	err = (uint8_t)sensorsim_measure(&battery_sim_state, &battery_level);
	if (err) {
		LOG_ERR("Sensorsim measure failed, err %d", err);
	}

	err = ble_bas_battery_level_update(&ble_bas, BLE_CONN_HANDLE_ALL, battery_level);
	if ((err != NRF_SUCCESS) &&
	    (err != NRF_ERROR_INVALID_STATE) &&
	    (err != NRF_ERROR_RESOURCES) &&
	    (err != NRF_ERROR_BUSY) &&
	    (err != BLE_ERROR_GATTS_SYS_ATTR_MISSING)) {
		__ASSERT(false, "Battery level update error %d", nrf_error);
	}
}

/** @brief Function for starting advertising. */
static int advertising_start(bool erase_bonds)
{
	int err;

	err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (err) {
		LOG_ERR("Failed to start advertising, err %d", err);
		return -1;
	}

	return 0;
}

/**
 * @brief Function for handling the Battery measurement timer timeout.
 *
 * @details This function will be called each time the battery level measurement timer expires.
 *
 * @param[in] context Pointer used for passing some arbitrary information (context) from the
 *                    lite_start_timer() call to the timeout handler.
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
	struct ble_cgms_rec rec;

	memset(&rec, 0, sizeof(struct ble_cgms_rec));

	LOG_INF("Read glucose measurement: %dmg/dL", glucose_concentration);

	rec.meas.glucose_concentration = glucose_concentration;
	rec.meas.sensor_status_annunciation.warning = 0;
	rec.meas.sensor_status_annunciation.calib_temp = 0;
	rec.meas.sensor_status_annunciation.status = 0;
	rec.meas.flags = 0;
	rec.meas.time_offset = current_time_offset;

	nrf_ble_cgms_meas_create(&ble_cgms, &rec);
}

/**
 * @brief Function for handling the Battery measurement timer timeout.
 *
 * @details This function will be called each time the glucose measurement timer expires.
 *
 * @param[in] context Pointer used for passing some arbitrary information (context) from the
 *                    lite_start_timer() call to the timeout handler.
 */
static void glucose_meas_timeout_handler(void *context)
{
	int err;

	printk("glucose_meas_timeout_handler\n");

	ARG_UNUSED(context);

	if (ble_cgms.comm_interval != 0) {
		current_time_offset += ble_cgms.comm_interval;
	} else {
		current_time_offset += CONFIG_GLUCOSE_MEAS_INTERVAL;
	}

	read_glucose_measurement();

	ble_cgms.sensor_status.time_offset = current_time_offset;
	err = nrf_ble_cgms_update_status(&ble_cgms, &ble_cgms.sensor_status);
	if (err) {
		LOG_ERR("Failed to update BLE CGMS status, err %d", err);
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

	err = lite_timer_init(&battery_timer, LITE_TIMER_MODE_REPEATED,
			      battery_level_meas_timeout_handler);
	if (err) {
		LOG_ERR("Failed to initialize battery timer, err %d\n", err);
		return -1;
	}

	err = lite_timer_init(&glucose_meas_timer, LITE_TIMER_MODE_REPEATED,
			      glucose_meas_timeout_handler);
	if (err) {
		LOG_ERR("Failed to initialize glucose meas timer, err %d\n", err);
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
static int gap_params_init(void)
{
	int err;

	err = sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_GLUCOSE_METER);
	if (err) {
		LOG_ERR("Failed to set GAP appearance, err %d", err);
		return -1;
	}

	return 0;
}

static void cgms_evt_handler(struct nrf_ble_cgms *cgms, struct nrf_ble_cgms_evt *evt)
{
	int err;

	switch (evt->evt_type) {
	case NRF_BLE_CGMS_EVT_ERROR:
		LOG_ERR("BLE Service error, %d", evt->error.reason);
		__ASSERT(false, "BLE Service error %d", evt->error.reason);
		break;
	case NRF_BLE_CGMS_EVT_NOTIFICATION_ENABLED:
		break;
	case NRF_BLE_CGMS_EVT_NOTIFICATION_DISABLED:
		break;

	case NRF_BLE_CGMS_EVT_START_SESSION:
		LOG_INF("CGM Start Session");

		err = lite_timer_start(&glucose_meas_timer,
				       LITE_TIMER_MS_TO_TICKS(cgms->comm_interval * 60000), NULL);
		if (err) {
			LOG_ERR("Failed to start glucose meas timer, err %d", err);
		}
		break;

	case NRF_BLE_CGMS_EVT_STOP_SESSION:
		LOG_INF("CGM Stop Session");
		err = lite_timer_stop(&glucose_meas_timer);
		if (err) {
			LOG_ERR("Failed to stop glucose meas timer, err %d", err);
		}
		break;

	case NRF_BLE_CGMS_EVT_WRITE_COMM_INTERVAL:
		LOG_INF("CGM change communication interval");
		if (cgms->comm_interval == 0xFF) {
			cgms->comm_interval = CONFIG_GLUCOSE_MEAS_INTERVAL;
		}
		err = lite_timer_stop(&glucose_meas_timer);
		if (err) {
			LOG_ERR("Failed to stop glucose meas timer, err %d", err);
		}

		if (cgms->comm_interval != 0) {
			err = lite_timer_start(&glucose_meas_timer,
					       LITE_TIMER_MS_TO_TICKS(cgms->comm_interval * 60000),
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

uint16_t qwr_evt_handler(struct nrf_ble_qwr *qwr, struct nrf_ble_qwr_evt *evt)
{
	return NRF_BLE_QWR_REJ_REQUEST_ERR_CODE;
}

/**
 * @brief Function for initializing the services that will be used by the application.
 *
 * @details Initialize the Glucose, Battery and Device Information services.
 */
static int services_init(void)
{
	int err;

	struct nrf_ble_cgms_config cgms_config;
	struct ble_bas_config bas_config = {
		.evt_handler = NULL,
		.can_notify = true,
		.report_ref = NULL,
		.battery_level = 100,
	};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_config.batt_rd_sec);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_config.report_ref_rd_sec);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_config.cccd_wr_sec);

	struct nrf_ble_qwr_init qwr_init = {
		.mem_buffer.len = CONFIG_QWR_MEM_BUFF_SIZE,
		.mem_buffer.p_mem = qwr_mem,
		.callback = qwr_evt_handler,
		.error_handler = nrf_qwr_error_handler,
	};

	err = nrf_ble_qwr_init(&ble_qwr, &qwr_init);
	if (err) {
		LOG_ERR("Failed to initialize QWR service, err %d", err);
		return err;
	}

	/* Initialize Glucose Service */
	ble_cgms.comm_interval = CONFIG_GLUCOSE_MEAS_INTERVAL;

	memset(&cgms_config, 0, sizeof(cgms_config));

	cgms_config.evt_handler = cgms_evt_handler;
	cgms_config.gatt_queue = &ble_gatt_gueue;

	cgms_config.initial_run_time = 20;

	cgms_config.initial_sensor_status.time_offset = 0x00;
	cgms_config.initial_sensor_status.status.status |= NRF_BLE_CGMS_STATUS_SESSION_STOPPED;

	err = nrf_ble_cgms_init(&ble_cgms, &cgms_config);
	if (err) {
		LOG_ERR("Failed to initialize CGMS service, err %d", err);
		return err;
	}

	ble_cgms.comm_interval = CONFIG_GLUCOSE_MEAS_INTERVAL;

	/* Add a basic battery measurement with only mandatory fields */

	err = ble_bas_init(&ble_bas, &bas_config);
	if (err) {
		LOG_ERR("Failed to initialize BAS service, err %d", err);
		return err;
	}

	/* Initialize Device Information Service. */
	err = ble_dis_init();
	if (err) {
		LOG_ERR("Failed to initialize device information service, err %d\n", err);
		return -1;
	}

	return 0;
}

/** @brief Function for initializing the sensor simulators. */
static int sensor_simulator_init(void)
{
	int err;
	/** Battery Level sensor simulator configuration. */
	static struct sensorsim_cfg battery_sim_cfg = {
		.min = CONFIG_BATTERY_LEVEL_MIN,
		.max = CONFIG_BATTERY_LEVEL_MAX,
		.incr = CONFIG_BATTERY_LEVEL_INCREMENT,
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
	err = lite_timer_start(&battery_timer,
		LITE_TIMER_MS_TO_TICKS(CONFIG_BATTERY_LEVEL_MEAS_INTERVAL_MS),
		NULL);
	if (err) {
		LOG_ERR("Failed to start app timer, err %d", err);
		return err;
	}

	return 0;
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
			printk("Failed to set system attributes, nrf_error %d\n", err);
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
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		printk("BLE_GATTS_EVT_SYS_ATTR_MISSING\n");
		/* No system attributes have been stored */
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			printk("Failed to set system attributes, nrf_error %d\n", err);
		}
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, 0);

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
	int err;

	switch (evt->id) {
	case BLE_CONN_PARAMS_EVT_REJECTED:
		err = sd_ble_gap_disconnect(conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
		if (err) {
			LOG_ERR("Failed to disconnect BLE GAP, err %d", err);
		}
		LOG_ERR("Disconnected from peer, unacceptable conn params");
		break;
	default:
		break;
	}
}

static void led_indication_set(enum led_indicate led_indicate)
{
	nrf_gpio_pin_write(PIN_LED_0, led_indicate & (1<<0));
	nrf_gpio_pin_write(PIN_LED_1, led_indicate & (1<<1));
	nrf_gpio_pin_write(PIN_LED_2, led_indicate & (1<<2));
	nrf_gpio_pin_write(PIN_LED_3, led_indicate & (1<<3));
}

/**
 * @brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static int sleep_mode_enter(void)
{
#if CONFIG_SOFTDEVICE_S140
	int err;

	led_indication_set(LED_INDICATE_IDLE);
	/* Go to system-off mode (this function will not return; reset to wake up). */
	err = sd_power_system_off();
	if (err) {
		LOG_ERR("Failed to go to system-off mode, err %d", err);
		return err;
	}
#else
	LOG_ERR("SoftDevice power features are currently not supported on the S115 SoftDevice");
	return NRF_ERROR_NOT_SUPPORTED;
#endif

	return 0;
}

/**
 * @brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt Advertising event.
 */
static void ble_adv_evt_handler(enum ble_adv_evt ble_adv_evt)
{
	switch (ble_adv_evt) {
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
		(void)sleep_mode_enter();
		break;
	default:
		break;
	}
}

/**
 * @brief Function for handling BLE events.
 *
 * @param[in] ble_evt Bluetooth stack event.
 * @param[in] ctx Unused.
 */
static void ble_evt_handler(ble_evt_t const *ble_evt, void *ctx)
{
	int err = NRF_SUCCESS;

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Connected");
		led_indication_set(LED_INDICATE_CONNECTED);

		conn_handle = ble_evt->evt.gap_evt.conn_handle;
		err = nrf_ble_qwr_conn_handle_assign(&ble_qwr, conn_handle);
		if (err) {
			LOG_ERR("Failed to assign BLE QWR conn handle, err %d", err);
		}
		err = nrf_ble_cgms_conn_handle_assign(&ble_cgms, conn_handle);
		if (err) {
			LOG_ERR("Failed to assign BLE CGMS conn handle, err %d", err);
		}
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Disconnected");
		(void)advertising_start(false);
		conn_handle = BLE_CONN_HANDLE_INVALID;
		break;

	case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
		LOG_DBG("PHY update request.");
		ble_gap_phys_t const phys = {
			.rx_phys = BLE_GAP_PHY_AUTO,
			.tx_phys = BLE_GAP_PHY_AUTO,
		};
		err = sd_ble_gap_phy_update(ble_evt->evt.gap_evt.conn_handle, &phys);
		if (err) {
			LOG_ERR("Failed to update BLE GAP PHY, err %d", err);
		}
		break;

	case BLE_GATTC_EVT_TIMEOUT:
		/* Disconnect on GATT Client timeout event. */
		LOG_DBG("GATT Client Timeout.");
		err = sd_ble_gap_disconnect(ble_evt->evt.gattc_evt.conn_handle,
						BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (err) {
			LOG_ERR("Failed to disconnect GAP, err %d", err);
		}
		break;

	case BLE_GATTS_EVT_TIMEOUT:
		/* Disconnect on GATT Server timeout event. */
		LOG_DBG("GATT Server Timeout.");
		err = sd_ble_gap_disconnect(ble_evt->evt.gatts_evt.conn_handle,
						BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (err) {
			LOG_ERR("Failed to disconnect GAP, err %d", err);
		}
		break;

	default:
		/* No implementation needed. */
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
		LOG_ERR("Failed to enable SofdDevice helper requests, err %d", err);
		return err;
	}

	/* Enable BLE stack. */
	err = nrf_sdh_ble_enable(CONFIG_NRF_SDH_BLE_CONN_TAG);
	if (err) {
		LOG_ERR("Failed to enable SoftDevice helpers, err %d", err);
		return err;
	}

	/* Register a handler for BLE events. */
	NRF_SDH_BLE_OBSERVER(m_ble_observer, ble_evt_handler, NULL, CONFIG_APP_BLE_OBSERVER_PRIO);

	return 0;
}

/**
 * @brief Function for handling events from the BSP module.
 *
 * @param[in] event Event generated by button press.
 */
static void button_handler(uint8_t pin, uint8_t action)
{
	if (action != LITE_BUTTONS_PRESS) {
		return;
	}

	switch (pin) {
	case PIN_BTN_0:
		printk("Enter sleep mode\n");
		(void)sleep_mode_enter();
		break;

	case PIN_BTN_1:
		LOG_INF("Increase GL Concentration");
		glucose_concentration += CONFIG_GLUCOSE_CONCENTRATION_INC;
		if (glucose_concentration > CONFIG_GLUCOSE_CONCENTRATION_MAX) {
			glucose_concentration = CONFIG_GLUCOSE_CONCENTRATION_MIN;
		}
		break;

	case PIN_BTN_3:
		LOG_INF("Decrease GL Concentration");
		glucose_concentration -= CONFIG_GLUCOSE_CONCENTRATION_DEC;
		if (glucose_concentration < CONFIG_GLUCOSE_CONCENTRATION_MIN) {
			glucose_concentration = CONFIG_GLUCOSE_CONCENTRATION_MAX;
		}
		break;

	default:
		break;
	}
}

/** @brief Function for initializing the Advertising functionality. */
static int advertising_init(void)
{
	int err;
	uint8_t adv_flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
	struct ble_adv_config config = {
		.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
		.adv_data = {
			.name_type = BLE_ADV_DATA_FULL_NAME,
			.include_appearance = true,
			.flags = adv_flags,
		},


		.evt_handler = ble_adv_evt_handler,
		.error_handler = ble_adv_error_handler,
	};

	err = ble_adv_init(&ble_adv, &config);
	if (err) {
		LOG_ERR("BLE advertising init failed, err %d", err);
		return err;
	}

	ble_adv_conn_cfg_tag_set(&ble_adv, CONFIG_NRF_SDH_BLE_CONN_TAG);

	return 0;
}


struct lite_buttons_config btn_configs[4] = {
	{
		.pin_number = PIN_BTN_0,
		.active_state = LITE_BUTTONS_ACTIVE_LOW,
		.pull_config = LITE_BUTTONS_PIN_PULLUP,
		.handler = button_handler,
	},
	{
		.pin_number = PIN_BTN_1,
		.active_state = LITE_BUTTONS_ACTIVE_LOW,
		.pull_config = LITE_BUTTONS_PIN_PULLUP,
		.handler = button_handler,
	},
	{
		.pin_number = PIN_BTN_2,
		.active_state = LITE_BUTTONS_ACTIVE_LOW,
		.pull_config = LITE_BUTTONS_PIN_PULLUP,
		.handler = button_handler,
	},
	{
		.pin_number = PIN_BTN_3,
		.active_state = LITE_BUTTONS_ACTIVE_LOW,
		.pull_config = LITE_BUTTONS_PIN_PULLUP,
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

	err = lite_buttons_init(btn_configs, ARRAY_SIZE(btn_configs),
				LITE_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		LOG_ERR("lite_buttons_init error: %d", err);
		return err;
	}

	err = lite_buttons_enable();
	if (err) {
		LOG_ERR("lite_buttons_enable error: %d", err);
		return err;
	}

	nrf_gpio_cfg_output(PIN_LED_0);
	nrf_gpio_cfg_output(PIN_LED_1);
	nrf_gpio_cfg_output(PIN_LED_2);
	nrf_gpio_cfg_output(PIN_LED_3);

	nrf_gpio_pin_write(PIN_LED_0, 1);
	nrf_gpio_pin_write(PIN_LED_1, 1);
	nrf_gpio_pin_write(PIN_LED_2, 1);
	nrf_gpio_pin_write(PIN_LED_3, 1);

	return 0;
}

/** @brief Function for application main entry. */
int main(void)
{
	int err;
	bool erase_bonds;

	err = timers_init();
	if (err) {
		return -1;
	}
	err = buttons_leds_init(&erase_bonds);
	if (err) {
		return -1;
	}
/*	err = power_management_init();
 *	if (err) {
 *		return -1;
 *	}
 */
	err = ble_stack_init();
	if (err) {
		return -1;
	}
	err = gap_params_init();
	if (err) {
		return -1;
	}
	err = advertising_init();
	if (err) {
		return -1;
	}
	err = services_init();
	if (err) {
		return -1;
	}
	(void)sensor_simulator_init();

	err = ble_conn_params_event_handler_set(on_conn_params_evt);
	if (err) {
		LOG_ERR("Failed to setup conn param event handler, err %d\n", err);
		return -1;
	}

	/* Start execution. */
	LOG_INF("Continuous Glucose Monitoring example started.");
	err = application_timers_start();
	if (err) {
		return -1;
	}

	err = advertising_start(erase_bonds);
	if (err) {
		return -1;
	}

	/* Enter main loop. */
	while (true) {
		sd_app_evt_wait();
	}
}

 /** @} */
