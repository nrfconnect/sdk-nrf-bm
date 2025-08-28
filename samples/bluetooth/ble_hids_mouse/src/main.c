/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <nrf_soc.h>
#include <event_scheduler.h>
#include <ble_adv.h>
#include <ble_gap.h>
#include <ble_types.h>
#include <nrf_error.h>
#include <bluetooth/services/common.h>
#include <bluetooth/services/ble_dis.h>
#include <bluetooth/services/ble_hids.h>
#include <zephyr/toolchain.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <bm_buttons.h>

#include <board-config.h>

LOG_MODULE_REGISTER(app, CONFIG_BLE_HIDS_MOUSE_SAMPLE_LOG_LEVEL);

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

#define BTN_PRESSED 1

/* HID service instance */
BLE_HIDS_DEF(ble_hids);
/* BLE Advertising library instance */
BLE_ADV_DEF(ble_adv);

/* BLE Connection handle */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

static bool boot_mode;

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	int err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			LOG_ERR("Failed to set system attributes, nrf_error %d", err);
		}
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Peer disconnected");
		conn_handle = BLE_CONN_HANDLE_INVALID;
		break;

	case BLE_GAP_EVT_AUTH_STATUS:
		LOG_INF("Authentication status: %#x",
		       evt->evt.gap_evt.params.auth_status.auth_status);
		break;

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		LOG_INF("Pairing request");
		ble_gap_sec_params_t sec_param = {
			.io_caps = BLE_GAP_IO_CAPS_NONE,
			.min_key_size = 7,
			.max_key_size = 16,
			.kdist_own = { .id = 1, .enc = 1, },
			.kdist_peer = { .id = 1, .enc = 1, }
		};

		err = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle,
					    BLE_GAP_SEC_STATUS_SUCCESS, &sec_param, NULL);
		if (err) {
			LOG_ERR("Failed to reply to security request, nrf_error %d", err);
		}
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		LOG_INF("BLE_GATTS_EVT_SYS_ATTR_MISSING");
		/* No system attributes have been stored */
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			LOG_ERR("Failed to set system attributes, nrf_error %d", err);
		}
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, 0);

static void ble_adv_evt_handler(struct ble_adv *ble_adv, const struct ble_adv_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_ADV_EVT_ERROR:
		LOG_ERR("Advertising error %d", evt->error.reason);
		break;
	default:
		break;
	}
}

static void on_hids_evt(struct ble_hids *hids, const struct ble_hids_evt *hids_evt)
{
	switch (hids_evt->evt_type) {
	case BLE_HIDS_EVT_HOST_SUSP:
		LOG_INF("BLE_HIDS_EVT_HOST_SUSP");
		break;
	case BLE_HIDS_EVT_HOST_EXIT_SUSP:
		LOG_INF("BLE_HIDS_EVT_HOST_EXIT_SUSP");
		break;
	case BLE_HIDS_EVT_NOTIF_ENABLED:
		LOG_INF("BLE_HIDS_EVT_NOTIF_ENABLED");
		break;
	case BLE_HIDS_EVT_NOTIF_DISABLED:
		LOG_INF("BLE_HIDS_EVT_NOTIF_DISABLED");
		break;
	case BLE_HIDS_EVT_REP_CHAR_WRITE:
		LOG_INF("BLE_HIDS_EVT_REP_CHAR_WRITE");
		break;
	case BLE_HIDS_EVT_BOOT_MODE_ENTERED:
		LOG_INF("BLE_HIDS_EVT_BOOT_MODE_ENTERED");
		boot_mode = true;
		break;
	case BLE_HIDS_EVT_REPORT_MODE_ENTERED:
		LOG_INF("BLE_HIDS_EVT_REPORT_MODE_ENTERED");
		boot_mode = false;
		break;
	case BLE_HIDS_EVT_REPORT_READ:
		LOG_INF("BLE_HIDS_EVT_REPORT_READ");
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
		0x05, 0x01,     /* Usage Page (Generic Desktop) */
		0x09, 0x02,     /* Usage (Mouse) */

		0xA1, 0x01,     /* Collection (Application) */

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
		0xC0  /* End Collection */
	};

	memset((void *)input_report_array, 0, sizeof(input_report_array));

	/* Initialize HID Service */
	input_report = &input_report_array[INPUT_REP_BUTTONS_INDEX];
	input_report->len = INPUT_REP_BUTTONS_LEN;
	input_report->report_id = INPUT_REP_REF_BUTTONS_ID;
	input_report->report_type = BLE_HIDS_REPORT_TYPE_INPUT;

	input_report->sec.read = BLE_GAP_CONN_SEC_MODE_OPEN;
	input_report->sec.write = BLE_GAP_CONN_SEC_MODE_OPEN;
	input_report->sec.cccd_write = BLE_GAP_CONN_SEC_MODE_OPEN;

	input_report = &input_report_array[INPUT_REP_MOVEMENT_INDEX];
	input_report->len = INPUT_REP_MOVEMENT_LEN;
	input_report->report_id = INPUT_REP_REF_MOVEMENT_ID;
	input_report->report_type = BLE_HIDS_REPORT_TYPE_INPUT;

	input_report->sec.read = BLE_GAP_CONN_SEC_MODE_OPEN;
	input_report->sec.write = BLE_GAP_CONN_SEC_MODE_OPEN;
	input_report->sec.cccd_write = BLE_GAP_CONN_SEC_MODE_OPEN;

	input_report = &input_report_array[INPUT_REP_MPLAYER_INDEX];
	input_report->len = INPUT_REP_MEDIA_PLAYER_LEN;
	input_report->report_id = INPUT_REP_REF_MPLAYER_ID;
	input_report->report_type = BLE_HIDS_REPORT_TYPE_INPUT;

	input_report->sec.read = BLE_GAP_CONN_SEC_MODE_OPEN;
	input_report->sec.write = BLE_GAP_CONN_SEC_MODE_OPEN;
	input_report->sec.cccd_write = BLE_GAP_CONN_SEC_MODE_OPEN;

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
			.bcd_flags = {
				.normally_connectable = 1,
				.remote_wake = 1,
			},
			.sec.read = BLE_GAP_CONN_SEC_MODE_OPEN,
		},
		.report_map = {
			.data = report_map_data,
			.len = sizeof(report_map_data),
			.sec.read = BLE_GAP_CONN_SEC_MODE_OPEN,
		},
		.included_services_count = 0,
		.included_services_array = NULL,
	};

	return ble_hids_init(&ble_hids, &hids_config);
}

static void mouse_movement_send(struct ble_hids *hids, int16_t delta_x, int16_t delta_y)
{
	uint32_t nrf_err;
	struct ble_hids_boot_mouse_input_report inp_rep = {
		.buttons = 0x00,
	};

	if (boot_mode) {
		inp_rep.delta_x = MIN(delta_x, 0x00ff);
		inp_rep.delta_y = MIN(delta_y, 0x00ff);

		nrf_err = ble_hids_boot_mouse_inp_rep_send(hids, conn_handle, &inp_rep);
	} else {
		uint8_t buffer[INPUT_REP_MOVEMENT_LEN];

		delta_x = MIN(delta_x, 0x00ff);
		delta_y = MIN(delta_y, 0x0fff);

		buffer[0] = delta_x & 0x00ff;
		buffer[1] = ((delta_y & 0x000f) << 4) | ((delta_x & 0x0f00) >> 8);
		buffer[2] = (delta_y & 0x0ff0) >> 4;

		nrf_err = ble_hids_inp_rep_send(hids, conn_handle, INPUT_REP_MOVEMENT_INDEX,
						buffer, INPUT_REP_MOVEMENT_LEN);
	}

	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to send input report, nrf_error %#x", nrf_err);
	}
}

static void button_handler(uint8_t pin, uint8_t action)
{
	if (conn_handle == BLE_CONN_HANDLE_INVALID || action != BTN_PRESSED) {
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

int main(void)
{
	int err;
	uint32_t nrf_err;

	struct ble_adv_config ble_adv_cfg = {
		.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
		.evt_handler = ble_adv_evt_handler,
		.adv_data = {
			.name_type = BLE_ADV_DATA_FULL_NAME,
			.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		},
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

	err = bm_buttons_init(configs, ARRAY_SIZE(configs), BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		printk("Failed to initialize buttons, err: %d\n", err);
		return err;
	}

	err = bm_buttons_enable();
	if (err) {
		printk("Failed to enable buttons, err: %d\n", err);
		return err;
	}

	err = nrf_sdh_enable_request();
	if (err) {
		printk("Failed to enable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice enabled\n");

	err = nrf_sdh_ble_enable(CONFIG_NRF_SDH_BLE_CONN_TAG);
	if (err) {
		printk("Failed to enable BLE, err %d\n", err);
		return -1;
	}

	printk("Bluetooth is enabled!\n");

	err = ble_dis_init();
	if (err) {
		printk("Failed to initialize device information service, err %d\n", err);
		return -1;
	}

	nrf_err = hids_init();
	if (nrf_err != NRF_SUCCESS) {
		printk("Failed to initialize HIDS, nrf_error %#x", nrf_err);
		return -1;
	}

	printk("HIDS initialized\n");

	err = ble_adv_init(&ble_adv, &ble_adv_cfg);
	if (err) {
		printk("Failed to initialize BLE advertising, err %d\n", err);
		return -1;
	}

	err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (err) {
		printk("Failed to start advertising, err %d\n", err);
		return -1;
	}

	while (true) {
		while (LOG_PROCESS()) {
			/* Empty. */
		}
		sd_app_evt_wait();
	}

	return 0;
}
