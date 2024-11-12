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

LOG_MODULE_REGISTER(app, CONFIG_BLE_HIDS_KEYBOARD_SAMPLE_LOG_LEVEL);

#define BASE_USB_HID_SPEC_VERSION 0x0101

#define INPUT_REPORT_KEYS_INDEX	  0 /**< Index of Input Report. */
#define INPUT_REPORT_KEYS_MAX_LEN 8 /**< Maximum length of the Input Report characteristic. */
#define INPUT_REP_REF_ID	  0 /**< Id of reference to Keyboard Input Report. */

#define OUTPUT_REPORT_INDEX   0 /**< Index of Output Report. */
#define OUTPUT_REPORT_MAX_LEN 1 /**< Maximum length of the Output Report characteristic. */
#define OUTPUT_REP_REF_ID     0 /**< Id of reference to Keyboard Output Report. */

#define FEATURE_REPORT_INDEX   0 /**< Index of Feature Report. */
#define FEATURE_REPORT_MAX_LEN 2 /**< Maximum length of the Feature Report characteristic. */
#define FEATURE_REP_REF_ID     0 /**< Id of reference to Keyboard Feature Report. */

#define BTN_PRESSED 1

/* HID service instance */
BLE_HIDS_DEF(ble_hids);
/* BLE Advertising library instance */
BLE_ADV_DEF(ble_adv);

/* BLE Connection handle */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	uint32_t nrf_err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;
		nrf_err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (nrf_err) {
			LOG_ERR("Failed to set system attributes, nrf_error %d", nrf_err);
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

		nrf_err = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle,
					    BLE_GAP_SEC_STATUS_SUCCESS, &sec_param, NULL);
		if (nrf_err) {
			LOG_ERR("Failed to reply to security request, nrf_error %d", nrf_err);
		}
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		LOG_INF("BLE_GATTS_EVT_SYS_ATTR_MISSING");
		/* No system attributes have been stored */
		nrf_err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (nrf_err) {
			LOG_ERR("Failed to set system attributes, nrf_error %d", nrf_err);
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
		break;
	case BLE_HIDS_EVT_REPORT_MODE_ENTERED:
		LOG_INF("BLE_HIDS_EVT_REPORT_MODE_ENTERED");
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
	struct ble_hids_report_config *p_input_report;
	struct ble_hids_report_config *p_output_report;
	struct ble_hids_report_config *p_feature_report;

	static struct ble_hids_report_config input_report_array[1];
	static struct ble_hids_report_config output_report_array[1];
	static struct ble_hids_report_config feature_report_array[1];
	static uint8_t report_map_data[] = {
		0x05, 0x01, /* Usage Page (Generic Desktop) */
		0x09, 0x06, /* Usage (Keyboard) */
		0xA1, 0x01, /* Collection (Application) */
		0x05, 0x07, /* Usage Page (Key Codes) */
		0x19, 0xe0, /* Usage Minimum (224) */
		0x29, 0xe7, /* Usage Maximum (231) */
		0x15, 0x00, /* Logical Minimum (0) */
		0x25, 0x01, /* Logical Maximum (1) */
		0x75, 0x01, /* Report Size (1) */
		0x95, 0x08, /* Report Count (8) */
		0x81, 0x02, /* Input (Data, Variable, Absolute) */

		0x95, 0x01, /* Report Count (1) */
		0x75, 0x08, /* Report Size (8) */
		0x81, 0x01, /* Input (Constant) reserved byte(1) */

		0x95, 0x05, /* Report Count (5) */
		0x75, 0x01, /* Report Size (1) */
		0x05, 0x08, /* Usage Page (Page# for LEDs) */
		0x19, 0x01, /* Usage Minimum (1) */
		0x29, 0x05, /* Usage Maximum (5) */
		0x91, 0x02, /* Output (Data, Variable, Absolute), Led report */
		0x95, 0x01, /* Report Count (1) */
		0x75, 0x03, /* Report Size (3) */
		0x91, 0x01, /* Output (Data, Variable, Absolute), Led report padding */

		0x95, 0x06, /* Report Count (6) */
		0x75, 0x08, /* Report Size (8) */
		0x15, 0x00, /* Logical Minimum (0) */
		0x25, 0x65, /* Logical Maximum (101) */
		0x05, 0x07, /* Usage Page (Key codes) */
		0x19, 0x00, /* Usage Minimum (0) */
		0x29, 0x65, /* Usage Maximum (101) */
		0x81, 0x00, /* Input (Data, Array) Key array(6 bytes) */

		0x09, 0x05,	  /* Usage (Vendor Defined) */
		0x15, 0x00,	  /* Logical Minimum (0) */
		0x26, 0xFF, 0x00, /* Logical Maximum (255) */
		0x75, 0x08,	  /* Report Size (8 bit) */
		0x95, 0x02,	  /* Report Count (2) */
		0xB1, 0x02,	  /* Feature (Data, Variable, Absolute) */

		0xC0 /* End Collection (Application) */
	};

	memset((void *)input_report_array, 0, sizeof(input_report_array));
	memset((void *)output_report_array, 0, sizeof(output_report_array));
	memset((void *)feature_report_array, 0, sizeof(feature_report_array));

	/* Initialize HID Service */
	p_input_report = &input_report_array[INPUT_REPORT_KEYS_INDEX];
	p_input_report->len = INPUT_REPORT_KEYS_MAX_LEN;
	p_input_report->report_id = INPUT_REP_REF_ID;
	p_input_report->report_type = BLE_HIDS_REPORT_TYPE_INPUT;

	p_input_report->sec.read = BLE_GAP_CONN_SEC_MODE_OPEN;
	p_input_report->sec.write = BLE_GAP_CONN_SEC_MODE_OPEN;
	p_input_report->sec.cccd_write = BLE_GAP_CONN_SEC_MODE_OPEN;

	p_output_report = &output_report_array[OUTPUT_REPORT_INDEX];
	p_output_report->len = OUTPUT_REPORT_MAX_LEN;
	p_output_report->report_id = OUTPUT_REP_REF_ID;
	p_output_report->report_type = BLE_HIDS_REPORT_TYPE_OUTPUT;

	p_output_report->sec.read = BLE_GAP_CONN_SEC_MODE_OPEN;
	p_output_report->sec.write = BLE_GAP_CONN_SEC_MODE_OPEN;
	p_output_report->sec.cccd_write = BLE_GAP_CONN_SEC_MODE_NO_ACCESS;

	p_feature_report = &feature_report_array[FEATURE_REPORT_INDEX];
	p_feature_report->len = FEATURE_REPORT_MAX_LEN;
	p_feature_report->report_id = FEATURE_REP_REF_ID;
	p_feature_report->report_type = BLE_HIDS_REPORT_TYPE_FEATURE;

	p_feature_report->sec.read = BLE_GAP_CONN_SEC_MODE_OPEN;
	p_feature_report->sec.write = BLE_GAP_CONN_SEC_MODE_OPEN;

	struct ble_hids_config hids_config = {
		.evt_handler = on_hids_evt,
		.input_report = input_report_array,
		.output_report = output_report_array,
		.feature_report = feature_report_array,
		.input_report_count = 1,
		.output_report_count = 1,
		.feature_report_count = 1,
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

static int register_key(struct ble_hids *hids, const char key, bool pressed)
{
	uint32_t nrf_err;
	uint8_t report[INPUT_REPORT_KEYS_MAX_LEN] = {};

#define DATA_OFFSET 2

	if (pressed) {
		memcpy(report + DATA_OFFSET, &key, sizeof(key));
	}

	nrf_err = ble_hids_inp_rep_send(hids, conn_handle, INPUT_REPORT_KEYS_INDEX, &report,
				    sizeof(report));
	if (nrf_err) {
		printk("Failed to send input report, nrf_error %#x", nrf_err);
	}

	return 0;
}

static void button_handler(uint8_t pin, uint8_t action)
{
	if (conn_handle == BLE_CONN_HANDLE_INVALID) {
		return;
	}

	switch (pin) {
	case BOARD_PIN_BTN_0:
		register_key(&ble_hids, 0x04, action == BTN_PRESSED); /* Key a */
		break;
	case BOARD_PIN_BTN_1:
		register_key(&ble_hids, 0x05, action == BTN_PRESSED);  /* Key b */
		break;
	case BOARD_PIN_BTN_2:
		register_key(&ble_hids, 0x06, action == BTN_PRESSED);  /* Key c */
		break;
	case BOARD_PIN_BTN_3:
		register_key(&ble_hids, 0x07, action == BTN_PRESSED);  /* Key d */
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
		printk("Failed to initialize buttons, err %d\n", err);
		return err;
	}

	err = bm_buttons_enable();
	if (err) {
		printk("Failed to enable buttons, err %d\n", err);
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
		printk("Failed to initialize HIDS, nrf_error %#x", err);
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
