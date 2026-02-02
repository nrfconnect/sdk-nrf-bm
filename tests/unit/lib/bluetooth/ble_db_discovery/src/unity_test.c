/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <unity.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/services/uuid.h>

#include "cmock_ble_gq.h"

BLE_DB_DISCOVERY_DEF(db_discovery);
static struct ble_gq ble_gatt_queue;

static struct ble_db_discovery_evt db_evt;
static struct ble_db_discovery_evt db_evt_prev;

static void db_discovery_evt_handler(struct ble_db_discovery *db_discovery,
				     struct ble_db_discovery_evt *evt)
{
	db_evt_prev = db_evt;
	db_evt = *evt;
}

static struct ble_db_discovery_config db_disc_config = {
	.gatt_queue = &ble_gatt_queue,
	.evt_handler = db_discovery_evt_handler,
};

static uint8_t stub_ble_gq_item_add_success_num_calls;

static uint32_t stub_ble_gq_item_add_success(const struct ble_gq *gatt_queue,
					     struct ble_gq_req *req, uint16_t conn_handle,
					     int cmock_num_calls)
{
	stub_ble_gq_item_add_success_num_calls = cmock_num_calls;
	return NRF_SUCCESS;
}

static uint32_t stub_ble_gq_item_add_success_then_no_mem(const struct ble_gq *gatt_queue,
							 struct ble_gq_req *req,
							 uint16_t conn_handle, int cmock_num_calls)
{
	struct ble_gq_evt evt = {
		.evt_type = BLE_GQ_EVT_ERROR,
		.error.reason = NRF_ERROR_NO_MEM
	};

	if (cmock_num_calls < 1) {
		return NRF_SUCCESS;
	}

	req->evt_handler(req, &evt);

	return NRF_ERROR_NO_MEM;
}

void test_ble_db_discovery_init_error_null(void)
{
	uint32_t nrf_err;
	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};

	nrf_err = ble_db_discovery_init(NULL, &config);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
	nrf_err = ble_db_discovery_init(&db_discovery, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
	config.evt_handler = NULL;
	nrf_err = ble_db_discovery_init(&db_discovery, &config);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_db_discovery_init_success(void)
{
	int nrf_err;

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_db_discovery_service_register_null(void)
{
	uint32_t nrf_err;
	struct ble_db_discovery db_discovery = {0};
	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_service_register(&db_discovery, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
	nrf_err = ble_db_discovery_service_register(NULL, &hrs_uuid);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_db_discovery_service_register_invalid_state(void)
{
	uint32_t nrf_err;
	ble_uuid_t hrs_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_HEART_RATE_SERVICE
	};

	nrf_err = ble_db_discovery_service_register(&db_discovery, &hrs_uuid);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_db_discovery_service_register_no_mem(void)
{
	uint32_t nrf_err;
	ble_uuid_t hrs_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_IMMEDIATE_ALERT_SERVICE
	};

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	for (size_t i = 0; i < CONFIG_BLE_DB_DISCOVERY_MAX_SRV; ++i) {
		nrf_err = ble_db_discovery_service_register(&db_discovery, &hrs_uuid);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
		hrs_uuid.uuid++;
	}

	nrf_err = ble_db_discovery_service_register(&db_discovery, &hrs_uuid);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_ble_db_discovery_service_register_success(void)
{
	uint32_t nrf_err;
	ble_uuid_t hrs_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_HEART_RATE_SERVICE
	};

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_service_register(&db_discovery, &hrs_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_db_discovery_start_null(void)
{
	uint32_t nrf_err;
	ble_uuid_t hrs_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_HEART_RATE_SERVICE
	};

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_service_register(&db_discovery, &hrs_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_start(NULL, 0);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_db_discovery_start_invalid_state(void)
{
	uint32_t nrf_err;

	nrf_err = ble_db_discovery_start(&db_discovery, 0);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_start(&db_discovery, 0);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_db_discovery_start_busy(void)
{
	uint32_t nrf_err;

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};

	nrf_err = ble_db_discovery_service_register(&db_discovery, &hrs_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_db_discovery_start(&db_discovery, 0);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_start(&db_discovery, 0);
	TEST_ASSERT_EQUAL(NRF_ERROR_BUSY, nrf_err);
}

void test_ble_db_discovery_start_no_mem(void)
{
	uint32_t nrf_err;
	ble_uuid_t hrs_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_HEART_RATE_SERVICE
	};

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_service_register(&db_discovery, &hrs_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, 8, NRF_ERROR_NO_MEM);

	nrf_err = ble_db_discovery_start(&db_discovery, 8);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_ble_db_discovery_start_success(void)
{
	uint32_t nrf_err;
	ble_uuid_t hrs_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_HEART_RATE_SERVICE
	};

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_service_register(&db_discovery, &hrs_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, 8, NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	nrf_err = ble_db_discovery_start(&db_discovery, 8);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_db_discovery_on_ble_evt(void)
{
	uint32_t nrf_err;
	ble_evt_t evt = {
		.header.evt_id = BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP,
		.evt.gattc_evt.conn_handle = 8,
		.evt.gattc_evt.params.prim_srvc_disc_rsp.services[0].uuid.uuid =
			BLE_UUID_HEART_RATE_SERVICE,
		.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_SUCCESS,
		.evt.gattc_evt.params.prim_srvc_disc_rsp.services[0].uuid.type = BLE_UUID_TYPE_BLE,
		.evt.gattc_evt.params.prim_srvc_disc_rsp.count = 1,
	};
	ble_uuid_t hrs_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_HEART_RATE_SERVICE
	};

	__cmock_ble_gq_item_add_Stub(stub_ble_gq_item_add_success);
	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, 8, NRF_SUCCESS);

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_service_register(&db_discovery, &hrs_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	hrs_uuid.uuid = BLE_UUID_HEALTH_THERMOMETER_SERVICE;

	nrf_err = ble_db_discovery_service_register(&db_discovery, &hrs_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_start(&db_discovery, 8);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	evt.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_UNKNOWN;
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	evt.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP;
	evt.evt.gattc_evt.conn_handle = 8;
	evt.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_SUCCESS;
	evt.evt.gattc_evt.params.char_disc_rsp.count = 1;
	evt.evt.gattc_evt.params.char_disc_rsp.chars[0].uuid.uuid =
		BLE_UUID_HEART_RATE_MEASUREMENT_CHAR;
	evt.evt.gattc_evt.params.char_disc_rsp.chars[0].uuid.type = BLE_UUID_TYPE_BLE;

	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	evt.evt.gattc_evt.params.char_disc_rsp.chars[0].uuid.uuid =
		BLE_UUID_HEART_RATE_CONTROL_POINT_CHAR;

	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	evt.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_UNKNOWN;

	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	evt.header.evt_id = BLE_GATTC_EVT_DESC_DISC_RSP;
	evt.evt.gattc_evt.conn_handle = 8;
	evt.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_SUCCESS;
	evt.evt.gattc_evt.params.desc_disc_rsp.count = 1;
	evt.evt.gattc_evt.params.desc_disc_rsp.descs[0].handle = 8;
	evt.evt.gattc_evt.params.desc_disc_rsp.descs[0].uuid.uuid =
		BLE_UUID_DESCRIPTOR_CHAR_USER_DESC;
	evt.evt.gattc_evt.params.desc_disc_rsp.descs[0].uuid.type = BLE_UUID_TYPE_BLE;
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	evt.evt.gattc_evt.params.desc_disc_rsp.descs[0].uuid.uuid =
		BLE_UUID_DESCRIPTOR_CHAR_EXT_PROP;

	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	evt.evt.gattc_evt.params.desc_disc_rsp.descs[0].uuid.uuid =
		BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG;

	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	evt.evt.gattc_evt.params.desc_disc_rsp.descs[0].uuid.uuid = BLE_UUID_REPORT_REF_DESCR;

	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	evt.header.evt_id = BLE_GAP_EVT_DISCONNECTED;
	evt.evt.gap_evt.params.disconnected.reason = BLE_HCI_CONNECTION_TIMEOUT;

	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	TEST_ASSERT_EQUAL(4, stub_ble_gq_item_add_success_num_calls);
}

void test_ble_db_discovery_on_ble_evt_no_mem(void)
{
	uint32_t nrf_err;
	ble_evt_t evt;

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, 4, NRF_SUCCESS);
	__cmock_ble_gq_item_add_Stub(stub_ble_gq_item_add_success_then_no_mem);

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};

	nrf_err = ble_db_discovery_service_register(&db_discovery, &hrs_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_start(&db_discovery, 4);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt.header.evt_id = BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP;
	evt.evt.gattc_evt.conn_handle = 4;
	evt.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_SUCCESS;
	evt.evt.gattc_evt.params.prim_srvc_disc_rsp.services[0].uuid.uuid =
		BLE_UUID_HEART_RATE_SERVICE;
	evt.evt.gattc_evt.params.prim_srvc_disc_rsp.services[0].uuid.type = BLE_UUID_TYPE_BLE;
	evt.evt.gattc_evt.params.prim_srvc_disc_rsp.count = 1;
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt_prev.evt_type);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, db_evt_prev.params.error.reason);

	evt.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP;
	evt.evt.gattc_evt.conn_handle = 8;
	evt.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_SUCCESS;
	evt.evt.gattc_evt.params.char_disc_rsp.count = 1;
	evt.evt.gattc_evt.params.char_disc_rsp.chars[0].uuid.uuid = BLE_UUID_BATTERY_LEVEL_CHAR;
	evt.evt.gattc_evt.params.char_disc_rsp.chars[0].uuid.type = BLE_UUID_TYPE_BLE;
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt_prev.evt_type);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, db_evt_prev.params.error.reason);
}

void setUp(void)
{
	memset(&db_discovery, 0, sizeof(db_discovery));
}
void tearDown(void)
{
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
