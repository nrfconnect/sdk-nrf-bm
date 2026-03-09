/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <unity.h>
#include <stdint.h>
#include <string.h>

#include <bm/bluetooth/services/ble_nus.h>
#include <nrf_error.h>

#include "cmock_ble_gatts.h"
#include "cmock_ble.h"
#include "cmock_nrf_sdh_ble.h"

/* An arbitrary error, to test forwarding of errors from SoftDevice calls */
#define ERROR 0xbaadf00d

static struct ble_nus ble_nus;
static uint16_t test_case_conn_handle = 0x1000;
static bool evt_handler_called;

/* SoftDevice function stubs replacing real service calls during testing */

static uint32_t stub_sd_ble_gatts_service_add(uint8_t type, const ble_uuid_t *p_uuid,
					      uint16_t *p_handle, int calls)
{
	ble_uuid_t expected_uuid = {
		.type = 123,
		.uuid = BLE_UUID_NUS_SERVICE,
	};

	TEST_ASSERT_EQUAL(BLE_GATTS_SRVC_TYPE_PRIMARY, type);
	TEST_ASSERT_EQUAL(expected_uuid.type, p_uuid->type);
	TEST_ASSERT_EQUAL(expected_uuid.uuid, p_uuid->uuid);
	TEST_ASSERT_NOT_NULL(p_handle);
	*p_handle = test_case_conn_handle;

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_characteristic_add(uint16_t service_handle,
						     const ble_gatts_char_md_t *p_char_md,
						     const ble_gatts_attr_t *p_attr_char_value,
						     ble_gatts_char_handles_t *p_handles, int calls)
{
	ble_uuid_t expected_char_uuid = {.type = 123};

	TEST_ASSERT_EQUAL(expected_char_uuid.type, p_attr_char_value->p_uuid->type);
	TEST_ASSERT_EQUAL(test_case_conn_handle, service_handle);

	p_handles->cccd_handle = 0x101;
	p_handles->value_handle = 0x102;

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_value_get_notif_enabled(uint16_t conn_handle, uint16_t handle,
							  ble_gatts_value_t *p_value, int calls)
{
	TEST_ASSERT_EQUAL(test_case_conn_handle, conn_handle);
	TEST_ASSERT_EQUAL(0x101, handle);

	*p_value->p_value = BLE_GATT_HVX_NOTIFICATION;

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_value_get_notif_disabled(uint16_t conn_handle, uint16_t handle,
							   ble_gatts_value_t *p_value, int calls)
{
	TEST_ASSERT_EQUAL(test_case_conn_handle, conn_handle);
	TEST_ASSERT_EQUAL(0x101, handle);

	*p_value->p_value = BLE_GATT_HVX_INDICATION;

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_value_get_err(uint16_t conn_handle, uint16_t handle,
						ble_gatts_value_t *p_value, int calls)
{
	return NRF_ERROR_INVALID_PARAM;
}

static uint32_t stub_sd_ble_gatts_value_get_sys_attr_missing(
	uint16_t conn_handle, uint16_t handle,
	ble_gatts_value_t *p_value, int calls)
{
	return BLE_ERROR_GATTS_SYS_ATTR_MISSING;
}

/* Test handlers and helpers */

static void evt_handler_comm_started(struct ble_nus *nus, const struct ble_nus_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_COMM_STARTED, evt->evt_type);
	TEST_ASSERT_EQUAL(test_case_conn_handle, evt->conn_handle);
	evt_handler_called = true;
}

static void evt_handler_comm_stopped(struct ble_nus *nus, const struct ble_nus_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_COMM_STOPPED, evt->evt_type);
	TEST_ASSERT_EQUAL(test_case_conn_handle, evt->conn_handle);
	evt_handler_called = true;
}

static void evt_handler_rx_data(struct ble_nus *nus, const struct ble_nus_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_RX_DATA, evt->evt_type);
	TEST_ASSERT_EQUAL(0xAB, evt->rx_data.data[0]);
	TEST_ASSERT_EQUAL(0xCD, evt->rx_data.data[1]);
	TEST_ASSERT_EQUAL(2, evt->rx_data.length);
	evt_handler_called = true;
}

static void evt_handler_tx_rdy(struct ble_nus *nus, const struct ble_nus_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_TX_RDY, evt->evt_type);
	TEST_ASSERT_EQUAL(test_case_conn_handle, evt->conn_handle);
	evt_handler_called = true;
}

static void nus_init(struct ble_nus_config *nus_cfg)
{
	uint32_t nrf_err;
	uint8_t expected_uuid_type = 123;

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_uuid_vs_add_ReturnThruPtr_p_uuid_type(&expected_uuid_type);

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add);

	nrf_err = ble_nus_init(&ble_nus, nus_cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL_PTR(nus_cfg->evt_handler, ble_nus.evt_handler);
}

/* ble_nus_init() tests */

void test_ble_nus_init_error_null(void)
{
	uint32_t nrf_err;
	struct ble_nus_config nus_cfg = {0};

	nrf_err = ble_nus_init(NULL, &nus_cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_nus_init(&ble_nus, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_nus_init_error_invalid_param(void)
{
	uint32_t nrf_err;
	struct ble_nus_config nus_cfg = {0};

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	nrf_err = ble_nus_init(&ble_nus, &nus_cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	nrf_err = ble_nus_init(&ble_nus, &nus_cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	nrf_err = ble_nus_init(&ble_nus, &nus_cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	nrf_err = ble_nus_init(&ble_nus, &nus_cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_nus_init_success(void)
{
	uint32_t nrf_err;
	struct ble_nus_config nus_cfg = {0};
	uint8_t expected_uuid_type = 123;

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_uuid_vs_add_ReturnThruPtr_p_uuid_type(&expected_uuid_type);

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add);

	nrf_err = ble_nus_init(&ble_nus, &nus_cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

/* on_connect() tests */

void test_on_connect_no_handler(void)
{
	const ble_evt_t ble_evt = {
		.evt.gap_evt.conn_handle = test_case_conn_handle,
		.header.evt_id = BLE_GAP_EVT_CONNECTED,
	};
	struct ble_nus_config nus_cfg = {.evt_handler = NULL};

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);

	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_on_connect_cccd_error(void)
{
	const ble_evt_t ble_evt = {
		.evt.gap_evt.conn_handle = test_case_conn_handle,
		.header.evt_id = BLE_GAP_EVT_CONNECTED,
	};
	struct ble_nus_config nus_cfg = {
		.evt_handler = evt_handler_comm_started,
	};

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_err);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);

	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_on_connect_notif_disabled(void)
{
	const ble_evt_t ble_evt = {
		.evt.gap_evt.conn_handle = test_case_conn_handle,
		.header.evt_id = BLE_GAP_EVT_CONNECTED,
	};
	struct ble_nus_config nus_cfg = {
		.evt_handler = evt_handler_comm_started,
	};

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_disabled);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);

	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_on_connect_notif_enabled(void)
{
	const ble_evt_t ble_evt = {
		.evt.gap_evt.conn_handle = test_case_conn_handle,
		.header.evt_id = BLE_GAP_EVT_CONNECTED,
	};
	struct ble_nus_config nus_cfg = {
		.evt_handler = evt_handler_comm_started,
	};

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);

	TEST_ASSERT_TRUE(evt_handler_called);
}

/* on_write() tests */

void test_on_write_cccd_notif_enabled(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_WRITE,
		.evt.gatts_evt = {
			.conn_handle = test_case_conn_handle,
			.params.write = {
				.handle = 0x101,
				.len = 2,
			},
		},
	};
	uint16_t *const cccd_val = (uint16_t *)ble_evt.evt.gatts_evt.params.write.data;
	struct ble_nus_config nus_cfg = {
		.evt_handler = evt_handler_comm_started,
	};

	nus_init(&nus_cfg);

	*cccd_val = BLE_GATT_HVX_NOTIFICATION;

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	TEST_ASSERT_TRUE(evt_handler_called);
}

void test_on_write_cccd_notif_disabled(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_WRITE,
		.evt.gatts_evt = {
			.conn_handle = test_case_conn_handle,
			.params.write = {
				.handle = 0x101,
				.len = 2,
			},
		},
	};
	uint16_t *const cccd_val = (uint16_t *)ble_evt.evt.gatts_evt.params.write.data;
	struct ble_nus_config nus_cfg = {
		.evt_handler = evt_handler_comm_stopped,
	};

	nus_init(&nus_cfg);

	*cccd_val = BLE_GATT_HVX_INDICATION;

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	TEST_ASSERT_TRUE(evt_handler_called);
}

void test_on_write_rx_data(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_WRITE,
		.evt.gatts_evt = {
			.conn_handle = test_case_conn_handle,
			.params.write = {
				.handle = 0x102,
				.len = 2,
			},
		},
	};
	uint8_t *const data = ble_evt.evt.gatts_evt.params.write.data;
	struct ble_nus_config nus_cfg = {
		.evt_handler = evt_handler_rx_data,
	};

	nus_init(&nus_cfg);

	data[0] = 0xAB;
	data[1] = 0xCD;

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	TEST_ASSERT_TRUE(evt_handler_called);
}

void test_on_write_no_handler(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_WRITE,
		.evt.gatts_evt = {
			.conn_handle = test_case_conn_handle,
			.params.write = {
				.handle = 0x101,
				.len = 2,
			},
		},
	};
	uint16_t *const cccd_val = (uint16_t *)ble_evt.evt.gatts_evt.params.write.data;
	struct ble_nus_config nus_cfg = {.evt_handler = NULL};

	nus_init(&nus_cfg);

	*cccd_val = BLE_GATT_HVX_NOTIFICATION;

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	TEST_ASSERT_FALSE(evt_handler_called);
}

/* on_hvx_tx_complete() tests */

void test_on_hvx_tx_complete_notif_enabled(void)
{
	ble_evt_t ble_evt = {
		.evt.gatts_evt.conn_handle = test_case_conn_handle,
		.header.evt_id = BLE_GATTS_EVT_HVN_TX_COMPLETE,
	};
	struct ble_nus_config nus_cfg = {
		.evt_handler = evt_handler_tx_rdy,
	};

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	TEST_ASSERT_TRUE(evt_handler_called);
}

void test_on_hvx_tx_complete_no_handler(void)
{
	ble_evt_t ble_evt = {
		.evt.gatts_evt.conn_handle = test_case_conn_handle,
		.header.evt_id = BLE_GATTS_EVT_HVN_TX_COMPLETE,
	};
	struct ble_nus_config nus_cfg = {.evt_handler = NULL};

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_on_hvx_tx_complete_cccd_error(void)
{
	ble_evt_t ble_evt = {
		.evt.gatts_evt.conn_handle = test_case_conn_handle,
		.header.evt_id = BLE_GATTS_EVT_HVN_TX_COMPLETE,
	};
	struct ble_nus_config nus_cfg = {
		.evt_handler = evt_handler_tx_rdy,
	};

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_err);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	TEST_ASSERT_FALSE(evt_handler_called);
}

/* ble_nus_data_send() tests */

void test_ble_nus_data_send_error_null(void)
{
	uint32_t nrf_err;
	uint8_t data[2];
	uint16_t length = sizeof(data);

	nrf_err = ble_nus_data_send(NULL, NULL, NULL, test_case_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_nus_data_send(&ble_nus, data, NULL, test_case_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_nus_data_send(&ble_nus, NULL, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_nus_data_send(NULL, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_nus_data_send_invalid_handle(void)
{
	uint32_t nrf_err;
	uint8_t data[2] = {0x01, 0x02};
	uint16_t length = sizeof(data);
	struct ble_nus_config nus_cfg = {0};

	nus_init(&nus_cfg);

	nrf_err = ble_nus_data_send(&ble_nus, data, &length, BLE_CONN_HANDLE_INVALID);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_nus_data_send_cccd_error(void)
{
	uint32_t nrf_err;
	uint8_t data[2] = {0x01, 0x02};
	uint16_t length = sizeof(data);
	struct ble_nus_config nus_cfg = {0};

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_err);

	nrf_err = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_nus_data_send_notify_success(void)
{
	uint32_t nrf_err;
	uint8_t data[2] = {0x01, 0x02};
	uint16_t length = sizeof(data);
	struct ble_nus_config nus_cfg = {0};

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);

	ble_gatts_hvx_params_t expected_hvx = {0};

	expected_hvx.type = BLE_GATT_HVX_NOTIFICATION;
	expected_hvx.handle = ble_nus.tx_handles.value_handle;
	expected_hvx.p_data = data;
	expected_hvx.p_len = &length;

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(test_case_conn_handle, &expected_hvx, NRF_SUCCESS);

	nrf_err = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_nus_data_send_notify_error(void)
{
	uint32_t nrf_err;
	uint8_t data[2] = {0x01, 0x02};
	uint16_t length = sizeof(data);
	struct ble_nus_config nus_cfg = {0};

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_enabled);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(ERROR);

	nrf_err = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);
}

void test_ble_nus_data_send_notif_disabled_updates_gatt_success(void)
{
	uint32_t nrf_err;
	uint8_t data[2] = {0x01, 0x02};
	uint16_t length = sizeof(data);
	struct ble_nus_config nus_cfg = {0};

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_disabled);
	__cmock_sd_ble_gatts_value_set_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_nus_data_send_notif_disabled_updates_gatt_error(void)
{
	uint32_t nrf_err;
	uint8_t data[2] = {0x01, 0x02};
	uint16_t length = sizeof(data);
	struct ble_nus_config nus_cfg = {0};

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_notif_disabled);
	__cmock_sd_ble_gatts_value_set_ExpectAnyArgsAndReturn(ERROR);

	nrf_err = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);
}

void test_ble_nus_data_send_sys_attr_missing_updates_gatt(void)
{
	uint32_t nrf_err;
	uint8_t data[2] = {0x01, 0x02};
	uint16_t length = sizeof(data);
	struct ble_nus_config nus_cfg = {0};

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_sys_attr_missing);
	__cmock_sd_ble_gatts_value_set_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

/* Unity test framework hooks and entry point */

void setUp(void)
{
	memset(&ble_nus, 0, sizeof(ble_nus));
	evt_handler_called = false;
	test_case_conn_handle++;
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
