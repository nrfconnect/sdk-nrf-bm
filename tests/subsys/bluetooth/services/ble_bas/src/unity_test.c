/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <unity.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <ble.h>
#include <ble_err.h>
#include <ble_gatt.h>
#include <nrf_error.h>
#include <bluetooth/services/ble_bas.h>
#include <bluetooth/services/uuid.h>

#include "cmock_ble_gatts.h"

BLE_BAS_DEF(ble_bas);
static int evt_handler_called;

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

void test_ble_bas_on_ble_evt(void)
{
	ble_evt_t evt;

	ble_bas.can_notify = true;
	ble_bas.battery_level_handles.cccd_handle = 0x1234;
	evt.evt.gatts_evt.params.write.handle = 0x1234;
	evt.evt.gatts_evt.params.write.len = 2;

	evt.header.evt_id = BLE_GATTS_EVT_WRITE;
	evt.evt.gatts_evt.params.write.data[0] = BLE_GATT_HVX_NOTIFICATION;
	ble_bas.evt_handler = ble_bas_evt_handler_notif_enabled;
	ble_bas_on_ble_evt(&evt, &ble_bas);
	TEST_ASSERT_TRUE(evt_handler_called);

	evt_handler_called = false;
	evt.evt.gatts_evt.params.write.data[0] = 0x00;
	ble_bas.evt_handler = ble_bas_evt_handler_notif_disable;
	ble_bas_on_ble_evt(&evt, &ble_bas);
	TEST_ASSERT_TRUE(evt_handler_called);

	evt_handler_called = false;
	ble_bas.can_notify = false;
	ble_bas.evt_handler = ble_bas_evt_handler;
	ble_bas_on_ble_evt(&evt, &ble_bas);
	TEST_ASSERT_FALSE(evt_handler_called);

	ble_bas.can_notify = true;
	ble_bas.battery_level_handles.cccd_handle = 0x5678;
	ble_bas_on_ble_evt(&evt, &ble_bas);
	TEST_ASSERT_FALSE(evt_handler_called);

	ble_bas.battery_level_handles.cccd_handle = 0x1234;
	evt.evt.gatts_evt.params.write.len = 1;
	ble_bas_on_ble_evt(&evt, &ble_bas);
	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_ble_bas_init_efault(void)
{
	int ret;
	struct ble_bas_config bas_config = {
		.evt_handler = ble_bas_evt_handler,
	};

	ret = ble_bas_init(NULL, &bas_config);
	TEST_ASSERT_EQUAL(-EFAULT, ret);
	TEST_ASSERT_NOT_EQUAL(bas_config.evt_handler, ble_bas.evt_handler);

	ret = ble_bas_init(&ble_bas, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, ret);
	TEST_ASSERT_NOT_EQUAL(bas_config.evt_handler, ble_bas.evt_handler);
}

void test_ble_bas_init_einval(void)
{
	int ret;
	struct ble_bas_config bas_config = {
		.evt_handler = ble_bas_evt_handler,
	};

	static struct {
		uint8_t report_id;
		uint8_t report_type;
	} report_ref = {.report_id = 1, .report_type = 0x01};

	bas_config.report_ref = (void *)&report_ref;

	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	ret = ble_bas_init(&ble_bas, &bas_config);
	TEST_ASSERT_EQUAL(-EINVAL, ret);

	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	ret = ble_bas_init(&ble_bas, &bas_config);
	TEST_ASSERT_EQUAL(-EINVAL, ret);

	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	ret = ble_bas_init(&ble_bas, &bas_config);
	TEST_ASSERT_EQUAL(-EINVAL, ret);
}

uint32_t stub_sd_ble_gatts_service_add_success(uint8_t type, ble_uuid_t const *p_uuid,
					       uint16_t *p_handle, int cmock_num_calls)
{
	TEST_ASSERT_EQUAL(BLE_GATTS_SRVC_TYPE_PRIMARY, type);

	TEST_ASSERT_NOT_NULL(p_uuid);
	TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_uuid->type);
	TEST_ASSERT_EQUAL(BLE_UUID_BATTERY_SERVICE, p_uuid->uuid);

	TEST_ASSERT_NOT_NULL(p_handle);
	*p_handle = 0x1234;

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_characteristic_add_success(
	uint16_t service_handle, ble_gatts_char_md_t const *p_char_md,
	ble_gatts_attr_t const *p_attr_char_value, ble_gatts_char_handles_t *p_handles, int calls)
{
	const uint8_t expected_bat_lvl = 55;
	const ble_gap_conn_sec_mode_t perm_12 = {.lv = 1, .sm = 2};
	const ble_gap_conn_sec_mode_t perm_34 = {.lv = 3, .sm = 4};
	ble_gap_conn_sec_mode_t perm_open;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&perm_open);

	TEST_ASSERT_EQUAL(0x1234, service_handle);

	TEST_ASSERT_NOT_NULL(p_char_md);
	TEST_ASSERT_NOT_NULL(p_char_md->p_cccd_md);
	TEST_ASSERT_EQUAL(BLE_GATTS_VLOC_STACK, p_char_md->p_cccd_md->vloc);
	TEST_ASSERT_EQUAL(perm_open.lv, p_char_md->p_cccd_md->read_perm.lv);
	TEST_ASSERT_EQUAL(perm_open.sm, p_char_md->p_cccd_md->read_perm.sm);
	TEST_ASSERT_EQUAL(perm_34.lv, p_char_md->p_cccd_md->write_perm.lv);
	TEST_ASSERT_EQUAL(perm_34.sm, p_char_md->p_cccd_md->write_perm.sm);
	TEST_ASSERT_TRUE(p_char_md->char_props.read);
	TEST_ASSERT_TRUE(p_char_md->char_props.notify);

	TEST_ASSERT_NOT_NULL(p_attr_char_value);
	TEST_ASSERT_NOT_NULL(p_attr_char_value->p_attr_md);
	TEST_ASSERT_EQUAL(BLE_GATTS_VLOC_STACK, p_attr_char_value->p_attr_md->vloc);
	TEST_ASSERT_EQUAL(perm_12.lv, p_attr_char_value->p_attr_md->read_perm.lv);
	TEST_ASSERT_EQUAL(perm_12.sm, p_attr_char_value->p_attr_md->read_perm.sm);
	TEST_ASSERT_EQUAL(expected_bat_lvl, *p_attr_char_value->p_value);

	TEST_ASSERT_NOT_NULL(p_handles);
	p_handles->value_handle = 0xCAFE;

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
	} expected_report_ref = {
		.report_id = 1,
		.report_type = 0x01,
	};

	TEST_ASSERT_EQUAL(0xCAFE, char_handle);

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
	*p_handle = 0xF8EE;

	return NRF_SUCCESS;
}

void test_ble_bas_init_success(void)
{
	int ret;
	struct {
		uint8_t report_id;
		uint8_t report_type;
	} report_ref = {
		.report_id = 1,
		.report_type = 0x01,
	};
	struct ble_bas_config bas_cfg = {
		.evt_handler = ble_bas_evt_handler,
		.can_notify = true,
		.battery_level = 55,
		.batt_rd_sec = {.lv = 1, .sm = 2},
		.cccd_wr_sec = {.lv = 3, .sm = 4},
		.report_ref_rd_sec = {.lv = 5, .sm = 6},
		.report_ref = (void *)&report_ref,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add_success);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add_success);
	__cmock_sd_ble_gatts_descriptor_add_Stub(stub_sd_ble_gatts_descriptor_add_success);

	ret = ble_bas_init(&ble_bas, &bas_cfg);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_EQUAL(0x1234, ble_bas.service_handle);
	TEST_ASSERT_EQUAL(0xCAFE, ble_bas.battery_level_handles.value_handle);
	TEST_ASSERT_EQUAL(0xF8EE, ble_bas.report_ref_handle);
}

void test_ble_bas_battery_level_update_efault(void)
{
	int ret;
	uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
	uint32_t battery_level = 55;

	ret = ble_bas_battery_level_update(NULL, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(-EFAULT, ret);
}

void test_ble_bas_battery_level_update_einval(void)
{
	int ret;
	uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
	uint32_t battery_level = 55;

	__cmock_sd_ble_gatts_value_set_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	ret = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(-EINVAL, ret);

	ble_bas.can_notify = true;
	__cmock_sd_ble_gatts_value_set_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_ERROR_TIMEOUT);
	ret = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(-EINVAL, ret);
}

void test_ble_bas_battery_level_update_enotconn(void)
{
	int ret;
	uint16_t conn_handle = 0x0001;
	uint32_t battery_level = 55;

	ble_bas.can_notify = true;
	__cmock_sd_ble_gatts_value_set_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(BLE_ERROR_INVALID_CONN_HANDLE);
	ret = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(-ENOTCONN, ret);
}

void test_ble_bas_battery_level_update_epipe(void)
{
	int ret;
	uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
	uint32_t battery_level = 55;

	ble_bas.can_notify = true;
	__cmock_sd_ble_gatts_value_set_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_STATE);
	ret = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(-EPIPE, ret);
}

void test_ble_bas_battery_level_update_success(void)
{
	int ret;
	uint16_t conn_handle = 0x0007;
	uint32_t battery_level = 55;

	ble_bas.battery_level = 55;
	ret = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_EQUAL(battery_level, ble_bas.battery_level);

	memset(&ble_bas, 0, sizeof(ble_bas));
	__cmock_sd_ble_gatts_value_set_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	ret = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_EQUAL(battery_level, ble_bas.battery_level);

	memset(&ble_bas, 0, sizeof(ble_bas));
	ble_bas.can_notify = true;
	__cmock_sd_ble_gatts_value_set_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	ret = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_EQUAL(battery_level, ble_bas.battery_level);
}

static uint32_t stub_sd_ble_gatts_hvx_param_check(uint16_t conn_handle,
						  ble_gatts_hvx_params_t const *p_hvx_params,
						  int num_calls)
{
	TEST_ASSERT_NOT_NULL(p_hvx_params);
	TEST_ASSERT_EQUAL(0x1234, p_hvx_params->handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HVX_NOTIFICATION, p_hvx_params->type);
	TEST_ASSERT_EQUAL(0, p_hvx_params->offset);
	TEST_ASSERT_EQUAL(sizeof(uint8_t), *(p_hvx_params->p_len));
	TEST_ASSERT_EQUAL(44, *(uint8_t *)(p_hvx_params->p_data));
	return NRF_SUCCESS;
}

void test_ble_bas_battery_level_update_hvx_param_check(void)
{
	int ret;
	uint16_t conn_handle = 0x0007;
	uint8_t battery_level = 44;

	ble_bas.evt_handler = NULL;
	ble_bas.can_notify = true;
	ble_bas.battery_level_handles.value_handle = 0x1234;
	ble_bas.battery_level_handles.cccd_handle = 0x5678;

	__cmock_sd_ble_gatts_value_set_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_Stub(stub_sd_ble_gatts_hvx_param_check);
	ret = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);

	TEST_ASSERT_EQUAL(0, ret);
}

void test_ble_bas_battery_level_notify_efault(void)
{
	int ret;
	uint16_t conn_handle = 0x0001;

	ret = ble_bas_battery_level_notify(NULL, conn_handle);
	TEST_ASSERT_EQUAL(-EFAULT, ret);
}

void test_ble_bas_battery_level_notify_einval(void)
{
	int ret;
	uint16_t conn_handle = 0x0001;

	ble_bas.can_notify = false;
	ret = ble_bas_battery_level_notify(&ble_bas, conn_handle);
	TEST_ASSERT_EQUAL(-EINVAL, ret);

	ble_bas.can_notify = true;
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_ERROR_TIMEOUT);
	ret = ble_bas_battery_level_notify(&ble_bas, conn_handle);
	TEST_ASSERT_EQUAL(-EINVAL, ret);
}

void test_ble_bas_battery_level_notify_enotconn(void)
{
	int ret;
	uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

	ble_bas.can_notify = true;
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(BLE_ERROR_INVALID_CONN_HANDLE);
	ret = ble_bas_battery_level_notify(&ble_bas, conn_handle);
	TEST_ASSERT_EQUAL(-ENOTCONN, ret);
}

void test_ble_bas_battery_level_notify_epipe(void)
{
	int ret;
	uint16_t conn_handle = 0x0001;

	ble_bas.can_notify = true;
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_STATE);
	ret = ble_bas_battery_level_notify(&ble_bas, conn_handle);
	TEST_ASSERT_EQUAL(-EPIPE, ret);
}

static uint32_t stub_sd_ble_gatts_hvx_success(uint16_t conn_handle,
					      ble_gatts_hvx_params_t const *p_hvx_params,
					      int cmock_num_calls)
{
	const uint16_t expected_conn_handle = 0x0001;
	const uint8_t expected_bat_lvl = 42;
	const uint16_t expected_offset = 0;
	const uint16_t expected_char_value_handle = 0x0666;

	TEST_ASSERT_EQUAL(expected_conn_handle, conn_handle);

	TEST_ASSERT_NOT_NULL(p_hvx_params);
	TEST_ASSERT_EQUAL(expected_char_value_handle, p_hvx_params->handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HVX_NOTIFICATION, p_hvx_params->type);
	TEST_ASSERT_EQUAL(expected_offset, p_hvx_params->offset);
	TEST_ASSERT_EQUAL(sizeof(uint8_t), *(p_hvx_params->p_len));
	TEST_ASSERT_EQUAL(expected_bat_lvl, p_hvx_params->p_data[0]);

	return NRF_SUCCESS;
}

void test_ble_bas_battery_level_notify_success(void)
{
	int ret;
	uint16_t conn_handle = 0x0001;

	ble_bas.can_notify = true;
	ble_bas.battery_level = 42;
	ble_bas.battery_level_handles.value_handle = 0x0666;

	__cmock_sd_ble_gatts_hvx_Stub(stub_sd_ble_gatts_hvx_success);

	ret = ble_bas_battery_level_notify(&ble_bas, conn_handle);
	TEST_ASSERT_EQUAL(0, ret);
}

void setUp(void)
{
	memset(&ble_bas, 0, sizeof(ble_bas));
	evt_handler_called = false;
}

void tearDown(void)
{
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
