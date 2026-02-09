/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <stdint.h>
#include <unity.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/services/uuid.h>

#include "cmock_ble_gq.h"

BLE_DB_DISCOVERY_DEF(db_discovery);
static struct ble_gq ble_gatt_queue;

static struct ble_db_discovery_evt db_disc_evt[4];
static uint32_t db_disc_evt_count;

static uint16_t test_conn_handle;

static const ble_uuid_t srv1_uuid = {.uuid = 0x7890, .type = BLE_UUID_TYPE_BLE};
static const ble_uuid_t srv1_char1_uuid = {.uuid = 0xabcd, .type = BLE_UUID_TYPE_BLE};
static const ble_uuid_t srv1_char2_uuid = {.uuid = 0xef01, .type = BLE_UUID_TYPE_BLE};
static const ble_uuid_t srv1_char3_uuid = {.uuid = 0x65cd, .type = BLE_UUID_TYPE_BLE};
static const ble_uuid_t srv1_char4_uuid = {.uuid = 0x9832, .type = BLE_UUID_TYPE_BLE};

static const ble_uuid_t srv2_uuid = {.uuid = 0x0125, .type = BLE_UUID_TYPE_BLE};
static const ble_uuid_t srv2_char1_uuid = {.uuid = 0x1234, .type = BLE_UUID_TYPE_BLE};
static const ble_uuid_t srv2_char2_uuid = {.uuid = 0x4567, .type = BLE_UUID_TYPE_BLE};

static const ble_uuid_t srv3_uuid = {.uuid = 0x3070, .type = BLE_UUID_TYPE_BLE};

static const ble_uuid_t cccd_uuid = {
	.uuid = BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG,
	.type = BLE_UUID_TYPE_BLE,
};
static const ble_uuid_t cxpd_uuid = {
	.uuid = BLE_UUID_DESCRIPTOR_CHAR_EXT_PROP,
	.type = BLE_UUID_TYPE_BLE,
};
static const ble_uuid_t cudd_uuid = {
	.uuid = BLE_UUID_DESCRIPTOR_CHAR_USER_DESC,
	.type = BLE_UUID_TYPE_BLE,
};
static const ble_uuid_t rrd_uuid = {
	.uuid = BLE_UUID_REPORT_REF_DESCR,
	.type = BLE_UUID_TYPE_BLE,
};

static void db_discovery_evt_handler(struct ble_db_discovery *db_discovery,
				     struct ble_db_discovery_evt *evt)
{
	if (db_disc_evt_count >= ARRAY_SIZE(db_disc_evt)) {
		TEST_FAIL_MESSAGE("Not enough space to store all generated db_discovery events.");
	}

	db_disc_evt[db_disc_evt_count++] = *evt;
}

static struct ble_db_discovery_config db_disc_config = {
	.gatt_queue = &ble_gatt_queue,
	.evt_handler = db_discovery_evt_handler,
};

static int stub_ble_gq_item_add_success_num_calls;

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
	config.evt_handler = db_discovery_evt_handler;
	config.gatt_queue = NULL;
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

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_service_register(&db_discovery, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
	nrf_err = ble_db_discovery_service_register(NULL, &srv1_uuid);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_db_discovery_service_register_invalid_state(void)
{
	uint32_t nrf_err;

	nrf_err = ble_db_discovery_service_register(&db_discovery, &srv1_uuid);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_db_discovery_service_register_no_mem(void)
{
	uint32_t nrf_err;
	ble_uuid_t uuid = srv1_uuid;

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Register the first service UUID twice (here and one time in the for-loop) to test
	 * multiple registration of the same service UUID.
	 */
	nrf_err = ble_db_discovery_service_register(&db_discovery, &uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Successfully register service UUIDs up to the configured upper limit. */
	for (size_t i = 0; i < CONFIG_BLE_DB_DISCOVERY_MAX_SRV; ++i) {
		nrf_err = ble_db_discovery_service_register(&db_discovery, &uuid);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
		uuid.uuid++;
	}

	/* Check that any new service UUID registration fails after reaching the upper limit. */
	nrf_err = ble_db_discovery_service_register(&db_discovery, &uuid);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);

	/* Registering a previously registered service UUID should still return success. */
	uuid = srv1_uuid;
	nrf_err = ble_db_discovery_service_register(&db_discovery, &uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_db_discovery_service_register_success(void)
{
	uint32_t nrf_err;

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_service_register(&db_discovery, &srv1_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_db_discovery_start_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_db_discovery_start(NULL, test_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_db_discovery_start_invalid_state(void)
{
	uint32_t nrf_err;

	/* Expect discovery start to fail because the instance have not been initialized. */
	nrf_err = ble_db_discovery_start(&db_discovery, test_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Expect discovery start to fail because no service UUID have been registered. */
	nrf_err = ble_db_discovery_start(&db_discovery, test_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_db_discovery_start_busy(void)
{
	uint32_t nrf_err;

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_service_register(&db_discovery, &srv1_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, test_conn_handle,
							    NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAndReturn(&ble_gatt_queue, NULL, test_conn_handle,
						NRF_SUCCESS);
	__cmock_ble_gq_item_add_IgnoreArg_req();

	/* Start discovery. */
	nrf_err = ble_db_discovery_start(&db_discovery, test_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Expect another start to fail because a discovery procedure is already ongoing. */
	nrf_err = ble_db_discovery_start(&db_discovery, test_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_BUSY, nrf_err);
}

void test_ble_db_discovery_start_no_mem(void)
{
	uint32_t nrf_err;

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_service_register(&db_discovery, &srv1_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, test_conn_handle,
							    NRF_ERROR_NO_MEM);

	nrf_err = ble_db_discovery_start(&db_discovery, test_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

static uint32_t stub_ble_gq_item_add_disc_start_success(const struct ble_gq *gq,
							struct ble_gq_req *req,
							uint16_t conn_handle,
							int cmock_num_calls)
{
	TEST_ASSERT_EQUAL_PTR(&ble_gatt_queue, gq);
	TEST_ASSERT_EQUAL(test_conn_handle, conn_handle);

	TEST_ASSERT_NOT_NULL(req);
	TEST_ASSERT_EQUAL(BLE_GQ_REQ_SRV_DISCOVERY, req->type);
	TEST_ASSERT_NOT_NULL(req->evt_handler);
	TEST_ASSERT_EQUAL_PTR(&db_discovery, req->ctx);
	TEST_ASSERT_EQUAL(srv1_uuid.uuid, req->gattc_srv_disc.srvc_uuid.uuid);
	TEST_ASSERT_EQUAL(srv1_uuid.type, req->gattc_srv_disc.srvc_uuid.type);
	TEST_ASSERT_EQUAL(CONFIG_BLE_DB_DISCOVERY_SRV_DISC_START_HANDLE,
			  req->gattc_srv_disc.start_handle);

	return NRF_SUCCESS;
}

void test_ble_db_discovery_start_success(void)
{
	uint32_t nrf_err;

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_db_discovery_service_register(&db_discovery, &srv1_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, test_conn_handle,
							    NRF_SUCCESS);
	__cmock_ble_gq_item_add_Stub(stub_ble_gq_item_add_disc_start_success);

	nrf_err = ble_db_discovery_start(&db_discovery, test_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_db_discovery_on_ble_evt_check_arg_null(void)
{
	uint32_t nrf_err;
	ble_evt_t evt = {0};

	ble_db_discovery_on_ble_evt(&evt, NULL);

	ble_db_discovery_on_ble_evt(&evt, &db_discovery);

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_db_discovery_on_ble_evt(NULL, &db_discovery);
}

static uint32_t stub_ble_gq_item_scenario_discover_two_services(
	const struct ble_gq *gatt_queue, struct ble_gq_req *req, uint16_t conn_handle,
	int cmock_num_calls)
{
	stub_ble_gq_item_add_success_num_calls = cmock_num_calls + 1;

	TEST_ASSERT_EQUAL_PTR(&ble_gatt_queue, gatt_queue);
	TEST_ASSERT_EQUAL(test_conn_handle, conn_handle);

	TEST_ASSERT_NOT_NULL(req->evt_handler);
	TEST_ASSERT_EQUAL_PTR(&db_discovery, req->ctx);

	switch (stub_ble_gq_item_add_success_num_calls) {
	case 1:
		/* Check service 1 discovery request. */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_SRV_DISCOVERY, req->type);
		TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_uuid, &req->gattc_srv_disc.srvc_uuid));
		TEST_ASSERT_EQUAL(CONFIG_BLE_DB_DISCOVERY_SRV_DISC_START_HANDLE,
				  req->gattc_srv_disc.start_handle);
		break;
	case 2:
		/* Check characteristic 1 discovery request (service 1). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_CHAR_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x0001, req->gattc_char_disc.start_handle);
		TEST_ASSERT_EQUAL(0x0005, req->gattc_char_disc.end_handle);
		break;
	case 3:
		/* Check characteristic 2 discovery request (service 1). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_CHAR_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x0004, req->gattc_char_disc.start_handle);
		TEST_ASSERT_EQUAL(0x0005, req->gattc_char_disc.end_handle);
		break;
	case 4:
		/* Check service 2 discovery request. */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_SRV_DISCOVERY, req->type);
		TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv2_uuid, &req->gattc_srv_disc.srvc_uuid));
		TEST_ASSERT_EQUAL(CONFIG_BLE_DB_DISCOVERY_SRV_DISC_START_HANDLE,
				  req->gattc_srv_disc.start_handle);
		break;
	case 5:
		/* Check characteristic 1 discovery request (service 2). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_CHAR_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x0007, req->gattc_char_disc.start_handle);
		TEST_ASSERT_EQUAL(0xFFFF, req->gattc_char_disc.end_handle);
		break;
	case 6:
		/* Check characteristic 2 discovery request (service 2). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_CHAR_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x000A, req->gattc_char_disc.start_handle);
		TEST_ASSERT_EQUAL(0xFFFF, req->gattc_char_disc.end_handle);
		break;
	case 7:
		/* Check characteristic 3 discovery request (service 2). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_CHAR_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x000C, req->gattc_char_disc.start_handle);
		TEST_ASSERT_EQUAL(0xFFFF, req->gattc_char_disc.end_handle);
		break;
	case 8:
		/* Check descriptor discovery request (service 2). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_DESC_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x000C, req->gattc_desc_disc.start_handle);
		TEST_ASSERT_EQUAL(0xFFFF, req->gattc_desc_disc.end_handle);
		break;
	default:
		TEST_FAIL();
	}
	return NRF_SUCCESS;
}

void test_scenario_discover_two_services(void)
{
	uint32_t nrf_err;
	ble_evt_t evt;
	ble_gattc_handle_range_t range;

	__cmock_ble_gq_item_add_Stub(stub_ble_gq_item_scenario_discover_two_services);
	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, test_conn_handle,
							    NRF_SUCCESS);

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Register UUID of service 1. */
	nrf_err = ble_db_discovery_service_register(&db_discovery, &srv1_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Register UUID of service 2. */
	nrf_err = ble_db_discovery_service_register(&db_discovery, &srv2_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Start Discovery. Sends a Primary Service Discovery Request. */
	nrf_err = ble_db_discovery_start(&db_discovery, test_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Primary Service Discovery Response from SoftDevice (service 1).
	 * A Characteristic Discovery Request is expected sent in response to this.
	 */
	range = (ble_gattc_handle_range_t) {.start_handle = 0x0001, .end_handle = 0x0005};
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.prim_srvc_disc_rsp = {
				.count = 1,
				.services[0] = {.uuid = srv1_uuid, .handle_range = range},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(2, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice. (char 1 of service 1).
	 * Another Characteristic Discovery Request is expected sent in response to this.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.char_disc_rsp = {
				.count = 1,
				.chars[0] = {
					.uuid = srv1_char1_uuid,
					.handle_decl = 0x0002,
					.handle_value = 0x0003,
				},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(3, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice. (char 2 of service 1).
	 * A Service Discovery Request (next service) is expected sent in response to this.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.char_disc_rsp = {
				.count = 1,
				.chars[0] = {
					.uuid = srv1_char2_uuid,
					.handle_decl = 0x0004,
					.handle_value = 0x0005,
				},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(4, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Primary Service Discovery Response from SoftDevice (service 2).
	 * A Characteristic Discovery Request is expected sent in response to this.
	 */
	range = (ble_gattc_handle_range_t) {.start_handle = 0x0007, .end_handle = 0xFFFF};
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.prim_srvc_disc_rsp = {
				.count = 1,
				.services[0] = {.uuid = srv2_uuid, .handle_range = range},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(5, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice. (char 1 of service 2).
	 * A Characteristic Discovery Request is expected sent in response to this.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.char_disc_rsp = {
				.count = 1,
				.chars[0] = {
					.uuid = srv2_char1_uuid,
					.handle_decl = 0x0008,
					.handle_value = 0x0009,
				},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(6, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice. (char 2 of service 2).
	 * A Characteristic Discovery Request is expected sent in response to this.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.char_disc_rsp = {
				.count = 1,
				.chars[0] = {
					.uuid = srv2_char2_uuid,
					.handle_decl = 0x000A,
					.handle_value = 0x000B,
				},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(7, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice. (No more chars found).
	 * A Descriptor Discovery Request is expected sent in response to this.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND,
			.conn_handle = test_conn_handle,
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(8, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate Descriptor Discovery Response from SoftDevice (No descriptors found).
	 *
	 * Discovery completed!
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_DESC_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND,
			.conn_handle = test_conn_handle,
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(8, stub_ble_gq_item_add_success_num_calls);

	/* Expect a BLE_DB_DISCOVERY_COMPLETE event for each registered service (two services).
	 * Then, expect a BLE_DB_DISCOVERY_AVAILABLE event.
	 */
	TEST_ASSERT_EQUAL(3, db_disc_evt_count);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_COMPLETE, db_disc_evt[0].evt_type);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_COMPLETE, db_disc_evt[1].evt_type);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_AVAILABLE, db_disc_evt[2].evt_type);

	const struct ble_gatt_db_srv *db_srv;
	const struct ble_gatt_db_char *db_char;

	/* Check service 1 discovery result. */
	db_srv = &db_disc_evt[0].params.discovered_db;
	TEST_ASSERT_EQUAL(test_conn_handle, db_disc_evt[0].conn_handle);
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_uuid, &db_srv->srv_uuid));
	TEST_ASSERT_EQUAL(2, db_srv->char_count);
	TEST_ASSERT_EQUAL(0x0001, db_srv->handle_range.start_handle);
	TEST_ASSERT_EQUAL(0x0005, db_srv->handle_range.end_handle);
	db_char = &db_srv->characteristics[0];
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_char1_uuid, &db_char->characteristic.uuid));
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->cccd_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->ext_prop_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->user_desc_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->report_ref_handle);
	db_char = &db_srv->characteristics[1];
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_char2_uuid, &db_char->characteristic.uuid));
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->cccd_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->ext_prop_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->user_desc_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->report_ref_handle);

	/* Check service 2 discovery result. */
	db_srv = &db_disc_evt[1].params.discovered_db;
	TEST_ASSERT_EQUAL(test_conn_handle, db_disc_evt[1].conn_handle);
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv2_uuid, &db_srv->srv_uuid));
	TEST_ASSERT_EQUAL(2, db_srv->char_count);
	TEST_ASSERT_EQUAL(0x0007, db_srv->handle_range.start_handle);
	TEST_ASSERT_EQUAL(0xFFFF, db_srv->handle_range.end_handle);
	db_char = &db_srv->characteristics[0];
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv2_char1_uuid, &db_char->characteristic.uuid));
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->cccd_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->ext_prop_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->user_desc_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->report_ref_handle);
	db_char = &db_srv->characteristics[1];
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv2_char2_uuid, &db_char->characteristic.uuid));
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->cccd_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->ext_prop_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->user_desc_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->report_ref_handle);
}

static uint32_t stub_ble_gq_item_add_scenario_discover_one_srvc_with_descs(
	const struct ble_gq *gatt_queue, struct ble_gq_req *req, uint16_t conn_handle,
	int cmock_num_calls)
{
	stub_ble_gq_item_add_success_num_calls = cmock_num_calls + 1;

	TEST_ASSERT_EQUAL_PTR(&ble_gatt_queue, gatt_queue);
	TEST_ASSERT_EQUAL(test_conn_handle, conn_handle);

	TEST_ASSERT_NOT_NULL(req->evt_handler);
	TEST_ASSERT_EQUAL_PTR(&db_discovery, req->ctx);

	switch (stub_ble_gq_item_add_success_num_calls) {
	case 1:
		/* Check service 1 discovery request. */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_SRV_DISCOVERY, req->type);
		TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_uuid, &req->gattc_srv_disc.srvc_uuid));
		TEST_ASSERT_EQUAL(CONFIG_BLE_DB_DISCOVERY_SRV_DISC_START_HANDLE,
				  req->gattc_srv_disc.start_handle);
		break;
	case 2:
		/* Check characteristic 1 discovery request (service 1). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_CHAR_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x0001, req->gattc_char_disc.start_handle);
		TEST_ASSERT_EQUAL(0x000D, req->gattc_char_disc.end_handle);
		break;
	case 3:
		/* Check characteristic 2 discovery request (service 1). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_CHAR_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x0004, req->gattc_char_disc.start_handle);
		TEST_ASSERT_EQUAL(0x000D, req->gattc_char_disc.end_handle);
		break;
	case 4:
		/* Check characteristic 3 discovery request (service 1). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_CHAR_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x0006, req->gattc_char_disc.start_handle);
		TEST_ASSERT_EQUAL(0x000D, req->gattc_char_disc.end_handle);
		break;
	case 5:
		/* Check characteristic 4 discovery request (service 1). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_CHAR_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x000B, req->gattc_char_disc.start_handle);
		TEST_ASSERT_EQUAL(0x000D, req->gattc_char_disc.end_handle);
		break;
	case 6:
		/* Check characteristic 5 discovery request (service 1). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_CHAR_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x000D, req->gattc_char_disc.start_handle);
		TEST_ASSERT_EQUAL(0x000D, req->gattc_char_disc.end_handle);
		break;
	case 7:
		/* Check descriptor discovery request (service 1, characteristic 2). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_DESC_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x0006, req->gattc_desc_disc.start_handle);
		TEST_ASSERT_EQUAL(0x0008, req->gattc_desc_disc.end_handle);
		break;
	case 8:
		/* Check descriptor discovery request (service 1, characteristic 4). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_DESC_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x000D, req->gattc_desc_disc.start_handle);
		TEST_ASSERT_EQUAL(0x000D, req->gattc_desc_disc.end_handle);
		break;
	default:
		TEST_FAIL();
	}
	return NRF_SUCCESS;
}

void test_scenario_discover_one_srvc_with_descriptors(void)
{
	uint32_t nrf_err;
	ble_evt_t evt;
	ble_gattc_handle_range_t range;

	__cmock_ble_gq_item_add_Stub(stub_ble_gq_item_add_scenario_discover_one_srvc_with_descs);
	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, test_conn_handle,
							    NRF_SUCCESS);

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Register UUID of service 1. */
	nrf_err = ble_db_discovery_service_register(&db_discovery, &srv1_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Start Discovery. Sends a Primary Service Discovery Request. */
	nrf_err = ble_db_discovery_start(&db_discovery, test_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Primary Service Discovery Response from SoftDevice (service 1).
	 * A Characteristic Discovery Request is expected sent in response to this.
	 */
	range = (ble_gattc_handle_range_t) {.start_handle = 0x0001, .end_handle = 0x000D};
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.prim_srvc_disc_rsp = {
				.count = 1,
				.services[0] = {.uuid = srv1_uuid, .handle_range = range},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(2, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice. (char 1 of service 1).
	 * Another Characteristic Discovery Request is expected sent in response to this.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.char_disc_rsp = {
				.count = 1,
				.chars[0] = {
					.uuid = srv1_char1_uuid,
					.handle_decl = 0x0002,
					.handle_value = 0x0003,
				},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(3, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice. (char 2 of service 1).
	 * Another Characteristic Discovery Request is expected sent in response to this.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.char_disc_rsp = {
				.count = 1,
				.chars[0] = {
					.uuid = srv1_char2_uuid,
					.handle_decl = 0x0004,
					.handle_value = 0x0005,
				},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(4, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice. (char 3 of service 1).
	 * Another Characteristic Discovery Request is expected sent in response to this.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.char_disc_rsp = {
				.count = 1,
				.chars[0] = {
					.uuid = srv1_char3_uuid,
					.handle_decl = 0x0009,
					.handle_value = 0x000A,
				},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(5, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice. (char 4 of service 1).
	 * Another Characteristic Discovery Request is expected sent in response to this.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.char_disc_rsp = {
				.count = 1,
				.chars[0] = {
					.uuid = srv1_char4_uuid,
					.handle_decl = 0x000B,
					.handle_value = 0x000C,
				},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(6, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice. (No more chars found).
	 * A Descriptor Discovery Request is expected sent in response to this.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND,
			.conn_handle = test_conn_handle,
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(7, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate Descriptor Discovery Response from SoftDevice (char 2 of service 1).
	 * A Descriptor Discovery Request is expected sent in response to this.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_DESC_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.desc_disc_rsp = {
				.count = 3,
			},
		},
	};
	/* The descs array is defined with only one element. It is safe to go above the first
	 * element because the ble_evt_t structure is large enough. It is not allowed to go above
	 * the first element in the array initializer, so the values are set here as a workaround.
	 */
	ble_gattc_desc_t *descs = &(evt.evt.gattc_evt.params.desc_disc_rsp.descs[0]);

	descs->uuid = cccd_uuid;
	descs->handle = 0x0006;
	descs++;
	descs->uuid = cxpd_uuid;
	descs->handle = 0x0007;
	descs++;
	descs->uuid = cudd_uuid;
	descs->handle = 0x0008;
	TEST_ASSERT_LESS_OR_EQUAL_MESSAGE((size_t)&evt + sizeof(ble_evt_t), (size_t)(descs++),
					  "Potentially unsafe writes to the descs part of ble evt");

	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(8, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate Descriptor Discovery Response from SoftDevice (char 4 of service 1).
	 *
	 * Discovery completed!
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_DESC_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.desc_disc_rsp = {
				.count = 1,
				.descs[0] = {.uuid = rrd_uuid, .handle = 0x000D},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(8, stub_ble_gq_item_add_success_num_calls);

	/* Expect a BLE_DB_DISCOVERY_COMPLETE event for each registered service (one service).
	 * Then, expect a BLE_DB_DISCOVERY_AVAILABLE event.
	 */
	TEST_ASSERT_EQUAL(2, db_disc_evt_count);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_COMPLETE, db_disc_evt[0].evt_type);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_AVAILABLE, db_disc_evt[1].evt_type);

	const struct ble_gatt_db_srv *db_srv;
	const struct ble_gatt_db_char *db_char;

	/* Check service 1 discovery result. */
	db_srv = &db_disc_evt[0].params.discovered_db;
	TEST_ASSERT_EQUAL(test_conn_handle, db_disc_evt[0].conn_handle);
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_uuid, &db_srv->srv_uuid));
	TEST_ASSERT_EQUAL(4, db_srv->char_count);
	TEST_ASSERT_EQUAL(0x0001, db_srv->handle_range.start_handle);
	TEST_ASSERT_EQUAL(0x000D, db_srv->handle_range.end_handle);
	db_char = &db_srv->characteristics[0];
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_char1_uuid, &db_char->characteristic.uuid));
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->cccd_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->ext_prop_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->user_desc_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->report_ref_handle);
	db_char = &db_srv->characteristics[1];
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_char2_uuid, &db_char->characteristic.uuid));
	TEST_ASSERT_EQUAL(0x0006, db_char->cccd_handle);
	TEST_ASSERT_EQUAL(0x0007, db_char->ext_prop_handle);
	TEST_ASSERT_EQUAL(0x0008, db_char->user_desc_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->report_ref_handle);
	db_char = &db_srv->characteristics[2];
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_char3_uuid, &db_char->characteristic.uuid));
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->cccd_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->ext_prop_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->user_desc_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->report_ref_handle);
	db_char = &db_srv->characteristics[3];
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_char4_uuid, &db_char->characteristic.uuid));
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->cccd_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->ext_prop_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->user_desc_handle);
	TEST_ASSERT_EQUAL(0x000D, db_char->report_ref_handle);
}

static uint32_t stub_ble_gq_item_add_scenario_one_of_three_services_found(
	const struct ble_gq *gatt_queue, struct ble_gq_req *req, uint16_t conn_handle,
	int cmock_num_calls)
{
	stub_ble_gq_item_add_success_num_calls = cmock_num_calls + 1;

	TEST_ASSERT_EQUAL_PTR(&ble_gatt_queue, gatt_queue);
	TEST_ASSERT_EQUAL(test_conn_handle, conn_handle);

	TEST_ASSERT_NOT_NULL(req->evt_handler);
	TEST_ASSERT_EQUAL_PTR(&db_discovery, req->ctx);

	switch (stub_ble_gq_item_add_success_num_calls) {
	case 1:
		/* Check service 1 discovery request. */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_SRV_DISCOVERY, req->type);
		TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_uuid, &req->gattc_srv_disc.srvc_uuid));
		TEST_ASSERT_EQUAL(CONFIG_BLE_DB_DISCOVERY_SRV_DISC_START_HANDLE,
				  req->gattc_srv_disc.start_handle);
		break;
	case 2:
		/* Check service 2 discovery request. */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_SRV_DISCOVERY, req->type);
		TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv2_uuid, &req->gattc_srv_disc.srvc_uuid));
		TEST_ASSERT_EQUAL(CONFIG_BLE_DB_DISCOVERY_SRV_DISC_START_HANDLE,
				  req->gattc_srv_disc.start_handle);
		break;
	case 3:
		/* Check characteristic 1 discovery request (service 2). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_CHAR_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x0010, req->gattc_char_disc.start_handle);
		TEST_ASSERT_EQUAL(0x0012, req->gattc_char_disc.end_handle);
		break;
	case 4:
		/* Check service 3 discovery request. */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_SRV_DISCOVERY, req->type);
		TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv3_uuid, &req->gattc_srv_disc.srvc_uuid));
		TEST_ASSERT_EQUAL(CONFIG_BLE_DB_DISCOVERY_SRV_DISC_START_HANDLE,
				  req->gattc_srv_disc.start_handle);
		break;
	default:
		TEST_FAIL();
	}
	return NRF_SUCCESS;
}

void test_scenario_discover_one_of_three_services_found(void)
{
	uint32_t nrf_err;
	ble_evt_t evt;
	ble_gattc_handle_range_t range;

	__cmock_ble_gq_item_add_Stub(stub_ble_gq_item_add_scenario_one_of_three_services_found);
	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, test_conn_handle,
							    NRF_SUCCESS);

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Register UUID of service 1. */
	nrf_err = ble_db_discovery_service_register(&db_discovery, &srv1_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Register UUID of service 2. */
	nrf_err = ble_db_discovery_service_register(&db_discovery, &srv2_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Register UUID of service 3. */
	nrf_err = ble_db_discovery_service_register(&db_discovery, &srv3_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Start Discovery. Sends a Primary Service Discovery Request. */
	nrf_err = ble_db_discovery_start(&db_discovery, test_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Primary Service Discovery Response from SoftDevice (service 1 not found).
	 * Another Primary Service Discovery Request is expected sent in response to this.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND,
			.conn_handle = test_conn_handle,
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(2, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Primary Service Discovery Response from SoftDevice (service 2).
	 * A Characteristic Discovery Request is expected sent in response to this.
	 */
	range = (ble_gattc_handle_range_t) {.start_handle = 0x0010, .end_handle = 0x0012};
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.prim_srvc_disc_rsp = {
				.count = 1,
				.services[0] = {.uuid = srv2_uuid, .handle_range = range},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(3, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice. (char 1 of service 2).
	 * Another Characteristic Discovery Request is expected sent in response to this.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.char_disc_rsp = {
				.count = 1,
				.chars[0] = {
					.uuid = srv2_char1_uuid,
					.handle_decl = 0x0011,
					.handle_value = 0x0012,
				},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(4, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Primary Service Discovery Response from SoftDevice (service 3 not found).
	 *
	 * Discovery completed!
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND,
			.conn_handle = test_conn_handle,
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(4, stub_ble_gq_item_add_success_num_calls);

	/* Expect a BLE_DB_DISCOVERY_COMPLETE event for each registered service (three services).
	 * Then, expect a BLE_DB_DISCOVERY_AVAILABLE event.
	 */
	TEST_ASSERT_EQUAL(4, db_disc_evt_count);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_SRV_NOT_FOUND, db_disc_evt[0].evt_type);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_COMPLETE, db_disc_evt[1].evt_type);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_SRV_NOT_FOUND, db_disc_evt[2].evt_type);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_AVAILABLE, db_disc_evt[3].evt_type);

	const struct ble_gatt_db_srv *db_srv;
	const struct ble_gatt_db_char *db_char;

	/* Check service 1 discovery result. */
	db_srv = &db_disc_evt[0].params.discovered_db;
	TEST_ASSERT_EQUAL(test_conn_handle, db_disc_evt[0].conn_handle);
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_uuid, &db_srv->srv_uuid));
	TEST_ASSERT_EQUAL(0, db_srv->char_count);

	/* Check service 2 discovery result. */
	db_srv = &db_disc_evt[1].params.discovered_db;
	TEST_ASSERT_EQUAL(test_conn_handle, db_disc_evt[1].conn_handle);
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv2_uuid, &db_srv->srv_uuid));
	TEST_ASSERT_EQUAL(1, db_srv->char_count);
	TEST_ASSERT_EQUAL(0x0010, db_srv->handle_range.start_handle);
	TEST_ASSERT_EQUAL(0x0012, db_srv->handle_range.end_handle);
	db_char = &db_srv->characteristics[0];
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv2_char1_uuid, &db_char->characteristic.uuid));
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->cccd_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->ext_prop_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->user_desc_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->report_ref_handle);

	/* Check service 3 discovery result. */
	db_srv = &db_disc_evt[2].params.discovered_db;
	TEST_ASSERT_EQUAL(test_conn_handle, db_disc_evt[2].conn_handle);
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv3_uuid, &db_srv->srv_uuid));
	TEST_ASSERT_EQUAL(0, db_srv->char_count);
}

static uint32_t stub_ble_gq_item_add_scenario_ignore_other_conn_handles(
	const struct ble_gq *gatt_queue, struct ble_gq_req *req, uint16_t conn_handle,
	int cmock_num_calls)
{
	stub_ble_gq_item_add_success_num_calls = cmock_num_calls + 1;

	TEST_ASSERT_EQUAL_PTR(&ble_gatt_queue, gatt_queue);
	TEST_ASSERT_EQUAL(test_conn_handle, conn_handle);

	TEST_ASSERT_NOT_NULL(req->evt_handler);
	TEST_ASSERT_EQUAL_PTR(&db_discovery, req->ctx);

	switch (stub_ble_gq_item_add_success_num_calls) {
	case 1:
		/* Check service 1 discovery request. */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_SRV_DISCOVERY, req->type);
		TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_uuid, &req->gattc_srv_disc.srvc_uuid));
		TEST_ASSERT_EQUAL(CONFIG_BLE_DB_DISCOVERY_SRV_DISC_START_HANDLE,
				  req->gattc_srv_disc.start_handle);
		break;
	case 2:
		/* Check characteristic 1 discovery request (service 1). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_CHAR_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x0020, req->gattc_char_disc.start_handle);
		TEST_ASSERT_EQUAL(0x0022, req->gattc_char_disc.end_handle);
		break;
	default:
		TEST_FAIL();
	}
	return NRF_SUCCESS;
}

void test_scenario_ignore_discovery_responses_for_other_conn_handles(void)
{
	uint32_t nrf_err;
	ble_evt_t evt;
	ble_gattc_handle_range_t range;
	const uint16_t conn_handle_ignore = 0x0432;

	__cmock_ble_gq_item_add_Stub(stub_ble_gq_item_add_scenario_ignore_other_conn_handles);
	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, test_conn_handle,
							    NRF_SUCCESS);

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Register UUID of service 1. */
	nrf_err = ble_db_discovery_service_register(&db_discovery, &srv1_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Start Discovery. Sends a Primary Service Discovery Request. */
	nrf_err = ble_db_discovery_start(&db_discovery, test_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Primary Service Discovery Response from SoftDevice (service 1 found).
	 * A Characteristic Discovery Request is expected sent in response to this.
	 */
	range = (ble_gattc_handle_range_t) {.start_handle = 0x0020, .end_handle = 0x0022};
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.prim_srvc_disc_rsp = {
				.count = 1,
				.services[0] = {.uuid = srv1_uuid, .handle_range = range},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(2, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice. (incorrect conn_handle).
	 * This event should be ignored. The discovery was not started for this conn_handle.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = conn_handle_ignore,
			.params.char_disc_rsp = {
				.count = 1,
				.chars[0] = {
					.uuid = srv2_char2_uuid,
					.handle_decl = 0x0011,
					.handle_value = 0x0012,
				},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(2, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Descriptor Discovery Response from SoftDevice (incorrect conn_handle).
	 * This event should be ignored. The discovery was not started for this conn_handle.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_DESC_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = conn_handle_ignore,
			.params.desc_disc_rsp = {
				.count = 1,
				.descs[0] = {.uuid = cccd_uuid, .handle = 0x0013},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(2, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Primary Service Discovery Response from SoftDevice (incorrect conn_handle).
	 * This event should be ignored. The discovery was not started for this conn_handle.
	 */
	range = (ble_gattc_handle_range_t) {.start_handle = 0x0020, .end_handle = 0x0030};
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = conn_handle_ignore,
			.params.prim_srvc_disc_rsp = {
				.count = 1,
				.services[0] = {.uuid = srv3_uuid, .handle_range = range},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(2, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice. (char 1 of service 1).
	 *
	 * Discovery completed!
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.char_disc_rsp = {
				.count = 1,
				.chars[0] = {
					.uuid = srv1_char1_uuid,
					.handle_decl = 0x0021,
					.handle_value = 0x0022,
				},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);

	/* Expect a BLE_DB_DISCOVERY_COMPLETE event for each registered service (one service).
	 * Then, expect a BLE_DB_DISCOVERY_AVAILABLE event.
	 */
	TEST_ASSERT_EQUAL(2, db_disc_evt_count);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_COMPLETE, db_disc_evt[0].evt_type);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_AVAILABLE, db_disc_evt[1].evt_type);

	const struct ble_gatt_db_srv *db_srv;
	const struct ble_gatt_db_char *db_char;

	/* Check service 1 discovery result. */
	db_srv = &db_disc_evt[0].params.discovered_db;
	TEST_ASSERT_EQUAL(test_conn_handle, db_disc_evt[0].conn_handle);
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_uuid, &db_srv->srv_uuid));
	TEST_ASSERT_EQUAL(1, db_srv->char_count);
	TEST_ASSERT_EQUAL(0x0020, db_srv->handle_range.start_handle);
	TEST_ASSERT_EQUAL(0x0022, db_srv->handle_range.end_handle);
	db_char = &db_srv->characteristics[0];
	TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_char1_uuid, &db_char->characteristic.uuid));
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->cccd_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->ext_prop_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->user_desc_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, db_char->report_ref_handle);
}

static uint32_t stub_ble_gq_item_add_scenario_disconnect_during_discovery(
	const struct ble_gq *gatt_queue, struct ble_gq_req *req, uint16_t conn_handle,
	int cmock_num_calls)
{
	stub_ble_gq_item_add_success_num_calls = cmock_num_calls + 1;

	TEST_ASSERT_EQUAL_PTR(&ble_gatt_queue, gatt_queue);
	TEST_ASSERT_EQUAL(test_conn_handle, conn_handle);

	TEST_ASSERT_NOT_NULL(req->evt_handler);
	TEST_ASSERT_EQUAL_PTR(&db_discovery, req->ctx);

	switch (stub_ble_gq_item_add_success_num_calls) {
	case 1:
	case 4:
		/* Check service 1 discovery request. */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_SRV_DISCOVERY, req->type);
		TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_uuid, &req->gattc_srv_disc.srvc_uuid));
		TEST_ASSERT_EQUAL(CONFIG_BLE_DB_DISCOVERY_SRV_DISC_START_HANDLE,
				  req->gattc_srv_disc.start_handle);
		break;
	case 2:
		/* Check characteristic 1 discovery request (service 1). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_CHAR_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x0020, req->gattc_char_disc.start_handle);
		TEST_ASSERT_EQUAL(0x0024, req->gattc_char_disc.end_handle);
		break;
	case 3:
		/* Check characteristic 2 discovery request (service 1). */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_CHAR_DISCOVERY, req->type);
		TEST_ASSERT_EQUAL(0x0023, req->gattc_char_disc.start_handle);
		TEST_ASSERT_EQUAL(0x0024, req->gattc_char_disc.end_handle);
		break;
	default:
		TEST_FAIL();
	}
	return NRF_SUCCESS;
}

void test_scenario_disconnect_during_discovery(void)
{
	uint32_t nrf_err;
	ble_evt_t evt;
	ble_gattc_handle_range_t range;

	__cmock_ble_gq_item_add_Stub(stub_ble_gq_item_add_scenario_disconnect_during_discovery);
	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, test_conn_handle,
							    NRF_SUCCESS);

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Register UUID of service 1. */
	nrf_err = ble_db_discovery_service_register(&db_discovery, &srv1_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Start Discovery. Sends a Primary Service Discovery Request. */
	nrf_err = ble_db_discovery_start(&db_discovery, test_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Primary Service Discovery Response from SoftDevice (service 1 found).
	 * A Characteristic Discovery Request is expected sent in response to this.
	 */
	range = (ble_gattc_handle_range_t) {.start_handle = 0x0020, .end_handle = 0x0024};
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.prim_srvc_disc_rsp = {
				.count = 1,
				.services[0] = {.uuid = srv1_uuid, .handle_range = range},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(2, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice. (char 1 of service 1).
	 * Another Characteristic Discovery Request is expected sent in response to this.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.char_disc_rsp = {
				.count = 1,
				.chars[0] = {
					.uuid = srv1_char1_uuid,
					.handle_decl = 0x0021,
					.handle_value = 0x0022,
				},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(3, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Disconnected event from SoftDevice.
	 * Expect ongoing discovery to be stopped.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GAP_EVT_DISCONNECTED,
		.evt.gap_evt = {
			.conn_handle = test_conn_handle,
			.params.disconnected = {
				.reason = BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION,
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(3, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Check that a new discovery can be started. */
	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, test_conn_handle,
							    NRF_SUCCESS);

	nrf_err = ble_db_discovery_start(&db_discovery, test_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(4, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);
}

static uint32_t stub_ble_gq_item_add_success_then_no_mem(
	const struct ble_gq *gatt_queue, struct ble_gq_req *req, uint16_t conn_handle,
	int cmock_num_calls)
{
	struct ble_gq_evt evt = {
		.evt_type = BLE_GQ_EVT_ERROR,
		.error.reason = NRF_ERROR_NO_MEM,
	};

	stub_ble_gq_item_add_success_num_calls = cmock_num_calls + 1;

	switch (stub_ble_gq_item_add_success_num_calls) {
	case 1:
	case 3:
		/* Check service 1 discovery request. */
		TEST_ASSERT_EQUAL(BLE_GQ_REQ_SRV_DISCOVERY, req->type);
		TEST_ASSERT_TRUE(BLE_UUID_EQ(&srv1_uuid, &req->gattc_srv_disc.srvc_uuid));
		TEST_ASSERT_EQUAL(CONFIG_BLE_DB_DISCOVERY_SRV_DISC_START_HANDLE,
				  req->gattc_srv_disc.start_handle);
		return NRF_SUCCESS;
	case 2:
		return NRF_ERROR_NO_MEM;
	case 4:
		req->evt_handler(req, &evt);
		return NRF_SUCCESS;
	default:
		TEST_FAIL();
	}
}

void test_scenario_ble_gq_item_add_no_mem(void)
{
	uint32_t nrf_err;
	ble_evt_t evt;
	ble_gattc_handle_range_t range;
	const uint16_t conn_handle_2 = 0x0432;

	__cmock_ble_gq_item_add_Stub(stub_ble_gq_item_add_success_then_no_mem);
	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, test_conn_handle,
							    NRF_SUCCESS);

	nrf_err = ble_db_discovery_init(&db_discovery, &db_disc_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Register UUID of service 1. */
	nrf_err = ble_db_discovery_service_register(&db_discovery, &srv1_uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Start Discovery. Sends a Primary Service Discovery Request. */
	nrf_err = ble_db_discovery_start(&db_discovery, test_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(0, db_disc_evt_count);

	/* Simulate a Primary Service Discovery Response from SoftDevice (service 1 found).
	 * Fail to send another request using ble_gq, reason no memory.
	 */
	range = (ble_gattc_handle_range_t) {.start_handle = 0x0020, .end_handle = 0x0024};
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = test_conn_handle,
			.params.prim_srvc_disc_rsp = {
				.count = 1,
				.services[0] = {.uuid = srv2_uuid, .handle_range = range},
			},
		},
	};

	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(2, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(2, db_disc_evt_count);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_disc_evt[0].evt_type);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, db_disc_evt[0].params.error.reason);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_AVAILABLE, db_disc_evt[1].evt_type);

	/* Restart Discovery. Sends a Primary Service Discovery Request. */
	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&ble_gatt_queue, conn_handle_2,
							    NRF_SUCCESS);

	nrf_err = ble_db_discovery_start(&db_discovery, conn_handle_2);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(3, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(2, db_disc_evt_count);

	/* Simulate a Characteristic Discovery Response from SoftDevice.
	 * Fail to send another request using ble_gq, reason no memory.
	 */
	evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_CHAR_DISC_RSP,
		.evt.gattc_evt = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.conn_handle = conn_handle_2,
			.params.char_disc_rsp = {
				.count = 1,
				.chars[0] = {
					.uuid = srv2_char1_uuid,
					.handle_decl = 0x0021,
					.handle_value = 0x0022,
				},
			},
		},
	};
	ble_db_discovery_on_ble_evt(&evt, &db_discovery);
	TEST_ASSERT_EQUAL(4, stub_ble_gq_item_add_success_num_calls);
	TEST_ASSERT_EQUAL(4, db_disc_evt_count);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_ERROR, db_disc_evt[2].evt_type);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, db_disc_evt[2].params.error.reason);
	TEST_ASSERT_EQUAL(BLE_DB_DISCOVERY_AVAILABLE, db_disc_evt[3].evt_type);
}

void setUp(void)
{
	/* Zero the database discovery instance before each test. */
	memset(&db_discovery, 0, sizeof(db_discovery));

	/* Clear the database event global variables before each test. */
	memset(db_disc_evt, 0xFF, sizeof(db_disc_evt));
	db_disc_evt_count = 0;

	/* Increment connection handle to catch issues with data persisting between tests. */
	test_conn_handle++;

	stub_ble_gq_item_add_success_num_calls = -1;
}
void tearDown(void)
{
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
