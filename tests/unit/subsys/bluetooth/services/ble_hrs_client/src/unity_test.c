/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <ble_gap.h>
#include <ble_gattc.h>
#include <nrf_error.h>

#include <bm/bluetooth/services/ble_hrs_client.h>
#include <bm/bluetooth/services/uuid.h>
#include <bm/bluetooth/ble_gatt_db.h>

#include "cmock_ble.h"
#include "cmock_ble_gap.h"
#include "cmock_ble_gattc.h"
#include "cmock_ble_db_discovery.h"
#include "cmock_ble_gq.h"

/* Exposed for unit tests when CONFIG_UNITY=y (non-static in ble_hrs_client.c) */
extern void ble_hrs_client_on_ble_gq_event(const struct ble_gq_req *req, struct ble_gq_evt *gq_evt);

/* Arbitrary error to test forwarding */
#define ERROR 0xbaadf00d

#define CONN_HANDLE 5
#define HRM_HANDLE 0x0010
#define HRM_CCCD_HANDLE 0x0011

static const struct ble_gq gatt_queue;
static struct ble_db_discovery db_discovery;

static struct ble_hrs_client_evt last_evt;
static int evt_handler_called;

static void hrs_client_evt_handler(struct ble_hrs_client *ble_hrs_c, struct ble_hrs_client_evt *evt)
{
	evt_handler_called = 1;
	last_evt = *evt;
}

void test_ble_hrs_client_init_null(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};

	nrf_err = ble_hrs_client_init(NULL, &config);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_hrs_client_init(&ble_hrs_c, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_hrs_client_init_null_evt_handler(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = NULL,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_hrs_client_init_null_gatt_queue(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = NULL,
		.db_discovery = &db_discovery,
	};

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_hrs_client_init_service_register_fails(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, ERROR);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(ERROR, nrf_err);
}

void test_ble_hrs_client_init_success(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_hrs_client_handles_assign_null(void)
{
	uint32_t nrf_err;
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE,
	};

	nrf_err = ble_hrs_client_handles_assign(NULL, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_hrs_client_handles_assign(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE,
	};
	uint8_t hrm_data[] = { 0x00, 0x48 };
	uint8_t evt_buf[sizeof(ble_evt_t) + sizeof(hrm_data)] = {0};
	ble_evt_t *evt = (ble_evt_t *)evt_buf;

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Verify by behaviour: HRM notification is received after assign */

	evt->header.evt_id = BLE_GATTC_EVT_HVX;
	evt->evt.gattc_evt.conn_handle = CONN_HANDLE;
	evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE;
	evt->evt.gattc_evt.params.hvx.len = sizeof(hrm_data);
	memcpy(evt->evt.gattc_evt.params.hvx.data, hrm_data, sizeof(hrm_data));

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(evt, &ble_hrs_c);

	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION, last_evt.evt_type);
	TEST_ASSERT_EQUAL(0x48, last_evt.params.hrm.hr_value);
}

void test_ble_hrs_client_handles_assign_null_peer_handles(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	uint8_t hrm_data[] = { 0x00, 0x48 };
	uint8_t evt_buf[sizeof(ble_evt_t) + sizeof(hrm_data)] = {0};
	ble_evt_t *evt = (ble_evt_t *)evt_buf;

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Verify by behaviour: no peer handles means HVX does not produce HRM event */

	evt->header.evt_id = BLE_GATTC_EVT_HVX;
	evt->evt.gattc_evt.conn_handle = CONN_HANDLE;
	evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE;
	evt->evt.gattc_evt.params.hvx.len = sizeof(hrm_data);
	memcpy(evt->evt.gattc_evt.params.hvx.data, hrm_data, sizeof(hrm_data));

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(evt, &ble_hrs_c);

	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_ble_hrs_client_hrm_notif_enable_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_hrs_client_hrm_notif_enable(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_hrs_client_hrm_notif_enable_success(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE,
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_item_add_ExpectAndReturn(&gatt_queue, NULL, CONN_HANDLE, NRF_SUCCESS);
	__cmock_ble_gq_item_add_IgnoreArg_req();

	nrf_err = ble_hrs_client_hrm_notif_enable(&ble_hrs_c);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_hrs_client_hrm_notif_disable_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_hrs_client_hrm_notif_disable(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_hrs_client_hrm_notif_disable_success(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE,
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_item_add_ExpectAndReturn(&gatt_queue, NULL, CONN_HANDLE, NRF_SUCCESS);
	__cmock_ble_gq_item_add_IgnoreArg_req();

	nrf_err = ble_hrs_client_hrm_notif_disable(&ble_hrs_c);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_hrs_client_on_ble_gq_event_error_delivers_evt(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct ble_gq_req req = { .ctx = &ble_hrs_c };
	struct ble_gq_evt gq_evt = {
		.evt_type = BLE_GQ_EVT_ERROR,
		.conn_handle = CONN_HANDLE,
		.error = { .reason = ERROR },
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt_handler_called = 0;
	ble_hrs_client_on_ble_gq_event(&req, &gq_evt);

	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_CLIENT_EVT_ERROR, last_evt.evt_type);
	TEST_ASSERT_EQUAL(CONN_HANDLE, last_evt.conn_handle);
	TEST_ASSERT_EQUAL(ERROR, last_evt.params.error.reason);
}

void test_ble_hrs_on_db_disc_evt_wrong_service_ignored(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct ble_db_discovery_evt evt = {
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.conn_handle = CONN_HANDLE,
		.params.discovered_db.srv_uuid = {
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_BATTERY_SERVICE,
		},
		.params.discovered_db.char_count = 0,
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt_handler_called = 0;
	ble_hrs_on_db_disc_evt(&ble_hrs_c, &evt);

	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_ble_hrs_on_db_disc_evt_srv_not_found_ignored(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct ble_db_discovery_evt evt = {
		.evt_type = BLE_DB_DISCOVERY_SRV_NOT_FOUND,
		.conn_handle = CONN_HANDLE,
		.params.discovered_db.srv_uuid = {
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_HEART_RATE_SERVICE,
		},
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt_handler_called = 0;
	ble_hrs_on_db_disc_evt(&ble_hrs_c, &evt);

	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_ble_hrs_on_db_disc_evt_complete_with_hrm_char(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct ble_db_discovery_evt evt = {
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.conn_handle = CONN_HANDLE,
		.params.discovered_db.srv_uuid = {
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_HEART_RATE_SERVICE,
		},
		.params.discovered_db.char_count = 1,
		.params.discovered_db.charateristics = {
			[0] = {
				.characteristic.uuid.uuid = BLE_UUID_HEART_RATE_MEASUREMENT_CHAR,
				.characteristic.handle_value = HRM_HANDLE,
				.cccd_handle = HRM_CCCD_HANDLE,
			},
		},
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt_handler_called = 0;
	ble_hrs_on_db_disc_evt(&ble_hrs_c, &evt);

	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_CLIENT_EVT_DISCOVERY_COMPLETE, last_evt.evt_type);
	TEST_ASSERT_EQUAL(CONN_HANDLE, last_evt.conn_handle);
	TEST_ASSERT_EQUAL(HRM_CCCD_HANDLE, last_evt.params.peer_db.hrm_cccd_handle);
	TEST_ASSERT_EQUAL(HRM_HANDLE, last_evt.params.peer_db.hrm_handle);
}

void test_ble_hrs_on_db_disc_evt_hrm_char_at_index_one(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct ble_db_discovery_evt evt = {
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.conn_handle = CONN_HANDLE,
		.params.discovered_db.srv_uuid = {
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_HEART_RATE_SERVICE,
		},
		.params.discovered_db.char_count = 2,
		.params.discovered_db.charateristics = {
			[0] = {
				.characteristic.uuid.uuid = BLE_UUID_BODY_SENSOR_LOCATION_CHAR,
				.characteristic.handle_value = 0x000E,
				.cccd_handle = BLE_GATT_HANDLE_INVALID,
			},
			[1] = {
				.characteristic.uuid.uuid = BLE_UUID_HEART_RATE_MEASUREMENT_CHAR,
				.characteristic.handle_value = HRM_HANDLE,
				.cccd_handle = HRM_CCCD_HANDLE,
			},
		},
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt_handler_called = 0;
	ble_hrs_on_db_disc_evt(&ble_hrs_c, &evt);

	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_CLIENT_EVT_DISCOVERY_COMPLETE, last_evt.evt_type);
	TEST_ASSERT_EQUAL(HRM_CCCD_HANDLE, last_evt.params.peer_db.hrm_cccd_handle);
	TEST_ASSERT_EQUAL(HRM_HANDLE, last_evt.params.peer_db.hrm_handle);
}

void test_ble_hrs_on_db_disc_evt_complete_hrs_no_hrm_char(void)
{
	/* HRS found but no HRM characteristic (e.g. char_count 0);
	 * loop runs, no match, event still delivered
	 */
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct ble_db_discovery_evt evt = {
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.conn_handle = CONN_HANDLE,
		.params.discovered_db.srv_uuid = {
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_HEART_RATE_SERVICE,
		},
		.params.discovered_db.char_count = 0,
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt_handler_called = 0;
	ble_hrs_on_db_disc_evt(&ble_hrs_c, &evt);

	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_CLIENT_EVT_DISCOVERY_COMPLETE, last_evt.evt_type);
	TEST_ASSERT_EQUAL(CONN_HANDLE, last_evt.conn_handle);
	/* No HRM char found: peer_db handles remain invalid (zero) */
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, last_evt.params.peer_db.hrm_cccd_handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HANDLE_INVALID, last_evt.params.peer_db.hrm_handle);
}

void test_ble_hrs_on_db_disc_evt_does_not_overwrite_peer_db_when_already_assigned(void)
{
	/* conn_handle set and peer_hrs_db already valid.
	 * discovery must not overwrite peer_hrs_db
	 */
	uint32_t nrf_err;
	uint8_t hrm_data[] = { 0x00, 0x48 };
	uint8_t evt_buf[sizeof(ble_evt_t) + sizeof(hrm_data)] = {0};
	ble_evt_t *evt = (ble_evt_t *)evt_buf;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE,
	};
	struct ble_db_discovery_evt disc_evt = {
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.conn_handle = CONN_HANDLE,
		.params.discovered_db.srv_uuid = {
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_HEART_RATE_SERVICE,
		},
		.params.discovered_db.char_count = 1,
		.params.discovered_db.charateristics = {
			[0] = {
				.characteristic.uuid.uuid = BLE_UUID_HEART_RATE_MEASUREMENT_CHAR,
				/* different from HRM_HANDLE */
				.characteristic.handle_value = 0x9999,
				.cccd_handle = 0x999a,
			},
		},
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt_handler_called = 0;
	ble_hrs_on_db_disc_evt(&ble_hrs_c, &disc_evt);

	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_CLIENT_EVT_DISCOVERY_COMPLETE, last_evt.evt_type);

	/* peer_hrs_db must not have been overwritten.
	 * HVX with original HRM_HANDLE still works
	 */

	evt->header.evt_id = BLE_GATTC_EVT_HVX;
	evt->evt.gattc_evt.conn_handle = CONN_HANDLE;
	evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE;
	evt->evt.gattc_evt.params.hvx.len = sizeof(hrm_data);
	memcpy(evt->evt.gattc_evt.params.hvx.data, hrm_data, sizeof(hrm_data));

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(evt, &ble_hrs_c);

	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION, last_evt.evt_type);
	TEST_ASSERT_EQUAL(0x48, last_evt.params.hrm.hr_value);
}

void test_ble_hrs_on_db_disc_evt_assigns_peer_db_when_conn_handle_set(void)
{
	uint32_t nrf_err;
	uint8_t hrm_data[] = { 0x00, 0x48 };
	uint8_t evt_buf[sizeof(ble_evt_t) + sizeof(hrm_data)] = {0};
	ble_evt_t *evt = (ble_evt_t *)evt_buf;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct ble_db_discovery_evt disc_evt = {
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.conn_handle = CONN_HANDLE,
		.params.discovered_db.srv_uuid = {
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_HEART_RATE_SERVICE,
		},
		.params.discovered_db.char_count = 1,
		.params.discovered_db.charateristics = {
			[0] = {
				.characteristic.uuid.uuid = BLE_UUID_HEART_RATE_MEASUREMENT_CHAR,
				.characteristic.handle_value = HRM_HANDLE,
				.cccd_handle = HRM_CCCD_HANDLE,
			},
		},
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* peer_hrs_db is still invalid.
	 * discovery with HRS found should assign it
	 */
	evt_handler_called = 0;
	ble_hrs_on_db_disc_evt(&ble_hrs_c, &disc_evt);

	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_CLIENT_EVT_DISCOVERY_COMPLETE, last_evt.evt_type);

	/* Verify peer_db was assigned so that HVX is now accepted. */

	evt->header.evt_id = BLE_GATTC_EVT_HVX;
	evt->evt.gattc_evt.conn_handle = CONN_HANDLE;
	evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE;
	evt->evt.gattc_evt.params.hvx.len = sizeof(hrm_data);
	memcpy(evt->evt.gattc_evt.params.hvx.data, hrm_data, sizeof(hrm_data));

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(evt, &ble_hrs_c);

	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION, last_evt.evt_type);
	TEST_ASSERT_EQUAL(0x48, last_evt.params.hrm.hr_value);
}

void test_ble_hrs_client_on_ble_evt_disconnected_clears_handles(void)
{
	uint32_t nrf_err;
	uint8_t hrm_data[] = { 0x00, 0x48 };
	uint8_t evt_buf[sizeof(ble_evt_t) + sizeof(hrm_data)] = {0};
	ble_evt_t *hvx_evt = (ble_evt_t *)evt_buf;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE,
	};
	ble_evt_t disc_evt = {
		.header.evt_id = BLE_GAP_EVT_DISCONNECTED,
		.evt.gap_evt.conn_handle = CONN_HANDLE,
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_hrs_client_on_ble_evt(&disc_evt, &ble_hrs_c);

	/* Verify by behaviour: after disconnect, HVX for same conn no longer delivers HRM event. */

	hvx_evt->header.evt_id = BLE_GATTC_EVT_HVX;
	hvx_evt->evt.gattc_evt.conn_handle = CONN_HANDLE;
	hvx_evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE;
	hvx_evt->evt.gattc_evt.params.hvx.len = sizeof(hrm_data);
	memcpy(hvx_evt->evt.gattc_evt.params.hvx.data, hrm_data, sizeof(hrm_data));

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(hvx_evt, &ble_hrs_c);

	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_ble_hrs_client_on_ble_evt_disconnected_wrong_conn_ignored(void)
{
	uint32_t nrf_err;
	uint8_t hrm_data[] = { 0x00, 0x48 };
	uint8_t evt_buf[sizeof(ble_evt_t) + sizeof(hrm_data)] = {0};
	ble_evt_t *hvx_evt = (ble_evt_t *)evt_buf;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE,
	};
	ble_evt_t disc_evt = {
		.header.evt_id = BLE_GAP_EVT_DISCONNECTED,
		.evt.gap_evt.conn_handle = CONN_HANDLE + 1,
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_hrs_client_on_ble_evt(&disc_evt, &ble_hrs_c);

	/* Verify by behaviour: disconnect for other conn does not affect receiving HVX */

	hvx_evt->header.evt_id = BLE_GATTC_EVT_HVX;
	hvx_evt->evt.gattc_evt.conn_handle = CONN_HANDLE;
	hvx_evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE;
	hvx_evt->evt.gattc_evt.params.hvx.len = sizeof(hrm_data);
	memcpy(hvx_evt->evt.gattc_evt.params.hvx.data, hrm_data, sizeof(hrm_data));

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(hvx_evt, &ble_hrs_c);

	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION, last_evt.evt_type);
	TEST_ASSERT_EQUAL(0x48, last_evt.params.hrm.hr_value);
}

void test_ble_hrs_client_on_ble_evt_hvx_8bit_hr(void)
{
	uint32_t nrf_err;
	uint8_t hrm_data[] = { 0x00, 0x48 }; /* flags: 0 = 8-bit HR, value = 72 */
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE
	};
	/* hvx.data is a flexible array; use a buffer so data is in place */
	uint8_t evt_buf[sizeof(ble_evt_t) + sizeof(hrm_data)] = {0};
	ble_evt_t *evt = (ble_evt_t *)evt_buf;

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt->header.evt_id = BLE_GATTC_EVT_HVX;
	evt->evt.gattc_evt.conn_handle = CONN_HANDLE;
	evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE;
	evt->evt.gattc_evt.params.hvx.len = sizeof(hrm_data);
	memcpy(evt->evt.gattc_evt.params.hvx.data, hrm_data, sizeof(hrm_data));

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(evt, &ble_hrs_c);

	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION, last_evt.evt_type);
	TEST_ASSERT_EQUAL(0x48, last_evt.params.hrm.hr_value);
	TEST_ASSERT_EQUAL(0, last_evt.params.hrm.rr_intervals_cnt);
}

void test_ble_hrs_client_on_ble_evt_hvx_16bit_hr(void)
{
	uint32_t nrf_err;
	/* flags 0x01 = 16-bit HR, value 0x1234 little-endian */
	uint8_t hrm_data[] = { 0x01, 0x34, 0x12 };
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE
	};
	uint8_t evt_buf[sizeof(ble_evt_t) + sizeof(hrm_data)] = {0};
	ble_evt_t *evt = (ble_evt_t *)evt_buf;

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);
	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);

	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt->header.evt_id = BLE_GATTC_EVT_HVX;
	evt->evt.gattc_evt.conn_handle = CONN_HANDLE;
	evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE;
	evt->evt.gattc_evt.params.hvx.len = sizeof(hrm_data);
	memcpy(evt->evt.gattc_evt.params.hvx.data, hrm_data, sizeof(hrm_data));

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(evt, &ble_hrs_c);

	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION, last_evt.evt_type);
	TEST_ASSERT_EQUAL(0x1234, last_evt.params.hrm.hr_value);
}

void test_ble_hrs_client_on_ble_evt_hvx_with_rr_intervals(void)
{
	uint32_t nrf_err;
	/* flags 0x10 = RR intervals present,
	 * 8-bit HR 72, two RR intervals (256, 512) little-endian
	 */
	uint8_t hrm_data[] = { 0x10, 0x48, 0x00, 0x01, 0x00, 0x02 };
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE
	};
	uint8_t evt_buf[sizeof(ble_evt_t) + sizeof(hrm_data)] = {0};
	ble_evt_t *evt = (ble_evt_t *)evt_buf;

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt->header.evt_id = BLE_GATTC_EVT_HVX;
	evt->evt.gattc_evt.conn_handle = CONN_HANDLE;
	evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE;
	evt->evt.gattc_evt.params.hvx.len = sizeof(hrm_data);
	memcpy(evt->evt.gattc_evt.params.hvx.data, hrm_data, sizeof(hrm_data));

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(evt, &ble_hrs_c);

	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION, last_evt.evt_type);
	TEST_ASSERT_EQUAL(0x48, last_evt.params.hrm.hr_value);
	TEST_ASSERT_EQUAL(2, last_evt.params.hrm.rr_intervals_cnt);
	TEST_ASSERT_EQUAL(256, last_evt.params.hrm.rr_intervals[0]);
	TEST_ASSERT_EQUAL(512, last_evt.params.hrm.rr_intervals[1]);
}

void test_ble_hrs_client_on_ble_evt_hvx_zero_len_ignored(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE,
	};
	uint8_t evt_buf[sizeof(ble_evt_t)] = {0};
	ble_evt_t *evt = (ble_evt_t *)evt_buf;

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt->header.evt_id = BLE_GATTC_EVT_HVX;
	evt->evt.gattc_evt.conn_handle = CONN_HANDLE;
	evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE;
	evt->evt.gattc_evt.params.hvx.len = 0;

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(evt, &ble_hrs_c);

	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_ble_hrs_client_on_ble_evt_hvx_too_short_8bit_ignored(void)
{
	uint32_t nrf_err;
	/* flags=0x00 (8-bit HR) but only 1 byte total -- missing the HR value */
	uint8_t hrm_data[] = { 0x00 };
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE,
	};
	uint8_t evt_buf[sizeof(ble_evt_t) + sizeof(hrm_data)] = {0};
	ble_evt_t *evt = (ble_evt_t *)evt_buf;

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt->header.evt_id = BLE_GATTC_EVT_HVX;
	evt->evt.gattc_evt.conn_handle = CONN_HANDLE;
	evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE;
	evt->evt.gattc_evt.params.hvx.len = sizeof(hrm_data);
	memcpy(evt->evt.gattc_evt.params.hvx.data, hrm_data, sizeof(hrm_data));

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(evt, &ble_hrs_c);

	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_ble_hrs_client_on_ble_evt_hvx_too_short_16bit_ignored(void)
{
	uint32_t nrf_err;
	/* flags=0x01 (16-bit HR) but only 2 bytes total -- need 3 */
	uint8_t hrm_data[] = { 0x01, 0x34 };
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE,
	};
	uint8_t evt_buf[sizeof(ble_evt_t) + sizeof(hrm_data)] = {0};
	ble_evt_t *evt = (ble_evt_t *)evt_buf;

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt->header.evt_id = BLE_GATTC_EVT_HVX;
	evt->evt.gattc_evt.conn_handle = CONN_HANDLE;
	evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE;
	evt->evt.gattc_evt.params.hvx.len = sizeof(hrm_data);
	memcpy(evt->evt.gattc_evt.params.hvx.data, hrm_data, sizeof(hrm_data));

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(evt, &ble_hrs_c);

	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_ble_hrs_client_on_ble_evt_hvx_rr_truncated(void)
{
	uint32_t nrf_err;
	/* flags 0x10 = RR intervals present,
	 * 8-bit HR 0x48, one complete RR (256) + 1 trailing byte (truncated pair)
	 */
	uint8_t hrm_data[] = { 0x10, 0x48, 0x00, 0x01, 0xFF };
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE,
	};
	uint8_t evt_buf[sizeof(ble_evt_t) + sizeof(hrm_data)] = {0};
	ble_evt_t *evt = (ble_evt_t *)evt_buf;

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt->header.evt_id = BLE_GATTC_EVT_HVX;
	evt->evt.gattc_evt.conn_handle = CONN_HANDLE;
	evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE;
	evt->evt.gattc_evt.params.hvx.len = sizeof(hrm_data);
	memcpy(evt->evt.gattc_evt.params.hvx.data, hrm_data, sizeof(hrm_data));

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(evt, &ble_hrs_c);

	TEST_ASSERT_TRUE(evt_handler_called);
	TEST_ASSERT_EQUAL(BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION, last_evt.evt_type);
	TEST_ASSERT_EQUAL(0x48, last_evt.params.hrm.hr_value);
	/* Only 1 complete RR pair; the trailing byte is ignored */
	TEST_ASSERT_EQUAL(1, last_evt.params.hrm.rr_intervals_cnt);
	TEST_ASSERT_EQUAL(256, last_evt.params.hrm.rr_intervals[0]);
}

void test_ble_hrs_client_on_ble_evt_hvx_wrong_handle_ignored(void)
{
	uint32_t nrf_err;
	uint8_t hrm_data[] = { 0x00, 0x48 };
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE
	};
	uint8_t evt_buf[sizeof(ble_evt_t) + sizeof(hrm_data)] = {0};
	ble_evt_t *evt = (ble_evt_t *)evt_buf;

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt->header.evt_id = BLE_GATTC_EVT_HVX;
	evt->evt.gattc_evt.conn_handle = CONN_HANDLE;
	evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE + 1;
	evt->evt.gattc_evt.params.hvx.len = sizeof(hrm_data);
	memcpy(evt->evt.gattc_evt.params.hvx.data, hrm_data, sizeof(hrm_data));

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(evt, &ble_hrs_c);

	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_ble_hrs_client_on_ble_evt_hvx_wrong_conn_ignored(void)
{
	uint32_t nrf_err;
	uint8_t hrm_data[] = { 0x00, 0x48 };
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE
	};
	uint8_t evt_buf[sizeof(ble_evt_t) + sizeof(hrm_data)] = {0};
	ble_evt_t *evt = (ble_evt_t *)evt_buf;

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt->header.evt_id = BLE_GATTC_EVT_HVX;
	evt->evt.gattc_evt.conn_handle = CONN_HANDLE + 1;
	evt->evt.gattc_evt.params.hvx.handle = HRM_HANDLE;
	evt->evt.gattc_evt.params.hvx.len = sizeof(hrm_data);
	memcpy(evt->evt.gattc_evt.params.hvx.data, hrm_data, sizeof(hrm_data));

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(evt, &ble_hrs_c);

	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_ble_hrs_client_on_ble_evt_unhandled_evt_ignored(void)
{
	uint32_t nrf_err;
	struct ble_hrs_client ble_hrs_c = {0};
	struct ble_hrs_client_config config = {
		.evt_handler = hrs_client_evt_handler,
		.gatt_queue = &gatt_queue,
		.db_discovery = &db_discovery,
	};
	struct hrs_db peer_handles = {
		.hrm_cccd_handle = HRM_CCCD_HANDLE,
		.hrm_handle = HRM_HANDLE,
	};
	ble_evt_t evt = {
		.header.evt_id = BLE_GATTC_EVT_WRITE_RSP,
		.evt.gattc_evt.conn_handle = CONN_HANDLE,
	};

	__cmock_ble_db_discovery_service_register_ExpectAndReturn(&db_discovery, NULL, NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_IgnoreArg_uuid();

	nrf_err = ble_hrs_client_init(&ble_hrs_c, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_gq_conn_handle_register_ExpectAndReturn(&gatt_queue, CONN_HANDLE, NRF_SUCCESS);

	nrf_err = ble_hrs_client_handles_assign(&ble_hrs_c, CONN_HANDLE, &peer_handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt_handler_called = 0;
	ble_hrs_client_on_ble_evt(&evt, &ble_hrs_c);

	TEST_ASSERT_FALSE(evt_handler_called);
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
