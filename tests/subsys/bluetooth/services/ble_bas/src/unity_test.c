/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unity.h>

#include <ble.h>
#include <ble_err.h>
#include <ble_gatt.h>
#include <nrf_error.h>
#include <bm/bluetooth/services/ble_bas.h>
#include <bm/bluetooth/services/uuid.h>

#include <observers.h>

#include "ble_gatts.h"
#include "cmock_ble_gatts.h"

#define SERVICE_HANDLE		0x1234
#define USER_DESC_HANDLE	0x5678
#define CCCD_HANDLE		0x9ABC
#define SCCD_HANDLE		0xDEF0
#define VALUE_HANDLE		0xCAFE
#define REPORT_REF_HANDLE	0xF8EE
#define INVALID_HANDLE		0xFFFF
#define BATTERY_REFERENCE_VALUE	55

BLE_BAS_DEF(ble_bas);

static uint8_t battery_level;
static int evt_handler_called;
static int hvx_stub_called;

static struct {
	uint8_t report_id;
	uint8_t report_type;
} report_ref = { .report_id = 1, .report_type = 0x01 };

static struct ble_bas_config bas_cfg_template = {
	.can_notify = true,
	.battery_level = BATTERY_REFERENCE_VALUE,
	.report_ref = (void *)&report_ref,
	.sec_mode = {
		.battery_lvl_char = {
			.read = {.lv = 1, .sm = 2},
			.cccd_write = {.lv = 3, .sm = 4},
		},
		.battery_report_ref.read = {.lv = 5, .sm = 6},
	}
};

uint32_t stub_sd_ble_gatts_service_add_success(uint8_t type, ble_uuid_t const *p_uuid,
					       uint16_t *p_handle, int cmock_num_calls)
{
	TEST_ASSERT_EQUAL(BLE_GATTS_SRVC_TYPE_PRIMARY, type);

	TEST_ASSERT_NOT_NULL(p_uuid);
	TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_uuid->type);
	TEST_ASSERT_EQUAL(BLE_UUID_BATTERY_SERVICE, p_uuid->uuid);

	TEST_ASSERT_NOT_NULL(p_handle);
	*p_handle = SERVICE_HANDLE;

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_characteristic_add_success(
	uint16_t service_handle, ble_gatts_char_md_t const *p_char_md,
	ble_gatts_attr_t const *p_attr_char_value, ble_gatts_char_handles_t *p_handles, int calls)
{
	const uint8_t expected_bat_lvl = BATTERY_REFERENCE_VALUE;
	const ble_gap_conn_sec_mode_t perm_12 = {.lv = 1, .sm = 2};
	const ble_gap_conn_sec_mode_t perm_34 = {.lv = 3, .sm = 4};
	ble_gap_conn_sec_mode_t perm_open;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&perm_open);

	TEST_ASSERT_EQUAL(SERVICE_HANDLE, service_handle);

	TEST_ASSERT_NOT_NULL(p_char_md);
	if (ble_bas.can_notify) {
		TEST_ASSERT_NOT_NULL(p_char_md->p_cccd_md);
		TEST_ASSERT_EQUAL(BLE_GATTS_VLOC_STACK, p_char_md->p_cccd_md->vloc);
		TEST_ASSERT_EQUAL(perm_open.lv, p_char_md->p_cccd_md->read_perm.lv);
		TEST_ASSERT_EQUAL(perm_open.sm, p_char_md->p_cccd_md->read_perm.sm);
		TEST_ASSERT_EQUAL(perm_34.lv, p_char_md->p_cccd_md->write_perm.lv);
		TEST_ASSERT_EQUAL(perm_34.sm, p_char_md->p_cccd_md->write_perm.sm);
		TEST_ASSERT_TRUE(p_char_md->char_props.notify);
	}
	TEST_ASSERT_TRUE(p_char_md->char_props.read);

	TEST_ASSERT_NOT_NULL(p_attr_char_value);
	TEST_ASSERT_NOT_NULL(p_attr_char_value->p_attr_md);
	TEST_ASSERT_EQUAL(BLE_GATTS_VLOC_STACK, p_attr_char_value->p_attr_md->vloc);
	TEST_ASSERT_EQUAL(perm_12.lv, p_attr_char_value->p_attr_md->read_perm.lv);
	TEST_ASSERT_EQUAL(perm_12.sm, p_attr_char_value->p_attr_md->read_perm.sm);
	TEST_ASSERT_EQUAL(expected_bat_lvl, *p_attr_char_value->p_value);

	TEST_ASSERT_NOT_NULL(p_handles);
	p_handles->value_handle = VALUE_HANDLE;
	p_handles->user_desc_handle = USER_DESC_HANDLE;
	p_handles->cccd_handle = CCCD_HANDLE;
	p_handles->sccd_handle = SCCD_HANDLE;

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_descriptor_add_success(uint16_t char_handle,
							 const ble_gatts_attr_t *p_attr,
							 uint16_t *p_handle, int calls)
{
	const ble_gap_conn_sec_mode_t perm_56 = {.lv = 5, .sm = 6};
	const struct {
		uint8_t report_id;
		uint8_t report_type;
	} expected_report_ref = { .report_id = 1, .report_type = 0x01 };

	TEST_ASSERT_EQUAL(VALUE_HANDLE, char_handle);

	TEST_ASSERT_NOT_NULL(p_attr);
	TEST_ASSERT_NOT_NULL(p_attr->p_uuid);
	TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_attr->p_uuid->type);
	TEST_ASSERT_EQUAL(BLE_UUID_REPORT_REF_DESCR, p_attr->p_uuid->uuid);
	TEST_ASSERT_NOT_NULL(p_attr->p_attr_md);
	TEST_ASSERT_EQUAL(BLE_GATTS_VLOC_STACK, p_attr->p_attr_md->vloc);
	TEST_ASSERT_EQUAL(perm_56.lv, p_attr->p_attr_md->read_perm.lv);
	TEST_ASSERT_EQUAL(perm_56.sm, p_attr->p_attr_md->read_perm.sm);

	TEST_ASSERT_EQUAL(expected_report_ref.report_id, p_attr->p_value[0]);
	TEST_ASSERT_EQUAL(expected_report_ref.report_type, p_attr->p_value[1]);

	TEST_ASSERT_NOT_NULL(p_handle);
	*p_handle = REPORT_REF_HANDLE;

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_hvx_param_check(uint16_t conn_handle,
						  ble_gatts_hvx_params_t const *p_hvx_params,
						  int num_calls)
{
	hvx_stub_called++;
	TEST_ASSERT_NOT_NULL(p_hvx_params);
	TEST_ASSERT_EQUAL(VALUE_HANDLE, p_hvx_params->handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HVX_NOTIFICATION, p_hvx_params->type);
	TEST_ASSERT_EQUAL(0, p_hvx_params->offset);
	TEST_ASSERT_EQUAL(sizeof(uint8_t), *(p_hvx_params->p_len));
	TEST_ASSERT_EQUAL(battery_level, *(uint8_t *)(p_hvx_params->p_data));
	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_value_set_check(uint16_t conn_handle,
						  uint16_t handle,
						  ble_gatts_value_t *p_value,
						  int num_calls)
{
	TEST_ASSERT_NOT_NULL(p_value);
	TEST_ASSERT_EQUAL(VALUE_HANDLE, handle);
	TEST_ASSERT_EQUAL(battery_level, *p_value->p_value);
	return NRF_SUCCESS;
}

static void ble_bas_evt_handler(struct ble_bas *bas, const struct ble_bas_evt *evt)
{
	evt_handler_called = true;
}

static void ble_bas_evt_handler_notif_enabled(struct ble_bas *bas, const struct ble_bas_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_BAS_EVT_NOTIFICATION_ENABLED, evt->evt_type);
	evt_handler_called = true;
}

static void ble_bas_evt_handler_notif_disable(struct ble_bas *bas, const struct ble_bas_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_BAS_EVT_NOTIFICATION_DISABLED, evt->evt_type);
	evt_handler_called = true;
}

void bas_init(const struct ble_bas_config *cfg)
{
	uint32_t nrf_err;

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add_success);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add_success);
	__cmock_sd_ble_gatts_descriptor_add_Stub(stub_sd_ble_gatts_descriptor_add_success);

	nrf_err = ble_bas_init(&ble_bas, cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_bas_on_ble_evt(void)
{
	ble_evt_t evt = { .header.evt_id = BLE_GATTS_EVT_WRITE };
	struct ble_bas_config bas_cfg = bas_cfg_template;

	evt.evt.gatts_evt.params.write.handle = CCCD_HANDLE;
	evt.evt.gatts_evt.params.write.len = 2;
	evt.evt.gatts_evt.params.write.data[0] = BLE_GATT_HVX_NOTIFICATION;
	bas_cfg.evt_handler = ble_bas_evt_handler_notif_enabled;
	bas_init(&bas_cfg);
	ble_evt_send(&evt);
	TEST_ASSERT_TRUE(evt_handler_called);

	evt_handler_called = false;
	evt.evt.gatts_evt.params.write.data[0] = 0x00;
	bas_cfg.evt_handler = ble_bas_evt_handler_notif_disable;
	bas_init(&bas_cfg);
	ble_evt_send(&evt);
	TEST_ASSERT_TRUE(evt_handler_called);

	evt_handler_called = false;
	evt.evt.gatts_evt.params.write.data[0] = BLE_GATT_HVX_NOTIFICATION;
	bas_cfg.can_notify = false;
	bas_cfg.evt_handler = ble_bas_evt_handler;
	bas_init(&bas_cfg);
	ble_evt_send(&evt);
	TEST_ASSERT_FALSE(evt_handler_called);

	bas_cfg.can_notify = true;
	bas_cfg.evt_handler = ble_bas_evt_handler;
	evt.evt.gatts_evt.params.write.handle = INVALID_HANDLE;
	bas_init(&bas_cfg);
	ble_evt_send(&evt);
	TEST_ASSERT_FALSE(evt_handler_called);

	evt.evt.gatts_evt.params.write.handle = CCCD_HANDLE;
	evt.evt.gatts_evt.params.write.len = 1;
	bas_init(&bas_cfg);
	ble_evt_send(&evt);
	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_ble_bas_init_error_null(void)
{
	uint32_t nrf_err;
	struct ble_bas_config bas_config = { .evt_handler = ble_bas_evt_handler };

	nrf_err = ble_bas_init(NULL, &bas_config);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
	TEST_ASSERT_NOT_EQUAL(bas_config.evt_handler, ble_bas.evt_handler);

	nrf_err = ble_bas_init(&ble_bas, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
	TEST_ASSERT_NOT_EQUAL(bas_config.evt_handler, ble_bas.evt_handler);
}

void test_ble_bas_init_error_invalid_param(void)
{
	uint32_t nrf_err;
	struct ble_bas_config bas_config = { .evt_handler = ble_bas_evt_handler };

	bas_config.report_ref = (void *)&report_ref;

	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	nrf_err = ble_bas_init(&ble_bas, &bas_config);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	nrf_err = ble_bas_init(&ble_bas, &bas_config);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	nrf_err = ble_bas_init(&ble_bas, &bas_config);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_bas_init_success(void)
{
	uint32_t nrf_err;
	struct ble_bas_config bas_cfg = bas_cfg_template;

	bas_cfg.evt_handler = ble_bas_evt_handler;

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add_success);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add_success);
	__cmock_sd_ble_gatts_descriptor_add_Stub(stub_sd_ble_gatts_descriptor_add_success);
	__cmock_sd_ble_gatts_hvx_Stub(stub_sd_ble_gatts_hvx_param_check);

	nrf_err = ble_bas_init(&ble_bas, &bas_cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	nrf_err = ble_bas_battery_level_notify(&ble_bas, SERVICE_HANDLE);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, hvx_stub_called);
}

void test_ble_bas_battery_level_update_error_null(void)
{
	uint32_t nrf_err;
	uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

	nrf_err = ble_bas_battery_level_update(NULL, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_bas_battery_level_update_error_invalid_param(void)
{
	uint32_t nrf_err;
	uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
	struct ble_bas_config bas_cfg = bas_cfg_template;

	battery_level = 42;
	bas_cfg.can_notify = false;
	bas_init(&bas_cfg);
	__cmock_sd_ble_gatts_value_set_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	nrf_err = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	bas_cfg.can_notify = true;
	bas_init(&bas_cfg);
	__cmock_sd_ble_gatts_value_set_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_ERROR_TIMEOUT);
	nrf_err = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_bas_battery_level_update_error_not_found(void)
{
	uint32_t nrf_err;
	uint16_t conn_handle = 0x0001;
	struct ble_bas_config bas_cfg = bas_cfg_template;

	battery_level = 42;
	bas_cfg.can_notify = true;
	bas_init(&bas_cfg);

	__cmock_sd_ble_gatts_value_set_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(BLE_ERROR_INVALID_CONN_HANDLE);
	nrf_err = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, nrf_err);
}

void test_ble_bas_battery_level_update_error_invalid_state(void)
{
	uint32_t nrf_err;
	uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
	struct ble_bas_config bas_cfg = bas_cfg_template;

	battery_level = 21;
	bas_cfg.evt_handler = ble_bas_evt_handler;
	bas_init(&bas_cfg);
	__cmock_sd_ble_gatts_value_set_Stub(stub_sd_ble_gatts_value_set_check);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_STATE);
	nrf_err = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_bas_battery_level_update_success(void)
{
	uint32_t nrf_err;
	uint16_t conn_handle = 0x0001;
	struct ble_bas_config bas_cfg = bas_cfg_template;

	bas_cfg.can_notify = false;
	bas_init(&bas_cfg);

	/* Battery level hasn't changed 'Nothing to do' */
	nrf_err = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Change battery level, and ble_bas should update but not notify */
	battery_level = 42;
	__cmock_sd_ble_gatts_value_set_Stub(stub_sd_ble_gatts_value_set_check);
	nrf_err = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Change battery level, and ble_bas should update and notify */
	battery_level = 84;
	bas_cfg.can_notify = true;
	bas_init(&bas_cfg);
	__cmock_sd_ble_gatts_hvx_Stub(stub_sd_ble_gatts_hvx_param_check);
	nrf_err = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, hvx_stub_called);
}

void test_ble_bas_battery_level_notify_error_null(void)
{
	uint32_t nrf_err;
	uint16_t conn_handle = 0x0001;

	nrf_err = ble_bas_battery_level_notify(NULL, conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_bas_battery_level_notify_error_invalid_param(void)
{
	uint32_t nrf_err;
	uint16_t conn_handle = 0x0001;
	struct ble_bas_config bas_cfg = bas_cfg_template;

	bas_cfg.can_notify = false;
	bas_init(&bas_cfg);

	nrf_err = ble_bas_battery_level_notify(&ble_bas, conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	bas_cfg.can_notify = true;
	bas_init(&bas_cfg);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_ERROR_TIMEOUT);
	nrf_err = ble_bas_battery_level_notify(&ble_bas, conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_bas_battery_level_notify_error_not_found(void)
{
	uint32_t nrf_err;
	uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
	struct ble_bas_config bas_cfg = bas_cfg_template;

	bas_init(&bas_cfg);

	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(BLE_ERROR_INVALID_CONN_HANDLE);
	nrf_err = ble_bas_battery_level_notify(&ble_bas, conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, nrf_err);
}

void test_ble_bas_battery_level_notify_error_invalid_state(void)
{
	uint32_t nrf_err;
	uint16_t conn_handle = 0x0001;
	struct ble_bas_config bas_cfg = bas_cfg_template;

	bas_init(&bas_cfg);

	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_STATE);
	nrf_err = ble_bas_battery_level_notify(&ble_bas, conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_bas_battery_level_notify_success(void)
{
	uint32_t nrf_err;
	uint16_t conn_handle = 0x0001;
	struct ble_bas_config bas_cfg = bas_cfg_template;

	bas_init(&bas_cfg);

	__cmock_sd_ble_gatts_hvx_Stub(stub_sd_ble_gatts_hvx_param_check);

	nrf_err = ble_bas_battery_level_notify(&ble_bas, conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void setUp(void)
{
	memset(&ble_bas, 0, sizeof(ble_bas));
	evt_handler_called = false;
	battery_level = BATTERY_REFERENCE_VALUE;
	hvx_stub_called = 0;
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
