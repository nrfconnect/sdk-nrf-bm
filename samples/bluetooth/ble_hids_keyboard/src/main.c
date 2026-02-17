/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <hal/nrf_gpio.h>
#include <nrf_error.h>
#include <nrf_soc.h>
#include <ble_gap.h>
#include <ble_types.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/sensorsim.h>
#include <bm/bm_timer.h>
#include <bm/bm_buttons.h>
#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/ble_qwr.h>
#include <bm/bluetooth/services/common.h>
#include <bm/bluetooth/services/uuid.h>
#include <bm/bluetooth/services/ble_bas.h>
#include <bm/bluetooth/services/ble_dis.h>
#include <bm/bluetooth/services/ble_hids.h>
#include <bm/bluetooth/peer_manager/nrf_ble_lesc.h>
#include <bm/bluetooth/peer_manager/peer_manager.h>
#include <bm/bluetooth/peer_manager/peer_manager_handler.h>
#include <bm/bm_scheduler.h>

#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/toolchain.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_BLE_HIDS_KEYBOARD_LOG_LEVEL);

#define BASE_USB_HID_SPEC_VERSION 0x0101

/* Control key codes - required 8 of them */
#define INPUT_REPORT_KEYS_CTRL_CODE_MIN 224
/* Control key codes - required 8 of them */
#define INPUT_REPORT_KEYS_CTRL_CODE_MAX 231

/* Index of Input Report. */
#define INPUT_REPORT_KEYS_INDEX 0
/* Id of reference to Keyboard Input Report. */
#define INPUT_REP_REF_ID 0

/* Index of Output Report. */
#define OUTPUT_REPORT_INDEX 0
/* Id of reference to Keyboard Output Report. */
#define OUTPUT_REP_REF_ID 0
/* Index of Feature Report. */
#define FEATURE_REPORT_INDEX 0
/* Id of reference to Keyboard Feature Report. */
#define FEATURE_REP_REF_ID 0
/* CAPS LOCK bit in Output Report (based on 'LED Page (0x08)' of the
 * Universal Serial Bus HID Usage Tables).
 */
#define OUTPUT_REPORT_BIT_MASK_CAPS_LOCK 0x02

/* Perform bonding. */
#define SEC_PARAM_BOND 1
/* Man In The Middle protection not required. */
#define SEC_PARAM_MITM 0
/* LE Secure Connections enabled. */
#define SEC_PARAM_LESC 1
/* Keypress notifications not enabled. */
#define SEC_PARAM_KEYPRESS 1
/* No I/O capabilities. */
#define SEC_PARAM_IO_CAPABILITIES BLE_GAP_IO_CAPS_DISPLAY_YESNO
/* Out Of Band data not available. */
#define SEC_PARAM_OOB 0
/* Minimum encryption key size. */
#define SEC_PARAM_MIN_KEY_SIZE 7
/* Maximum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE 16

/* HID service instance */
BLE_HIDS_DEF(ble_hids);
/* BLE Advertising library instance */
BLE_ADV_DEF(ble_adv);
/* BLE QWR instance */
BLE_QWR_DEF(ble_qwr);
/* BLE BAS instance */
BLE_BAS_DEF(ble_bas);

/* BLE Connection handle */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
/* Peer ID */
static uint16_t peer_id;
/* Flag for ongoing authentication request */
static bool auth_key_request;
/* State of caps lock */
static bool caps_on;
/* Boot protocol mode */
static bool boot_mode;

/* Forward declaration */
static void identities_set(enum pm_peer_id_list_skip skip);

/* FIFO for keeping track of keystrokes that can not be sent immediately. */
RING_BUF_DECLARE(report_fifo, CONFIG_SAMPLE_BLE_HIDS_REPORT_FIFO_SIZE *
		 (sizeof(struct ble_hids_input_report) + CONFIG_BLE_HIDS_INPUT_REPORT_MAX_LEN));

uint32_t report_fifo_put(struct ble_hids_input_report *report)
{
	uint32_t written;
	uint32_t space;
	unsigned int key;

	key = irq_lock();
	written = 0;
	space = ring_buf_space_get(&report_fifo);
	if (space >= (sizeof(struct ble_hids_input_report) +
		      CONFIG_BLE_HIDS_INPUT_REPORT_MAX_LEN)) {
		written = ring_buf_put(&report_fifo, (void *)report,
				       sizeof(struct ble_hids_input_report));
		written = ring_buf_put(&report_fifo, (void *)report->data,
				       CONFIG_BLE_HIDS_INPUT_REPORT_MAX_LEN);
	}
	irq_unlock(key);

	if (!written) {
		LOG_ERR("Could not put input report in buffer");
		return NRF_ERROR_NO_MEM;
	}

	return NRF_SUCCESS;
}

void report_fifo_process(void)
{
	uint32_t nrf_err;
	uint32_t written;
	struct ble_hids_input_report report;
	struct ble_hids_boot_keyboard_input_report boot_report;
	uint8_t keys[CONFIG_BLE_HIDS_INPUT_REPORT_MAX_LEN];
	unsigned int key;

	key = irq_lock();
	written = 0;
	if (!ring_buf_is_empty(&report_fifo)) {
		written = ring_buf_get(&report_fifo, (uint8_t *)&report,
				       sizeof(struct ble_hids_input_report));
		__ASSERT_NO_MSG(written == sizeof(struct ble_hids_input_report));

		written = ring_buf_get(&report_fifo, (uint8_t *)keys,
				CONFIG_BLE_HIDS_INPUT_REPORT_MAX_LEN);
		__ASSERT_NO_MSG(written == CONFIG_BLE_HIDS_INPUT_REPORT_MAX_LEN);
	}
	irq_unlock(key);

	if (!written) {
		return;
	}

	report.data = keys;
	if (boot_mode) {
		boot_report.data = report.data;
		boot_report.len = report.len;
		nrf_err = ble_hids_boot_kb_inp_rep_send(&ble_hids, conn_handle, &boot_report);
	} else {
		nrf_err = ble_hids_inp_rep_send(&ble_hids, conn_handle, &report);
	}

	if (nrf_err && nrf_err != BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
		LOG_ERR("Failed to send queued input report, nrf_error %#x", nrf_err);
	}
}

bool report_fifo_is_empty(void)
{
	return ring_buf_is_empty(&report_fifo);
}

void report_fifo_clear(void)
{
	ring_buf_reset(&report_fifo);
}

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	uint32_t nrf_err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;

		nrf_err = ble_qwr_conn_handle_assign(&ble_qwr, conn_handle);
		if (nrf_err) {
			LOG_ERR("Failed to assign qwr handle, nrf_error %#x", nrf_err);
		}
		nrf_gpio_pin_write(BOARD_PIN_LED_1, BOARD_LED_ACTIVE_STATE);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Peer disconnected, reason %d",
			evt->evt.gap_evt.params.disconnected.reason);

		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}
		nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
		report_fifo_clear();
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

	case BLE_GATTS_EVT_HVN_TX_COMPLETE:
		report_fifo_process();
		break;

	default:
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

static void ble_adv_evt_handler(struct ble_adv *ble_adv, const struct ble_adv_evt *evt)
{
	uint32_t nrf_err;
	ble_gap_addr_t *peer_addr;
	ble_gap_addr_t allow_list_addrs[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	ble_gap_irk_t allow_list_irks[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	uint32_t addr_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
	uint32_t irk_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

	switch (evt->evt_type) {
	case BLE_ADV_EVT_ERROR:
		LOG_ERR("Advertising error %#x", evt->error.reason);
		break;
	case BLE_ADV_EVT_DIRECTED_HIGH_DUTY:
	case BLE_ADV_EVT_DIRECTED:
	case BLE_ADV_EVT_FAST:
	case BLE_ADV_EVT_SLOW:
	case BLE_ADV_EVT_FAST_ALLOW_LIST:
	case BLE_ADV_EVT_SLOW_ALLOW_LIST:
		LOG_DBG("Started advertising, adv_evt_type %d", evt->evt_type);
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

		nrf_err = ble_adv_allow_list_reply(ble_adv, allow_list_addrs, addr_cnt,
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
				nrf_err = ble_adv_peer_addr_reply(ble_adv, peer_addr);
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

static void on_hid_rep_char_write(const struct ble_hids_evt *evt)
{
	uint32_t nrf_err;
	uint8_t report_val;
	uint8_t report_index;

	if (evt->params.char_write.char_id.report_type == BLE_HIDS_REPORT_TYPE_OUTPUT) {
		report_index = evt->params.char_write.char_id.report_index;

		if (report_index == OUTPUT_REPORT_INDEX) {
			/* This code assumes that the output report is one byte long.
			 * Hence the following static assert is made.
			 */
			__ASSERT_NO_MSG(CONFIG_BLE_HIDS_OUTPUT_REPORT_MAX_LEN == 1);

			nrf_err = ble_hids_outp_rep_get(&ble_hids, report_index,
							CONFIG_BLE_HIDS_OUTPUT_REPORT_MAX_LEN, 0,
							conn_handle, &report_val);
			if (nrf_err) {
				LOG_ERR("ble_hids_outp_rep_get failed, nrf_error %#x", nrf_err);
				return;
			}

			if (!caps_on && ((report_val & OUTPUT_REPORT_BIT_MASK_CAPS_LOCK) != 0)) {
				LOG_INF("Caps Lock is turned on");
				nrf_gpio_pin_write(BOARD_PIN_LED_2, BOARD_LED_ACTIVE_STATE);
				caps_on = true;
			} else if (caps_on &&
				   ((report_val & OUTPUT_REPORT_BIT_MASK_CAPS_LOCK) == 0)) {
				LOG_INF("Caps Lock is turned off");
				nrf_gpio_pin_write(BOARD_PIN_LED_2, !BOARD_LED_ACTIVE_STATE);
				caps_on = false;
			}
		}
	}
}

static void on_hids_evt(struct ble_hids *hids, const struct ble_hids_evt *hids_evt)
{
	switch (hids_evt->evt_type) {
	case BLE_HIDS_EVT_HOST_SUSP:
		LOG_DBG("Host suspended event");
		break;
	case BLE_HIDS_EVT_HOST_EXIT_SUSP:
		LOG_DBG("Exit suspended event");
		break;
	case BLE_HIDS_EVT_NOTIF_ENABLED:
		LOG_DBG("Notifications enabled event");
		break;
	case BLE_HIDS_EVT_NOTIF_DISABLED:
		LOG_DBG("Notifications disabled event");
		break;
	case BLE_HIDS_EVT_REP_CHAR_WRITE:
		on_hid_rep_char_write(hids_evt);
		break;
	case BLE_HIDS_EVT_BOOT_MODE_ENTERED:
		LOG_DBG("Entered boot mode");
		boot_mode = true;
		break;
	case BLE_HIDS_EVT_REPORT_MODE_ENTERED:
		LOG_DBG("Entered report mode");
		boot_mode = false;
		break;
	case BLE_HIDS_EVT_REPORT_READ:
		LOG_DBG("Read report event");
		break;
	default:
		break;
	}
}

static uint32_t hids_init(void)
{
	struct ble_hids_report_config *input_report;
	struct ble_hids_report_config *output_report;
	struct ble_hids_report_config *feature_report;

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
		0x91, 0x02, /* Output (Data, Variable, Absolute), LED report */
		0x95, 0x01, /* Report Count (1) */
		0x75, 0x03, /* Report Size (3) */
		0x91, 0x01, /* Output (Data, Variable, Absolute), LED report padding */

		0x95, 0x06, /* Report Count (6) */
		0x75, 0x08, /* Report Size (8) */
		0x15, 0x00, /* Logical Minimum (0) */
		0x25, 0x65, /* Logical Maximum (101) */
		0x05, 0x07, /* Usage Page (Key codes) */
		0x19, 0x00, /* Usage Minimum (0) */
		0x29, 0x65, /* Usage Maximum (101) */
		0x81, 0x00, /* Input (Data, Array) Key array(6 bytes) */

		0x09, 0x05, /* Usage (Vendor Defined) */
		0x15, 0x00, /* Logical Minimum (0) */
		0x26, 0xFF, 0x00, /* Logical Maximum (255) */
		0x75, 0x08, /* Report Size (8 bit) */
		0x95, 0x02, /* Report Count (2) */
		0xB1, 0x02, /* Feature (Data, Variable, Absolute) */

		0xC0 /* End Collection (Application) */
	};

	memset((void *)input_report_array, 0, sizeof(input_report_array));
	memset((void *)output_report_array, 0, sizeof(output_report_array));
	memset((void *)feature_report_array, 0, sizeof(feature_report_array));

	/* Initialize HID Service */
	input_report = &input_report_array[INPUT_REPORT_KEYS_INDEX];
	input_report->len = CONFIG_BLE_HIDS_INPUT_REPORT_MAX_LEN;
	input_report->report_id = INPUT_REP_REF_ID;
	input_report->report_type = BLE_HIDS_REPORT_TYPE_INPUT;

	input_report->sec_mode.read = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	input_report->sec_mode.write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	input_report->sec_mode.cccd_write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;

	output_report = &output_report_array[OUTPUT_REPORT_INDEX];
	output_report->len = CONFIG_BLE_HIDS_OUTPUT_REPORT_MAX_LEN;
	output_report->report_id = OUTPUT_REP_REF_ID;
	output_report->report_type = BLE_HIDS_REPORT_TYPE_OUTPUT;

	output_report->sec_mode.write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	output_report->sec_mode.read = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;

	feature_report = &feature_report_array[FEATURE_REPORT_INDEX];
	feature_report->len = CONFIG_BLE_HIDS_FEATURE_REPORT_MAX_LEN;
	feature_report->report_id = FEATURE_REP_REF_ID;
	feature_report->report_type = BLE_HIDS_REPORT_TYPE_FEATURE;

	feature_report->sec_mode.read = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	feature_report->sec_mode.write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;

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
			.flags = {
				.remote_wake = 1,
				.normally_connectable = 1,
			},
		},
		.report_map = {
			.data = report_map_data,
			.len = sizeof(report_map_data),
		},
		.included_services_count = 0,
		.included_services_array = NULL,
		.sec_mode = BLE_HIDS_CONFIG_SEC_MODE_DEFAULT_KEYBOARD,
	};

	return ble_hids_init(&ble_hids, &hids_config);
}

static uint8_t button_ctrl_code_get(uint8_t key)
{
	if (INPUT_REPORT_KEYS_CTRL_CODE_MIN <= key && key <= INPUT_REPORT_KEYS_CTRL_CODE_MAX) {
		return (uint8_t)(1U << (key - INPUT_REPORT_KEYS_CTRL_CODE_MIN));
	}
	return 0;
}

static int key_set(struct ble_hids *hids, uint8_t *report, size_t report_size, uint8_t key)
{
	uint8_t ctrl_mask = button_ctrl_code_get(key);

	if (ctrl_mask) {
		report[0] |= ctrl_mask;
		return 0;
	}
	for (size_t i = 0; i < (report_size - 2); ++i) {
		if (report[i + 2] == 0) {
			report[i + 2] = key;
			return 0;
		}
	}
	/* All slots busy */
	return -EBUSY;
}


static int key_clear(struct ble_hids *hids, uint8_t *report, size_t report_size, uint8_t key)
{
	uint8_t ctrl_mask = button_ctrl_code_get(key);

	if (ctrl_mask) {
		report[0] &= ~ctrl_mask;
		return 0;
	}
	for (size_t i = 0; i < (report_size - 2); ++i) {
		if (report[i + 2] == key) {
			report[i + 2] = 0;
			return 0;
		}
	}

	/* Key not found */
	return -EINVAL;
}

static int on_key_press(struct ble_hids *hids, const char key, bool pressed)
{
	uint32_t nrf_err;
	static uint8_t keys_report[CONFIG_BLE_HIDS_INPUT_REPORT_MAX_LEN];
	struct ble_hids_input_report inp_rep = {
		.report_index = INPUT_REPORT_KEYS_INDEX,
		.data = keys_report,
		.len = sizeof(keys_report),
	};
	struct ble_hids_boot_keyboard_input_report boot_inp_rep = {
		.data = keys_report,
		.len = sizeof(keys_report),
	};

	if (pressed) {
		key_set(hids, keys_report, sizeof(keys_report), key);
	} else {
		key_clear(hids, keys_report, sizeof(keys_report), key);
	}

	if (!report_fifo_is_empty()) {
		return report_fifo_put(&inp_rep);
	}

	if (boot_mode) {
		nrf_err = ble_hids_boot_kb_inp_rep_send(hids, conn_handle, &boot_inp_rep);
	} else {
		nrf_err = ble_hids_inp_rep_send(hids, conn_handle, &inp_rep);
	}

	if (nrf_err && nrf_err != BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
		if (nrf_err == NRF_ERROR_RESOURCES) {
			return report_fifo_put(&inp_rep);
		}
		LOG_ERR("Failed to send input report, nrf_error %#x", nrf_err);
	}

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
	static const char hello_world_str[] = {
		0x0b,	/* Key h */
		0x08,	/* Key e */
		0x0f,	/* Key l */
		0x0f,	/* Key l */
		0x12,	/* Key o */
		0x28,	/* Key Return */
	};
	static const char *chr = hello_world_str;

	if (conn_handle == BLE_CONN_HANDLE_INVALID) {
		return;
	}

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

	switch (pin) {
	case BOARD_PIN_BTN_2:
		on_key_press(&ble_hids, 0xE1, action == BM_BUTTONS_PRESS); /* Shift */
		break;
	case BOARD_PIN_BTN_3:
		if (action == BM_BUTTONS_PRESS) {
			on_key_press(&ble_hids, *chr, true);
		} else {
			on_key_press(&ble_hids, *chr, false);
			if (++chr == (hello_world_str + sizeof(hello_world_str))) {
				chr = hello_world_str;
			}
		}
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
			/* Note: You should check on what kind of allow list policy your
			 * application should use.
			 */

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

uint16_t ble_qwr_evt_handler(struct ble_qwr *qwr, const struct ble_qwr_evt *qwr_evt)
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

int main(void)
{
	int err;
	uint32_t nrf_err;

	ble_uuid_t adv_uuid_list[] = {
		{ .uuid = BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE, .type = BLE_UUID_TYPE_BLE },
	};

	struct ble_adv_config ble_adv_cfg = {
		.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
		.evt_handler = ble_adv_evt_handler,
		.adv_data = {
			.name_type = BLE_ADV_DATA_FULL_NAME,
			.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,

		},
		.sr_data.uuid_lists.complete = {
			.uuid = &adv_uuid_list[0],
			.len = ARRAY_SIZE(adv_uuid_list),
		}
	};

	struct ble_bas_config bas_config = {
		.evt_handler = NULL,
		.can_notify = true,
		.report_ref = NULL,
		.battery_level = 100,
		.sec_mode = BLE_BAS_CONFIG_SEC_MODE_DEFAULT,
	};

	struct ble_dis_config dis_config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
	};

	struct ble_qwr_config qwr_config = {
		.evt_handler = ble_qwr_evt_handler,
	};

	struct bm_buttons_config configs[] = {
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

	LOG_INF("BLE HIDS Keyboard sample started.");

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_1);
	nrf_gpio_cfg_output(BOARD_PIN_LED_2);

	err = bm_buttons_init(configs, ARRAY_SIZE(configs), BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		LOG_ERR("Failed to initialize buttons, err %d", err);
		goto idle;
	}

	err = bm_buttons_enable();
	if (err) {
		LOG_ERR("Failed to enable buttons, err %d", err);
		goto idle;
	}

	const bool erase_bonds = bm_buttons_is_pressed(BOARD_PIN_BTN_1);

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

	LOG_INF("Bluetooth is enabled!");

	nrf_err = peer_manager_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize Peer Manager, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_qwr_init(&ble_qwr, &qwr_config);
	if (nrf_err) {
		LOG_ERR("ble_qwr_init failed, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_dis_init(&dis_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize device information service, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_bas_init(&ble_bas, &bas_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize BAS service, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = hids_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize HIDS, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("HIDS initialized");

	nrf_err = sd_ble_gap_appearance_set(BLE_APPEARANCE_HID_KEYBOARD);
	if (nrf_err) {
		LOG_ERR("Failed to sd_ble_gap_appearance_set, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_adv_init(&ble_adv, &ble_adv_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize BLE advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = advertising_start(erase_bonds);
	if (nrf_err) {
		LOG_ERR("Failed to start advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Advertising as %s", CONFIG_BLE_ADV_NAME);

	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
	LOG_INF("BLE HIDS Keyboard sample initialized");

idle:
	while (true) {
		(void)nrf_ble_lesc_request_handler();

		log_flush();

		k_cpu_idle();
	}

	return 0;
}
