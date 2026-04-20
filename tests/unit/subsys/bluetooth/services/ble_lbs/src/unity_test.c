/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unity.h>

#include <ble.h>
#include <ble_gatt.h>
#include <nrf_error.h>
#include <bm/bluetooth/services/ble_lbs.h>
#include <bm/bluetooth/services/uuid.h>

#include "cmock_ble.h"
#include "cmock_ble_gatts.h"

#define SERVICE_HANDLE		0x1234
#define BUTTON_VALUE_HANDLE	0xCAFE
#define BUTTON_CCCD_HANDLE	0xC0DE
#define LED_VALUE_HANDLE	0xBEEF
#define UUID_TYPE_VS		0x42
#define CONN_HANDLE		0x0055

/* An arbitrary error, to test forwarding of errors from SoftDevice calls. */
#define ERROR			0xbaadf00d

BLE_LBS_DEF(ble_lbs);

static int evt_handler_called;
static struct ble_lbs_evt last_evt;
static uint8_t expected_button_state;
static int hvx_stub_called;

static const ble_uuid128_t expected_base_uuid = {
	.uuid128 = BLE_UUID_LBS_BASE,
};

static const struct ble_lbs_config lbs_cfg_template = {
	.evt_handler = NULL,
	.sec_mode = {
		.lbs_button_char = {
			.read = { .lv = 1, .sm = 2 },
			.cccd_write = { .lv = 3, .sm = 4 },
		},
		.lbs_led_char = {
			.read = { .lv = 5, .sm = 6 },
			.write = { .lv = 7, .sm = 8 },
		},
	},
};

static uint32_t stub_sd_ble_uuid_vs_add_success(const ble_uuid128_t *p_vs_uuid,
						uint8_t *p_uuid_type, int calls)
{
	TEST_ASSERT_NOT_NULL(p_vs_uuid);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_base_uuid.uuid128, p_vs_uuid->uuid128,
				      sizeof(expected_base_uuid.uuid128));
	TEST_ASSERT_NOT_NULL(p_uuid_type);

	*p_uuid_type = UUID_TYPE_VS;

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_service_add_success(uint8_t type, const ble_uuid_t *p_uuid,
						      uint16_t *p_handle, int calls)
{
	TEST_ASSERT_EQUAL(BLE_GATTS_SRVC_TYPE_PRIMARY, type);

	TEST_ASSERT_NOT_NULL(p_uuid);
	TEST_ASSERT_EQUAL(UUID_TYPE_VS, p_uuid->type);
	TEST_ASSERT_EQUAL(BLE_UUID_LBS_SERVICE, p_uuid->uuid);

	TEST_ASSERT_NOT_NULL(p_handle);
	*p_handle = SERVICE_HANDLE;

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_characteristic_add_success(
	uint16_t service_handle, const ble_gatts_char_md_t *p_char_md,
	const ble_gatts_attr_t *p_attr_char_value, ble_gatts_char_handles_t *p_handles, int calls)
{
	const ble_gap_conn_sec_mode_t expected_button_read = { .lv = 1, .sm = 2 };
	const ble_gap_conn_sec_mode_t expected_button_cccd = { .lv = 3, .sm = 4 };
	const ble_gap_conn_sec_mode_t expected_led_read = { .lv = 5, .sm = 6 };
	const ble_gap_conn_sec_mode_t expected_led_write = { .lv = 7, .sm = 8 };
	ble_gap_conn_sec_mode_t perm_open;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&perm_open);

	TEST_ASSERT_EQUAL(SERVICE_HANDLE, service_handle);

	TEST_ASSERT_NOT_NULL(p_char_md);
	TEST_ASSERT_NOT_NULL(p_attr_char_value);
	TEST_ASSERT_NOT_NULL(p_attr_char_value->p_uuid);
	TEST_ASSERT_NOT_NULL(p_attr_char_value->p_attr_md);
	TEST_ASSERT_NOT_NULL(p_attr_char_value->p_value);
	TEST_ASSERT_NOT_NULL(p_handles);

	TEST_ASSERT_EQUAL(UUID_TYPE_VS, p_attr_char_value->p_uuid->type);
	TEST_ASSERT_EQUAL(BLE_GATTS_VLOC_STACK, p_attr_char_value->p_attr_md->vloc);
	TEST_ASSERT_EQUAL(sizeof(uint8_t), p_attr_char_value->init_len);
	TEST_ASSERT_EQUAL(sizeof(uint8_t), p_attr_char_value->max_len);
	TEST_ASSERT_EQUAL(0, *p_attr_char_value->p_value);

	if (p_attr_char_value->p_uuid->uuid == BLE_UUID_LBS_BUTTON_CHAR) {
		/* Button characteristic */

		TEST_ASSERT_TRUE(p_char_md->char_props.read);
		TEST_ASSERT_TRUE(p_char_md->char_props.notify);
		TEST_ASSERT_FALSE(p_char_md->char_props.write);

		TEST_ASSERT_NOT_NULL(p_char_md->p_cccd_md);
		TEST_ASSERT_EQUAL(BLE_GATTS_VLOC_STACK, p_char_md->p_cccd_md->vloc);
		TEST_ASSERT_EQUAL(perm_open.lv, p_char_md->p_cccd_md->read_perm.lv);
		TEST_ASSERT_EQUAL(perm_open.sm, p_char_md->p_cccd_md->read_perm.sm);
		TEST_ASSERT_EQUAL(expected_button_cccd.lv, p_char_md->p_cccd_md->write_perm.lv);
		TEST_ASSERT_EQUAL(expected_button_cccd.sm, p_char_md->p_cccd_md->write_perm.sm);

		TEST_ASSERT_EQUAL(expected_button_read.lv,
				  p_attr_char_value->p_attr_md->read_perm.lv);
		TEST_ASSERT_EQUAL(expected_button_read.sm,
				  p_attr_char_value->p_attr_md->read_perm.sm);

		p_handles->value_handle = BUTTON_VALUE_HANDLE;
		p_handles->cccd_handle = BUTTON_CCCD_HANDLE;
	} else {
		/* LED characteristic */
		TEST_ASSERT_EQUAL(BLE_UUID_LBS_LED_CHAR, p_attr_char_value->p_uuid->uuid);

		TEST_ASSERT_TRUE(p_char_md->char_props.read);
		TEST_ASSERT_TRUE(p_char_md->char_props.write);
		TEST_ASSERT_FALSE(p_char_md->char_props.notify);
		TEST_ASSERT_NULL(p_char_md->p_cccd_md);

		TEST_ASSERT_EQUAL(expected_led_read.lv,
				  p_attr_char_value->p_attr_md->read_perm.lv);
		TEST_ASSERT_EQUAL(expected_led_read.sm,
				  p_attr_char_value->p_attr_md->read_perm.sm);
		TEST_ASSERT_EQUAL(expected_led_write.lv,
				  p_attr_char_value->p_attr_md->write_perm.lv);
		TEST_ASSERT_EQUAL(expected_led_write.sm,
				  p_attr_char_value->p_attr_md->write_perm.sm);

		p_handles->value_handle = LED_VALUE_HANDLE;
	}

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_hvx_check(uint16_t conn_handle,
					    const ble_gatts_hvx_params_t *p_hvx_params, int calls)
{
	hvx_stub_called++;

	TEST_ASSERT_EQUAL(CONN_HANDLE, conn_handle);
	TEST_ASSERT_NOT_NULL(p_hvx_params);
	TEST_ASSERT_EQUAL(BLE_GATT_HVX_NOTIFICATION, p_hvx_params->type);
	TEST_ASSERT_EQUAL(BUTTON_VALUE_HANDLE, p_hvx_params->handle);
	TEST_ASSERT_NOT_NULL(p_hvx_params->p_data);
	TEST_ASSERT_NOT_NULL(p_hvx_params->p_len);
	TEST_ASSERT_EQUAL(sizeof(uint8_t), *p_hvx_params->p_len);
	TEST_ASSERT_EQUAL(expected_button_state, *p_hvx_params->p_data);

	return NRF_SUCCESS;
}

static void lbs_evt_handler(struct ble_lbs *lbs, const struct ble_lbs_evt *evt)
{
	evt_handler_called++;
	last_evt = *evt;
}

void test_ble_lbs_init_null(void)
{
	uint32_t nrf_err;
	struct ble_lbs_config cfg = lbs_cfg_template;

	cfg.evt_handler = lbs_evt_handler;

	/* NULL parameters return NRF_ERROR_NULL. */
	nrf_err = ble_lbs_init(NULL, &cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_lbs_init(&ble_lbs, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_lbs_init_error_forward(void)
{
	uint32_t nrf_err;
	struct ble_lbs_config cfg = lbs_cfg_template;

	cfg.evt_handler = lbs_evt_handler;

	/* Errors from sd_ble_uuid_vs_add are forwarded. */
	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(ERROR);
	nrf_err = ble_lbs_init(&ble_lbs, &cfg);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);

	/* Errors from sd_ble_gatts_service_add are forwarded. */
	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(ERROR);
	nrf_err = ble_lbs_init(&ble_lbs, &cfg);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);

	/* Errors from the button characteristic add are forwarded. */
	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(ERROR);
	nrf_err = ble_lbs_init(&ble_lbs, &cfg);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);

	/* Errors from the LED characteristic add are forwarded. */
	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(ERROR);
	nrf_err = ble_lbs_init(&ble_lbs, &cfg);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);
}

void test_ble_lbs_init(void)
{
	uint32_t nrf_err;
	struct ble_lbs_config cfg = lbs_cfg_template;

	cfg.evt_handler = lbs_evt_handler;

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_lbs_init(&ble_lbs, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_lbs_on_ble_evt(void)
{
	uint32_t nrf_err;
	ble_evt_t evt = { 0 };
	struct ble_lbs_config cfg = lbs_cfg_template;

	cfg.evt_handler = lbs_evt_handler;

	__cmock_sd_ble_uuid_vs_add_Stub(stub_sd_ble_uuid_vs_add_success);
	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add_success);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add_success);

	nrf_err = ble_lbs_init(&ble_lbs, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Non-write event id (default branch) is silently ignored. */
	evt.header.evt_id = BLE_GAP_EVT_CONNECTED;
	ble_lbs_on_ble_evt(&evt, &ble_lbs);
	TEST_ASSERT_EQUAL(0, evt_handler_called);

	/* Write event with mismatched handle is ignored. */
	evt.header.evt_id = BLE_GATTS_EVT_WRITE;
	evt.evt.gatts_evt.conn_handle = CONN_HANDLE;
	evt.evt.gatts_evt.params.write.handle = 0xDEAD;
	evt.evt.gatts_evt.params.write.len = 1;
	evt.evt.gatts_evt.params.write.data[0] = 0xAA;
	ble_lbs_on_ble_evt(&evt, &ble_lbs);
	TEST_ASSERT_EQUAL(0, evt_handler_called);

	/* Write event to LED handle but with wrong length is ignored. */
	evt.evt.gatts_evt.params.write.handle = LED_VALUE_HANDLE;
	evt.evt.gatts_evt.params.write.len = 0;
	ble_lbs_on_ble_evt(&evt, &ble_lbs);
	TEST_ASSERT_EQUAL(0, evt_handler_called);

	evt.evt.gatts_evt.params.write.len = 2;
	ble_lbs_on_ble_evt(&evt, &ble_lbs);
	TEST_ASSERT_EQUAL(0, evt_handler_called);

	/* Valid LED write delivers a BLE_LBS_EVT_LED_WRITE event to the handler. */
	evt.evt.gatts_evt.params.write.len = 1;
	evt.evt.gatts_evt.params.write.data[0] = 0x77;
	ble_lbs_on_ble_evt(&evt, &ble_lbs);
	TEST_ASSERT_EQUAL(1, evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_LBS_EVT_LED_WRITE, last_evt.evt_type);
	TEST_ASSERT_EQUAL(CONN_HANDLE, last_evt.conn_handle);
	TEST_ASSERT_EQUAL(0x77, last_evt.led_write.value);
}

void test_ble_lbs_on_ble_evt_no_handler(void)
{
	uint32_t nrf_err;
	ble_evt_t evt = { 0 };
	struct ble_lbs_config cfg = lbs_cfg_template;

	__cmock_sd_ble_uuid_vs_add_Stub(stub_sd_ble_uuid_vs_add_success);
	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add_success);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add_success);

	nrf_err = ble_lbs_init(&ble_lbs, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt_handler_called = 0;
	evt.evt.gatts_evt.params.write.handle = LED_VALUE_HANDLE;
	evt.evt.gatts_evt.params.write.len = 1;

	ble_lbs_on_ble_evt(&evt, &ble_lbs);
	TEST_ASSERT_EQUAL(0, evt_handler_called);
}

void test_ble_lbs_on_button_change_error_null(void)
{
	uint32_t nrf_err;

	/* NULL instance returns NRF_ERROR_NULL. */
	nrf_err = ble_lbs_on_button_change(NULL, CONN_HANDLE, 1);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_lbs_on_button_change_error_forward(void)
{
	uint32_t nrf_err;
	struct ble_lbs_config cfg = lbs_cfg_template;

	__cmock_sd_ble_uuid_vs_add_Stub(stub_sd_ble_uuid_vs_add_success);
	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add_success);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add_success);

	nrf_err = ble_lbs_init(&ble_lbs, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Errors from sd_ble_gatts_hvx are forwarded. */
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(ERROR);
	nrf_err = ble_lbs_on_button_change(&ble_lbs, CONN_HANDLE, 1);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);
}

void test_ble_lbs_on_button_change(void)
{
	uint32_t nrf_err;
	struct ble_lbs_config cfg = lbs_cfg_template;

	__cmock_sd_ble_uuid_vs_add_Stub(stub_sd_ble_uuid_vs_add_success);
	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add_success);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add_success);

	nrf_err = ble_lbs_init(&ble_lbs, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Successful notification: validates parameters via the stub. */
	expected_button_state = 0xCD;
	__cmock_sd_ble_gatts_hvx_Stub(stub_sd_ble_gatts_hvx_check);

	nrf_err = ble_lbs_on_button_change(&ble_lbs, CONN_HANDLE, expected_button_state);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, hvx_stub_called);
}

void setUp(void)
{
	memset(&ble_lbs, 0, sizeof(ble_lbs));
	memset(&last_evt, 0, sizeof(last_evt));
	evt_handler_called = 0;
	hvx_stub_called = 0;
	expected_button_state = 0;
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
