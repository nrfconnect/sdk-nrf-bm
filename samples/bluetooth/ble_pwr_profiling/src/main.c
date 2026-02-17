/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <stdint.h>
#include <string.h>
#include <ble_gap.h>
#include <nrf_soc.h>
#include <bm/bluetooth/services/common.h>
#include <bm/bm_timer.h>
#include <bm/bm_buttons.h>
#include <bm/bluetooth/ble_adv_data.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/ble_qwr.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>
#include <zephyr/drivers/retained_mem/nrf_retained_mem.h>

#include <hal/nrf_regulators.h>
#include <hal/nrf_gpio.h>
#include <helpers/nrfx_reset_reason.h>
#if defined(CONFIG_HAS_NORDIC_RAM_CTRL)
#include <helpers/nrfx_ram_ctrl.h>
#endif

#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_BLE_PWR_PROFILING_LOG_LEVEL);

/** Custom UUID base for the Service. */
#define BLE_UUID_BASE { 0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, \
			0xDE, 0xEF, 0x12, 0x12, 0x30, 0x16, 0x00, 0x00 }

/** Byte 12 and 13 of the  Service UUID. */
#define BLE_UUID_PWR_SERVICE 0x1630
/** Byte 12 and 13 of the Characteristic UUID. */
#define BLE_UUID_PWR_CHARACTERISTIC 0x1631

/* Notification connection timeout. */
#define NOTIF_CONN_TIMEOUT CONFIG_SAMPLE_BLE_PWR_PROFILING_NOTIF_CONNECTION_TIMEOUT

/** Characteristic notification timer. */
static struct bm_timer char_notif_timer;
/** Connection timer. */
static struct bm_timer connection_timer;
/** Poweroff timer. */
static struct bm_timer poweroff_timer;

/** BLE QWR instance. */
BLE_QWR_DEF(ble_qwr);
/** Characteristic value. */
static uint8_t char_value[CONFIG_SAMPLE_BLE_PWR_PROFILING_CHAR_VALUE_LEN];
/** Handle of the current connection. */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
/** Connection interval. */
static uint16_t conn_interval_ms = (CONFIG_BLE_CONN_PARAMS_MIN_CONN_INTERVAL * 5) / 4;
/** Attribute handle of power profiling service. */
static uint16_t service_handle;
/** Attribute handles related to power profiling characteristic. */
static ble_gatts_char_handles_t char_handles;

/** Advertising modes. */
enum adv_mode {
	ADV_MODE_IDLE,
	ADV_MODE_CONN,
	ADV_MODE_NONCONN,
};

/** Current advertising mode. */
static enum adv_mode adv_mode_current;
/** Advertising parameters. */
static ble_gap_adv_params_t adv_params;
/* Advertising handle. */
static uint8_t adv_handle;
/* Advertising data. */
static ble_gap_adv_data_t gap_adv_data;
/* Encoded advertising data. */
static uint8_t enc_adv_data[2][BLE_GAP_ADV_SET_DATA_SIZE_MAX];
/* Encoded scan response data. */
static uint8_t enc_scan_rsp_data[2][BLE_GAP_ADV_SET_DATA_SIZE_MAX];

/** Power of the SoC. */
static void poweroff(void)
{
	LOG_INF("Power off");
	log_flush();

#if defined(CONFIG_SAMPLE_BLE_PWR_PROFILING_LED)
	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
#endif

#if defined(CONFIG_HAS_NORDIC_RAM_CTRL)
	uint8_t *ram_start;
	size_t ram_size;

#if defined(NRF_MEMORY_RAM_BASE)
	ram_start = (uint8_t *)NRF_MEMORY_RAM_BASE;
#else
	ram_start = (uint8_t *)NRF_MEMORY_RAM0_BASE;
#endif

	ram_size = 0;
#if defined(NRF_MEMORY_RAM_SIZE)
	ram_size += NRF_MEMORY_RAM_SIZE;
#endif
#if defined(NRF_MEMORY_RAM0_SIZE)
	ram_size += NRF_MEMORY_RAM0_SIZE;
#endif
#if defined(NRF_MEMORY_RAM1_SIZE)
	ram_size += NRF_MEMORY_RAM1_SIZE;
#endif
#if defined(NRF_MEMORY_RAM2_SIZE)
	ram_size += NRF_MEMORY_RAM2_SIZE;
#endif

	/* Disable retention for all memory blocks */
	nrfx_ram_ctrl_retention_enable_set(ram_start, ram_size, false);

#endif /* defined(CONFIG_HAS_NORDIC_RAM_CTRL) */

#if defined(CONFIG_RETAINED_MEM_NRF_RAM_CTRL)
	/* Restore retention for retained_mem driver regions defined in devicetree */
	(void)z_nrf_retained_mem_retention_apply();
#endif

#if defined(CONFIG_SOC_SERIES_NRF54L)
	nrfx_reset_reason_clear(UINT32_MAX);
#endif

	nrf_regulators_system_off(NRF_REGULATORS);

	CODE_UNREACHABLE;
}

static uint32_t ble_pwr_profiling_char_add(const uint8_t uuid_type, uint16_t service_handle,
					   ble_gatts_char_handles_t *char_handles)
{
	ble_uuid_t char_uuid = {
		.type = uuid_type,
		.uuid = BLE_UUID_PWR_CHARACTERISTIC,
	};
	ble_gatts_attr_md_t cccd_md = {
		.vloc = BLE_GATTS_VLOC_STACK
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
			.notify = true,
		},
		.p_cccd_md = &cccd_md,
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = char_value,
		.init_len = CONFIG_SAMPLE_BLE_PWR_PROFILING_CHAR_VALUE_LEN,
		.max_len = CONFIG_SAMPLE_BLE_PWR_PROFILING_CHAR_VALUE_LEN,
	};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

	/* Add characteristic declaration and value attributes. */
	return sd_ble_gatts_characteristic_add(service_handle, &char_md, &attr_char_value,
					       char_handles);
}

/* Send characteristic notification to peer if in a connected state and notification is enabled. */
static void notification_send(void)
{
	uint32_t nrf_err;
	uint16_t len = CONFIG_SAMPLE_BLE_PWR_PROFILING_CHAR_VALUE_LEN;

	/* Increase last byte of characteristic value to have different values on each update*/
	char_value[0]++;

	/* Send value if connected and notifying. */
	if (conn_handle != BLE_CONN_HANDLE_INVALID) {
		ble_gatts_hvx_params_t hvx_params = {
			.handle = char_handles.value_handle,
			.type   = BLE_GATT_HVX_NOTIFICATION,
			.offset = 0,
			.p_len  = &len,
			.p_data = char_value,
		};

		nrf_err = sd_ble_gatts_hvx(conn_handle, &hvx_params);
		if ((nrf_err != NRF_SUCCESS) &&
		    (nrf_err != NRF_ERROR_INVALID_STATE) &&
		    (nrf_err != NRF_ERROR_RESOURCES) &&
		    (nrf_err != BLE_ERROR_GATTS_SYS_ATTR_MISSING)) {
			LOG_ERR("sd_ble_gatts_hvx failed, nrf_error %#x", nrf_err);
		}
	}
}

/* Connection interval timeout.
 * This function will be called when the connection interval timer expires. This will
 * trigger another characteristic notification to the peer.
 */
static void char_notif_timeout_handler(void *ctx)
{
	ARG_UNUSED(ctx);

	/*  Send one notification. */
	notification_send();
}


/* Connection timeout.
 * This function will be called when the connection timer expires. This will stop the
 * timer for characteristic notification and disconnect from the peer.
 */
static void connection_timeout_handler(void *ctx)
{
	int err;
	uint32_t nrf_err;

	ARG_UNUSED(ctx);

	/* Stop all notifications (by stopping the timer for connection interval that triggers
	 * notifications and disconnecting from peer).
	 */
	err = bm_timer_stop(&char_notif_timer);
	if (err) {
		LOG_ERR("Failed to stop timer, err %d", err);
	}

	nrf_err = sd_ble_gap_disconnect(conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
	if (nrf_err) {
		LOG_ERR("Failed to disconnect, nrf_error %#x", nrf_err);
	}
}

/* Poweroff timeout.
 * This function will be called when the poweroff timer triggers.
 */
static void poweroff_timeout_handler(void *ctx)
{
	ARG_UNUSED(ctx);

	poweroff();
}

/* Handle gatt write event.
 * If notifications are enabled, this will start a timer to send a notification on each connection
 * interval. In addition a connection timer is started, that disconnects the peripheral on timeout.
 */
static void on_write(const ble_evt_t *ble_evt)
{
	int err;
	bool notif_enabled;
	const ble_gatts_evt_write_t *evt_write = &ble_evt->evt.gatts_evt.params.write;

	if ((evt_write->handle == char_handles.cccd_handle) && (evt_write->len == 2)) {
		/* CCCD written. Start notifications */
		notif_enabled = is_notification_enabled(evt_write->data);

		if (notif_enabled) {
			err = bm_timer_start(&char_notif_timer,
					     BM_TIMER_MS_TO_TICKS(conn_interval_ms), NULL);
			if (err) {
				LOG_ERR("Failed to start conn interval timer, err %d", err);
			}

			err = bm_timer_start(&connection_timer,
					     BM_TIMER_MS_TO_TICKS(NOTIF_CONN_TIMEOUT), NULL);
			if (err) {
				LOG_ERR("Failed to start notif timer, err %d", err);
			}

			notification_send();
		} else {
			err = bm_timer_stop(&char_notif_timer);
			if (err) {
				LOG_ERR("Failed to stop conn interval timer, err %d", err);
			}

			err = bm_timer_stop(&connection_timer);
			if (err) {
				LOG_ERR("Failed to stop notif timer, err %d", err);
			}
		}
	}
}

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	int err;
	uint32_t nrf_err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;
#if defined(CONFIG_SAMPLE_BLE_PWR_PROFILING_LED)
		nrf_gpio_pin_write(BOARD_PIN_LED_1, BOARD_LED_ACTIVE_STATE);
#endif
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Peer disconnected");
		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
			err = bm_timer_stop(&connection_timer);
			if (err) {
				LOG_ERR("Failed to stop timer, err %d", err);
			}
			poweroff();
		}
#if defined(CONFIG_SAMPLE_BLE_PWR_PROFILING_LED)
		nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
#endif
		break;

	case BLE_GAP_EVT_AUTH_STATUS:
		LOG_INF("Authentication status: %#x",
			evt->evt.gap_evt.params.auth_status.auth_status);
		break;

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		/* Pairing not supported */
		nrf_err = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle,
						      BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP,
						      NULL, NULL);
		if (nrf_err) {
			LOG_ERR("Failed to reply with Security params, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		/* No system attributes have been stored */
		nrf_err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (nrf_err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_GATTS_EVT_WRITE:
		on_write(evt);
		break;

	case BLE_GAP_EVT_ADV_SET_TERMINATED:
		const uint8_t reason = evt->evt.gap_evt.params.adv_set_terminated.reason;

		if (reason == BLE_GAP_EVT_ADV_SET_TERMINATED_REASON_TIMEOUT ||
		    reason == BLE_GAP_EVT_ADV_SET_TERMINATED_REASON_LIMIT_REACHED) {
			poweroff();
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
			LOG_ERR("Disconnect failed on conn params update rejection, nrf_error %#x",
				nrf_err);
		} else {
			LOG_INF("Disconnected from peer, unacceptable conn params");
		}
		break;

	case BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED:
		if (evt->conn_handle != conn_handle) {
			LOG_DBG("Connection handle does not match, expected %d, was %d",
				conn_handle, evt->conn_handle);
			break;
		}
		break;

	case BLE_CONN_PARAMS_EVT_UPDATED:
		if (evt->conn_handle == conn_handle) {
			conn_interval_ms = (evt->conn_params.max_conn_interval * 5) / 4;
		}
		break;

	default:
		break;
	}
}

static uint16_t on_ble_qwr_evt(struct ble_qwr *qwr, const struct ble_qwr_evt *qwr_evt)
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

	return BLE_GATT_STATUS_SUCCESS;
}

/* Initialize BLE service */
static uint32_t ble_service_init(uint16_t *service_handle, uint8_t *uuid_type,
				 ble_gatts_char_handles_t *char_handles)
{
	uint32_t nrf_err;
	ble_uuid_t ble_uuid;
	ble_uuid128_t uuid_base = {
		.uuid128 = BLE_UUID_BASE
	};

	if (!service_handle || !uuid_type) {
		return NRF_ERROR_NULL;
	}

	/* Add a custom base UUID. */
	nrf_err = sd_ble_uuid_vs_add(&uuid_base, uuid_type);
	if (nrf_err) {
		LOG_ERR("Failed to add base UUID, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	ble_uuid.type = *uuid_type;
	ble_uuid.uuid = BLE_UUID_PWR_SERVICE;

	/* Add the service. */
	nrf_err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, service_handle);
	if (nrf_err) {
		LOG_ERR("Failed to add pwr profiling service, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	/* Add characteristic. */
	nrf_err = ble_pwr_profiling_char_add(*uuid_type, *service_handle, char_handles);
	if (nrf_err) {
		LOG_ERR("Failed to add pwr profiling characteristic, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

/* Add optional advertising data and start advertising in the given mode */
static void adv_data_update_and_start(enum adv_mode adv_mode)
{
	uint32_t nrf_err;
	ble_gap_adv_data_t new_adv_data = {0};

	struct ble_adv_data adv_data = {
		.name_type = BLE_ADV_DATA_FULL_NAME,
		.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
	};
	struct ble_adv_data sr_data = {0};

	if (adv_mode_current != ADV_MODE_IDLE) {
		nrf_err = sd_ble_gap_adv_stop(adv_handle);
		if (nrf_err) {
			LOG_ERR("Failed to stop advertising, nrf_error %#x", nrf_err);
			return;
		}

		LOG_INF("Advertising stopped. Reconfiguring...");
	}

	switch (adv_mode) {
	case ADV_MODE_CONN:
		memset(&adv_params, 0, sizeof(adv_params));
		adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
		adv_params.interval = CONFIG_SAMPLE_BLE_PWR_PROFILING_CONN_ADVERTISING_INTERVAL;
		adv_params.duration = CONFIG_SAMPLE_BLE_PWR_PROFILING_CONN_ADVERTISING_TIMEOUT;
		break;

	case ADV_MODE_NONCONN:
		sr_data.uuid_lists.complete.uuid = NULL;
		sr_data.uuid_lists.complete.len = 0;

		memset(&adv_params, 0, sizeof(adv_params));
		adv_params.properties.type = BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED;
		adv_params.interval = CONFIG_SAMPLE_BLE_PWR_PROFILING_NONCONN_ADVERTISING_INTERVAL;
		adv_params.duration = CONFIG_SAMPLE_BLE_PWR_PROFILING_NONCONN_ADVERTISING_TIMEOUT;
		break;

	default:
		break;
	}

	new_adv_data.adv_data.p_data =
		(new_adv_data.adv_data.p_data != enc_adv_data[0])
			? enc_adv_data[0]
			: enc_adv_data[1];

	new_adv_data.adv_data.len = BLE_GAP_ADV_SET_DATA_SIZE_MAX;

	nrf_err = ble_adv_data_encode(&adv_data, new_adv_data.adv_data.p_data,
				      &new_adv_data.adv_data.len);
	if (nrf_err) {
		return;
	}

	new_adv_data.scan_rsp_data.p_data =
		(new_adv_data.scan_rsp_data.p_data != enc_scan_rsp_data[0])
			? enc_scan_rsp_data[0]
			: enc_scan_rsp_data[1];

	new_adv_data.scan_rsp_data.len = BLE_GAP_ADV_SET_DATA_SIZE_MAX;

	nrf_err = ble_adv_data_encode(&sr_data, new_adv_data.scan_rsp_data.p_data,
				      &new_adv_data.scan_rsp_data.len);
	if (nrf_err) {
		return;
	}

	memcpy(&gap_adv_data, &new_adv_data, sizeof(gap_adv_data));

	nrf_err = sd_ble_gap_adv_set_configure(&adv_handle, &gap_adv_data, &adv_params);
	if (nrf_err) {
		LOG_ERR("Failed to set advertising data, nrf_error %#x", nrf_err);
		return;
	}

	nrf_err = sd_ble_gap_adv_start(adv_handle, CONFIG_NRF_SDH_BLE_CONN_TAG);
	if (nrf_err) {
		LOG_ERR("Failed to start advertising, nrf_error %#x", nrf_err);
		return;
	}

	adv_mode_current = adv_mode;

	LOG_INF("Advertising as %s", CONFIG_SAMPLE_BLE_PWR_PROFILING_ADV_NAME);
}

static void button_handler(uint8_t pin, uint8_t action)
{
	if (action != BM_BUTTONS_PRESS) {
		return;
	}

	/* Cancel poweroff */
	(void)bm_timer_stop(&poweroff_timer);

	switch (pin) {
	case BOARD_PIN_BTN_2:
		adv_data_update_and_start(ADV_MODE_CONN);
		break;

	case BOARD_PIN_BTN_3:
		adv_data_update_and_start(ADV_MODE_NONCONN);
		break;

	default:
		break;
	}
}

static uint32_t adv_init(void)
{
	uint32_t nrf_err;
	ble_gap_conn_sec_mode_t sec_mode = {0};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
	nrf_err = sd_ble_gap_device_name_set(&sec_mode, CONFIG_SAMPLE_BLE_PWR_PROFILING_ADV_NAME,
					     strlen(CONFIG_SAMPLE_BLE_PWR_PROFILING_ADV_NAME));
	if (nrf_err) {
		LOG_ERR("Failed to set advertising name, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	gap_adv_data.adv_data.p_data = enc_adv_data[0];
	gap_adv_data.adv_data.len = BLE_GAP_ADV_SET_DATA_SIZE_MAX;

	adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
	adv_params.duration = BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED;
	adv_params.interval = BLE_GAP_ADV_INTERVAL_MAX;
	adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
	adv_params.primary_phy = BLE_GAP_PHY_AUTO;

	nrf_err = sd_ble_gap_adv_set_configure(&adv_handle, NULL, &adv_params);
	if (nrf_err) {
		LOG_ERR("Failed to set GAP advertising parameters, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

int main(void)
{
	int err;
	uint32_t nrf_err;
	uint8_t uuid_type;
	struct ble_qwr_config qwr_config = {
		.evt_handler = on_ble_qwr_evt,
	};

	struct bm_buttons_config configs[] = {
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

	LOG_INF("BLE PWR Profiling sample started");

#if defined(CONFIG_SAMPLE_BLE_PWR_PROFILING_LED)
	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_1);
#endif

	nrf_err = ble_conn_params_evt_handler_set(on_conn_params_evt);
	if (nrf_err) {
		LOG_ERR("Failed to setup conn param event handler, nrf_error %#x", nrf_err);
		goto idle;
	}

	err = bm_timer_init(&char_notif_timer, BM_TIMER_MODE_REPEATED, char_notif_timeout_handler);
	if (err) {
		LOG_ERR("Failed to initialize characteristic notification timer, err %d", err);
		goto idle;
	}

	err = bm_timer_init(&connection_timer, BM_TIMER_MODE_REPEATED, connection_timeout_handler);
	if (err) {
		LOG_ERR("Failed to initialize connection timer, err %d", err);
		goto idle;
	}

	err = bm_timer_init(&poweroff_timer, BM_TIMER_MODE_SINGLE_SHOT, poweroff_timeout_handler);
	if (err) {
		LOG_ERR("Failed to initialize poweroff timer, err %d", err);
		goto idle;
	}

	err = bm_buttons_init(configs, ARRAY_SIZE(configs), BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		LOG_ERR("Failed to initialize buttons, err %d", err);
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

	nrf_err = ble_qwr_init(&ble_qwr, &qwr_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize QWR, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = adv_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_service_init(&service_handle, &uuid_type, &char_handles);
	if (nrf_err) {
		LOG_ERR("Failed to initialize pwr profiling service, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Services initialized");

	err = bm_buttons_enable();
	if (err) {
		LOG_ERR("Failed to enable buttons, err %d", err);
		goto idle;
	}

	const bool connectable_adv = bm_buttons_is_pressed(BOARD_PIN_BTN_2);
	const bool nonconnectable_adv = bm_buttons_is_pressed(BOARD_PIN_BTN_3);

	if (connectable_adv) {
		adv_data_update_and_start(ADV_MODE_CONN);
	} else if (nonconnectable_adv) {
		adv_data_update_and_start(ADV_MODE_NONCONN);
	} else {
		/* No advertising mode is selected at startup, power off */
		LOG_INF("No advertising selected, schedule power off in 5 seconds");
		err = bm_timer_start(&poweroff_timer, BM_TIMER_MS_TO_TICKS(5000), NULL);
	}

#if defined(CONFIG_SAMPLE_BLE_PWR_PROFILING_LED)
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
#endif

	LOG_INF("BLE PWR Profiling sample initialized");

idle:
	while (true) {
		log_flush();

		k_cpu_idle();
	}

	return 0;
}
