/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <unity.h>

#include <errno.h>
#include <stdint.h>
#include <ble_gq.h>
#include <zephyr/sys/slist.h>

#include "cmock_ble_gattc.h"
#include "cmock_ble_gatts.h"

#define MAX_CONNS 2
#define BLE_GQ_REQ_UNDEFINED 7
#define BLE_GQ_QUEUE_SIZE 8
#define BLE_GQ_HEAP_SIZE 1024

BLE_GQ_CUSTOM_DEF(ble_gq, MAX_CONNS, BLE_GQ_HEAP_SIZE, MAX_CONNS * BLE_GQ_QUEUE_SIZE);
static uint16_t glob_conn_handle;
static uint32_t glob_error;

static void ble_gq_error_handler(uint16_t conn_handle, uint32_t err, void *ctx)
{
	glob_conn_handle = conn_handle;
	glob_error = err;
}

void test_ble_gq_item_add_efault(void)
{
	struct ble_gq_req req = {0};
	int ret;

	ret = ble_gq_item_add(NULL, &req, 1);
	TEST_ASSERT_EQUAL(-EFAULT, ret);

	ret = ble_gq_item_add(&ble_gq, NULL, 1);
	TEST_ASSERT_EQUAL(-EFAULT, ret);
}

void test_ble_gq_item_add_einval(void)
{
	struct ble_gq_req req = {.type = BLE_GQ_REQ_NUM};
	int ret;

	ret = ble_gq_item_add(&ble_gq, &req, 0);
	TEST_ASSERT_EQUAL(-EINVAL, ret);

	ret = ble_gq_item_add(&ble_gq, &req, 99);
	TEST_ASSERT_EQUAL(-EINVAL, ret);
}

void test_ble_gq_item_add_req_gatt_read(void)
{
	uint16_t conn_handle = 0;
	int ret;
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_GATTC_READ,
		.error_handler = {
			.cb = ble_gq_error_handler,
			.ctx = NULL
		},
		.gattc_read = {
			.handle = 0,
			.offset = 0
		}
	};
	ble_gq.conn_handles[0] = 0;

	__cmock_sd_ble_gattc_read_ExpectAndReturn(conn_handle, req.gattc_read.handle,
						  req.gattc_read.offset, NRF_ERROR_BUSY);
	__cmock_sd_ble_gattc_read_ExpectAndReturn(conn_handle, req.gattc_read.handle,
						  req.gattc_read.offset, NRF_ERROR_INVALID_STATE);

	ret = ble_gq_item_add(&ble_gq, &req, conn_handle);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_EQUAL(conn_handle, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, glob_error);
}

void test_ble_gq_item_add_req_gatt_write(void)
{
	uint16_t conn_handle = 0;
	int ret;
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_GATTC_WRITE,
		.error_handler = {
			.cb = ble_gq_error_handler,
			.ctx = NULL
		},
		.gattc_write = {
			.handle = 0,
			.offset = 0,
			.len = sizeof("testdata"),
			.p_value = "testdata"
		}
	};
	ble_gq.conn_handles[0] = 0;

	__cmock_sd_ble_gattc_write_ExpectAndReturn(conn_handle, &req.gattc_write, NRF_ERROR_BUSY);
	__cmock_sd_ble_gattc_write_IgnoreAndReturn(NRF_ERROR_RESOURCES);
	ret = ble_gq_item_add(&ble_gq, &req, conn_handle);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_EQUAL(conn_handle, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_RESOURCES, glob_error);
}

void test_ble_gq_item_add_req_srv_discovery(void)
{
	uint16_t conn_handle = 0;
	int ret;
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_SRV_DISCOVERY,
		.error_handler = {
			.cb = ble_gq_error_handler,
			.ctx = NULL
		},
		.gattc_srv_disc = {
			.start_handle = 0,
			.srvc_uuid = {0}
		}
	};
	ble_gq.conn_handles[0] = 0;

	__cmock_sd_ble_gattc_primary_services_discover_ExpectAndReturn(
		conn_handle, req.gattc_srv_disc.start_handle, &req.gattc_srv_disc.srvc_uuid,
		NRF_ERROR_BUSY);
	__cmock_sd_ble_gattc_primary_services_discover_ExpectAndReturn(
		conn_handle, req.gattc_srv_disc.start_handle, &req.gattc_srv_disc.srvc_uuid,
		NRF_ERROR_TIMEOUT);

	ret = ble_gq_item_add(&ble_gq, &req, 0);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_EQUAL(conn_handle, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_TIMEOUT, glob_error);
}

void test_ble_gq_item_add_req_char_discovery(void)
{
	uint16_t conn_handle = 0;
	int ret;
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_CHAR_DISCOVERY,
		.error_handler = {
			.cb = ble_gq_error_handler,
			.ctx = NULL
		},
		.gattc_char_disc = {
			.start_handle = 0,
			.end_handle = 0
		}
	};
	ble_gq.conn_handles[0] = 0;

	__cmock_sd_ble_gattc_characteristics_discover_ExpectAndReturn(
		conn_handle, &req.gattc_char_disc, NRF_ERROR_BUSY);
	__cmock_sd_ble_gattc_characteristics_discover_ExpectAndReturn(
		conn_handle, &req.gattc_char_disc, NRF_ERROR_INVALID_ADDR);

	ret = ble_gq_item_add(&ble_gq, &req, 0);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_EQUAL(conn_handle, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_ADDR, glob_error);
}

void test_ble_gq_item_add_req_desc_discovery(void)
{
	uint16_t conn_handle = 0;
	int ret;
	struct ble_gq_req req = {
			.type = BLE_GQ_REQ_DESC_DISCOVERY,
			.error_handler = {
					.cb = ble_gq_error_handler,
					.ctx = NULL
			},
			.gattc_desc_disc = {
					.start_handle = 0,
					.end_handle = 0
			}
	};
	ble_gq.conn_handles[0] = 0;

	__cmock_sd_ble_gattc_descriptors_discover_ExpectAndReturn(
		conn_handle, &req.gattc_desc_disc, NRF_ERROR_BUSY);
	__cmock_sd_ble_gattc_descriptors_discover_ExpectAndReturn(
		conn_handle, &req.gattc_desc_disc, BLE_ERROR_INVALID_CONN_HANDLE);

	ret = ble_gq_item_add(&ble_gq, &req, 0);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_EQUAL(conn_handle, glob_conn_handle);
	TEST_ASSERT_EQUAL(BLE_ERROR_INVALID_CONN_HANDLE, glob_error);
}

void test_ble_gq_item_add_req_gatts_hvx(void)
{
	uint8_t data[] = { 0x01, 0x02, 0x03 };
	uint16_t len = sizeof(data);
	uint16_t conn_handle = 0;
	int ret;
	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_GATTS_HVX,
		.error_handler = {
			.cb = ble_gq_error_handler,
			.ctx = NULL
		},
		.gatts_hvx = {
			.type = BLE_GATT_HVX_NOTIFICATION,
			.handle = 0,
			.offset = 0,
			.p_data = data,
			.p_len = &len
		}
	};
	ble_gq.conn_handles[0] = 0;

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(conn_handle, &req.gatts_hvx, NRF_ERROR_BUSY);
	__cmock_sd_ble_gatts_hvx_IgnoreAndReturn(NRF_SUCCESS);
	ret = ble_gq_item_add(&ble_gq, &req, 0);
	TEST_ASSERT_EQUAL(0, ret);

	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(conn_handle, &req.gatts_hvx, NRF_ERROR_BUSY);
	__cmock_sd_ble_gatts_hvx_IgnoreAndReturn(NRF_ERROR_INVALID_ADDR);

	ret = ble_gq_item_add(&ble_gq, &req, 0);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_EQUAL(conn_handle, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_ADDR, glob_error);

	req.gatts_hvx.p_len = NULL;
	ret = ble_gq_item_add(&ble_gq, &req, 0);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, glob_error);
}

void test_ble_gq_conn_handle_register_efault(void)
{
	int ret = ble_gq_conn_handle_register(NULL, 0);

	TEST_ASSERT_EQUAL(-EFAULT, ret);
}

void test_ble_gq_conn_handle_register_enomem(void)
{
	int ret;

	ret = ble_gq_conn_handle_register(&ble_gq, 0);
	TEST_ASSERT_EQUAL(0, ret);

	for (int i = 0; i < ble_gq.max_conns; i++) {
		ble_gq.conn_handles[i] = i + 1;
	}
	ret = ble_gq_conn_handle_register(&ble_gq, 3);
	TEST_ASSERT_EQUAL(-ENOMEM, ret);
}

void test_ble_gq_on_ble_evt(void)
{
	ble_evt_t ble_evt = {0};
	uint16_t conn_handle = 0x0C4;

	ble_evt.header.evt_id = BLE_GAP_EVT_DISCONNECTED;
	ble_evt.evt.gap_evt.conn_handle = 0x0C4;
	ble_gq.conn_handles[0] = 0x0C4;

	ble_gq_on_ble_evt(NULL, NULL);

	ble_gq_on_ble_evt(&ble_evt, (void *)&ble_gq);
	TEST_ASSERT_EQUAL(ble_gq.conn_handles[0], BLE_CONN_HANDLE_INVALID);

	/* Verify that req_queue_purge_schedule cleared the connection id 0x0C4
	 * on evt BLE_GAP_EVT_DISCONNECTED
	 */
	for (int i = 0; i < ble_gq.max_conns; i++) {
		TEST_ASSERT_NOT_EQUAL(ble_gq.purge_list[i], 0x0C4);
	}

	ble_evt.header.evt_id = BLE_GATTC_EVT_READ_RSP;
	ble_evt.evt.gattc_evt.conn_handle = 0x0C4;
	ble_gq.conn_handles[0] = 0x0C4;
	static struct ble_gq_req req = {
		.type = BLE_GQ_REQ_GATTC_READ,
		.error_handler = {
			.cb = ble_gq_error_handler,
			.ctx = NULL
		},
		.gattc_read = {
			.handle = 0,
			.offset = 0
		}
	};

	sys_slist_init(&ble_gq.req_queue[0]);
	sys_slist_append(&ble_gq.req_queue[0], &req.node);
	__cmock_sd_ble_gattc_read_ExpectAndReturn(conn_handle, req.gattc_read.handle,
			req.gattc_read.offset, NRF_ERROR_TIMEOUT);
	ble_gq_on_ble_evt(&ble_evt, (void *)&ble_gq);
	TEST_ASSERT_EQUAL(conn_handle, glob_conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_TIMEOUT, glob_error);
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
