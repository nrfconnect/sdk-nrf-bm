/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <ble_gap.h>
#include <ble_hci.h>
#include <ble_types.h>
#include <hal/nrf_gpio.h>
#include <nrf_error.h>
#include <nrf_soc.h>

#include <bm/bm_buttons.h>
#include <bm/bm_timer.h>
#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/services/ble_bas.h>
#include <bm/bluetooth/services/ble_dis.h>
#include <bm/bluetooth/services/ble_mds.h>
#include <bm/bluetooth/services/uuid.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>

#include <memfault/core/trace_event.h>
#include <memfault/metrics/metrics.h>
#include <memfault/metrics/platform/timer.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/util.h>

#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_BLE_MDS_LOG_LEVEL);

#define RUN_STATUS_LED BOARD_PIN_LED_0
#define CONN_STATUS_LED BOARD_PIN_LED_1
#define BATTERY_LEVEL_MAX 100U

BLE_ADV_DEF(ble_adv); /* BLE advertising instance */
BLE_BAS_DEF(ble_bas); /* BLE battery service instance */
BLE_MDS_DEF(ble_mds); /* BLE Memfault Diagnostic Service instance */

static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
static uint8_t battery_level = BATTERY_LEVEL_MAX;

static struct bm_timer battery_timer;

static volatile bool memfault_heartbeat_pending;

#if IS_ENABLED(CONFIG_MEMFAULT_METRICS_TIMER_CUSTOM)
/* CONFIG_MEMFAULT_METRICS_TIMER_CUSTOM delegates heartbeat scheduling to the platform.
 * On boot, Memfault itself calls memfault_platform_metrics_timer_boot() below,
 * a function we get to define, and hands us a callback and an interval.
 * This whole block is us building that schedule ourselves: we save the callback,
 * start our own repeating timer via bm_timer for the given interval,
 * and then poll every loop in main() via memfault_metrics_timer_process(),
 * to check if that timer has fired yet, and if it has, run the callback.
 */
static struct bm_timer memfault_metrics_timer;
static MemfaultPlatformTimerCallback *memfault_metrics_timer_cb;
static volatile bool memfault_metrics_timer_due;

static void memfault_metrics_timer_timeout_handler(void *context)
{
	ARG_UNUSED(context);

	memfault_metrics_timer_due = true;
}

bool memfault_platform_metrics_timer_boot(uint32_t period_sec,
					  MemfaultPlatformTimerCallback callback)
{
	int err;
	uint32_t period_ms;

	if ((period_sec == 0U) || (callback == NULL)) {
		return false;
	}

	memfault_metrics_timer_cb = callback;

	err = bm_timer_init(&memfault_metrics_timer, BM_TIMER_MODE_REPEATED,
			    memfault_metrics_timer_timeout_handler);
	if (err) {
		return false;
	}

	period_ms = period_sec * 1000U;
	err = bm_timer_start(&memfault_metrics_timer, BM_TIMER_MS_TO_TICKS(period_ms), NULL);
	if (err) {
		return false;
	}

	return true;
}

/* Polls the memfault_metrics_timer_due flag, set by the bm_timer expiry handler,
 * and runs the callback once it fires.
 */
static void memfault_metrics_timer_process(void)
{
	if (!memfault_metrics_timer_due || (memfault_metrics_timer_cb == NULL)) {
		return;
	}
	memfault_metrics_timer_due = false;

	/*
	 * The "memfault_metrics_timer_cb" serializes every metric set via
	 * MEMFAULT_METRIC_SET_UNSIGNED, MEMFAULT_METRIC_ADD, etc. into a
	 * heartbeat event, and queues it for the memfault packetizer.
	 */
	memfault_metrics_timer_cb();
}
#endif /* IS_ENABLED(CONFIG_MEMFAULT_METRICS_TIMER_CUSTOM) */

static void leds_init(void)
{
	nrf_gpio_cfg_output(RUN_STATUS_LED);
	nrf_gpio_cfg_output(CONN_STATUS_LED);
	nrf_gpio_pin_write(RUN_STATUS_LED, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(CONN_STATUS_LED, !BOARD_LED_ACTIVE_STATE);
}

static void battery_level_timeout_handler(void *context)
{
	uint32_t nrf_err;
	int err;

	ARG_UNUSED(context);

	if (battery_level == 0U) {
		battery_level = BATTERY_LEVEL_MAX;
	} else {
		battery_level--;
	}

	err = MEMFAULT_METRIC_SET_UNSIGNED(battery_soc_pct, battery_level);
	if (err) {
		LOG_ERR("Failed to set battery_soc_pct metric, err %d", err);
	}

	nrf_err = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	if ((nrf_err != NRF_SUCCESS) &&
	    (nrf_err != BLE_ERROR_INVALID_CONN_HANDLE) &&
	    (nrf_err != NRF_ERROR_INVALID_STATE) &&
	    (nrf_err != BLE_ERROR_GATTS_SYS_ATTR_MISSING)) {
		LOG_ERR("Failed to update battery level, nrf_error %#x", nrf_err);
	}
}

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	uint32_t nrf_err;

	ARG_UNUSED(ctx);

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;
		nrf_gpio_pin_write(CONN_STATUS_LED, BOARD_LED_ACTIVE_STATE);

		nrf_err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (nrf_err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Peer disconnected, reason %#x",
			evt->evt.gap_evt.params.disconnected.reason);
		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}
		nrf_gpio_pin_write(CONN_STATUS_LED, !BOARD_LED_ACTIVE_STATE);
		break;

	case BLE_GAP_EVT_AUTH_STATUS:
		LOG_INF("Authentication status: %#x",
			evt->evt.gap_evt.params.auth_status.auth_status);
		break;

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		/* Pairing is not required by this sample. */
		nrf_err = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle,
						      BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP,
						      NULL, NULL);
		if (nrf_err) {
			LOG_ERR("Failed to reply with security params, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		nrf_err = sd_ble_gatts_sys_attr_set(evt->evt.gatts_evt.conn_handle, NULL, 0, 0);
		if (nrf_err) {
			LOG_ERR("Failed to set missing system attributes, nrf_error %#x",
				nrf_err);
		}
		break;

	default:
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

static void on_conn_params_evt(const struct ble_conn_params_evt *evt)
{
	uint32_t nrf_err;

	switch (evt->evt_type) {
	case BLE_CONN_PARAMS_EVT_REJECTED:
		nrf_err = sd_ble_gap_disconnect(evt->conn_handle,
						BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
		if (nrf_err) {
			LOG_ERR("Failed to disconnect, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED:
		LOG_INF("ATT MTU updated: %u", evt->att_mtu);
		break;

	case BLE_CONN_PARAMS_EVT_DATA_LENGTH_UPDATED:
		LOG_INF("Data length updated: tx %u rx %u",
			evt->data_length.tx,
			evt->data_length.rx);
		break;

	case BLE_CONN_PARAMS_EVT_ERROR:
		LOG_ERR("Connection parameter error, nrf_error %#x", evt->error.reason);
		break;

	default:
		break;
	}
}

static void ble_adv_evt_handler(struct ble_adv *adv, const struct ble_adv_evt *evt)
{
	ARG_UNUSED(adv);

	switch (evt->evt_type) {
	case BLE_ADV_EVT_ERROR:
		LOG_ERR("Advertising error, nrf_error %#x", evt->error.reason);
		break;
	case BLE_ADV_EVT_IDLE:
		LOG_INF("Advertising stopped");
		break;
	default:
		break;
	}
}

static void button_handler(uint8_t pin, enum bm_buttons_evt_type action)
{
	static bool time_measure_start;
	int err;

	switch (pin) {
	case BOARD_PIN_BTN_0:
		if (action != BM_BUTTONS_PRESS) {
			break;
		}

		time_measure_start = !time_measure_start;
		if (time_measure_start) {
			err = MEMFAULT_METRIC_TIMER_START(button_elapsed_time_ms);
			if (err) {
				LOG_ERR("Failed to start button timer metric, err %d", err);
			} else {
				LOG_INF("Button0 - button_elapsed_time_ms metric timer started");
			}
		} else {
			err = MEMFAULT_METRIC_TIMER_STOP(button_elapsed_time_ms);
			if (err) {
				LOG_ERR("Failed to stop button timer metric, err %d", err);
			} else {
				LOG_INF("Button0 - button_elapsed_time_ms metric timer stopped");
			}

			memfault_heartbeat_pending = true;
			LOG_INF("Memfault heartbeat scheduled");
		}
		break;

	case BOARD_PIN_BTN_1:
		MEMFAULT_TRACE_EVENT_WITH_LOG(button_state_changed, "Button1 state: %u",
					      (uint32_t)(action == BM_BUTTONS_PRESS));
		LOG_INF("Button1 - button_state_changed event tracked, button 1 state: %u",
			(uint32_t)(action == BM_BUTTONS_PRESS));
		break;

	case BOARD_PIN_BTN_2:
		if (action != BM_BUTTONS_PRESS) {
			break;
		}

		err = MEMFAULT_METRIC_ADD(button_press_count, 1);
		if (err) {
			LOG_ERR("Failed to increase button_press_count metric, err %d", err);
		} else {
			LOG_INF("Button2 - button_press_count metric increased");
		}
		break;

	case BOARD_PIN_BTN_3: {
		if (action != BM_BUTTONS_PRESS) {
			break;
		}

		volatile uint32_t div;

		LOG_INF("Division by zero will now be triggered");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiv-by-zero"
		div = 1 / 0;
#pragma GCC diagnostic pop
		ARG_UNUSED(div);
		break;
	}

	default:
		break;
	}
}

int main(void)
{
	int err;
	uint32_t nrf_err;
	ble_gap_conn_sec_mode_t device_name_write_sec;
	struct bm_buttons_config button_configs[] = {
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
	struct ble_bas_config bas_cfg = {
		.can_notify = true,
		.battery_level = battery_level,
		.sec_mode = BLE_BAS_CONFIG_SEC_MODE_DEFAULT,
	};
	struct ble_dis_config dis_cfg = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
	};
	struct ble_mds_config mds_cfg = {
		.sec_mode = BLE_MDS_CONFIG_SEC_MODE_DEFAULT,
	};

	/* Advertise MDS and Battery so scanning apps spot Memfault support, before connecting. */
	ble_uuid_t adv_uuid_list[] = {
		{
			.uuid = BLE_UUID_MDS_SERVICE,
			/* Type will be set after MDS is initialized. */
		},
		{
			.uuid = BLE_UUID_BATTERY_SERVICE,
			.type = BLE_UUID_TYPE_BLE,
		},
	};
	struct ble_adv_config adv_cfg = {
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

	LOG_INF("BLE MDS sample started");

	leds_init();

	err = bm_timer_init(&battery_timer, BM_TIMER_MODE_REPEATED,
			    battery_level_timeout_handler);
	if (err) {
		LOG_ERR("Failed to initialize battery timer, err %d", err);
		goto idle;
	}

	err = bm_buttons_init(button_configs, ARRAY_SIZE(button_configs),
			      BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		LOG_ERR("Failed to initialize buttons, err %d", err);
		goto idle;
	}

	err = bm_buttons_enable();
	if (err) {
		LOG_ERR("Failed to enable buttons, err %d", err);
		goto idle;
	}

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

	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&device_name_write_sec);
	nrf_err = sd_ble_gap_device_name_set(&device_name_write_sec, CONFIG_SAMPLE_BLE_DEVICE_NAME,
					     strlen(CONFIG_SAMPLE_BLE_DEVICE_NAME));
	if (nrf_err) {
		LOG_ERR("Failed to set device name, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_conn_params_evt_handler_set(on_conn_params_evt);
	if (nrf_err) {
		LOG_ERR("Failed to setup conn param event handler, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_bas_init(&ble_bas, &bas_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize battery service, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_dis_init(&dis_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize device information service, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_mds_init(&ble_mds, &mds_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize Memfault Diagnostic Service, nrf_error %#x", nrf_err);
		goto idle;
	}

	/* Set the type for MDS in adv_uuid_list now that MDS is initialized. */
	adv_uuid_list[0].type = ble_mds_service_uuid_type(&ble_mds);

	nrf_err = ble_adv_init(&ble_adv, &adv_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("BLE MDS sample initialized");

	err = bm_timer_start(&battery_timer,
			     BM_TIMER_MS_TO_TICKS(
				     CONFIG_SAMPLE_BLE_MDS_BATTERY_LEVEL_MEAS_INTERVAL),
			     NULL);
	if (err) {
		LOG_ERR("Failed to start battery timer, err %d", err);
		goto idle;
	}

	nrf_gpio_pin_write(RUN_STATUS_LED, BOARD_LED_ACTIVE_STATE);

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (nrf_err) {
		LOG_ERR("Failed to start advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Advertising as %s", CONFIG_SAMPLE_BLE_DEVICE_NAME);

idle:
	while (true) {
#if IS_ENABLED(CONFIG_MEMFAULT_METRICS_TIMER_CUSTOM)
		/* With CONFIG_MEMFAULT_METRICS_TIMER_CUSTOM, Memfault's own internal
		 * `timer_boot` implementation is disabled so we need to drive the heartbeat.
		 */
		memfault_metrics_timer_process();
#endif
		/* Flushes the ISR-pended trace event and serializes it here in the main loop,
		 * since bm_buttons invoke handlers in ISR context, where serialization is not safe.
		 */
		memfault_trace_event_try_flush_isr_event();
		if (memfault_heartbeat_pending) {
			memfault_heartbeat_pending = false;
			memfault_metrics_heartbeat_debug_trigger();
		}
		/* Process Memfault data and send the next chunk over BLE when ready. */
		ble_mds_process(&ble_mds);

		log_flush();

		k_cpu_idle();
	}

	return 0;
}
