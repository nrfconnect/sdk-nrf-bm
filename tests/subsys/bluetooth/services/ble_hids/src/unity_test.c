/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <nrf_error.h>
#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <bm/bluetooth/services/ble_hids.h>
#include <bm/bluetooth/services/uuid.h>

#include "cmock_ble_gatts.h"
#include "cmock_ble.h"
#include "cmock_nrf_sdh_ble.h"

#define CONN_HANDLE 0x0001
#define CONN_IDX 0

#define INPUT_REPORT_COUNT 3

#define INPUT_REPORT_1_LEN 3
#define INPUT_REPORT_2_LEN 2

#define OUTPUT_REPORT_1_LEN 1
#define OUTPUT_REPORT_2_LEN 2

#define FEATURE_REPORT_1_LEN 3
#define FEATURE_REPORT_2_LEN 4
#define FEATURE_REPORT_3_LEN 5

#define BASE_USB_HID_SPEC_VERSION 0x0101
#define OUTPUT_REPORT_INDEX 0
#define INPUT_REPORT_KEYS_INDEX 0
#define BOOT_MOUSE_INPUT_REPORT_MIN_LEN 3
#define INPUT_REPORT_KEYS_MAX_LEN 8
#define OUTPUT_REPORT_MAX_LEN 1
#define FEATURE_REPORT_MAX_LEN 1
#define GATTS_WRITE_MAX_DATA_LEN (9)
#define INPUT_REP_REF_ID 0
#define OUTPUT_REP_REF_ID 0
#define FEATURE_REP_REF_ID 0
#define FEATURE_REPORT_INDEX 0

#define DEFAULT_X_DELTA 0x50
#define DEFAULT_Y_DELTA 0XF0

#define CONTROL_POINT_HANDLE 0x00A0
#define PROTOCOL_MODE_HANDLE 0x00A1
#define KB_INPUT_CCCD_HANDLE 0x00A2
#define KB_INPUT_REPORT_HANDLE 0x00A3
#define KB_OUTPUT_REPORT_HANDLE 0x00A4
#define MOUSE_INPUT_REPORT_CCCD_HANDLE 0x00A5
#define MOUSE_INPUT_REPORT_HANDLE 0x00A6
#define INPUT_REPORT_CCCD_HANDLE 0x00A7
#define REPORT_VALUE_HANDLE 0x00A8
#define PROTOCOL_VALUE_HANDLE 0x195D

#define REPORT_MA_DATA_1                                                                           \
	{0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7, 0x15,             \
	 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08,             \
	 0x81, 0x01, 0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91,             \
	 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00,             \
	 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0x09, 0x05, 0x15,             \
	 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x02, 0xB1, 0x02, 0xC0}

#define INITIALIZED_HIDS                                                                           \
	{                                                                                          \
		.evt_handler = on_hids_evt,                                                        \
		.input_report_count = 2,                                                           \
		.inp_rep_init_array = input_report,                                                \
		.output_report_count = 2,                                                          \
		.outp_rep_init_array = output_report,                                              \
		.feature_report_count = 3,                                                         \
		.feature_rep_init_array = feature_report,                                          \
		.link_ctx_storage = {                                                              \
			.max_links_cnt = CONFIG_BLE_HIDS_MAX_CLIENTS,                              \
			.link_ctx_size = sizeof(uint32_t) * BYTES_TO_WORDS(BLE_HIDS_LINK_CTX_SIZE),\
		},                                                                                 \
	}

enum on_write_evt {
	ON_WRITE_CONTROL_PONT,
	ON_WRITE_PROTOCOLS_MODE,
	ON_KB_INP_CCCD,
	ON_KB_INP_VALUE,
	ON_KB_OUTP_VALUE,
	ON_MOUSE_INP_CCCD,
	ON_MOUSE_INP_VALUE,
	ON_INPUT_REPORT_CCCD,
	ON_REP_VALUE_IDENTIFY,
};

enum on_rw_auth_evt {
	ON_OTHER_TYPE,
	ON_PROTOCOL_MODE_RW_AUTH,
	ON_BOOT_KB_INP_REP_RW_AUTH,
	ON_BOOT_KB_OUTP_REP_RW_AUTH,
	ON_BOOT_MOUSE_INP_REP_RW_AUTH,
	ON_REP_VALUE_IDEN_RW_AUTH
};

struct char_write {
	/* Id of characteristic having been written. */
	struct ble_hids_char_id char_id;
	/* Offset for the write operation. */
	uint16_t offset;
	/* Length of the incoming data. */
	uint16_t len;
	/* Incoming data, variable length */
	uint8_t const *data;
};

static struct ble_hids_report_config input_report[2];
static struct ble_hids_report_config output_report[2];
static struct ble_hids_report_config feature_report[3];
struct ble_hids_link_ctx_storage link_ctx_storage = {
	.max_links_cnt = CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT,
	.link_ctx_size = sizeof(uint32_t) * BYTES_TO_WORDS(BLE_HIDS_LINK_CTX_SIZE),
};

static bool error_expected;
static bool error_requested;
static bool hids_evt_expected;
static bool hids_evt_requested;
static uint32_t error = NRF_SUCCESS;
static struct ble_hids_char_id char_evt_id;
static struct char_write char_write_evt;

ble_gatts_hvx_params_t hvx_params;

uint32_t stub_sd_ble_gatts_hvx_different_len(uint16_t conn_handle,
					     ble_gatts_hvx_params_t const *p_hvx_params,
					     int cmock_num_calls)
{
	TEST_ASSERT_EQUAL(CONN_HANDLE, conn_handle);
	*p_hvx_params->p_len = *p_hvx_params->p_len - 2;

	return NRF_SUCCESS;
}

/* Hids event handler. */
static void on_hids_evt(struct ble_hids *hids, const struct ble_hids_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_HIDS_EVT_NOTIF_DISABLED:
		char_evt_id = evt->params.notification.char_id;
		TEST_ASSERT_TRUE(hids_evt_expected);
		hids_evt_expected = false;
		hids_evt_requested = true;
		break;

	case BLE_HIDS_EVT_NOTIF_ENABLED:
		char_evt_id = evt->params.notification.char_id;
		TEST_ASSERT_TRUE(hids_evt_expected);
		hids_evt_expected = false;
		hids_evt_requested = true;
		break;

	case BLE_HIDS_EVT_REP_CHAR_WRITE:
		TEST_ASSERT_TRUE(hids_evt_expected);

		char_write_evt.char_id = evt->params.char_write.char_id;
		char_write_evt.data = evt->params.char_write.data;
		char_write_evt.len = evt->params.char_write.len;
		char_write_evt.offset = evt->params.char_write.offset;

		hids_evt_expected = false;
		hids_evt_requested = true;
		break;

	case BLE_HIDS_EVT_REPORT_READ:
		char_evt_id = evt->params.char_auth_read.char_id;
		hids_evt_expected = false;
		hids_evt_requested = true;
		break;

	case BLE_HIDS_EVT_BOOT_MODE_ENTERED:
	case BLE_HIDS_EVT_HOST_SUSP:
	case BLE_HIDS_EVT_HOST_EXIT_SUSP:
	case BLE_HIDS_EVT_REPORT_MODE_ENTERED:
		TEST_ASSERT_TRUE(hids_evt_expected);
		hids_evt_expected = false;
		hids_evt_requested = true;
		break;

	case BLE_HIDS_EVT_ERROR:
		TEST_ASSERT_TRUE(error_expected);
		error_expected = false;
		error_requested = true;
		error = evt->params.error.reason;
		break;

	default:
		/* No implementation needed. */
		break;
	}
}

/* Emulate BLE_GAP_EVT_CONNECTED event. */
static void emulate_ble_connected_evt(struct ble_hids *ble_hids)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_CONNECTED,
		.evt.gap_evt.conn_handle = CONN_HANDLE,
	};

	ble_hids_on_ble_evt(&ble_evt, ble_hids);
}

/* Emulate BLE_GATTS_EVT_WRITE event. */
static void emulate_ble_write_evt(struct ble_hids *ble_hids, enum on_write_evt on_write_gatts,
				  uint8_t *data, uint16_t len)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_WRITE,
		.evt.gatts_evt.conn_handle = CONN_HANDLE,
	};
	ble_gatts_evt_write_t *evt_write = &ble_evt.evt.gatts_evt.params.write;
	static uint8_t write_evt_data[sizeof(ble_evt_t) + GATTS_WRITE_MAX_DATA_LEN];
	uint8_t offset = offsetof(ble_evt_t, evt.gatts_evt.params.write.data);

	switch (on_write_gatts) {
	case ON_WRITE_CONTROL_PONT:
		/* Control point. */
		evt_write->handle = CONTROL_POINT_HANDLE;
		evt_write->len = len;

		memcpy(write_evt_data, &ble_evt, sizeof(ble_evt_t));
		memcpy(write_evt_data + offset, data, len);

		ble_hids->hid_control_point_handles.value_handle = CONTROL_POINT_HANDLE;
		ble_hids_on_ble_evt((ble_evt_t *)write_evt_data, ble_hids);
	break;

	case ON_WRITE_PROTOCOLS_MODE:
		/* Protocols mode. */
		evt_write->handle = PROTOCOL_MODE_HANDLE;
		evt_write->len = len;

		memcpy(write_evt_data, &ble_evt, sizeof(ble_evt_t));
		memcpy(write_evt_data + offset, data, len);

		ble_hids->protocol_mode_handles.value_handle = PROTOCOL_MODE_HANDLE;
		ble_hids_on_ble_evt((ble_evt_t *)write_evt_data, ble_hids);
		break;

	case ON_KB_INP_CCCD:
		evt_write->handle = KB_INPUT_CCCD_HANDLE;
		evt_write->len = len;

		ble_hids->boot_kb_inp_rep_handles.cccd_handle = KB_INPUT_CCCD_HANDLE;
		ble_hids_on_ble_evt(&ble_evt, ble_hids);
		break;

	case ON_KB_INP_VALUE:
		evt_write->handle = KB_INPUT_REPORT_HANDLE;
		evt_write->len = len;

		memcpy(write_evt_data, &ble_evt, sizeof(ble_evt_t));
		memcpy(write_evt_data + offset, data, len);

		ble_hids->boot_kb_inp_rep_handles.value_handle = KB_INPUT_REPORT_HANDLE;
		ble_hids_on_ble_evt((ble_evt_t *)write_evt_data, ble_hids);
		break;

	case ON_KB_OUTP_VALUE:
		evt_write->handle = KB_OUTPUT_REPORT_HANDLE;
		evt_write->len = len;

		memcpy(write_evt_data, &ble_evt, sizeof(ble_evt_t));
		memcpy(write_evt_data + offset, data, len);

		ble_hids->boot_kb_outp_rep_handles.value_handle = KB_OUTPUT_REPORT_HANDLE;

		ble_hids_on_ble_evt((ble_evt_t *)write_evt_data, ble_hids);
		break;

	case ON_MOUSE_INP_CCCD:
		evt_write->handle = MOUSE_INPUT_REPORT_CCCD_HANDLE;
		evt_write->len = len;

		ble_hids->boot_mouse_inp_rep_handles.cccd_handle = MOUSE_INPUT_REPORT_CCCD_HANDLE;

		ble_hids_on_ble_evt(&ble_evt, ble_hids);
		break;

	case ON_MOUSE_INP_VALUE:
		evt_write->handle = MOUSE_INPUT_REPORT_HANDLE;
		evt_write->len = len;

		memcpy(write_evt_data, &ble_evt, sizeof(ble_evt_t));
		memcpy(write_evt_data + offset, data, len);

		memcpy(evt_write->data, data, len);
		ble_hids->boot_mouse_inp_rep_handles.value_handle = MOUSE_INPUT_REPORT_HANDLE;

		ble_hids_on_ble_evt((ble_evt_t *)write_evt_data, ble_hids);
		break;

	case ON_INPUT_REPORT_CCCD:
		evt_write->handle = INPUT_REPORT_CCCD_HANDLE;
		evt_write->len = len;

		memcpy(write_evt_data, &ble_evt, sizeof(ble_evt_t));
		memcpy(write_evt_data + offset, data, len);

		ble_hids->inp_rep_array[0].char_handles.cccd_handle = INPUT_REPORT_CCCD_HANDLE;

		ble_hids_on_ble_evt((ble_evt_t *)write_evt_data, ble_hids);
		break;

	case ON_REP_VALUE_IDENTIFY:
		evt_write->handle = REPORT_VALUE_HANDLE;
		evt_write->len = len;

		memcpy(write_evt_data, &ble_evt, sizeof(ble_evt_t));
		memcpy(write_evt_data + offset, data, len);

		ble_hids->input_report_count = 2;
		ble_hids->output_report_count = 2;
		ble_hids->feature_report_count = 3;

		input_report[0].len = INPUT_REPORT_1_LEN;
		input_report[1].len = INPUT_REPORT_2_LEN;

		output_report[0].len = OUTPUT_REPORT_1_LEN;
		output_report[1].len = OUTPUT_REPORT_2_LEN;

		feature_report[0].len = FEATURE_REPORT_1_LEN;
		feature_report[1].len = FEATURE_REPORT_2_LEN;
		feature_report[2].len = FEATURE_REPORT_3_LEN;

		ble_hids->feature_rep_array[2].char_handles.value_handle = REPORT_VALUE_HANDLE;

		ble_hids_on_ble_evt((ble_evt_t *)write_evt_data, ble_hids);
		break;

	default:
		return;
	}
}

/* Emulate BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST event. */
static void emulate_ble_rw_authorize_evt(struct ble_hids *ble_hids,
					 enum on_rw_auth_evt auth_evt_type)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST,
		.evt.gatts_evt.conn_handle = CONN_HANDLE,
	};
	ble_gatts_evt_rw_authorize_request_t *evt_rw_auth;

	evt_rw_auth = &ble_evt.evt.gatts_evt.params.authorize_request;

	switch (auth_evt_type) {
	case ON_OTHER_TYPE:
		evt_rw_auth->type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;

		ble_hids_on_ble_evt(&ble_evt, ble_hids);
		break;

	case ON_PROTOCOL_MODE_RW_AUTH:
		evt_rw_auth->type = BLE_GATTS_AUTHORIZE_TYPE_READ;
		evt_rw_auth->request.read.handle = PROTOCOL_MODE_HANDLE;
		ble_hids->protocol_mode_handles.value_handle = PROTOCOL_MODE_HANDLE;

		ble_hids_on_ble_evt(&ble_evt, ble_hids);
		break;

	case ON_BOOT_KB_INP_REP_RW_AUTH:
		evt_rw_auth->type = BLE_GATTS_AUTHORIZE_TYPE_READ;
		evt_rw_auth->request.read.handle = KB_INPUT_REPORT_HANDLE;
		ble_hids->boot_kb_inp_rep_handles.value_handle = KB_INPUT_REPORT_HANDLE;

		ble_hids_on_ble_evt(&ble_evt, ble_hids);
		break;

	case ON_BOOT_KB_OUTP_REP_RW_AUTH:
		evt_rw_auth->type = BLE_GATTS_AUTHORIZE_TYPE_READ;
		evt_rw_auth->request.read.handle = KB_OUTPUT_REPORT_HANDLE;
		ble_hids->boot_kb_outp_rep_handles.value_handle = KB_OUTPUT_REPORT_HANDLE;

		ble_hids_on_ble_evt(&ble_evt, ble_hids);
		break;

	case ON_BOOT_MOUSE_INP_REP_RW_AUTH:
		evt_rw_auth->type = BLE_GATTS_AUTHORIZE_TYPE_READ;
		evt_rw_auth->request.read.handle = MOUSE_INPUT_REPORT_HANDLE;
		ble_hids->boot_mouse_inp_rep_handles.value_handle = MOUSE_INPUT_REPORT_HANDLE;

		ble_hids_on_ble_evt(&ble_evt, ble_hids);
		break;

	case ON_REP_VALUE_IDEN_RW_AUTH:
		evt_rw_auth->type = BLE_GATTS_AUTHORIZE_TYPE_READ;
		evt_rw_auth->request.read.handle = REPORT_VALUE_HANDLE;

		input_report[0].len = INPUT_REPORT_1_LEN;
		input_report[1].len = INPUT_REPORT_2_LEN;

		output_report[0].len = OUTPUT_REPORT_1_LEN;
		output_report[1].len = OUTPUT_REPORT_2_LEN;

		feature_report[0].len = FEATURE_REPORT_1_LEN;
		feature_report[1].len = FEATURE_REPORT_2_LEN;
		feature_report[2].len = FEATURE_REPORT_3_LEN;

		ble_hids->outp_rep_array[1].char_handles.value_handle = REPORT_VALUE_HANDLE;
		ble_hids_on_ble_evt(&ble_evt, ble_hids);
		break;

	default:
		return;
	}
}

/* Test ble_hids_init with NULL pointer. */
void test_ble_hids_init_null(void)
{
	uint32_t err;
	struct ble_hids_config ble_hids_init_obj = {0};
	struct ble_hids ble_hids = {
		.link_ctx_storage = {
			.max_links_cnt = CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT,
			.link_ctx_size = sizeof(uint32_t) * BYTES_TO_WORDS(BLE_HIDS_LINK_CTX_SIZE),
		},
	};

	err = ble_hids_init(NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	err = ble_hids_init(&ble_hids, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	err = ble_hids_init(NULL, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

/* Test to much count of input report, output report feature report and all of them. */
void test_struct_ble_hids_config_too_much_rep(void)
{
	uint32_t err;
	struct ble_hids_config ble_hids_init_obj = {0};
	struct ble_hids ble_hids = {
		.link_ctx_storage = {
			.max_links_cnt = CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT,
			.link_ctx_size = sizeof(uint32_t) * BYTES_TO_WORDS(BLE_HIDS_LINK_CTX_SIZE),
		},
	};

	/* Test to much input report characteristic. */
	ble_hids_init_obj.input_report_count = CONFIG_BLE_HIDS_INPUT_REPORT_MAX_NUM + 1;

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, err);

	/* Test to much output report characteristic. */
	ble_hids_init_obj.input_report_count = 0;
	ble_hids_init_obj.output_report_count = CONFIG_BLE_HIDS_OUTPUT_REPORT_MAX_NUM + 1;

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, err);

	/* Test to much feature report characteristic. */
	ble_hids_init_obj.output_report_count = 0;
	ble_hids_init_obj.feature_report_count = CONFIG_BLE_HIDS_FEATURE_REPORT_MAX_NUM + 1;

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, err);

	/* Test to much all. */
	ble_hids_init_obj.input_report_count = CONFIG_BLE_HIDS_INPUT_REPORT_MAX_NUM + 1;
	ble_hids_init_obj.output_report_count = CONFIG_BLE_HIDS_OUTPUT_REPORT_MAX_NUM + 1;
	ble_hids_init_obj.feature_report_count = CONFIG_BLE_HIDS_FEATURE_REPORT_MAX_NUM + 1;

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, err);
}


/* Test ble_hids_init with keyboard. */
void test_ble_hids_init_kb_no_mem(void)
{
	uint32_t err;
	uint8_t report_madata[] = REPORT_MA_DATA_1;
	ble_uuid_t ext_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = 1,
	};
	struct ble_hids_config ble_hids_init_obj = {
		.evt_handler = on_hids_evt,
		.input_report_count = 1,
		.input_report = input_report,
		.output_report_count = 1,
		.output_report = output_report,
		.feature_report_count = 1,
		.feature_report = feature_report,
		.report_map = {
			.len = sizeof(report_madata),
			.data = report_madata,
			.ext_rep_ref_num = 1,
			.ext_rep_ref = &ext_uuid,
		},
		.hid_information = {
			.bcd_hid = BASE_USB_HID_SPEC_VERSION,
			.b_country_code = 0,
			.flags = {
				.remote_wake = 1,
				.normally_connectable = 1,
			},
		},

		.included_services_count = 0,
		.included_services_array = NULL,
		.sec_mode = BLE_HIDS_CONFIG_SEC_MODE_DEFAULT_KEYBOARD,
	};
	struct ble_hids ble_hids = {
		.link_ctx_storage = {
			.max_links_cnt = CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT,
			.link_ctx_size = sizeof(uint32_t) * BYTES_TO_WORDS(BLE_HIDS_LINK_CTX_SIZE),
		},
	};
	struct ble_hids_report_config *p_input_report;
	struct ble_hids_report_config *p_output_report;
	struct ble_hids_report_config *p_feature_report;

	/* Initialize HID Service */
	p_input_report = &input_report[INPUT_REPORT_KEYS_INDEX];
	p_input_report->len = INPUT_REPORT_KEYS_MAX_LEN;
	p_input_report->report_id = INPUT_REP_REF_ID;
	p_input_report->report_type = BLE_HIDS_REPORT_TYPE_INPUT;

	p_input_report->sec_mode.read = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	p_input_report->sec_mode.write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	p_input_report->sec_mode.cccd_write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;

	p_output_report = &output_report[OUTPUT_REPORT_INDEX];
	p_output_report->len = OUTPUT_REPORT_MAX_LEN;
	p_output_report->report_id = OUTPUT_REP_REF_ID;
	p_output_report->report_type = BLE_HIDS_REPORT_TYPE_OUTPUT;

	p_output_report->sec_mode.write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	p_output_report->sec_mode.read = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;

	p_feature_report = &feature_report[FEATURE_REPORT_INDEX];
	p_feature_report->len = FEATURE_REPORT_MAX_LEN;
	p_feature_report->report_id = FEATURE_REP_REF_ID;
	p_feature_report->report_type = BLE_HIDS_REPORT_TYPE_FEATURE;

	p_feature_report->sec_mode.read = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	p_feature_report->sec_mode.write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	p_feature_report->sec_mode.cccd_write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle,
							 NRF_ERROR_NO_MEM);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.protocol_mode_handles,
		NRF_ERROR_NO_MEM);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.protocol_mode_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.inp_rep_array[0].char_handles,
		NRF_ERROR_NO_MEM);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.protocol_mode_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.inp_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.inp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.inp_rep_array[0].ref_handle, NRF_ERROR_NO_MEM);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.protocol_mode_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.inp_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.inp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.inp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.feature_rep_array[0].char_handles,
		NRF_ERROR_NO_MEM);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.protocol_mode_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.inp_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.inp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.inp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.feature_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.outp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.outp_rep_array[0].ref_handle, NRF_ERROR_NO_MEM);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.protocol_mode_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.inp_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.inp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.inp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.feature_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.outp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.outp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.rep_map_handles, NRF_ERROR_NO_MEM);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.protocol_mode_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.inp_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.inp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.inp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.feature_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.outp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.outp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.rep_map_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.feature_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.feature_rep_array[0].ref_handle, NRF_ERROR_NO_MEM);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.protocol_mode_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.inp_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.inp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.inp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.feature_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.outp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.outp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.rep_map_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.feature_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.feature_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.boot_kb_inp_rep_handles,
								NRF_ERROR_NO_MEM);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.protocol_mode_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.inp_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.inp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.inp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.feature_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.outp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.outp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.rep_map_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.feature_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.feature_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.boot_kb_inp_rep_handles,
								NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_uuid_encode_ExpectAndReturn(&ext_uuid, NULL, NULL, NRF_ERROR_INVALID_ADDR);
	__cmock_sd_ble_uuid_encode_IgnoreArg_p_uuid_le_len();
	__cmock_sd_ble_uuid_encode_IgnoreArg_p_uuid_le();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_ADDR, err);

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.protocol_mode_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.inp_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.inp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.inp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.feature_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.outp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.outp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.rep_map_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.feature_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.feature_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.boot_kb_inp_rep_handles,
								NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_uuid_encode_ExpectAndReturn(&ext_uuid, NULL, NULL, NRF_SUCCESS);
	__cmock_sd_ble_uuid_encode_IgnoreArg_p_uuid_le_len();
	__cmock_sd_ble_uuid_encode_IgnoreArg_p_uuid_le();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.rep_map_handles.value_handle, NULL, &ble_hids.rep_map_ext_rep_ref_handle,
		NRF_ERROR_NO_MEM);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.protocol_mode_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.inp_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.inp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.inp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.feature_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.outp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.outp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.rep_map_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.feature_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.feature_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.boot_kb_inp_rep_handles,
								NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_uuid_encode_ExpectAndReturn(&ext_uuid, NULL, NULL, NRF_SUCCESS);
	__cmock_sd_ble_uuid_encode_IgnoreArg_p_uuid_le_len();
	__cmock_sd_ble_uuid_encode_IgnoreArg_p_uuid_le();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.rep_map_handles.value_handle, NULL, &ble_hids.rep_map_ext_rep_ref_handle,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.boot_kb_outp_rep_handles,
								NRF_ERROR_NO_MEM);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.protocol_mode_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.inp_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.inp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.inp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.feature_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.outp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.outp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.rep_map_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.feature_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.feature_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.boot_kb_inp_rep_handles,
								NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_uuid_encode_ExpectAndReturn(&ext_uuid, NULL, NULL, NRF_SUCCESS);
	__cmock_sd_ble_uuid_encode_IgnoreArg_p_uuid_le_len();
	__cmock_sd_ble_uuid_encode_IgnoreArg_p_uuid_le();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.rep_map_handles.value_handle, NULL, &ble_hids.rep_map_ext_rep_ref_handle,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.boot_kb_outp_rep_handles,
								NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.hid_information_handles,
								NRF_ERROR_NO_MEM);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.protocol_mode_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.inp_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.inp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.inp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.feature_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.outp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.outp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.rep_map_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.feature_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.feature_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.boot_kb_inp_rep_handles,
								NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_uuid_encode_ExpectAndReturn(&ext_uuid, NULL, NULL, NRF_SUCCESS);
	__cmock_sd_ble_uuid_encode_IgnoreArg_p_uuid_le_len();
	__cmock_sd_ble_uuid_encode_IgnoreArg_p_uuid_le();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.rep_map_handles.value_handle, NULL, &ble_hids.rep_map_ext_rep_ref_handle,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.boot_kb_outp_rep_handles,
								NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.hid_information_handles,
								NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.hid_control_point_handles,
								NRF_ERROR_NO_MEM);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);
}

/* Test ble_hids_init with keyboard. */
void test_ble_hids_init_kb_correct(void)
{
	uint32_t err;
	uint8_t report_madata[] = REPORT_MA_DATA_1;
	ble_uuid_t ext_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = 1,
	};
	struct ble_hids_config ble_hids_init_obj = {
		.evt_handler = on_hids_evt,
		.input_report_count = 1,
		.input_report = input_report,
		.output_report_count = 1,
		.output_report = output_report,
		.feature_report_count = 1,
		.feature_report = feature_report,
		.report_map = {
			.len = sizeof(report_madata),
			.data = report_madata,
			.ext_rep_ref_num = 1,
			.ext_rep_ref = &ext_uuid,
		},
		.hid_information = {
			.bcd_hid = BASE_USB_HID_SPEC_VERSION,
			.b_country_code = 0,
			.flags = {
				.remote_wake = 1,
				.normally_connectable = 1,
			},
		},

		.included_services_count = 0,
		.included_services_array = NULL,
		.sec_mode = BLE_HIDS_CONFIG_SEC_MODE_DEFAULT_KEYBOARD,
	};
	struct ble_hids ble_hids = {
		.link_ctx_storage = {
			.max_links_cnt = CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT,
			.link_ctx_size = sizeof(uint32_t) * BYTES_TO_WORDS(BLE_HIDS_LINK_CTX_SIZE),
		},
	};
	struct ble_hids_report_config *p_input_report;
	struct ble_hids_report_config *p_output_report;
	struct ble_hids_report_config *p_feature_report;

	/* Initialize HID Service */
	p_input_report = &input_report[INPUT_REPORT_KEYS_INDEX];
	p_input_report->len = INPUT_REPORT_KEYS_MAX_LEN;
	p_input_report->report_id = INPUT_REP_REF_ID;
	p_input_report->report_type = BLE_HIDS_REPORT_TYPE_INPUT;

	p_input_report->sec_mode.read = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	p_input_report->sec_mode.write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	p_input_report->sec_mode.cccd_write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;

	p_output_report = &output_report[OUTPUT_REPORT_INDEX];
	p_output_report->len = OUTPUT_REPORT_MAX_LEN;
	p_output_report->report_id = OUTPUT_REP_REF_ID;
	p_output_report->report_type = BLE_HIDS_REPORT_TYPE_OUTPUT;

	p_output_report->sec_mode.write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	p_output_report->sec_mode.read = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;

	p_feature_report = &feature_report[FEATURE_REPORT_INDEX];
	p_feature_report->len = FEATURE_REPORT_MAX_LEN;
	p_feature_report->report_id = FEATURE_REP_REF_ID;
	p_feature_report->report_type = BLE_HIDS_REPORT_TYPE_FEATURE;

	p_feature_report->sec_mode.read = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	p_feature_report->sec_mode.write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;
	p_feature_report->sec_mode.cccd_write = BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM;

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY, NULL,
							 &ble_hids.service_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.protocol_mode_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.inp_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.inp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.inp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.feature_rep_array[0].char_handles,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.outp_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.outp_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(
		ble_hids.service_handle, NULL, NULL, &ble_hids.rep_map_handles, NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.feature_rep_array[0].char_handles.value_handle, NULL,
		&ble_hids.feature_rep_array[0].ref_handle, NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.boot_kb_inp_rep_handles,
								NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_uuid_encode_ExpectAndReturn(&ext_uuid, NULL, NULL, NRF_SUCCESS);
	__cmock_sd_ble_uuid_encode_IgnoreArg_p_uuid_le_len();
	__cmock_sd_ble_uuid_encode_IgnoreArg_p_uuid_le();

	__cmock_sd_ble_gatts_descriptor_add_ExpectAndReturn(
		ble_hids.rep_map_handles.value_handle, NULL, &ble_hids.rep_map_ext_rep_ref_handle,
		NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_IgnoreArg_p_attr();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.boot_kb_outp_rep_handles,
								NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.hid_information_handles,
								NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	__cmock_sd_ble_gatts_characteristic_add_ExpectAndReturn(ble_hids.service_handle, NULL, NULL,
								&ble_hids.hid_control_point_handles,
								NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_char_md();
	__cmock_sd_ble_gatts_characteristic_add_IgnoreArg_p_attr_char_value();

	err = ble_hids_init(&ble_hids, &ble_hids_init_obj);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
	TEST_ASSERT_NOT_NULL(ble_hids.inp_rep_init_array);
	TEST_ASSERT_NOT_NULL(ble_hids.outp_rep_init_array);
	TEST_ASSERT_NOT_NULL(ble_hids.feature_rep_init_array);
	TEST_ASSERT_EQUAL_UINT8(ble_hids_init_obj.input_report_count, ble_hids.input_report_count);
	TEST_ASSERT_EQUAL_UINT8(ble_hids_init_obj.output_report_count,
				ble_hids.output_report_count);
	TEST_ASSERT_EQUAL_UINT8(ble_hids_init_obj.feature_report_count,
				ble_hids.feature_report_count);
}

void test_ble_hids_inp_rep_send_error_null(void)
{
	uint32_t err;
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	struct ble_hids_input_report report;

	input_report[0].len = INPUT_REPORT_1_LEN;
	input_report[1].len = INPUT_REPORT_2_LEN;
	/* Test NULL */
	err = ble_hids_inp_rep_send(NULL, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	err = ble_hids_inp_rep_send(&ble_hids, CONN_HANDLE, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_ble_hids_inp_rep_send_error_invalid_param(void)
{
	uint32_t err;
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0xC0, 0xC1};
	struct ble_hids_input_report report;

	input_report[0].len = INPUT_REPORT_1_LEN;
	input_report[1].len = INPUT_REPORT_2_LEN;
	/* Incorrect characteristic index. */

	report.data = data;
	report.len = INPUT_REPORT_2_LEN;
	report.report_index = 4;

	err = ble_hids_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, err);
}

void test_ble_hids_inp_rep_send_error_invalid_state(void)
{
	uint32_t err;
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0xC0, 0xC1};
	struct ble_hids_input_report report;

	input_report[0].len = INPUT_REPORT_1_LEN;
	input_report[1].len = INPUT_REPORT_2_LEN;

	/* Invalid connection handle. */
	report.data = data;
	report.len = INPUT_REPORT_2_LEN;
	report.report_index = 1;
	err = ble_hids_inp_rep_send(&ble_hids, BLE_CONN_HANDLE_INVALID, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, err);
}

void test_ble_hids_inp_rep_send_error_not_found(void)
{
	uint32_t err;
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0xC0, 0xC1};
	struct ble_hids_input_report report;

	input_report[0].len = INPUT_REPORT_1_LEN;
	input_report[1].len = INPUT_REPORT_2_LEN;

	/* ctx_link_get_error */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	report.data = data, report.len = INPUT_REPORT_2_LEN, report.report_index = 1,
	err = ble_hids_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, err);
}

void test_ble_hids_inp_rep_send_error_data_size(void)
{
	uint32_t err;
	uint16_t len = 0;
	void *host_redata;
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0xC0, 0xC1};
	struct ble_hids_input_report report;

	input_report[0].len = INPUT_REPORT_1_LEN;
	input_report[1].len = INPUT_REPORT_2_LEN;

	hvx_params.p_len = &len;
	host_redata = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
				 CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	/* Data length bigger than input report max length. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	report.data = data, report.len = INPUT_REPORT_2_LEN + 1, report.report_index = 1,
	err = ble_hids_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, err);

	/* Actual bytes written count is other than data length. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_hvx_Stub(stub_sd_ble_gatts_hvx_different_len);

	report.data = data;
	report.len = INPUT_REPORT_2_LEN;
	report.report_index = 1;
	err = ble_hids_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, err);
}

void test_ble_hids_inp_rep_send_error_invalid_addr(void)
{
	uint32_t err;
	uint16_t len = 0;
	void *host_redata;
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0xC0, 0xC1};
	struct ble_hids_input_report report;

	input_report[0].len = INPUT_REPORT_1_LEN;
	input_report[1].len = INPUT_REPORT_2_LEN;

	hvx_params.p_len = &len;
	host_redata = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
				 CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	/* gatts_hvx error. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(CONN_HANDLE, NULL, NRF_ERROR_INVALID_ADDR);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	report.data = data, report.len = INPUT_REPORT_2_LEN, report.report_index = 1,
	err = ble_hids_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_ADDR, err);
}
void test_ble_hids_inp_rep_send(void)
{
	uint32_t err;
	uint16_t len = 0;
	uint8_t offset =
		sizeof(struct ble_hids_client_context) + BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE +
		BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE + BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE;
	void *host_redata;
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0xC0, 0xC1};
	struct ble_hids_input_report report;
	uint8_t *exp_data;

	input_report[0].len = INPUT_REPORT_1_LEN;
	input_report[1].len = INPUT_REPORT_2_LEN;

	hvx_params.p_len = &len;
	host_redata = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
				 CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	report.data = data;
	report.len = INPUT_REPORT_2_LEN;
	report.report_index = 1, err = ble_hids_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	exp_data = (uint8_t *)host_redata + offset + INPUT_REPORT_1_LEN;
	TEST_ASSERT_EQUAL_MEMORY(data, exp_data, INPUT_REPORT_2_LEN);
}

void test_ble_hids_boot_kb_inp_rep_send_error_null(void)
{
	uint32_t err;
	struct ble_hids_boot_keyboard_input_report report;

	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7};

	report.data = data;
	report.len = BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE;
	err = ble_hids_boot_kb_inp_rep_send(NULL, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	report.data = data;
	report.len = BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE;
	err = ble_hids_boot_kb_inp_rep_send(&ble_hids, CONN_HANDLE, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_ble_hids_boot_kb_inp_rep_send_error_invalid_state(void)
{
	uint32_t err;
	struct ble_hids_boot_keyboard_input_report report;

	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7};

	/* Test invalid connection handle. */
	report.data = data;
	report.len = BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE;
	err = ble_hids_boot_kb_inp_rep_send(&ble_hids, BLE_CONN_HANDLE_INVALID, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, err);
}

void test_ble_hids_boot_kb_inp_rep_send_error_not_found(void)
{
	uint32_t err;
	struct ble_hids_boot_keyboard_input_report report;

	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7};

	/* ctx_link_get_error */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	report.data = data;
	report.len = BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE;
	err = ble_hids_boot_kb_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, err);
}

void test_ble_hids_boot_kb_inp_rep_send_error_invalid_attr_handle(void)
{
	uint32_t err;
	uint16_t len = 0;
	void *host_redata;
	ble_gatts_hvx_params_t hvx_params;
	struct ble_hids_boot_keyboard_input_report report;

	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7};

	hvx_params.p_len = &len;
	host_redata = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
				 CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	/* gatts_hvx error */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(CONN_HANDLE, NULL, BLE_ERROR_INVALID_ATTR_HANDLE);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	report.data = data;
	report.len = BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE;
	err = ble_hids_boot_kb_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(BLE_ERROR_INVALID_ATTR_HANDLE, err);
}

void test_ble_hids_boot_kb_inp_rep_send_error_data_size(void)
{
	uint32_t err;
	uint16_t len = 0;
	void *host_redata;
	ble_gatts_hvx_params_t hvx_params;
	struct ble_hids_boot_keyboard_input_report report;

	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7};

	hvx_params.p_len = &len;
	host_redata = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
				 CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(CONN_HANDLE, NULL, NRF_ERROR_DATA_SIZE);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	report.data = data;
	report.len = INPUT_REPORT_KEYS_MAX_LEN;
	err = ble_hids_boot_kb_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, err);

	/* Actual bytes written count is other than data length. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_hvx_Stub(stub_sd_ble_gatts_hvx_different_len);

	report.data = data;
	report.len = INPUT_REPORT_KEYS_MAX_LEN;
	err = ble_hids_boot_kb_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, err);
}

void test_ble_hids_boot_kb_inp_rep_send(void)
{
	uint32_t err;
	uint16_t len = 0;
	void *host_redata;
	uint8_t offset = sizeof(struct ble_hids_client_context);
	ble_gatts_hvx_params_t hvx_params;
	struct ble_hids_boot_keyboard_input_report report;

	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7};

	hvx_params.p_len = &len;
	host_redata = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
				 CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	/* Test success */
	len = INPUT_REPORT_KEYS_MAX_LEN;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	report.data = data;
	report.len = BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE;
	err = ble_hids_boot_kb_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	uint8_t *stored_data = (uint8_t *)host_redata + offset;

	TEST_ASSERT_EQUAL_MEMORY(data, stored_data, sizeof(data));
}

void test_ble_hids_boot_mouse_inp_rep_send_error_null(void)
{
	uint32_t err;
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	struct ble_hids_boot_mouse_input_report report;

	/* Test NULL wihtout optional data. */
	report.buttons = 0x00;
	report.delta_x = DEFAULT_X_DELTA;
	report.delta_y = DEFAULT_Y_DELTA;
	report.optional_data_len = 0;
	err = ble_hids_boot_mouse_inp_rep_send(NULL, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	err = ble_hids_boot_mouse_inp_rep_send(&ble_hids, CONN_HANDLE, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_ble_hids_boot_mouse_inp_rep_send_error_invalid_state(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint32_t err;
	struct ble_hids_boot_mouse_input_report report;

	/* Test invalid connection handle. */
	report.buttons = 0x00;
	report.delta_x = DEFAULT_X_DELTA;
	report.delta_y = DEFAULT_Y_DELTA;
	report.optional_data_len = 0;
	err = ble_hids_boot_mouse_inp_rep_send(&ble_hids, BLE_CONN_HANDLE_INVALID, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, err);
}

void test_ble_hids_boot_mouse_inp_rep_send_error_data_size(void)
{
	uint32_t err;
	uint16_t len = 0;
	uint16_t optional_data_len = BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE;
	ble_gatts_hvx_params_t hvx_params;
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	struct ble_hids_boot_mouse_input_report report;

	/* Test to long notification data. */
	report.buttons = 0x00;
	report.delta_x = DEFAULT_X_DELTA;
	report.delta_y = DEFAULT_Y_DELTA;
	report.optional_data_len = optional_data_len;
	err = ble_hids_boot_mouse_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, err);

	/* Test. Actual bytes written count is other than data length. */
	hvx_params.p_len = &len;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_hvx_Stub(stub_sd_ble_gatts_hvx_different_len);

	report.buttons = 0x00;
	report.delta_x = DEFAULT_X_DELTA;
	report.delta_y = DEFAULT_Y_DELTA;
	report.optional_data_len = 0;
	err = ble_hids_boot_mouse_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, err);
}

void test_ble_hids_boot_mouse_inp_rep_send_error_not_found(void)
{
	uint32_t err;
	void *host_redata;
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	struct ble_hids_boot_mouse_input_report report;

	host_redata = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
				 CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	/* Test link_ctx_get error. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	report.buttons = 0x00;
	report.delta_x = DEFAULT_X_DELTA;
	report.delta_y = DEFAULT_Y_DELTA;
	report.optional_data_len = 0;
	err = ble_hids_boot_mouse_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, err);
}

void test_ble_hids_boot_mouse_inp_rep_send_error_forbidden(void)
{
	uint32_t err;
	void *host_redata;
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	struct ble_hids_boot_mouse_input_report report;

	host_redata = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
				 CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	/* Test gatts_hvx error. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(CONN_HANDLE, NULL, NRF_ERROR_FORBIDDEN);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	report.buttons = 0x00;
	report.delta_x = DEFAULT_X_DELTA;
	report.delta_y = DEFAULT_Y_DELTA;
	report.optional_data_len = 0;
	err = ble_hids_boot_mouse_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_ERROR_FORBIDDEN, err);
}

void test_ble_hids_boot_mouse_inp_rep_send(void)
{
	uint32_t err;
	uint16_t len = 0;
	uint8_t optional_data[] = {0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7};
	uint16_t optional_data_len = BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE;
	void *host_redata;
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data_exp[BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE];
	uint8_t *data;
	uint8_t offset = sizeof(struct ble_hids_client_context) +
			 BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE +
			 BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE;
	struct ble_hids_boot_mouse_input_report report;

	host_redata = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
				 CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	len = BOOT_MOUSE_INPUT_REPORT_MIN_LEN;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	report.buttons = 0x00;
	report.delta_x = DEFAULT_X_DELTA;
	report.delta_y = DEFAULT_Y_DELTA;
	report.optional_data_len = 0;
	err = ble_hids_boot_mouse_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	/* Test success with max length of optional data. */
	optional_data_len =
		BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE - BOOT_MOUSE_INPUT_REPORT_MIN_LEN;
	len = BOOT_MOUSE_INPUT_REPORT_MIN_LEN + optional_data_len;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	report.buttons = 0x00;
	report.delta_x = DEFAULT_X_DELTA;
	report.delta_y = DEFAULT_Y_DELTA;
	memcpy(report.optional_data, optional_data, optional_data_len);
	report.optional_data_len = optional_data_len;
	err = ble_hids_boot_mouse_inp_rep_send(&ble_hids, CONN_HANDLE, &report);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	data_exp[0] = 0;
	data_exp[1] = DEFAULT_X_DELTA;
	data_exp[2] = DEFAULT_Y_DELTA;
	memcpy(&data_exp[3], optional_data, optional_data_len);

	data = (uint8_t *)host_redata + offset;
	TEST_ASSERT_EQUAL_MEMORY(data_exp, data, len);
}

void test_ble_hids_outp_rep_get_error_null(void)
{
	uint32_t err;
	uint16_t len = 1;
	uint8_t report_val[OUTPUT_REPORT_1_LEN + OUTPUT_REPORT_2_LEN];
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	output_report[0].len = OUTPUT_REPORT_1_LEN;
	output_report[1].len = OUTPUT_REPORT_2_LEN;
	input_report[0].len = INPUT_REPORT_1_LEN;
	input_report[1].len = INPUT_REPORT_2_LEN;

	/* Test NULL. */
	err = ble_hids_outp_rep_get(NULL, 0, len, 0, CONN_HANDLE, report_val);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	err = ble_hids_outp_rep_get(&ble_hids, 0, len, 0, CONN_HANDLE, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_ble_hids_outp_rep_get_error_invalid_param(void)
{
	uint32_t err;
	uint16_t len = 1;
	uint8_t report_val[OUTPUT_REPORT_1_LEN + OUTPUT_REPORT_2_LEN];
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	output_report[0].len = OUTPUT_REPORT_1_LEN;
	output_report[1].len = OUTPUT_REPORT_2_LEN;
	input_report[0].len = INPUT_REPORT_1_LEN;
	input_report[1].len = INPUT_REPORT_2_LEN;

	/* Rep index greater than output report count. */
	err = ble_hids_outp_rep_get(&ble_hids, 3, len, 0, CONN_HANDLE, report_val);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, err);
}

void test_ble_hids_outp_rep_get_error_no_mem(void)
{
	uint32_t err;
	uint16_t len = 1;
	uint8_t report_val[OUTPUT_REPORT_1_LEN + OUTPUT_REPORT_2_LEN];
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	output_report[0].len = OUTPUT_REPORT_1_LEN;
	output_report[1].len = OUTPUT_REPORT_2_LEN;
	input_report[0].len = INPUT_REPORT_1_LEN;
	input_report[1].len = INPUT_REPORT_2_LEN;

	/* Test link_ctx_get error. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 2);

	err = ble_hids_outp_rep_get(&ble_hids, 0, len, 0, CONN_HANDLE, report_val);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);
}

void test_ble_hids_outp_rep_get_error_not_found(void)
{
	uint32_t err;
	uint16_t len = 1;
	uint8_t report_val[OUTPUT_REPORT_1_LEN + OUTPUT_REPORT_2_LEN];
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	output_report[0].len = OUTPUT_REPORT_1_LEN;
	output_report[1].len = OUTPUT_REPORT_2_LEN;
	input_report[0].len = INPUT_REPORT_1_LEN;
	input_report[1].len = INPUT_REPORT_2_LEN;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	err = ble_hids_outp_rep_get(&ble_hids, 0, len, 0, CONN_HANDLE, report_val);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, err);
}

void test_ble_hids_outp_rep_get_error_invalid_length(void)
{
	uint32_t err;
	uint8_t offset = 1;
	uint8_t report_val[OUTPUT_REPORT_1_LEN + OUTPUT_REPORT_2_LEN];
	void *host_redata;
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	output_report[0].len = OUTPUT_REPORT_1_LEN;
	output_report[1].len = OUTPUT_REPORT_2_LEN;
	input_report[0].len = INPUT_REPORT_1_LEN;
	input_report[1].len = INPUT_REPORT_2_LEN;

	host_redata = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
				 CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	/* Test invalid data length. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	err = ble_hids_outp_rep_get(&ble_hids, 0, OUTPUT_REPORT_1_LEN + 1, offset, CONN_HANDLE,
					 report_val);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_LENGTH, err);
}

void test_ble_hids_outp_rep_get(void)
{
	uint32_t err;
	uint8_t data[] = {0xFA};
	uint8_t report_val[OUTPUT_REPORT_1_LEN + OUTPUT_REPORT_2_LEN];
	void *host_redata;
	uint8_t data_offset =
		sizeof(struct ble_hids_client_context) + BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE +
		BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE +
		BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE + INPUT_REPORT_1_LEN + INPUT_REPORT_2_LEN;
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	output_report[0].len = OUTPUT_REPORT_1_LEN;
	output_report[1].len = OUTPUT_REPORT_2_LEN;
	input_report[0].len = INPUT_REPORT_1_LEN;
	input_report[1].len = INPUT_REPORT_2_LEN;

	host_redata = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
				 CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	/* Test 0 data length and 0 offset. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	err = ble_hids_outp_rep_get(&ble_hids, 0, 0, 0, CONN_HANDLE, report_val);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	/* Test success. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	uint8_t *exp_data = (uint8_t *)host_redata + data_offset;

	memcpy(exp_data, data, OUTPUT_REPORT_1_LEN);

	err = ble_hids_outp_rep_get(&ble_hids, 0, OUTPUT_REPORT_1_LEN, 0, CONN_HANDLE,
					 report_val);

	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
	TEST_ASSERT_EQUAL_MEMORY(data, report_val, OUTPUT_REPORT_1_LEN);

	/* Test two different output report with input report. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	/* Get report value from second output report. */
	err = ble_hids_outp_rep_get(&ble_hids, 1, 1, 0, CONN_HANDLE, report_val);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_on_ble_connected_evt_error_not_found(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	void *p_client;

	ble_hids.protocol_mode_handles.value_handle = PROTOCOL_VALUE_HANDLE;
	p_client = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			    CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	/* Test link_ctx_error. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	error_expected = true;

	emulate_ble_connected_evt(&ble_hids);

	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, error);
	TEST_ASSERT_TRUE(error_requested);
}

void test_on_ble_connected_evt_error_invalid_addr(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	void *p_client;

	ble_hids.protocol_mode_handles.value_handle = PROTOCOL_VALUE_HANDLE;
	p_client = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			    CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	error_requested = false;

	/* Test value set gatts error. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_value_set_ExpectAndReturn(CONN_HANDLE,
						       ble_hids.protocol_mode_handles.value_handle,
						       NULL, NRF_ERROR_INVALID_ADDR);
	__cmock_sd_ble_gatts_value_set_IgnoreArg_p_value();

	error_expected = true;

	emulate_ble_connected_evt(&ble_hids);

	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_ADDR, error);
	TEST_ASSERT_TRUE(error_requested);
}

void test_on_ble_connected_evt(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	void *p_client;

	ble_hids.protocol_mode_handles.value_handle = PROTOCOL_VALUE_HANDLE;
	p_client = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			    CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	struct ble_hids_client_context *p_client_contex =
		(struct ble_hids_client_context *)p_client;

	error_requested = false;
	error = NRF_SUCCESS;

	/* Test success. */
	p_client_contex->protocol_mode = 0x25;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_value_set_ExpectAndReturn(
		CONN_HANDLE, ble_hids.protocol_mode_handles.value_handle, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_value_set_IgnoreArg_p_value();

	emulate_ble_connected_evt(&ble_hids);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, error);
	TEST_ASSERT_EQUAL(CONFIG_BLE_HIDS_DEFAULT_PROTOCOL_MODE, p_client_contex->protocol_mode);
}

#define HIDS_CONTROL_POINT_SUSPEND 0x00
/* Exit Suspend command. */
#define HIDS_CONTROL_POINT_EXIT_SUSPEND 0x01

void test_on_control_point_error_not_found(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	void *host;
	uint8_t data[] = {
		HIDS_CONTROL_POINT_SUSPEND
	};
	uint16_t len = sizeof(data);

	/* Test on_control_point_write. */
	host = NULL;

	/* link_ctx_get fail. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	error_expected = true;
	emulate_ble_write_evt(&ble_hids, ON_WRITE_CONTROL_PONT, data, len);

	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, error);
	TEST_ASSERT_TRUE(error_requested);
}

void test_on_control_point_suspend(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	void *host;
	uint8_t data[] = {HIDS_CONTROL_POINT_SUSPEND};
	uint16_t len = sizeof(data);
	struct ble_hids_client_context *ctx_data;

	/* Test on_control_point_write. */
	host = NULL;

	error_requested = false;

	/* Test success. */
	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	ctx_data = (struct ble_hids_client_context *)host;

	ctx_data->ctrl_pt = 0x98;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	hids_evt_expected = true;
	emulate_ble_write_evt(&ble_hids, ON_WRITE_CONTROL_PONT, data, len);

	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL(HIDS_CONTROL_POINT_SUSPEND, ctx_data->ctrl_pt);
	hids_evt_requested = false;
}

void test_on_control_point_exit_suspend(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	void *host;
	uint8_t data[] = {HIDS_CONTROL_POINT_EXIT_SUSPEND};
	uint16_t len = sizeof(data);
	struct ble_hids_client_context *ctx_data;

	/* Test on_control_point_write. */
	host = NULL;

	error_requested = false;

	/* Test success. */
	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	ctx_data = (struct ble_hids_client_context *)host;

	ctx_data->ctrl_pt = 0x98;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	hids_evt_expected = true;
	emulate_ble_write_evt(&ble_hids, ON_WRITE_CONTROL_PONT, data, len);

	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL(HIDS_CONTROL_POINT_EXIT_SUSPEND, ctx_data->ctrl_pt);
	hids_evt_requested = false;
}

void test_on_protocols_mode_error_not_found(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	void *host;
	uint8_t data[] = {0x01};
	uint16_t len = sizeof(data);

	struct ble_hids_client_context exp_context = {
		.protocol_mode = 1,
		.ctrl_pt = 0,
	};
	/* Test on_protocols mode. */
	host = NULL;

	/* Test link_ctx_get_fails. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	error_expected = true;
	emulate_ble_write_evt(&ble_hids, ON_WRITE_PROTOCOLS_MODE, data, len);

	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, error);
	TEST_ASSERT_TRUE(error_requested);

	/* Test on_protocols mode. Boot mode entered. */
	data[0] = 0;
	exp_context.protocol_mode = 0;
	exp_context.ctrl_pt = 0;
	host = NULL;

	/* Test link_ctx_get_fails. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	error_expected = true;
	emulate_ble_write_evt(&ble_hids, ON_WRITE_PROTOCOLS_MODE, data, len);

	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, error);
	TEST_ASSERT_TRUE(error_requested);

	error_requested = false;
}

void test_on_protocols_mode(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	void *host;
	uint8_t data[] = {0x01};
	uint16_t len = sizeof(data);

	struct ble_hids_client_context exp_context = {
		.protocol_mode = 1,
		.ctrl_pt = 0,
	};
	/* Test on_protocols mode. */
	host = NULL;

	error_requested = false;

	/* Test success. */
	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	hids_evt_expected = true;
	emulate_ble_write_evt(&ble_hids, ON_WRITE_PROTOCOLS_MODE, data, len);

	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(&exp_context, (struct ble_hids_client_context *)host,
				 sizeof(exp_context));
	hids_evt_requested = false;

	/* Test on_protocols mode. Boot mode entered. */
	data[0] = 0;
	exp_context.protocol_mode = 0;
	exp_context.ctrl_pt = 0;
	host = NULL;

	error_requested = false;

	/* Test success. */
	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	hids_evt_expected = true;
	emulate_ble_write_evt(&ble_hids, ON_WRITE_PROTOCOLS_MODE, data, len);

	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(&exp_context, (struct ble_hids_client_context *)host,
				 sizeof(exp_context));
	hids_evt_requested = false;
}

void test_on_kb_inp_cccd_handle_test(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0x01, 0x02};
	uint16_t len = sizeof(data);
	struct ble_hids_char_id exp_char_id = {
		.uuid = BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT_CHAR,
		.report_type = 0,
		.report_index = 0,
	};

	/* Test notification disable. */
	hids_evt_expected = true;
	emulate_ble_write_evt(&ble_hids, ON_KB_INP_CCCD, data, len);

	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(&exp_char_id, &char_evt_id, sizeof(struct ble_hids_char_id));
	hids_evt_requested = false;

	memset(&char_evt_id, 0, sizeof(char_evt_id));

	/* Test notification enabled. */
	hids_evt_expected = true;
	emulate_ble_write_evt(&ble_hids, ON_KB_INP_CCCD, data, len);

	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(&exp_char_id, &char_evt_id, sizeof(struct ble_hids_char_id));

	hids_evt_requested = false;
}

void test_on_kb_inp_handle_value_error_not_found(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
	uint16_t len = 2;

	/* Test link_ctx_get_fails. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	error_expected = true;
	emulate_ble_write_evt(&ble_hids, ON_KB_INP_VALUE, data, len);

	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, error);
	TEST_ASSERT_TRUE(error_requested);
}

void test_on_kb_inp_handle_value(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
	uint16_t len = 2;
	uint8_t *report;
	void *host = NULL;
	uint8_t data_offset = sizeof(struct ble_hids_client_context);

	struct char_write char_write_exp = {
		.char_id.uuid = BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT_CHAR,
		.char_id.report_type = 0,
		.char_id.report_index = 0,
		.offset = 0,
		.len = 8,
		.data = data,
	};

	error_requested = false;

	/* Test to long data. */
	len = 9;
	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	hids_evt_expected = false;

	emulate_ble_write_evt(&ble_hids, ON_KB_INP_VALUE, data, len);

	TEST_ASSERT_FALSE(hids_evt_requested);

	/* Test success. */
	len = 8;
	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	hids_evt_expected = true;

	emulate_ble_write_evt(&ble_hids, ON_KB_INP_VALUE, data, len);

	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(char_write_exp.data, char_write_evt.data, char_write_exp.len);
	TEST_ASSERT_EQUAL_UINT16(char_write_exp.len, char_write_evt.len);
	TEST_ASSERT_EQUAL_MEMORY(&char_write_exp.char_id, &char_write_evt.char_id,
				 sizeof(char_write_exp.char_id));

	hids_evt_requested = false;

	report = (uint8_t *)host + data_offset;
	TEST_ASSERT_EQUAL_MEMORY(data, report, len);
}

void test_on_kb_outp_value_test(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
	uint16_t len = 1;
	uint8_t *report;
	void *host = NULL;
	uint8_t data_offset = sizeof(struct ble_hids_client_context);

	struct char_write char_write_exp = {
		.char_id.uuid = BLE_UUID_BOOT_KEYBOARD_OUTPUT_REPORT_CHAR,
		.char_id.report_type = 0,
		.char_id.report_index = 0,
		.offset = 0,
		.len = 1,
		.data = data,
	};

	/* Test success. */
	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	hids_evt_expected = true;

	emulate_ble_write_evt(&ble_hids, ON_KB_OUTP_VALUE, data, len);

	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(char_write_exp.data, char_write_evt.data, char_write_exp.len);
	TEST_ASSERT_EQUAL_UINT16(char_write_exp.len, char_write_evt.len);
	TEST_ASSERT_EQUAL_MEMORY(&char_write_exp.char_id, &char_write_evt.char_id,
				 sizeof(char_write_exp.char_id));
	hids_evt_requested = false;

	report = (uint8_t *)host + data_offset + BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE;
	TEST_ASSERT_EQUAL_MEMORY(data, report, len);
}

void test_on_mouse_inp_cccd(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0x01, 0x02};
	uint16_t len = sizeof(data);
	struct ble_hids_char_id exp_char_id = {
		.uuid = BLE_UUID_BOOT_MOUSE_INPUT_REPORT_CHAR,
		.report_type = 0,
		.report_index = 0,
	};

	/* Test notification disable. */
	hids_evt_expected = true;
	emulate_ble_write_evt(&ble_hids, ON_MOUSE_INP_CCCD, data, len);

	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(&exp_char_id, &char_evt_id, sizeof(struct ble_hids_char_id));
	hids_evt_requested = false;
}

void test_on_mouse_inp_value(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	uint16_t len = BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE;
	uint8_t *report;
	void *host = NULL;
	uint8_t data_offset = sizeof(struct ble_hids_client_context);

	struct char_write char_write_exp = {
		.char_id.uuid = BLE_UUID_BOOT_MOUSE_INPUT_REPORT_CHAR,
		.char_id.report_type = 0,
		.char_id.report_index = 0,
		.offset = 0,
		.len = 8,
		.data = data,
	};

	/* Test success. */
	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	hids_evt_expected = true;

	emulate_ble_write_evt(&ble_hids, ON_MOUSE_INP_VALUE, data, len);

	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(char_write_exp.data, char_write_evt.data, char_write_exp.len);
	TEST_ASSERT_EQUAL_UINT16(char_write_exp.len, char_write_evt.len);
	TEST_ASSERT_EQUAL_MEMORY(&char_write_exp.char_id, &char_write_evt.char_id,
				 sizeof(char_write_exp.char_id));
	hids_evt_requested = false;

	report = (uint8_t *)host + data_offset + BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE +
		   BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE;
	TEST_ASSERT_EQUAL_MEMORY(data, report, len);
}

void test_on_inp_rep_cccd(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0x01, 0x02};
	uint16_t len = sizeof(data);
	struct ble_hids_char_id exp_char_id = {
		.uuid = BLE_UUID_REPORT_CHAR,
		.report_type = BLE_HIDS_REPORT_TYPE_INPUT,
		.report_index = 0,
	};

	/* Test no input report. */
	ble_hids.input_report_count = 0;
	hids_evt_expected = false;

	emulate_ble_write_evt(&ble_hids, ON_INPUT_REPORT_CCCD, data, len);

	/* Test notification disable. */
	ble_hids.input_report_count = 1;
	hids_evt_expected = true;

	emulate_ble_write_evt(&ble_hids, ON_INPUT_REPORT_CCCD, data, len);

	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(&exp_char_id, &char_evt_id, sizeof(struct ble_hids_char_id));
	hids_evt_requested = false;

	memset(&char_evt_id, 0, sizeof(char_evt_id));

	/* Test notification enabled. */
	hids_evt_expected = true;
	emulate_ble_write_evt(&ble_hids, ON_INPUT_REPORT_CCCD, data, len);

	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(&exp_char_id, &char_evt_id, sizeof(struct ble_hids_char_id));

	hids_evt_requested = false;
}

void test_on_rep_value_identify_error_too_long_data(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5};
	uint8_t empty_data[FEATURE_REPORT_3_LEN] = {0};
	uint16_t len = FEATURE_REPORT_3_LEN + 1;
	uint8_t *report;
	void *host = NULL;
	uint8_t data_offset =
		sizeof(struct ble_hids_client_context) + BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE +
		BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE + BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE;

	/* Test last feature report. */
	/* Test to long data */
	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	hids_evt_expected = false;

	emulate_ble_write_evt(&ble_hids, ON_REP_VALUE_IDENTIFY, data, len);

	report = (uint8_t *)host + data_offset + INPUT_REPORT_1_LEN + INPUT_REPORT_2_LEN +
		   OUTPUT_REPORT_1_LEN + OUTPUT_REPORT_2_LEN + FEATURE_REPORT_1_LEN +
		   FEATURE_REPORT_2_LEN;
	TEST_ASSERT_EQUAL_MEMORY(empty_data, report, len - 1);
}

void test_on_rep_value_identify(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	uint8_t data[] = {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5};
	uint16_t len = FEATURE_REPORT_3_LEN + 1;
	uint8_t *report;
	void *host = NULL;
	uint8_t data_offset =
		sizeof(struct ble_hids_client_context) + BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE +
		BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE + BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE;

	struct char_write char_write_exp = {
		.char_id.uuid = BLE_UUID_REPORT_CHAR,
		.char_id.report_type = BLE_HIDS_REPORT_TYPE_FEATURE,
		.char_id.report_index = 2,
		.offset = 0,
		.len = FEATURE_REPORT_3_LEN,
		.data = data,
	};

	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	report = (uint8_t *)host + data_offset + INPUT_REPORT_1_LEN + INPUT_REPORT_2_LEN +
		   OUTPUT_REPORT_1_LEN + OUTPUT_REPORT_2_LEN + FEATURE_REPORT_1_LEN +
		   FEATURE_REPORT_2_LEN;

	len = FEATURE_REPORT_3_LEN;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	hids_evt_expected = true;

	emulate_ble_write_evt(&ble_hids, ON_REP_VALUE_IDENTIFY, data, len);

	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(char_write_exp.data, char_write_evt.data, char_write_exp.len);
	TEST_ASSERT_EQUAL_UINT16(char_write_exp.len, char_write_evt.len);
	TEST_ASSERT_EQUAL_MEMORY(&char_write_exp.char_id, &char_write_evt.char_id,
				 sizeof(char_write_exp.char_id));
	TEST_ASSERT_EQUAL_MEMORY(data, report, len);

	hids_evt_requested = false;
}

void test_on_protocol_mode_rw_auth_test_error_not_found(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);

	TEST_ASSERT_FALSE(hids_evt_requested);

	/* Test link_ctx_get fail. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	error_expected = true;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_PROTOCOL_MODE_RW_AUTH);

	TEST_ASSERT_TRUE(error_requested);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, error);
}

void test_on_protocol_mode_rw_auth_test_error_invalid_state(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	void *host = NULL;

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);
	TEST_ASSERT_FALSE(hids_evt_requested);

	/* Test gatts rw authorize reply fail. */
	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_rw_authorize_reply_ExpectAndReturn(CONN_HANDLE, NULL,
								NRF_ERROR_INVALID_STATE);
	__cmock_sd_ble_gatts_rw_authorize_reply_IgnoreArg_p_rw_authorize_reply_params();

	error_expected = true;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_PROTOCOL_MODE_RW_AUTH);

	TEST_ASSERT_TRUE(error_requested);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, error);
}

void test_on_protocol_mode_rw_auth_test(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);
	TEST_ASSERT_FALSE(hids_evt_requested);

	/* Test success */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_rw_authorize_reply_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_rw_authorize_reply_IgnoreArg_p_rw_authorize_reply_params();

	error_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_PROTOCOL_MODE_RW_AUTH);

	TEST_ASSERT_FALSE(error_requested);
}

void test_on_boot_kb_inp_rep_rw_auth_error_not_found(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);

	TEST_ASSERT_FALSE(hids_evt_requested);

	/* Test link_ctx_get fail. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	error_expected = true;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_BOOT_KB_INP_REP_RW_AUTH);

	TEST_ASSERT_TRUE(error_requested);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, error);
}

void test_on_boot_kb_inp_rep_rw_auth_error_timeout(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	void *host = NULL;

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);

	TEST_ASSERT_FALSE(hids_evt_requested);

	/* Test gatts rw authorize reply fail. */
	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_rw_authorize_reply_ExpectAndReturn(CONN_HANDLE, NULL,
								NRF_ERROR_TIMEOUT);
	__cmock_sd_ble_gatts_rw_authorize_reply_IgnoreArg_p_rw_authorize_reply_params();

	error_expected = true;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_BOOT_KB_INP_REP_RW_AUTH);

	TEST_ASSERT_TRUE(error_requested);
	TEST_ASSERT_EQUAL(NRF_ERROR_TIMEOUT, error);
}

void test_on_boot_kb_inp_rep_rw_auth(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);

	TEST_ASSERT_FALSE(hids_evt_requested);

	struct ble_hids_char_id char_id_exp = {
		.uuid = BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT_CHAR,
		.report_type = 0,
		.report_index = 0,
	};

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_rw_authorize_reply_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_rw_authorize_reply_IgnoreArg_p_rw_authorize_reply_params();

	hids_evt_expected = true;

	emulate_ble_rw_authorize_evt(&ble_hids, ON_BOOT_KB_INP_REP_RW_AUTH);
	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(&char_id_exp, &char_evt_id, sizeof(struct ble_hids_char_id));

	hids_evt_requested = false;

	/* Test NULL event handler. */
	ble_hids.evt_handler = NULL;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_rw_authorize_reply_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_rw_authorize_reply_IgnoreArg_p_rw_authorize_reply_params();

	hids_evt_expected = false;

	emulate_ble_rw_authorize_evt(&ble_hids, ON_BOOT_KB_INP_REP_RW_AUTH);
	TEST_ASSERT_FALSE(hids_evt_requested);

	ble_hids.evt_handler = on_hids_evt;
}

void test_on_boot_kb_outp_rep_rw_auth_test_error_not_found(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);

	TEST_ASSERT_FALSE(hids_evt_requested);

	/* Test link_ctx_get fail. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	error_expected = true;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_BOOT_KB_OUTP_REP_RW_AUTH);

	TEST_ASSERT_TRUE(error_requested);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, error);
}

void test_on_boot_kb_outp_rep_rw_auth_test_error_timeout(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	void *host = NULL;

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);

	TEST_ASSERT_FALSE(hids_evt_requested);

	/* Test gatts rw authorize reply fail. */
	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_rw_authorize_reply_ExpectAndReturn(CONN_HANDLE, NULL,
								NRF_ERROR_TIMEOUT);
	__cmock_sd_ble_gatts_rw_authorize_reply_IgnoreArg_p_rw_authorize_reply_params();

	error_expected = true;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_BOOT_KB_OUTP_REP_RW_AUTH);

	TEST_ASSERT_TRUE(error_requested);
	TEST_ASSERT_EQUAL(NRF_ERROR_TIMEOUT, error);
}

void test_on_boot_kb_outp_rep_rw_auth_test(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	struct ble_hids_char_id char_id_exp = {
		.uuid = BLE_UUID_BOOT_KEYBOARD_OUTPUT_REPORT_CHAR,
		.report_type = 0,
		.report_index = 0,
	};

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);

	TEST_ASSERT_FALSE(hids_evt_requested);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_rw_authorize_reply_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_rw_authorize_reply_IgnoreArg_p_rw_authorize_reply_params();

	hids_evt_expected = true;

	emulate_ble_rw_authorize_evt(&ble_hids, ON_BOOT_KB_OUTP_REP_RW_AUTH);
	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(&char_id_exp, &char_evt_id, sizeof(struct ble_hids_char_id));

	hids_evt_requested = false;
}

void test_on_boot_mouse_inp_rep_rw_auth_error_not_found(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);

	TEST_ASSERT_FALSE(hids_evt_requested);

	/* Test link_ctx_get fail. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	error_expected = true;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_BOOT_MOUSE_INP_REP_RW_AUTH);

	TEST_ASSERT_TRUE(error_requested);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, error);
}

void test_on_boot_mouse_inp_rep_rw_auth_error_timeout(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	void *host = NULL;

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);

	TEST_ASSERT_FALSE(hids_evt_requested);

	/* Test gatts rw authorize reply fail. */
	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_rw_authorize_reply_ExpectAndReturn(CONN_HANDLE, NULL,
								NRF_ERROR_TIMEOUT);
	__cmock_sd_ble_gatts_rw_authorize_reply_IgnoreArg_p_rw_authorize_reply_params();

	error_expected = true;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_BOOT_MOUSE_INP_REP_RW_AUTH);

	TEST_ASSERT_TRUE(error_requested);
	TEST_ASSERT_EQUAL(NRF_ERROR_TIMEOUT, error);
}

void test_on_boot_mouse_inp_rep_rw_auth(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	struct ble_hids_char_id char_id_exp = {
		.uuid = BLE_UUID_BOOT_MOUSE_INPUT_REPORT_CHAR,
		.report_type = 0,
		.report_index = 0,
	};

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);

	TEST_ASSERT_FALSE(hids_evt_requested);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_rw_authorize_reply_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_rw_authorize_reply_IgnoreArg_p_rw_authorize_reply_params();

	hids_evt_expected = true;

	emulate_ble_rw_authorize_evt(&ble_hids, ON_BOOT_MOUSE_INP_REP_RW_AUTH);
	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(&char_id_exp, &char_evt_id, sizeof(struct ble_hids_char_id));

	hids_evt_requested = false;
}

void test_on_inp_rep_rw_auth_error_not_found(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);

	TEST_ASSERT_FALSE(hids_evt_requested);

	/* Test link_ctx_get fail. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	error_expected = true;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_REP_VALUE_IDEN_RW_AUTH);

	TEST_ASSERT_TRUE(error_requested);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, error);
}

void test_on_inp_rep_rw_auth_error_timeout(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;
	void *host = NULL;

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);

	TEST_ASSERT_FALSE(hids_evt_requested);

	/* Test gatts rw authorize reply fail. */
	host = (void *)((uint8_t *)ble_hids.link_ctx_storage.ctx_data_pool +
			  CONN_IDX * ble_hids.link_ctx_storage.link_ctx_size);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_rw_authorize_reply_ExpectAndReturn(CONN_HANDLE, NULL,
								NRF_ERROR_TIMEOUT);
	__cmock_sd_ble_gatts_rw_authorize_reply_IgnoreArg_p_rw_authorize_reply_params();

	error_expected = true;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_REP_VALUE_IDEN_RW_AUTH);

	TEST_ASSERT_TRUE(error_requested);
	TEST_ASSERT_EQUAL(NRF_ERROR_TIMEOUT, error);
}

void test_on_inp_rep_rw_auth(void)
{
	struct ble_hids ble_hids = INITIALIZED_HIDS;

	struct ble_hids_char_id char_id_exp = {
		.uuid = BLE_UUID_REPORT_CHAR,
		.report_type = BLE_HIDS_REPORT_TYPE_OUTPUT,
		.report_index = 1,
	};

	/* Test event type other than BLE_GATTS_AUTHORIZE_TYPE_READ. */
	hids_evt_expected = false;
	emulate_ble_rw_authorize_evt(&ble_hids, ON_OTHER_TYPE);

	TEST_ASSERT_FALSE(hids_evt_requested);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, CONN_IDX);

	__cmock_sd_ble_gatts_rw_authorize_reply_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_rw_authorize_reply_IgnoreArg_p_rw_authorize_reply_params();

	hids_evt_expected = true;

	emulate_ble_rw_authorize_evt(&ble_hids, ON_REP_VALUE_IDEN_RW_AUTH);
	TEST_ASSERT_TRUE(hids_evt_requested);
	TEST_ASSERT_EQUAL_MEMORY(&char_id_exp, &char_evt_id, sizeof(struct ble_hids_char_id));

	hids_evt_requested = false;
}

void setUp(void)
{
	error = NRF_SUCCESS;
	error_expected = false;
	error_requested = false;
	hids_evt_expected = false;
	hids_evt_requested = false;

	memset(&char_evt_id, 0, sizeof(char_evt_id));
	memset(&hvx_params, 0, sizeof(ble_gatts_hvx_params_t));
	memset(input_report, 0, sizeof(struct ble_hids_report_config));
	memset(output_report, 0, sizeof(struct ble_hids_report_config));
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
