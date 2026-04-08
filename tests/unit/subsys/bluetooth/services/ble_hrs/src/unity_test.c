/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>

#include <stddef.h>
#include <stdint.h>
#include <ble_gap.h>
#include <bm/bluetooth/services/ble_hrs.h>
#include <bm/bluetooth/services/uuid.h>

#include "cmock_ble_gatts.h"

/* An arbitrary error, to test forwarding of errors from SoftDevice calls */
#define ERROR 0xbaadf00d

/* Captured HVX payload from stub_sd_ble_gatts_hvx_capture() for post-call verification. */
static uint8_t captured_hvx_data[20];
static uint16_t captured_hvx_len;

void ble_hrs_on_ble_evt(const ble_evt_t *evt, struct ble_hrs *hrs);

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

static uint32_t stub_sd_ble_gatts_hvx_capture(uint16_t conn_handle,
					      const ble_gatts_hvx_params_t *p_hvx_params,
					      int calls)
{
	/* Captures the encoded HVX notification data for assertion in tests. */
	captured_hvx_len = *p_hvx_params->p_len;
	memcpy(captured_hvx_data, p_hvx_params->p_data, captured_hvx_len);
	return NRF_SUCCESS;
}

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

void test_ble_hrs_heart_rate_measurement_send_enotfound(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.service_handle = 0,
		.conn_handle = BLE_CONN_HANDLE_INVALID,
		.rr_interval_count = 2,
		.max_hrm_len = 0,
		.is_sensor_contact_supported = true,
		.is_sensor_contact_detected = false
	};
	uint16_t heart_rate_measurement = 72;

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);
	__cmock_sd_ble_gatts_hvx_IgnoreAndReturn(ERROR);

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, heart_rate_measurement);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);
}

void test_ble_hrs_heart_rate_measurement_send_invalid_state(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.service_handle = 0,
		.conn_handle = BLE_CONN_HANDLE_INVALID,
		.rr_interval_count = 2,
		.max_hrm_len = 0,
		.is_sensor_contact_supported = true,
		.is_sensor_contact_detected = false
	};
	uint16_t heart_rate_measurement = 72;

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);
	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(hrs.conn_handle, NULL, ERROR);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, heart_rate_measurement);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);
}

void test_ble_hrs_heart_rate_measurement_send_invalid_param(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.service_handle = 0,
		.conn_handle = BLE_CONN_HANDLE_INVALID,
		.rr_interval_count = 2,
		.max_hrm_len = 0,
		.is_sensor_contact_supported = true,
		.is_sensor_contact_detected = false
	};
	uint16_t heart_rate_measurement = 72;

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);
	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(hrs.conn_handle, NULL, ERROR);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, heart_rate_measurement);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);
}

void test_ble_hrs_heart_rate_measurement_send_null(void)
{
	uint32_t nrf_err;
	uint16_t heart_rate_measurement = 72;

	nrf_err = ble_hrs_heart_rate_measurement_send(NULL, heart_rate_measurement);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

}


void test_ble_hrs_heart_rate_measurement_send_error_invalid_state(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.service_handle = 0,
		.conn_handle = BLE_CONN_HANDLE_INVALID,
		.rr_interval_count = 2,
		.max_hrm_len = 0,
		.is_sensor_contact_supported = true,
		.is_sensor_contact_detected = false
	};
	uint16_t heart_rate_measurement = 71;

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_disabled);

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, heart_rate_measurement);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_hrs_heart_rate_measurement_send_sensor_contact_detected(void)
{
	/* Test that sensor contact detected flag is encoded in the HRM.
	 * Flags byte layout: BIT(1) = contact detected, BIT(2) = contact supported.
	 * With both set, flags should be 0x06.
	 */
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.service_handle = 0,
		.conn_handle = BLE_CONN_HANDLE_INVALID,
		.rr_interval_count = 0,
		.max_hrm_len = 20,
		.is_sensor_contact_supported = true,
		.is_sensor_contact_detected = true
	};

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);
	__cmock_sd_ble_gatts_hvx_Stub(stub_sd_ble_gatts_hvx_capture);

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, 72);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Verify flags: BIT(2) sensor contact supported | BIT(1) sensor contact detected. */
	TEST_ASSERT_EQUAL(0x06, captured_hvx_data[0]);
}

void test_ble_hrs_heart_rate_measurement_send_16bit_heart_rate(void)
{
	/* Test that heart rate > 255 triggers 16-bit encoding in hrm_encode.
	 * 300 = 0x012C, encoded as two bytes: [0x2C, 0x01].
	 * Flags byte: BIT(0) = 16-bit HR format.
	 * Layout: [flags][hr_low][hr_high] -> len = 3.
	 */
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.service_handle = 0,
		.conn_handle = BLE_CONN_HANDLE_INVALID,
		.rr_interval_count = 0,
		.max_hrm_len = 20,
		.is_sensor_contact_supported = false,
		.is_sensor_contact_detected = false
	};

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);
	__cmock_sd_ble_gatts_hvx_Stub(stub_sd_ble_gatts_hvx_capture);

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, 300);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Verify flags: BIT(0) = 16-bit heart rate format. */
	TEST_ASSERT_EQUAL(0x01, captured_hvx_data[0]);
	/* Verify 16-bit heart rate: 300 = 0x012C -> low byte, high byte. */
	TEST_ASSERT_EQUAL(0x2C, captured_hvx_data[1]);
	TEST_ASSERT_EQUAL(0x01, captured_hvx_data[2]);
	/* flags(1) + hr(2) = 3 bytes total. */
	TEST_ASSERT_EQUAL(3, captured_hvx_len);
}

void test_ble_hrs_heart_rate_measurement_send_with_rr_intervals_encoded(void)
{
	/* Test that RR intervals are actually encoded into the HRM payload when max_hrm_len
	 * is large enough.
	 *
	 * Layout: [flags][hr][rr0_low][rr0_high][rr1_low][rr1_high] -> len = 6.
	 * 800 = 0x0320, 850 = 0x0352.
	 * Flags byte: BIT(4) = RR interval included.
	 */
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.service_handle = 0,
		.conn_handle = BLE_CONN_HANDLE_INVALID,
		.rr_interval_count = 0,
		.max_hrm_len = 20,
		.is_sensor_contact_supported = false,
		.is_sensor_contact_detected = false
	};

	/* Pre-fill two RR intervals that will fit in the encoded buffer. */
	nrf_err = ble_hrs_rr_interval_add(&hrs, 800);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	nrf_err = ble_hrs_rr_interval_add(&hrs, 850);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(2, hrs.rr_interval_count);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);
	__cmock_sd_ble_gatts_hvx_Stub(stub_sd_ble_gatts_hvx_capture);

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, 72);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Both RR intervals should have been consumed during encoding. */
	TEST_ASSERT_EQUAL(0, hrs.rr_interval_count);

	/* Verify flags: BIT(4) = RR interval included. */
	TEST_ASSERT_EQUAL(0x10, captured_hvx_data[0]);
	/* Verify heart rate. */
	TEST_ASSERT_EQUAL(72, captured_hvx_data[1]);
	/* Verify RR interval 800 = 0x0320: low byte, high byte. */
	TEST_ASSERT_EQUAL(0x20, captured_hvx_data[2]);
	TEST_ASSERT_EQUAL(0x03, captured_hvx_data[3]);
	/* Verify RR interval 850 = 0x0352: low byte, high byte. */
	TEST_ASSERT_EQUAL(0x52, captured_hvx_data[4]);
	TEST_ASSERT_EQUAL(0x03, captured_hvx_data[5]);
	/* flags(1) + hr(1) + 2*rr(2) = 6 bytes total. */
	TEST_ASSERT_EQUAL(6, captured_hvx_len);
}

void test_ble_hrs_heart_rate_measurement_send(void)
{
	uint32_t nrf_err;
	struct ble_hrs hrs = {
		.service_handle = 0,
		.conn_handle = BLE_CONN_HANDLE_INVALID,
		.rr_interval_count = 2,
		.max_hrm_len = 0,
		.is_sensor_contact_supported = true,
		.is_sensor_contact_detected = false
	};
	uint16_t heart_rate_measurement = 72;

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);
	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(hrs.conn_handle, NULL, NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_IgnoreArg_p_hvx_params();

	nrf_err = ble_hrs_heart_rate_measurement_send(&hrs, heart_rate_measurement);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

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

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
