/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <unity.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <bm/bluetooth/services/ble_bas_client.h>
#include <bm/bluetooth/services/uuid.h>
#include <nrf_error.h>

#include "cmock_ble_db_discovery.h"
#include "cmock_ble_gq.h"

#define CCCD_HANDLE 0x9ABC

/* An arbitrary error, to test forwarding of errors from SoftDevice calls */
#define ERROR 0xbaadf00d

static uint16_t test_case_conn_handle = 0x1000;

BLE_GQ_DEF(gatt_queue);
BLE_BAS_CLIENT_DEF(ble_bas_client);
BLE_DB_DISCOVERY_DEF(db_disc);

static struct ble_bas_client_evt bas_client_evt;

static void ble_bas_client_evt_handler(struct ble_bas_client *bas_client,
				       const struct ble_bas_client_evt *evt)
{
	bas_client_evt = *evt;
}

static uint32_t stub_ble_gq_item_add_no_mem(const struct ble_gq *gatt_queue, struct ble_gq_req *req,
					    uint16_t conn_handle, int cmock_num_calls)
{
	struct ble_gq_evt evt = {
		.evt_type = BLE_GQ_EVT_ERROR,
		.error.reason = NRF_ERROR_NO_MEM,
	};

	req->evt_handler(req, &evt);

	return NRF_ERROR_NO_MEM;
}

static uint32_t stub_ble_gq_item_add_check_bl_handle(const struct ble_gq *gatt_queue,
						     struct ble_gq_req *req, uint16_t conn_handle,
						     int cmock_num_calls)
{
	TEST_ASSERT_EQUAL(test_case_conn_handle, req->gattc_read.handle);

	return NRF_SUCCESS;
}

void test_ble_bas_client_init_error_null(void)
{
	uint32_t nrf_err;
	struct ble_bas_client_config bas_client_config = {
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_disc,
	};

	nrf_err = ble_bas_client_init(&ble_bas_client, NULL);
	TEST_ASSERT_EQUAL(nrf_err, NRF_ERROR_NULL);

	nrf_err = ble_bas_client_init(NULL, &bas_client_config);
	TEST_ASSERT_EQUAL(nrf_err, NRF_ERROR_NULL);
}

void test_ble_bas_client_init_error_internal(void)
{
	uint32_t nrf_err;
	struct ble_bas_client_config bas_client_config = {
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_disc,
	};

	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_ERROR_INTERNAL);

	nrf_err = ble_bas_client_init(&ble_bas_client, &bas_client_config);
	TEST_ASSERT_EQUAL(nrf_err, NRF_ERROR_INTERNAL);
}

void test_ble_bas_client_init_success(void)
{
	uint32_t nrf_err;
	struct ble_bas_client_config bas_client_config = {
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_disc,
	};

	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_bas_client_init(&ble_bas_client, &bas_client_config);
	TEST_ASSERT_EQUAL(nrf_err, NRF_SUCCESS);
}

void test_ble_bas_client_handles_assign_error_null(void)
{
	uint32_t nrf_err;
	struct ble_bas_client_config cfg = {
		.db_discovery = &db_disc,
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
	};

	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_bas_client_init(&ble_bas_client, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	struct ble_gatt_db_srv discovered_service = {
		.srv_uuid = {
			.uuid = BLE_UUID_BATTERY_SERVICE,
			.type = BLE_UUID_TYPE_BLE,
		},
		.char_count = 1,
		.characteristics = {
			{
				.characteristic.uuid.uuid = BLE_UUID_BATTERY_LEVEL_CHAR,
				.characteristic.handle_value = test_case_conn_handle,
				.cccd_handle = 0x100,
			},
		},
	};

	struct ble_db_discovery_evt db_evt = {
		.conn_handle = test_case_conn_handle,
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.discovered_db = &discovered_service,
	};

	ble_bas_on_db_disc_evt(&ble_bas_client, &db_evt);
	TEST_ASSERT_EQUAL(bas_client_evt.evt_type, BLE_BAS_CLIENT_EVT_DISCOVERY_COMPLETE);

	nrf_err = ble_bas_client_handles_assign(NULL, test_case_conn_handle,
						&bas_client_evt.discovery_complete.handles);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_bas_client_handles_assign_error_no_mem(void)
{
	uint32_t nrf_err;
	struct ble_bas_client_config cfg = {
		.db_discovery = &db_disc,
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
	};

	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_ERROR_NO_MEM);
	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_bas_client_init(&ble_bas_client, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	struct ble_gatt_db_srv discovered_service = {
		.srv_uuid = {
			.uuid = BLE_UUID_BATTERY_SERVICE,
			.type = BLE_UUID_TYPE_BLE,
		},
		.char_count = 1,
		.characteristics = {
			{
				.characteristic.uuid.uuid = BLE_UUID_BATTERY_LEVEL_CHAR,
				.characteristic.handle_value = test_case_conn_handle,
				.cccd_handle = 0x100},
			},
		};

	struct ble_db_discovery_evt db_evt = {
		.conn_handle = test_case_conn_handle,
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.discovered_db = &discovered_service,
	};

	ble_bas_on_db_disc_evt(&ble_bas_client, &db_evt);
	TEST_ASSERT_EQUAL(bas_client_evt.evt_type, BLE_BAS_CLIENT_EVT_DISCOVERY_COMPLETE);

	nrf_err = ble_bas_client_handles_assign(&ble_bas_client, test_case_conn_handle,
						&bas_client_evt.discovery_complete.handles);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_ble_bas_client_handles_assign_success(void)
{
	uint32_t nrf_err;
	struct ble_bas_client_config cfg = {
		.db_discovery = &db_disc,
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
	};

	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_bas_client_init(&ble_bas_client, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	struct ble_gatt_db_srv discovered_service = {
		.srv_uuid = {
			.uuid = BLE_UUID_BATTERY_SERVICE,
			.type = BLE_UUID_TYPE_BLE,
		},
		.char_count = 1,
		.characteristics = {
			{
				.characteristic.uuid.uuid = BLE_UUID_BATTERY_LEVEL_CHAR,
				.characteristic.handle_value = test_case_conn_handle,
				.cccd_handle = 0x100},
			},
		};

	struct ble_db_discovery_evt db_evt = {
		.conn_handle = test_case_conn_handle,
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.discovered_db = &discovered_service,
	};

	ble_bas_on_db_disc_evt(&ble_bas_client, &db_evt);
	TEST_ASSERT_EQUAL(bas_client_evt.evt_type, BLE_BAS_CLIENT_EVT_DISCOVERY_COMPLETE);

	nrf_err = ble_bas_client_handles_assign(&ble_bas_client, test_case_conn_handle,
						&bas_client_evt.discovery_complete.handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_item_add_Stub(stub_ble_gq_item_add_check_bl_handle);

	nrf_err = ble_bas_client_bl_read(&ble_bas_client);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_bas_client_bl_notif_enable_success(void)
{
	uint32_t nrf_err;
	struct ble_bas_client_config cfg = {
		.db_discovery = &db_disc,
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
	};

	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_bas_client_init(&ble_bas_client, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_bas_client_handles_assign(&ble_bas_client, test_case_conn_handle, NULL);
	nrf_err = ble_bas_client_bl_notif_enable(&ble_bas_client);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_bas_client_bl_notif_enable_invalid_state(void)
{
	uint32_t nrf_err;
	struct ble_bas_client_config cfg = {
		.db_discovery = &db_disc,
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
	};

	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_bas_client_init(&ble_bas_client, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_bas_client_handles_assign(&ble_bas_client, BLE_CONN_HANDLE_INVALID, NULL);
	nrf_err = ble_bas_client_bl_notif_enable(&ble_bas_client);
	TEST_ASSERT_EQUAL(nrf_err, NRF_ERROR_INVALID_STATE);
}

void test_ble_bas_client_bl_notif_enable_error_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_bas_client_bl_notif_enable(NULL);
	TEST_ASSERT_EQUAL(nrf_err, NRF_ERROR_NULL);
}

void test_ble_bas_client_bl_notif_disable_success(void)
{
	uint32_t nrf_err;
	struct ble_bas_client_config cfg = {
		.db_discovery = &db_disc,
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
	};

	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_bas_client_init(&ble_bas_client, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_bas_client_handles_assign(&ble_bas_client, test_case_conn_handle, NULL);
	nrf_err = ble_bas_client_bl_notif_disable(&ble_bas_client);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_bas_client_bl_notif_disable_invalid_state(void)
{
	uint32_t nrf_err;
	struct ble_bas_client_config cfg = {
		.db_discovery = &db_disc,
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
	};

	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_bas_client_init(&ble_bas_client, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_bas_client_handles_assign(&ble_bas_client, BLE_CONN_HANDLE_INVALID, NULL);
	nrf_err = ble_bas_client_bl_notif_disable(&ble_bas_client);
	TEST_ASSERT_EQUAL(nrf_err, NRF_ERROR_INVALID_STATE);
}

void test_ble_bas_client_bl_notif_disable_error_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_bas_client_bl_notif_disable(NULL);
	TEST_ASSERT_EQUAL(nrf_err, NRF_ERROR_NULL);
}

void test_ble_bas_client_bl_read_success(void)
{
	uint32_t nrf_err;
	struct ble_bas_client_config cfg = {
		.db_discovery = &db_disc,
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
	};

	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_bas_client_init(&ble_bas_client, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_bas_client_handles_assign(&ble_bas_client, test_case_conn_handle, NULL);
	nrf_err = ble_bas_client_bl_read(&ble_bas_client);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_bas_client_bl_read_error_invalid_state(void)
{
	uint32_t nrf_err;
	struct ble_bas_client_config cfg = {
		.db_discovery = &db_disc,
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
	};

	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_bas_client_init(&ble_bas_client, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_bas_client_handles_assign(&ble_bas_client, BLE_CONN_HANDLE_INVALID, NULL);
	nrf_err = ble_bas_client_bl_read(&ble_bas_client);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_bas_client_bl_read_error_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_bas_client_bl_read(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_bas_client_bl_read_no_mem(void)
{
	uint32_t nrf_err;
	struct ble_bas_client_config cfg = {
		.db_discovery = &db_disc,
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
	};

	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_item_add_Stub(stub_ble_gq_item_add_no_mem);
	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_bas_client_init(&ble_bas_client, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_bas_client_handles_assign(&ble_bas_client, test_case_conn_handle, NULL);
	nrf_err = ble_bas_client_bl_read(&ble_bas_client);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_ble_bas_client_on_ble_evt_success(void)
{
	uint32_t nrf_err;
	const uint8_t test_battery_level = 0x13;
	struct ble_bas_client_config cfg = {
		.db_discovery = &db_disc,
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
	};
	ble_evt_t evt = {
		.header.evt_id = BLE_GATTC_EVT_HVX,
		.evt.gattc_evt = {
			.conn_handle = test_case_conn_handle,
			.params.hvx = {
				.len = 1,
				.data = {test_battery_level},
				.type = 0,
				.handle = test_case_conn_handle,
			},
		},
	};

	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_bas_client_init(&ble_bas_client, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	struct ble_gatt_db_srv discovered_service = {
		.srv_uuid = {
			.uuid = BLE_UUID_BATTERY_SERVICE,
			.type = BLE_UUID_TYPE_BLE,
		},
		.char_count = 1,
		.characteristics = {
			{
				.characteristic.uuid.uuid = BLE_UUID_BATTERY_LEVEL_CHAR,
				.characteristic.handle_value = test_case_conn_handle,
				.cccd_handle = 0x100,
			},
		},
	};

	struct ble_db_discovery_evt db_evt = {
		.conn_handle = test_case_conn_handle,
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.discovered_db = &discovered_service,
	};

	ble_bas_on_db_disc_evt(&ble_bas_client, &db_evt);
	TEST_ASSERT_EQUAL(bas_client_evt.evt_type, BLE_BAS_CLIENT_EVT_DISCOVERY_COMPLETE);

	nrf_err = ble_bas_client_handles_assign(&ble_bas_client, test_case_conn_handle,
						&bas_client_evt.discovery_complete.handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_bas_client_on_ble_evt(&evt, &ble_bas_client);
	TEST_ASSERT_EQUAL(BLE_BAS_CLIENT_EVT_BATT_NOTIFICATION, bas_client_evt.evt_type);
	TEST_ASSERT_EQUAL(test_battery_level, bas_client_evt.battery.level);
	TEST_ASSERT_EQUAL(test_case_conn_handle, bas_client_evt.conn_handle);

	evt.header.evt_id = BLE_GATTC_EVT_READ_RSP;
	evt.evt.gattc_evt.params.read_rsp.data[0] = test_battery_level;
	ble_bas_client_on_ble_evt(&evt, &ble_bas_client);
	TEST_ASSERT_EQUAL(BLE_BAS_CLIENT_EVT_BATT_READ_RESP, bas_client_evt.evt_type);
	TEST_ASSERT_EQUAL(test_battery_level, bas_client_evt.battery.level);
	TEST_ASSERT_EQUAL(test_case_conn_handle, bas_client_evt.conn_handle);

	evt.header.evt_id = BLE_GAP_EVT_DISCONNECTED;
	evt.evt.gap_evt.conn_handle = test_case_conn_handle;
	ble_bas_client_on_ble_evt(&evt, &ble_bas_client);
	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, ble_bas_client.conn_handle);
}

void test_ble_bas_on_db_disc_evt_invalid_handle(void)
{
	uint32_t nrf_err;
	struct ble_bas_client_config cfg = {
		.db_discovery = &db_disc,
		.evt_handler = ble_bas_client_evt_handler,
		.gatt_queue = &gatt_queue,
	};
	struct ble_gatt_db_srv discovered_service = {
		.srv_uuid = {
			.uuid = BLE_UUID_BATTERY_SERVICE,
			.type = BLE_UUID_TYPE_BLE,
		},
		.char_count = 1,
		.characteristics = {
			{
				.characteristic.uuid.uuid = BLE_UUID_BATTERY_LEVEL_CHAR,
				.characteristic.handle_value = test_case_conn_handle,
				.cccd_handle = 0x100,
			},
		},
	};
	struct ble_db_discovery_evt db_evt = {
		.conn_handle = test_case_conn_handle,
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.discovered_db = &discovered_service,
	};

	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_bas_client_init(&ble_bas_client, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_bas_client_handles_assign(&ble_bas_client, test_case_conn_handle, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_bas_on_db_disc_evt(&ble_bas_client, &db_evt);

	__cmock_ble_gq_item_add_Stub(stub_ble_gq_item_add_check_bl_handle);
	nrf_err = ble_bas_client_bl_read(&ble_bas_client);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void setUp(void)
{
	memset(&ble_bas_client, 0, sizeof(ble_bas_client));
	test_case_conn_handle++;
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
