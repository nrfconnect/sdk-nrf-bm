/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <errno.h>
#include <unity.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <bluetooth/services/ble_bas.h>
#include "ble.h"
#include "ble_err.h"
#include "ble_gatt.h"
#include "ble_gatts.h"
#include "cmock_ble_gatts.h"
#include "nrf_error.h"

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

void test_ble_bas_on_ble_evt_test(void)
{
	ble_evt_t evt = {0};

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
	evt.evt.gatts_evt.params.write.data[0] = BLE_GATT_HVX_INDICATION;
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

static uint32_t sd_ble_gatts_characteristic_add_stub(uint16_t service_handle,
						     ble_gatts_char_md_t const *p_char_md,
						     ble_gatts_attr_t const *p_attr_char_value,
						     ble_gatts_char_handles_t *p_handles, int calls)
{
	TEST_ASSERT_NOT_NULL(p_char_md);
	TEST_ASSERT_NOT_NULL(p_attr_char_value);
	TEST_ASSERT_NOT_NULL(p_handles);

	TEST_ASSERT_EQUAL(0x1234, service_handle);

	ble_gatts_attr_md_t expected_cccd_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.write_perm = {.lv = 3, .sm = 4},
	};
	ble_gatts_char_md_t expected_char_md = {
		.char_props = {
			.read = true,
			.notify = true,
		},
	};
	ble_gatts_attr_md_t expected_addr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.read_perm = {.lv = 1, .sm = 2},
	};
	uint8_t expected_bat_lvl = 55;
	ble_gatts_attr_t expected_attr_char_value = {
		.p_attr_md = &expected_addr_md,
		.p_value = &expected_bat_lvl,
	};

	TEST_ASSERT_EQUAL(expected_cccd_md.vloc, p_char_md->p_cccd_md->vloc);
	TEST_ASSERT_EQUAL(expected_cccd_md.write_perm.sm, p_char_md->p_cccd_md->write_perm.sm);
	TEST_ASSERT_EQUAL(expected_char_md.char_props.read, p_char_md->char_props.read);
	TEST_ASSERT_EQUAL(expected_char_md.char_props.notify, p_char_md->char_props.notify);
	TEST_ASSERT_EQUAL(expected_attr_char_value.p_attr_md->vloc,
			  p_attr_char_value->p_attr_md->vloc);
	TEST_ASSERT_EQUAL(expected_attr_char_value.p_attr_md->read_perm.sm,
			  p_attr_char_value->p_attr_md->read_perm.sm);
	TEST_ASSERT_EQUAL(*expected_attr_char_value.p_value, *p_attr_char_value->p_value);
	return NRF_SUCCESS;
}

static uint32_t sd_ble_gatts_descriptor_add_stub(uint16_t char_handle,
						 const ble_gatts_attr_t *p_attr, uint16_t *p_handle,
						 int calls)
{
	TEST_ASSERT_NOT_NULL(p_attr);
	TEST_ASSERT_NOT_NULL(p_handle);

	static struct {
		uint8_t report_id;
		uint8_t report_type;
	} report_ref = {.report_id = 1, .report_type = 0x01};

	ble_gatts_attr_md_t expected_attr_md = {.vloc = BLE_GATTS_VLOC_STACK,
						.read_perm = {.lv = 5, .sm = 6}};

	TEST_ASSERT_EQUAL(expected_attr_md.vloc, p_attr->p_attr_md->vloc);
	TEST_ASSERT_EQUAL(expected_attr_md.read_perm.lv, p_attr->p_attr_md->read_perm.lv);
	TEST_ASSERT_EQUAL(expected_attr_md.read_perm.sm, p_attr->p_attr_md->read_perm.sm);
	TEST_ASSERT_EQUAL(report_ref.report_id, p_attr->p_value[0]);
	TEST_ASSERT_EQUAL(report_ref.report_type, p_attr->p_value[1]);

	return NRF_SUCCESS;
}

void test_ble_bas_init_success(void)
{
	int ret;
	struct ble_bas_config bas_cfg = {
		.evt_handler = ble_bas_evt_handler,
		.can_notify = true,
		.battery_level = 55,
		.batt_rd_sec = {.lv = 1, .sm = 2},
		.cccd_wr_sec = {.lv = 3, .sm = 4},
		.report_ref_rd_sec = {.lv = 5, .sm = 6},
	};
	uint16_t expected_service_handle = 0x1234;

	static struct {
		uint8_t report_id;
		uint8_t report_type;
	} report_ref = {.report_id = 1, .report_type = 0x01};

	bas_cfg.report_ref = (void *)&report_ref;

	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_ReturnThruPtr_p_handle(&expected_service_handle);

	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_Stub(sd_ble_gatts_characteristic_add_stub);

	__cmock_sd_ble_gatts_descriptor_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_descriptor_add_Stub(sd_ble_gatts_descriptor_add_stub);

	ret = ble_bas_init(&ble_bas, &bas_cfg);
	TEST_ASSERT_EQUAL(0, ret);
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

static uint32_t sd_ble_gatts_hvx_stub(uint16_t conn_handle,
				      ble_gatts_hvx_params_t const *p_hvx_params, int num_calls)
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

	ble_bas.evt_handler = NULL;
	ble_bas.battery_level_handles.value_handle = 0x1234;
	ble_bas.can_notify = true;

	uint32_t battery_level = 44;

	ble_bas.battery_level_handles.cccd_handle = 0x5678;

	__cmock_sd_ble_gatts_value_set_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_hvx_Stub(sd_ble_gatts_hvx_stub);
	ret = ble_bas_battery_level_update(&ble_bas, conn_handle, battery_level);

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ret);
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

void test_ble_bas_battery_level_notify_success(void)
{
	int ret;
	uint16_t conn_handle = 0x0001;

	ble_bas.can_notify = true;
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	ret = ble_bas_battery_level_notify(&ble_bas, conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, ret);
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
