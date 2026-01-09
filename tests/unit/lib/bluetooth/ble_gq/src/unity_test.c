/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <unity.h>
#include <nrf_error.h>
#include <stdint.h>
#include <bm/bluetooth/ble_gq.h>
#include <zephyr/sys/util.h>

#include "cmock_ble_gattc.h"
#include "cmock_ble_gatts.h"

#define ATTR_HANDLE_1 (0xCAFE)
#define TEST_OFFSET_1 17
#define TEST_STRING_1 "testdata"
#define TEST_DATA_1 "abcdefgh"

#define CONN_HANDLE_FIRST (42)
#define CONN_HANDLE_LAST (CONN_HANDLE_FIRST + MAX_CONNS - 1)
#define CONN_HANDLE_FUNC(i, _) (CONN_HANDLE_FIRST + (i))

#define CONN_HANDLE_1 CONN_HANDLE_FIRST
#define CONN_HANDLE_2 CONN_HANDLE_LAST

#define MAX_CONNS 3
#define BLE_GQ_QUEUE_SIZE 8
#define BLE_GQ_HEAP_SIZE 1024

BLE_GQ_CUSTOM_DEF(ble_gq, MAX_CONNS, BLE_GQ_HEAP_SIZE, MAX_CONNS * BLE_GQ_QUEUE_SIZE);

static const uint16_t conn_handles[] = {LISTIFY(MAX_CONNS, CONN_HANDLE_FUNC, (,))};
static uint16_t glob_conn_handle;
static uint32_t glob_error;
static int stub_sd_ble_gattc_write_busy_busy_success_num_calls;
static int stub_sd_ble_gatts_hvx_busy_busy_success_num_calls;

static void ble_gq_error_handler(const struct ble_gq_req *req, struct ble_gq_evt *evt)
{
	glob_conn_handle = evt->conn_handle;

	switch (evt->evt_type) {
	case BLE_GQ_EVT_ERROR:
		glob_error = evt->error.reason;
		break;
	default:
		TEST_FAIL();
		break;
	};
}

static uint32_t stub_sd_ble_gattc_write_success(
	uint16_t conn_handle, const ble_gattc_write_params_t *p_write_params, int cmock_num_calls)
{
	TEST_ASSERT_EQUAL(CONN_HANDLE_1, conn_handle);
	TEST_ASSERT_NOT_NULL(p_write_params);

	TEST_ASSERT_EQUAL(ATTR_HANDLE_1, p_write_params->handle);
	TEST_ASSERT_EQUAL(TEST_OFFSET_1, p_write_params->offset);
	TEST_ASSERT_EQUAL(sizeof(TEST_STRING_1), p_write_params->len);
	TEST_ASSERT_EQUAL_MEMORY(TEST_STRING_1, p_write_params->p_value, sizeof(TEST_STRING_1));

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gattc_write_busy_busy_success(
	uint16_t conn_handle, const ble_gattc_write_params_t *p_write_params, int cmock_num_calls)
{
	stub_sd_ble_gattc_write_busy_busy_success_num_calls = cmock_num_calls + 1;

	TEST_ASSERT_EQUAL(CONN_HANDLE_2, conn_handle);
	TEST_ASSERT_NOT_NULL(p_write_params);

	TEST_ASSERT_EQUAL(ATTR_HANDLE_1, p_write_params->handle);
	TEST_ASSERT_EQUAL(TEST_OFFSET_1, p_write_params->offset);
	TEST_ASSERT_EQUAL(sizeof(TEST_STRING_1), p_write_params->len);
	TEST_ASSERT_EQUAL_MEMORY(TEST_STRING_1, p_write_params->p_value, sizeof(TEST_STRING_1));

	switch (cmock_num_calls) {
	case 0:
	case 1:
		return NRF_ERROR_BUSY;
	case 2:
		return NRF_SUCCESS;
	default:
		TEST_FAIL();
	}
}

static uint32_t stub_sd_ble_gatts_hvx_busy_busy_success(
	uint16_t conn_handle, const ble_gatts_hvx_params_t *p_hvx_params, int cmock_num_calls)
{
	stub_sd_ble_gatts_hvx_busy_busy_success_num_calls = cmock_num_calls + 1;

	TEST_ASSERT_EQUAL(CONN_HANDLE_1, conn_handle);
	TEST_ASSERT_NOT_NULL(p_hvx_params);

	TEST_ASSERT_EQUAL(ATTR_HANDLE_1, p_hvx_params->handle);
	TEST_ASSERT_EQUAL(BLE_GATT_HVX_INDICATION, p_hvx_params->type);
	TEST_ASSERT_EQUAL(TEST_OFFSET_1, p_hvx_params->offset);
	TEST_ASSERT_EQUAL(sizeof(TEST_DATA_1), *(p_hvx_params->p_len));
	TEST_ASSERT_EQUAL_MEMORY(TEST_DATA_1, p_hvx_params->p_data, sizeof(TEST_DATA_1));

	switch (cmock_num_calls) {
	case 0:
	case 1:
		return NRF_ERROR_BUSY;
	case 2:
		return NRF_SUCCESS;
	default:
		TEST_FAIL();
	}
}

void test_ble_gq_conn_handle_register_error_null(void)
{
	uint32_t nrf_err = ble_gq_conn_handle_register(NULL, 0);

	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_gq_conn_handle_register_error_no_mem(void)
{
	uint32_t nrf_err;

	for (uint32_t i = 0; i < MAX_CONNS; ++i) {
		nrf_err = ble_gq_conn_handle_register(&ble_gq, conn_handles[i]);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	}

	nrf_err = ble_gq_conn_handle_register(&ble_gq, 0);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_ble_gq_conn_handle_register_twice(void)
{
	uint32_t nrf_err;

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_2);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_2);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_gq_item_add_error_null(void)
{
	uint32_t nrf_err;
	struct ble_gq_req req = {0};

	nrf_err = ble_gq_item_add(NULL, &req, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_gq_item_add(&ble_gq, NULL, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_gq_item_add_error_invalid_param(void)
{
	uint32_t nrf_err;
	struct ble_gq_req req = {0};

	/* Invalid request type, registered connection handle. */
	req.type = BLE_GQ_REQ_NUM;
	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	/* Valid request type, unregistered connection_handle. */
	req.type = BLE_GQ_REQ_GATTC_READ;

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_2);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_gq_item_add_req_gatt_read_success(void)
{
	uint32_t nrf_err;
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_GATTC_READ,
		.evt_handler = ble_gq_error_handler,
		.gattc_read = {
			.handle = ATTR_HANDLE_1,
			.offset = TEST_OFFSET_1,
		},
	};

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gattc_read_ExpectAndReturn(CONN_HANDLE_1, req.gattc_read.handle,
						  req.gattc_read.offset, NRF_SUCCESS);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);
}

void test_ble_gq_item_add_req_gatt_read_busy_success(void)
{
	uint32_t nrf_err;
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_GATTC_READ,
		.evt_handler = ble_gq_error_handler,
		.gattc_read = {
			.handle = ATTR_HANDLE_1,
			.offset = TEST_OFFSET_1,
		},
	};

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gattc_read_ExpectAndReturn(CONN_HANDLE_1, req.gattc_read.handle,
						  req.gattc_read.offset, NRF_ERROR_BUSY);
	__cmock_sd_ble_gattc_read_ExpectAndReturn(CONN_HANDLE_1, req.gattc_read.handle,
						  req.gattc_read.offset, NRF_SUCCESS);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);
}

void test_ble_gq_item_add_req_gatt_read_busy_error(void)
{
	uint32_t nrf_err;
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_GATTC_READ,
		.evt_handler = ble_gq_error_handler,
		.gattc_read = {
			.handle = ATTR_HANDLE_1,
			.offset = TEST_OFFSET_1,
		},
	};

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gattc_read_ExpectAndReturn(CONN_HANDLE_1, req.gattc_read.handle,
						  req.gattc_read.offset, NRF_ERROR_BUSY);
	__cmock_sd_ble_gattc_read_ExpectAndReturn(CONN_HANDLE_1, req.gattc_read.handle,
						  req.gattc_read.offset, NRF_ERROR_INVALID_STATE);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(CONN_HANDLE_1, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, glob_error);
}

void test_ble_gq_item_add_req_gatt_read_busy_busy_success(void)
{
	uint32_t nrf_err;
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_GATTC_READ,
		.evt_handler = ble_gq_error_handler,
		.gattc_read = {
			.handle = ATTR_HANDLE_1,
			.offset = TEST_OFFSET_1,
		},
	};

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gattc_read_ExpectAndReturn(CONN_HANDLE_1, req.gattc_read.handle,
						  req.gattc_read.offset, NRF_ERROR_BUSY);
	__cmock_sd_ble_gattc_read_ExpectAndReturn(CONN_HANDLE_1, req.gattc_read.handle,
						  req.gattc_read.offset, NRF_ERROR_BUSY);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);

	/* Event is in queue. Receive an (arbitrary) GATT event to trigger queue processing. */
	__cmock_sd_ble_gattc_read_ExpectAndReturn(CONN_HANDLE_1, req.gattc_read.handle,
						  req.gattc_read.offset, NRF_SUCCESS);

	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_WRITE,
		.evt.gatts_evt.conn_handle = CONN_HANDLE_1,
	};

	ble_gq_on_ble_evt(&ble_evt, (void *)&ble_gq);

	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);
}

void test_ble_gq_item_add_req_gatt_write_success(void)
{
	uint32_t nrf_err;
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_GATTC_WRITE,
		.evt_handler = ble_gq_error_handler,
		.gattc_write = {
			.handle = ATTR_HANDLE_1,
			.offset = TEST_OFFSET_1,
			.len = sizeof(TEST_STRING_1),
			.p_value = TEST_STRING_1,
		},
	};

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gattc_write_Stub(stub_sd_ble_gattc_write_success);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);
}

void test_ble_gq_item_add_req_gatt_write_busy_error(void)
{
	uint32_t nrf_err;
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_GATTC_WRITE,
		.evt_handler = ble_gq_error_handler,
		.gattc_write = {
			.handle = ATTR_HANDLE_1,
			.offset = TEST_OFFSET_1,
			.len = sizeof(TEST_STRING_1),
			.p_value = TEST_STRING_1,
		},
	};

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gattc_write_ExpectAnyArgsAndReturn(NRF_ERROR_BUSY);
	__cmock_sd_ble_gattc_write_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_STATE);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(CONN_HANDLE_1, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, glob_error);
}

void test_ble_gq_item_add_req_gatt_write_busy_busy_success(void)
{
	uint32_t nrf_err;
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_GATTC_WRITE,
		.evt_handler = ble_gq_error_handler,
		.gattc_write = {
			.handle = ATTR_HANDLE_1,
			.offset = TEST_OFFSET_1,
			.len = sizeof(TEST_STRING_1),
			.p_value = TEST_STRING_1,
		},
	};

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_2);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gattc_write_Stub(stub_sd_ble_gattc_write_busy_busy_success);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_2);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);

	/* Event is in queue. Receive an (arbitrary) GATT event to trigger queue processing. */
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_WRITE,
		.evt.gatts_evt.conn_handle = CONN_HANDLE_2,
	};

	ble_gq_on_ble_evt(&ble_evt, (void *)&ble_gq);

	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);

	TEST_ASSERT_EQUAL(3, stub_sd_ble_gattc_write_busy_busy_success_num_calls);
}

void test_ble_gq_item_add_req_srv_discovery_busy_busy_success(void)
{
	uint32_t nrf_err;
	ble_uuid_t srvc_uuid = {.uuid = 0xBADE, .type = 0x78};
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_SRV_DISCOVERY,
		.evt_handler = ble_gq_error_handler,
		.gattc_srv_disc = {
			.start_handle = ATTR_HANDLE_1,
			.srvc_uuid = srvc_uuid,
		},
	};

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gattc_primary_services_discover_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, ATTR_HANDLE_1, &srvc_uuid, 1, NRF_ERROR_BUSY);
	__cmock_sd_ble_gattc_primary_services_discover_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, ATTR_HANDLE_1, &srvc_uuid, 1, NRF_ERROR_BUSY);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);

	/* Event is in queue. Receive an (arbitrary) GATT event to trigger queue processing. */
	__cmock_sd_ble_gattc_primary_services_discover_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, ATTR_HANDLE_1, &srvc_uuid, 1, NRF_SUCCESS);

	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_WRITE,
		.evt.gatts_evt.conn_handle = CONN_HANDLE_1,
	};

	ble_gq_on_ble_evt(&ble_evt, (void *)&ble_gq);

	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);
}

void test_ble_gq_item_add_req_char_discovery_busy_busy_success(void)
{
	uint32_t nrf_err;
	ble_gattc_handle_range_t handle_range = {.start_handle = 0xAAAA, .end_handle = 0xBBBB};
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_CHAR_DISCOVERY,
		.evt_handler = ble_gq_error_handler,
		.gattc_char_disc = {
			.start_handle = handle_range.start_handle,
			.end_handle = handle_range.end_handle,
		},
	};

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gattc_characteristics_discover_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, &handle_range, 1, NRF_ERROR_BUSY);
	__cmock_sd_ble_gattc_characteristics_discover_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, &handle_range, 1, NRF_ERROR_BUSY);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);

	/* Event is in queue. Receive an (arbitrary) GATT event to trigger queue processing. */
	__cmock_sd_ble_gattc_characteristics_discover_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, &handle_range, 1, NRF_SUCCESS);

	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_WRITE,
		.evt.gatts_evt.conn_handle = CONN_HANDLE_1,
	};

	ble_gq_on_ble_evt(&ble_evt, (void *)&ble_gq);

	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);
}

void test_ble_gq_item_add_req_desc_discovery_busy_busy_success(void)
{
	uint32_t nrf_err;
	ble_gattc_handle_range_t handle_range = {.start_handle = 0xCCCC, .end_handle = 0xDDDD};
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_DESC_DISCOVERY,
		.evt_handler = ble_gq_error_handler,
		.gattc_desc_disc = {
			.start_handle = handle_range.start_handle,
			.end_handle = handle_range.end_handle,
		},
	};

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gattc_descriptors_discover_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, &handle_range, 1, NRF_ERROR_BUSY);
	__cmock_sd_ble_gattc_descriptors_discover_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, &handle_range, 1, NRF_ERROR_BUSY);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);

	/* Event is in queue. Receive an (arbitrary) GATT event to trigger queue processing. */
	__cmock_sd_ble_gattc_descriptors_discover_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, &handle_range, 1, NRF_ERROR_BUSY);

	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_WRITE,
		.evt.gatts_evt.conn_handle = CONN_HANDLE_1,
	};

	ble_gq_on_ble_evt(&ble_evt, (void *)&ble_gq);

	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);
}

void test_ble_gq_item_add_req_gatts_hvx_busy_busy_success(void)
{
	uint32_t nrf_err;
	uint16_t len = sizeof(TEST_DATA_1);
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_GATTS_HVX,
		.evt_handler = ble_gq_error_handler,
		.gatts_hvx = {
			.type = BLE_GATT_HVX_INDICATION,
			.handle = ATTR_HANDLE_1,
			.offset = TEST_OFFSET_1,
			.p_data = TEST_DATA_1,
			.p_len = &len,
		},
	};

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gatts_hvx_Stub(stub_sd_ble_gatts_hvx_busy_busy_success);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Event is in queue. Receive an (arbitrary) GATT event to trigger queue processing. */
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_WRITE,
		.evt.gatts_evt.conn_handle = CONN_HANDLE_1,
	};

	ble_gq_on_ble_evt(&ble_evt, (void *)&ble_gq);

	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);

	TEST_ASSERT_EQUAL(3, stub_sd_ble_gatts_hvx_busy_busy_success_num_calls);
}

void test_ble_gq_item_add_req_gatts_hvx_busy_error(void)
{
	uint32_t nrf_err;
	uint16_t len = sizeof(TEST_DATA_1);
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_GATTS_HVX,
		.evt_handler = ble_gq_error_handler,
		.gatts_hvx = {
			.type = BLE_GATT_HVX_NOTIFICATION,
			.handle = ATTR_HANDLE_1,
			.offset = TEST_OFFSET_1,
			.p_data = TEST_DATA_1,
			.p_len = &len,
		},
	};

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_ERROR_BUSY);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_ADDR);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(CONN_HANDLE_1, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_ADDR, glob_error);
}

void test_ble_gq_item_add_req_gatts_hvx_error_invalid_param(void)
{
	uint32_t nrf_err;
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_GATTS_HVX,
		.evt_handler = ble_gq_error_handler,
		.gatts_hvx = {
			.type = BLE_GATT_HVX_NOTIFICATION,
			.handle = ATTR_HANDLE_1,
			.offset = TEST_OFFSET_1,
			.p_data = TEST_DATA_1,
			/* Invalid length pointer. */
			.p_len = NULL,
		},
	};

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(CONN_HANDLE_1, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, glob_error);
}

void test_ble_gq_on_ble_evt_null(void)
{
	ble_gq_on_ble_evt(NULL, NULL);
}

void test_ble_gq_on_ble_evt_disconnected_event_item_purge(void)
{
	uint32_t nrf_err;
	ble_gattc_handle_range_t handle_range = {.start_handle = 0xAAAA, .end_handle = 0xBBBB};
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_CHAR_DISCOVERY,
		.evt_handler = ble_gq_error_handler,
		.gattc_char_disc = {
			.start_handle = handle_range.start_handle,
			.end_handle = handle_range.end_handle,
		},
	};

	nrf_err = ble_gq_conn_handle_register(&ble_gq, CONN_HANDLE_2);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gattc_characteristics_discover_ExpectAnyArgsAndReturn(NRF_ERROR_BUSY);
	__cmock_sd_ble_gattc_characteristics_discover_ExpectAnyArgsAndReturn(NRF_ERROR_BUSY);

	nrf_err = ble_gq_item_add(&ble_gq, &req, CONN_HANDLE_2);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);

	/* Deregister and start purge of data items by sending a BLE disconnect event. */
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_DISCONNECTED,
		.evt.gap_evt.conn_handle = CONN_HANDLE_2,
	};

	ble_gq_on_ble_evt(&ble_evt, (void *)&ble_gq);

	/* Purge in progress. Receive an (arbitrary) GATT event to trigger queue processing.
	 * The item in the queue should be purged, so expect no call to the SoftDevice.
	 */
	ble_evt = (ble_evt_t) {
		.header.evt_id = BLE_GATTC_EVT_READ_RSP,
		.evt.gap_evt.conn_handle = CONN_HANDLE_2,
	};

	ble_gq_on_ble_evt(&ble_evt, (void *)&ble_gq);

	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, glob_error);
}

void setUp(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_DISCONNECTED,
	};

	/* Deregister all the registered connection handles by sending disconnect events. */
	for (uint32_t i = 0; i < ARRAY_SIZE(conn_handles); ++i) {
		ble_evt.evt.gap_evt.conn_handle = conn_handles[i];
		ble_gq_on_ble_evt(&ble_evt, (void *)&ble_gq);
	}

	glob_conn_handle = BLE_CONN_HANDLE_INVALID;
	glob_error = NRF_SUCCESS;

	stub_sd_ble_gattc_write_busy_busy_success_num_calls = 0;
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
