/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <unity.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/services/uuid.h>

#include "cmock_ble.h"
#include "cmock_ble_gap.h"
#include "cmock_ble_gattc.h"
#include "cmock_ble_gq.h"

BLE_GQ_DEF(ble_gatt_queue);
BLE_DB_DISCOVERY_DEF(db_discovery)

static struct ble_db_discovery_evt db_evt;
static struct ble_db_discovery_evt db_evt_prev;

void ble_evt_send(const ble_evt_t *evt)
{
	TYPE_SECTION_FOREACH(struct nrf_sdh_ble_evt_observer, nrf_sdh_ble_evt_observers, obs)
	{
		obs->handler(evt, obs->context);
	}
}

static void db_discovery_evt_handler(struct ble_db_discovery *db_discovery,
				     struct ble_db_discovery_evt *evt)
{
	db_evt_prev = db_evt;
	db_evt = *evt;
}

void test_ble_adv_init_error_null(void)
{
	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};
	int ret;

	ret = ble_db_discovery_init(NULL, &config);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, ret);
	ret = ble_db_discovery_init(&db_discovery, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, ret);
	config.evt_handler = NULL;
	ret = ble_db_discovery_init(&db_discovery, &config);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, ret);
}

void test_ble_db_discovery_init(void)
{
	int ret;

	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};

	ret = ble_db_discovery_init(&db_discovery, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, ret);
	TEST_ASSERT_EQUAL(0, db_discovery.num_of_handlers_reg);
	TEST_ASSERT_NOT_EQUAL(NULL, db_discovery.evt_handler);
	TEST_ASSERT_NOT_EQUAL(NULL, db_discovery.gatt_queue);
}

void test_ble_db_discovery_evt_register_null(void)
{
	uint32_t ret;

	struct ble_db_discovery db_discovery = {0};
	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};

	ret = ble_db_discovery_init(&db_discovery, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, ret);

	ret = ble_db_discovery_evt_register(&db_discovery, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, ret);
	ret = ble_db_discovery_evt_register(NULL, &hrs_uuid);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, ret);
}

void test_ble_db_discovery_evt_register_invalid_state(void)
{

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};

	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE,
			  ble_db_discovery_evt_register(&db_discovery, &hrs_uuid));
}

void test_ble_db_discovery_evt_register_no_mem(void)
{
	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_init(&db_discovery, &config));

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_IMMEDIATE_ALERT_SERVICE};

	for (size_t i = 0; i < CONFIG_BLE_DB_DISCOVERY_MAX_SRV; ++i) {
		TEST_ASSERT_EQUAL(NRF_SUCCESS,
				  ble_db_discovery_evt_register(&db_discovery, &hrs_uuid));
		hrs_uuid.uuid++;
	}
	TEST_ASSERT_EQUAL(CONFIG_BLE_DB_DISCOVERY_MAX_SRV, db_discovery.num_of_handlers_reg);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM,
			  ble_db_discovery_evt_register(&db_discovery, &hrs_uuid));
}

void test_ble_db_discovery_evt_register(void)
{
	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_init(&db_discovery, &config));

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_evt_register(&db_discovery, &hrs_uuid));
}

void test_ble_db_discovery_start_null(void)
{
	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_init(&db_discovery, &config));

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_evt_register(&db_discovery, &hrs_uuid));

	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, ble_db_discovery_start(NULL, 0));
}

void test_ble_db_discovery_start_invalid_state(void)
{
	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};

	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, ble_db_discovery_start(&db_discovery, 0));

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_init(&db_discovery, &config));

	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, ble_db_discovery_start(&db_discovery, 0));
}

void test_ble_db_discovery_start_busy(void)
{
	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_init(&db_discovery, &config));

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_evt_register(&db_discovery, &hrs_uuid));

	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_start(&db_discovery, 0));

	TEST_ASSERT_EQUAL(NRF_ERROR_BUSY, ble_db_discovery_start(&db_discovery, 0));
}

void test_ble_db_discovery_start_no_mem(void)
{
	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_init(&db_discovery, &config));

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_evt_register(&db_discovery, &hrs_uuid));

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, 8, NRF_ERROR_NO_MEM);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, ble_db_discovery_start(&db_discovery, 8));
}

void test_ble_db_discovery_start(void)
{
	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_init(&db_discovery, &config));

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_evt_register(&db_discovery, &hrs_uuid));

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, 8, NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_start(&db_discovery, 8));
}

void test_ble_db_discovery_on_ble_evt(void)
{
	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};
	ble_evt_t evt;

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, 8, NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_init(&db_discovery, &config));

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};
	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_evt_register(&db_discovery, &hrs_uuid));

	hrs_uuid.uuid = BLE_UUID_HEALTH_THERMOMETER_SERVICE;
	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_evt_register(&db_discovery, &hrs_uuid));

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_start(&db_discovery, 8));

	__cmock_ble_gq_on_ble_evt_ExpectAnyArgs();
	evt.header.evt_id = BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP;
	evt.evt.gattc_evt.conn_handle = 8;
	evt.evt.gattc_evt.params.prim_srvc_disc_rsp.services[0].uuid.uuid =
		BLE_UUID_HEART_RATE_SERVICE;
	evt.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_SUCCESS;
	evt.evt.gattc_evt.params.prim_srvc_disc_rsp.services[0].uuid.type = BLE_UUID_TYPE_BLE;
	evt.evt.gattc_evt.params.prim_srvc_disc_rsp.count = 1;
	ble_evt_send(&evt);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	__cmock_ble_gq_on_ble_evt_ExpectAnyArgs();
	evt.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_UNKNOWN;
	ble_evt_send(&evt);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	__cmock_ble_gq_on_ble_evt_ExpectAnyArgs();
	evt.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP;
	evt.evt.gattc_evt.conn_handle = 8;
	evt.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_SUCCESS;
	evt.evt.gattc_evt.params.char_disc_rsp.count = 1;
	evt.evt.gattc_evt.params.char_disc_rsp.chars[0].uuid.uuid =
		BLE_UUID_HEART_RATE_MEASUREMENT_CHAR;
	evt.evt.gattc_evt.params.char_disc_rsp.chars[0].uuid.type = BLE_UUID_TYPE_BLE;
	ble_evt_send(&evt);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_on_ble_evt_ExpectAnyArgs();
	evt.evt.gattc_evt.params.char_disc_rsp.chars[0].uuid.uuid =
		BLE_UUID_HEART_RATE_CONTROL_POINT_CHAR;
	ble_evt_send(&evt);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_on_ble_evt_ExpectAnyArgs();
	evt.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_UNKNOWN;
	ble_evt_send(&evt);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_on_ble_evt_ExpectAnyArgs();
	evt.header.evt_id = BLE_GATTC_EVT_DESC_DISC_RSP;
	evt.evt.gattc_evt.conn_handle = 8;
	evt.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_SUCCESS;
	evt.evt.gattc_evt.params.desc_disc_rsp.count = 1;
	evt.evt.gattc_evt.params.desc_disc_rsp.descs[0].handle = 8;
	evt.evt.gattc_evt.params.desc_disc_rsp.descs[0].uuid.uuid =
		BLE_UUID_DESCRIPTOR_CHAR_USER_DESC;
	evt.evt.gattc_evt.params.desc_disc_rsp.descs[0].uuid.type = BLE_UUID_TYPE_BLE;
	ble_evt_send(&evt);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	__cmock_ble_gq_on_ble_evt_ExpectAnyArgs();
	evt.evt.gattc_evt.params.desc_disc_rsp.descs[0].uuid.uuid =
		BLE_UUID_DESCRIPTOR_CHAR_EXT_PROP;
	ble_evt_send(&evt);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	__cmock_ble_gq_on_ble_evt_ExpectAnyArgs();
	evt.evt.gattc_evt.params.desc_disc_rsp.descs[0].uuid.uuid =
		BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG;
	ble_evt_send(&evt);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	__cmock_ble_gq_on_ble_evt_ExpectAnyArgs();
	evt.evt.gattc_evt.params.desc_disc_rsp.descs[0].uuid.uuid = BLE_UUID_REPORT_REF_DESCR;
	ble_evt_send(&evt);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);

	__cmock_ble_gq_on_ble_evt_ExpectAnyArgs();
	evt.header.evt_id = BLE_GAP_EVT_DISCONNECTED;
	evt.evt.gap_evt.params.disconnected.reason = BLE_HCI_CONNECTION_TIMEOUT;
	ble_evt_send(&evt);
	TEST_ASSERT_NOT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt.evt_type);
}

static uint32_t ble_gq_item_add_no_mem_stub(const struct ble_gq *gatt_queue, struct ble_gq_req *req,
					    uint16_t conn_handle, int cmock_num_calls)
{
	struct ble_gq_evt evt = {.evt_type = BLE_GQ_EVT_ERROR, .error.reason = NRF_ERROR_NO_MEM};
	req->evt_handler(req, &evt);

	return NRF_ERROR_NO_MEM;
}

void test_ble_db_discovery_on_ble_evt_no_mem(void)
{
	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};
	ble_evt_t evt;

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, 4, NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_init(&db_discovery, &config));

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};
	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_evt_register(&db_discovery, &hrs_uuid));

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_start(&db_discovery, 4));

	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_item_add_StubWithCallback(ble_gq_item_add_no_mem_stub);

	__cmock_ble_gq_on_ble_evt_ExpectAnyArgs();
	evt.header.evt_id = BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP;
	evt.evt.gattc_evt.conn_handle = 4;
	evt.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_SUCCESS;
	evt.evt.gattc_evt.params.prim_srvc_disc_rsp.services[0].uuid.uuid =
		BLE_UUID_HEART_RATE_SERVICE;
	evt.evt.gattc_evt.params.prim_srvc_disc_rsp.services[0].uuid.type = BLE_UUID_TYPE_BLE;
	evt.evt.gattc_evt.params.prim_srvc_disc_rsp.count = 1;
	ble_evt_send(&evt);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_evt_prev.evt_type);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, db_evt_prev.params.error.reason);

	__cmock_ble_gq_on_ble_evt_ExpectAnyArgs();
	evt.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP;
	evt.evt.gattc_evt.conn_handle = 8;
	evt.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_SUCCESS;
	evt.evt.gattc_evt.params.char_disc_rsp.count = 1;
	evt.evt.gattc_evt.params.char_disc_rsp.chars[0].uuid.uuid = BLE_UUID_BATTERY_LEVEL_CHAR;
	evt.evt.gattc_evt.params.char_disc_rsp.chars[0].uuid.type = BLE_UUID_TYPE_BLE;
	ble_evt_send(&evt);
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
