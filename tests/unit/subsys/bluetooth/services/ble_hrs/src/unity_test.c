/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>

#include <stddef.h>
#include <stdint.h>
#include <ble_err.h>
#include <ble_gap.h>
#include <bm/bluetooth/services/ble_hrs.h>
#include <bm/bluetooth/services/uuid.h>

#include "cmock_ble_gatts.h"

/* An arbitrary error, to test forwarding of errors from SoftDevice calls */
#define ERROR 0xbaadf00d

/* Test connection and handle constants */
#define CONN_HANDLE 0x0001
#define HRM_CCCD_HANDLE 0x0010
#define HRM_VALUE_HANDLE 0x0011
#define TEST_ATT_MTU 100

/* Test event handler state */
static enum ble_hrs_evt_type last_evt_type;
static bool evt_handler_called;

void ble_hrs_on_ble_evt(const ble_evt_t *evt, struct ble_hrs *hrs);

/* Soft Device stubs */
static uint32_t stub_sd_ble_gatts_value_get_notif_enabled(uint16_t conn_handle, uint16_t handle,
							  ble_gatts_value_t *p_value, int calls)
{
	*p_value->p_value = BLE_GATT_HVX_NOTIFICATION;
	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_value_get_notif_disabled(uint16_t conn_handle, uint16_t handle,
							   ble_gatts_value_t *p_value, int calls)
{
	*p_value->p_value = 0x00;
	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_characteristic_add(uint16_t service_handle,
						     const ble_gatts_char_md_t *p_char_md,
						     const ble_gatts_attr_t *p_attr_char_value,
						     ble_gatts_char_handles_t *p_handles,
						     int cmock_num_calls)
{
	TEST_ASSERT_NOT_NULL(p_char_md);
	TEST_ASSERT_NOT_NULL(p_attr_char_value);
	TEST_ASSERT_NOT_NULL(p_handles);
	if (p_attr_char_value->p_uuid->uuid == BLE_UUID_HEART_RATE_MEASUREMENT_CHAR) {
		TEST_ASSERT_EQUAL(BLE_GATTS_VLOC_STACK, p_char_md->p_cccd_md->vloc);
		TEST_ASSERT_TRUE(p_char_md->char_props.notify);
		TEST_ASSERT_TRUE(p_attr_char_value->p_attr_md->vlen);
		TEST_ASSERT_GREATER_OR_EQUAL(0, p_attr_char_value->init_len);
		TEST_ASSERT_TRUE(p_attr_char_value->max_len);
	} else if (p_attr_char_value->p_uuid->uuid == BLE_UUID_BODY_SENSOR_LOCATION_CHAR) {
		TEST_ASSERT_TRUE(p_char_md->char_props.read);
		TEST_ASSERT_EQUAL(sizeof(uint8_t), p_attr_char_value->init_len);
		TEST_ASSERT_EQUAL(sizeof(uint8_t), p_attr_char_value->max_len);
		TEST_ASSERT_EQUAL(BLE_GATTS_VLOC_STACK, p_attr_char_value->p_attr_md->vloc);
	} else {
		TEST_FAIL_MESSAGE("Unexpected UUID in characteristic add stub");
	}
	TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_attr_char_value->p_uuid->type);
	TEST_ASSERT_NOT_NULL(p_attr_char_value->p_value);

	return NRF_SUCCESS;
}

/* Helpers and handlers for testing */

static void ble_hrs_evt_handler(struct ble_hrs *hrs, const struct ble_hrs_evt *evt)
{
	evt_handler_called = true;
	last_evt_type = evt->evt_type;
}

/* ble_hrs_rr_interval_add() tests. */

void test_ble_hrs_rr_interval_add_success(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {0};

	/* Add RR interval measurement. */
	nrf_err = ble_hrs_rr_interval_add(&hrs, 100);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, hrs.rr_interval_count);
	TEST_ASSERT_EQUAL(100, hrs.rr_interval[0]);

	/* Add another RR interval measurement. */
	nrf_err = ble_hrs_rr_interval_add(&hrs, 200);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(2, hrs.rr_interval_count);
	TEST_ASSERT_EQUAL(200, hrs.rr_interval[1]);

	/* Add a third RR interval measurement. */
	nrf_err = ble_hrs_rr_interval_add(&hrs, 300);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(3, hrs.rr_interval_count);
	TEST_ASSERT_EQUAL(300, hrs.rr_interval[2]);
}

void test_ble_hrs_rr_interval_add_null(void)
{
	/* Try to use null for hrs struct */
	uint32_t nrf_err = ble_hrs_rr_interval_add(NULL, 0);

	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_hrs_rr_interval_add_overflow(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.max_hrm_len = CONFIG_BLE_HRS_MAX_BUFFERED_RR_INTERVALS * sizeof(uint16_t) + 1
	};
	uint16_t rr_interval_second = 0;

	/* Fill the buffer to max */
	for (int i = 0; i < CONFIG_BLE_HRS_MAX_BUFFERED_RR_INTERVALS; i++) {
		nrf_err = ble_hrs_rr_interval_add(&hrs, i + 1);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	}
	rr_interval_second = hrs.rr_interval[1];

	/* Now adding one more should remove the first one and add new to last*/
	nrf_err = ble_hrs_rr_interval_add(&hrs, 999);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(CONFIG_BLE_HRS_MAX_BUFFERED_RR_INTERVALS, hrs.rr_interval_count);
	TEST_ASSERT_EQUAL(999, hrs.rr_interval[CONFIG_BLE_HRS_MAX_BUFFERED_RR_INTERVALS - 1]);
	TEST_ASSERT_EQUAL(rr_interval_second, hrs.rr_interval[0]);
}

/* ble_hrs_rr_interval_buffer_is_full() tests. */

void test_ble_hrs_rr_interval_buffer_is_full(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {0};

	/* Check if buffer is empty */
	TEST_ASSERT_FALSE(ble_hrs_rr_interval_buffer_is_full(&hrs));

	/* Fill the buffer to max */
	for (int i = 0; i < CONFIG_BLE_HRS_MAX_BUFFERED_RR_INTERVALS; i++) {
		nrf_err = ble_hrs_rr_interval_add(&hrs, i + 1);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	}

	/* Check if buffer is full */
	TEST_ASSERT_TRUE(ble_hrs_rr_interval_buffer_is_full(&hrs));
}

/* ble_hrs_sensor_contact_supported_set() tests. */

void test_ble_hrs_sensor_contact_supported_set(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {0};

	hrs.conn_handle = BLE_CONN_HANDLE_INVALID;
	/* Set sensor contact supported to true */
	nrf_err = ble_hrs_sensor_contact_supported_set(&hrs, true);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(hrs.is_sensor_contact_supported);
}

void test_ble_hrs_sensor_contact_supported_set_invalid_state(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {0};
	/* Try to set sensor contact supported while in connection
	 * Simulate being in a connection
	 */
	const ble_evt_t evt = {
		.header.evt_id = BLE_GAP_EVT_CONNECTED
	};

	ble_hrs_on_ble_evt(&evt, &hrs);

	nrf_err = ble_hrs_sensor_contact_supported_set(&hrs, true);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_hrs_sensor_contact_supported_set_null(void)
{
	uint32_t nrf_err;

	/* Try to use null for hrs struct */
	nrf_err = ble_hrs_sensor_contact_supported_set(NULL, false);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_hrs_sensor_contact_detected_update_success(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {0};

	/* Update sensor contact detected state */
	nrf_err = ble_hrs_sensor_contact_detected_update(&hrs, true);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(hrs.is_sensor_contact_detected);
}

void test_ble_hrs_sensor_contact_detected_update_null(void)
{
	uint32_t nrf_err;

	/* Update sensor contact detected state */
	nrf_err = ble_hrs_sensor_contact_detected_update(NULL, true);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

/* ble_hrs_body_sensor_location_set() tests. */

void test_ble_hrs_body_sensor_location_set_success(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {0};
	uint8_t body_sensor_location = BLE_HRS_BODY_SENSOR_LOCATION_FINGER;

	__cmock_sd_ble_gatts_value_set_ExpectAndReturn(hrs.conn_handle,
						       hrs.bsl_handles.value_handle,
						       NULL,
						       NRF_SUCCESS);
	__cmock_sd_ble_gatts_value_set_IgnoreArg_p_value();

	nrf_err = ble_hrs_body_sensor_location_set(&hrs, body_sensor_location);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_hrs_body_sensor_location_set_invalid_param(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {0};
	uint8_t body_sensor_location = BLE_HRS_BODY_SENSOR_LOCATION_FINGER;

	__cmock_sd_ble_gatts_value_set_ExpectAndReturn(hrs.conn_handle,
						       hrs.bsl_handles.value_handle,
						       NULL,
						       ERROR);
	__cmock_sd_ble_gatts_value_set_IgnoreArg_p_value();

	nrf_err = ble_hrs_body_sensor_location_set(&hrs, body_sensor_location);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);
}

void test_ble_hrs_body_sensor_location_set_null(void)
{
	uint32_t nrf_err;
	uint8_t body_sensor_location = BLE_HRS_BODY_SENSOR_LOCATION_FINGER;

	nrf_err = ble_hrs_body_sensor_location_set(NULL, body_sensor_location);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

/* ble_hrs_heart_rate_measurement_send() tests. */

void test_ble_hrs_heart_rate_measurement_send_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_hrs_heart_rate_measurement_send(NULL, 72);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_hrs_heart_rate_measurement_send_no_connection(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.conn_handle = BLE_CONN_HANDLE_INVALID,
	};

	__cmock_sd_ble_gatts_value_get_ExpectAnyArgsAndReturn(BLE_ERROR_INVALID_CONN_HANDLE);

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, 72);
	TEST_ASSERT_EQUAL(BLE_ERROR_INVALID_CONN_HANDLE, nrf_err);
}

void test_ble_hrs_heart_rate_measurement_send_cccd_unexpected_error(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.conn_handle = CONN_HANDLE,
		.hrm_handles.cccd_handle = HRM_CCCD_HANDLE,
		.max_hrm_len = 20,
	};

	__cmock_sd_ble_gatts_value_get_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, 72);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_hrs_heart_rate_measurement_send_notify_success(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.conn_handle = CONN_HANDLE,
		.hrm_handles.cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handles.value_handle = HRM_VALUE_HANDLE,
		.max_hrm_len = 20,
	};

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, 72);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_hrs_heart_rate_measurement_send_notify_error(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.conn_handle = CONN_HANDLE,
		.hrm_handles.cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handles.value_handle = HRM_VALUE_HANDLE,
		.max_hrm_len = 20,
	};

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(CONN_HANDLE, NULL, ERROR);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, 72);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);
}

void test_ble_hrs_heart_rate_measurement_send_notif_disabled(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.conn_handle = CONN_HANDLE,
		.hrm_handles.cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handles.value_handle = HRM_VALUE_HANDLE,
		.max_hrm_len = 20,
	};

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_disabled);

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, 72);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_hrs_heart_rate_measurement_send_notif_disabled_value_set_err(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.conn_handle = CONN_HANDLE,
		.hrm_handles.cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handles.value_handle = HRM_VALUE_HANDLE,
		.max_hrm_len = 20,
	};

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_disabled);

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, 72);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_hrs_heart_rate_measurement_send_sys_attr_missing(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.conn_handle = CONN_HANDLE,
		.hrm_handles.cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handles.value_handle = HRM_VALUE_HANDLE,
		.max_hrm_len = 20,
	};

	__cmock_sd_ble_gatts_value_get_Stub(NULL);
	__cmock_sd_ble_gatts_value_get_ExpectAnyArgsAndReturn(BLE_ERROR_GATTS_SYS_ATTR_MISSING);

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, 72);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

/* hrm_encode coverage via ble_hrs_heart_rate_measurement_send. */
void test_ble_hrs_send_sensor_contact_detected(void)
{
	/* is_sensor_contact_detected flag set in encoded output. */
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.conn_handle = CONN_HANDLE,
		.hrm_handles.cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handles.value_handle = HRM_VALUE_HANDLE,
		.max_hrm_len = 20,
		.is_sensor_contact_supported = true,
		.is_sensor_contact_detected = true,
	};

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, 72);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_hrs_send_16bit_heart_rate(void)
{
	/* heart_rate > 255 triggers 16-bit encoding. */
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.conn_handle = CONN_HANDLE,
		.hrm_handles.cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handles.value_handle = HRM_VALUE_HANDLE,
		.max_hrm_len = 20,
	};

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, 300);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_hrs_send_with_rr_intervals(void)
{
	/* rr_interval_count > 0 encodes RR intervals into the payload */
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.conn_handle = CONN_HANDLE,
		.hrm_handles.cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handles.value_handle = HRM_VALUE_HANDLE,
		.max_hrm_len = 20,
		.rr_interval = {800, 810},
		.rr_interval_count = 2,
	};

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, 72);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, hrs.rr_interval_count);
}

void test_ble_hrs_send_rr_intervals_overflow(void)
{
	/* max_hrm_len too small to fit all RR intervals, triggers memmove */
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.conn_handle = CONN_HANDLE,
		.hrm_handles.cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handles.value_handle = HRM_VALUE_HANDLE,
		/* flags(1) + hr(1) = 2 bytes, only room for 1 rr_interval (2 bytes) */
		.max_hrm_len = 4,
		.rr_interval = {800, 810, 820},
		.rr_interval_count = 3,
	};

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(CONN_HANDLE, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, 72);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	/* 1 interval fit, 2 remaining moved to start. */
	TEST_ASSERT_EQUAL(2, hrs.rr_interval_count);
	TEST_ASSERT_EQUAL(810, hrs.rr_interval[0]);
	TEST_ASSERT_EQUAL(820, hrs.rr_interval[1]);
}

/* ble_hrs_on_ble_evt() tests */

void test_ble_hrs_on_ble_evt_connected(void)
{
	struct ble_hrs hrs = {
		.conn_handle = BLE_CONN_HANDLE_INVALID,
	};
	const ble_evt_t evt = {
		.header.evt_id = BLE_GAP_EVT_CONNECTED,
		.evt.gap_evt.conn_handle = CONN_HANDLE,
	};

	ble_hrs_on_ble_evt(&evt, &hrs);
	TEST_ASSERT_EQUAL(CONN_HANDLE, hrs.conn_handle);
}

void test_ble_hrs_on_ble_evt_disconnected(void)
{
	struct ble_hrs hrs = {
		.conn_handle = CONN_HANDLE,
	};
	const ble_evt_t evt = {
		.header.evt_id = BLE_GAP_EVT_DISCONNECTED,
	};

	ble_hrs_on_ble_evt(&evt, &hrs);
	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, hrs.conn_handle);
}

void test_ble_hrs_on_ble_evt_write(void)
{
	struct ble_hrs hrs = {
		.hrm_handles.cccd_handle = HRM_CCCD_HANDLE,
	};

	/* Extra bytes for flexible array member .data[] */
	static uint8_t evt_buf[sizeof(ble_evt_t) + 2];
	ble_evt_t *evt = (ble_evt_t *)evt_buf;

	memset(evt_buf, 0, sizeof(evt_buf));
	evt->header.evt_id = BLE_GATTS_EVT_WRITE;
	evt->evt.gatts_evt.conn_handle = CONN_HANDLE;
	evt->evt.gatts_evt.params.write.handle = HRM_CCCD_HANDLE;
	evt->evt.gatts_evt.params.write.len = 2;
	evt->evt.gatts_evt.params.write.data[0] = BLE_GATT_HVX_NOTIFICATION;
	evt->evt.gatts_evt.params.write.data[1] = 0x00;

	/* No evt_handler set => early return, no crash. */
	hrs.evt_handler = NULL;
	evt_handler_called = false;
	ble_hrs_on_ble_evt(evt, &hrs);
	TEST_ASSERT_FALSE(evt_handler_called);

	/* Wrong write handle => handler not called. */
	hrs.evt_handler = ble_hrs_evt_handler;
	evt->evt.gatts_evt.params.write.handle = 0xBEEF;
	evt_handler_called = false;
	ble_hrs_on_ble_evt(evt, &hrs);
	TEST_ASSERT_FALSE(evt_handler_called);

	/* Wrong write length => handler not called. */
	evt->evt.gatts_evt.params.write.handle = HRM_CCCD_HANDLE;
	evt->evt.gatts_evt.params.write.len = 1;
	evt_handler_called = false;
	ble_hrs_on_ble_evt(evt, &hrs);
	TEST_ASSERT_FALSE(evt_handler_called);

	/* Notification enabled => handler called with NOTIFICATION_ENABLED. */
	evt->evt.gatts_evt.params.write.len = 2;
	evt->evt.gatts_evt.params.write.data[0] = BLE_GATT_HVX_NOTIFICATION;
	evt_handler_called = false;
	ble_hrs_on_ble_evt(evt, &hrs);
	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_EVT_NOTIFICATION_ENABLED, last_evt_type);

	/* Notification disabled => handler called with NOTIFICATION_DISABLED. */
	evt->evt.gatts_evt.params.write.data[0] = 0x00;
	evt_handler_called = false;
	ble_hrs_on_ble_evt(evt, &hrs);
	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_EVT_NOTIFICATION_DISABLED, last_evt_type);
}

/* ble_hrs_init() tests. */

void test_ble_hrs_init_success(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {0};
	struct ble_hrs_config hrs_config = {
		.is_sensor_contact_supported = true,
		.body_sensor_location = (uint8_t[]){ BLE_HRS_BODY_SENSOR_LOCATION_FINGER }
	};

	__cmock_sd_ble_gatts_service_add_IgnoreAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add);

	nrf_err = ble_hrs_init(&hrs, &hrs_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_hrs_init_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_hrs_init(NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_hrs_init_invalid_param(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {0};
	struct ble_hrs_config hrs_config = {
		.is_sensor_contact_supported = true,
		.body_sensor_location = (uint8_t[]){ BLE_HRS_BODY_SENSOR_LOCATION_FINGER }
	};

	__cmock_sd_ble_gatts_service_add_IgnoreAndReturn(ERROR);

	nrf_err = ble_hrs_init(&hrs, &hrs_config);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);
}

void test_ble_hrs_init_hrm_char_add_fail(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {0};
	struct ble_hrs_config hrs_config = {
		.is_sensor_contact_supported = true,
		.body_sensor_location = (uint8_t[]){ BLE_HRS_BODY_SENSOR_LOCATION_FINGER }
	};

	__cmock_sd_ble_gatts_service_add_IgnoreAndReturn(NRF_SUCCESS);
	/* heart_rate_measurement_char_add fails. */
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(ERROR);

	nrf_err = ble_hrs_init(&hrs, &hrs_config);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);
}

void test_ble_hrs_init_bsl_char_add_fail(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {0};
	struct ble_hrs_config hrs_config = {
		.is_sensor_contact_supported = true,
		.body_sensor_location = (uint8_t[]){ BLE_HRS_BODY_SENSOR_LOCATION_FINGER }
	};

	__cmock_sd_ble_gatts_service_add_IgnoreAndReturn(NRF_SUCCESS);
	/* heart_rate_measurement_char_add succeeds, body_sensor_location_char_add fails. */
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(ERROR);

	nrf_err = ble_hrs_init(&hrs, &hrs_config);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);
}

/* ble_hrs_conn_params_evt() tests. */

void test_ble_hrs_conn_params_evt_mtu_updated(void)
{
	struct ble_hrs hrs = {
		.conn_handle = CONN_HANDLE,
		.max_hrm_len = 0,
	};
	const struct ble_conn_params_evt evt = {
		.conn_handle = CONN_HANDLE,
		.evt_type = BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED,
		.att_mtu = TEST_ATT_MTU,
	};

	/* max_hrm_len = att_mtu - 3 (ATT header) */
	ble_hrs_conn_params_evt(&hrs, &evt);
	TEST_ASSERT_EQUAL(TEST_ATT_MTU - 3, hrs.max_hrm_len);
}

void test_ble_hrs_conn_params_evt_wrong_conn_handle(void)
{
	struct ble_hrs hrs = {
		.conn_handle = CONN_HANDLE,
		.max_hrm_len = 20,
	};
	const struct ble_conn_params_evt evt = {
		.conn_handle = 0xAAAA,
		.evt_type = BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED,
		.att_mtu = 100,
	};

	/* conn_handle mismatch => no change */
	ble_hrs_conn_params_evt(&hrs, &evt);
	TEST_ASSERT_EQUAL(20, hrs.max_hrm_len);
}

void test_ble_hrs_conn_params_evt_wrong_evt_type(void)
{
	struct ble_hrs hrs = {
		.conn_handle = CONN_HANDLE,
		.max_hrm_len = 20,
	};
	const struct ble_conn_params_evt evt = {
		.conn_handle = CONN_HANDLE,
		.evt_type = BLE_CONN_PARAMS_EVT_REJECTED,
		.att_mtu = 100,
	};

	/* Wrong event type => no change */
	ble_hrs_conn_params_evt(&hrs, &evt);
	TEST_ASSERT_EQUAL(20, hrs.max_hrm_len);
}

/* Unity test framework hooks and entry point */

void setUp(void)
{
	evt_handler_called = false;
	last_evt_type = 0;
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
