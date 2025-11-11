/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/services/uuid.h>
#include <bm/bluetooth/ble_gq.h>

#include "cmock_ble.h"
#include "cmock_ble_gap.h"
#include "cmock_ble_gattc.h"

BLE_GQ_DEF(ble_gatt_queue);

static void db_discovery_evt_handler(struct ble_db_discovery *db_discovery,
				     struct ble_db_discovery_evt *adv)
{
}

void test_ble_adv_init_error_null(void)
{
	struct ble_db_discovery db_discovery = {0};
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

	struct ble_db_discovery db_discovery = {0};
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
	struct ble_db_discovery db_discovery = {0};

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};

	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE,
			  ble_db_discovery_evt_register(&db_discovery, &hrs_uuid));
}

void test_ble_db_discovery_evt_register_no_mem(void)
{
	struct ble_db_discovery db_discovery = {0};
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
	struct ble_db_discovery db_discovery = {0};
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
	struct ble_db_discovery db_discovery = {0};
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
	struct ble_db_discovery db_discovery = {0};
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
	struct ble_db_discovery db_discovery = {0};
	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_init(&db_discovery, &config));

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_evt_register(&db_discovery, &hrs_uuid));

	__cmock_sd_ble_gattc_primary_services_discover_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_start(&db_discovery, 0));

	TEST_ASSERT_EQUAL(NRF_ERROR_BUSY, ble_db_discovery_start(&db_discovery, 0));
}

void test_ble_db_discovery_start(void)
{
	struct ble_db_discovery db_discovery = {0};
	struct ble_db_discovery_config config = {
		.gatt_queue = &ble_gatt_queue,
		.evt_handler = db_discovery_evt_handler,
	};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_init(&db_discovery, &config));

	ble_uuid_t hrs_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_HEART_RATE_SERVICE};

	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_evt_register(&db_discovery, &hrs_uuid));

	__cmock_sd_ble_gattc_primary_services_discover_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, ble_db_discovery_start(&db_discovery, 0));
}

void setUp(void)
{
}
void tearDown(void)
{
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
