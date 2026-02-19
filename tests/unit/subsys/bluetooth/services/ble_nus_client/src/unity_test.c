/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <unity.h>
#include <stdint.h>
#include <string.h>

#include <bm/bluetooth/services/ble_nus_client.h>
#include <nrf_error.h>

#include "cmock_ble_gatts.h"
#include "cmock_ble_gq.h"
#include "cmock_ble_gap.h"
#include "cmock_ble_gattc.h"
#include "cmock_ble.h"
#include "cmock_nrf_sdh_ble.h"
#include "cmock_ble_db_discovery.h"

BLE_GQ_DEF(m_ble_gatt_queue);
BLE_DB_DISCOVERY_DEF(m_db_disc);
BLE_NUS_CLIENT_DEF(ble_nus_client);

uint16_t test_case_conn_handle = 0x1000;
bool evt_handler_called;
struct ble_nus_client_context *last_link_ctx;

static struct ble_nus_client_evt nus_client_evt;
static struct ble_nus_client_evt nus_client_evt_prev;

static uint32_t stub_ble_gq_item_add_no_mem(const struct ble_gq *gatt_queue, struct ble_gq_req *req,
					    uint16_t conn_handle, int cmock_num_calls)
{
	struct ble_gq_evt evt = {.evt_type = BLE_GQ_EVT_ERROR, .error.reason = NRF_ERROR_NO_MEM};

	req->evt_handler(req, &evt);

	return NRF_ERROR_NO_MEM;
}

void ble_evt_send(const ble_evt_t *evt)
{
	TYPE_SECTION_FOREACH(struct nrf_sdh_ble_evt_observer, nrf_sdh_ble_evt_observers, obs)
	{
		obs->handler(evt, obs->context);
	}
}

static void ble_nus_client_evt_handler(struct ble_nus_client *ble_nus_client,
				       struct ble_nus_client_evt const *ble_nus_evt)
{
	(void)ble_nus_client;
	nus_client_evt_prev = nus_client_evt;
	nus_client_evt = *ble_nus_evt;
}

static void nus_client_init(struct ble_nus_client_config *nus_cfg)
{
	uint32_t nrf_err;
	uint8_t expected_uuid_type = 123;

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_uuid_vs_add_ReturnThruPtr_p_uuid_type(&expected_uuid_type);

	nrf_err = ble_nus_client_init(&ble_nus_client, nus_cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL_PTR(nus_cfg->evt_handler, ble_nus_client.evt_handler);
}

void test_ble_nus_client_init(void)
{
	uint32_t err_code;

	struct ble_nus_client_config init;

	init.evt_handler = ble_nus_client_evt_handler;
	init.gatt_queue = &m_ble_gatt_queue;
	init.db_discovery = &m_db_disc;

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	err_code = ble_nus_client_init(&ble_nus_client, &init);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err_code);
}

void test_ble_nus_client_init_null(void)
{
	uint32_t nrf_err;

	struct ble_nus_client_config cfg = {.db_discovery = &m_db_disc,
					    .evt_handler = ble_nus_client_evt_handler,
					    .gatt_queue = &m_ble_gatt_queue};

	nrf_err = ble_nus_client_init(NULL, &cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_nus_client_init_invalid_param(void)
{
	uint32_t nrf_err;

	struct ble_nus_client_config cfg = {.db_discovery = &m_db_disc,
					    .evt_handler = ble_nus_client_evt_handler,
					    .gatt_queue = &m_ble_gatt_queue};

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_ERROR_NO_MEM);

	nrf_err = ble_nus_client_init(&ble_nus_client, &cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_nus_client_tx_notif_enable(void)
{
	uint32_t nrf_err;

	struct ble_nus_client_config nus_cfg = {.evt_handler = ble_nus_client_evt_handler,
						.gatt_queue = &m_ble_gatt_queue,
						.db_discovery = &m_db_disc};

	ble_nus_client.uuid_type = BLE_UUID_TYPE_BLE;
	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	nus_client_init(&nus_cfg);
	struct ble_db_discovery_evt db_evt = {
		.conn_handle = test_case_conn_handle,
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.params.discovered_db = {
			.srv_uuid = {.uuid = BLE_UUID_NUS_SERVICE,
				     .type = ble_nus_client.uuid_type},
			.char_count = 2,
			.charateristics = {
				{.characteristic.uuid.uuid = BLE_UUID_NUS_RX_CHARACTERISTIC,
				 .cccd_handle = 0x100},
				{.characteristic.uuid.uuid = BLE_UUID_NUS_TX_CHARACTERISTIC,
				 .cccd_handle = 0x100}}}};

	ble_nus_client_on_db_disc_evt(&ble_nus_client, &db_evt);
	ble_nus_client_handles_assign(&ble_nus_client, db_evt.conn_handle,
				      &nus_client_evt.params.discovery_complete.handles);

	nrf_err = ble_nus_client_tx_notif_enable(&ble_nus_client);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_nus_client_tx_notif_enable_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_nus_client_tx_notif_enable(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_nus_client_tx_notif_enable_invalid_state(void)
{
	uint32_t nrf_err;

	struct ble_nus_client_config nus_cfg = {.evt_handler = ble_nus_client_evt_handler,
						.gatt_queue = &m_ble_gatt_queue,
						.db_discovery = &m_db_disc};

	ble_nus_client.uuid_type = BLE_UUID_TYPE_BLE;
	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	nus_client_init(&nus_cfg);
	ble_nus_client.conn_handle = BLE_CONN_HANDLE_INVALID;
	nrf_err = ble_nus_client_tx_notif_enable(&ble_nus_client);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_nus_client_string_send(void)
{
	uint32_t nrf_err;

	struct ble_nus_client_config cfg = {.evt_handler = ble_nus_client_evt_handler,
					    .gatt_queue = &m_ble_gatt_queue,
					    .db_discovery = &m_db_disc};

	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_item_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	nus_client_init(&cfg);
	struct ble_db_discovery_evt db_evt = {
		.conn_handle = test_case_conn_handle,
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.params.discovered_db = {
			.srv_uuid = {.uuid = BLE_UUID_NUS_SERVICE,
				     .type = ble_nus_client.uuid_type},
			.char_count = 2,
			.charateristics = {
				{.characteristic.uuid.uuid = BLE_UUID_NUS_RX_CHARACTERISTIC,
				 .cccd_handle = 0x100},
				{.characteristic.uuid.uuid = BLE_UUID_NUS_TX_CHARACTERISTIC,
				 .cccd_handle = 0x100}}}};

	ble_nus_client_handles_assign(&ble_nus_client, db_evt.conn_handle,
				      &nus_client_evt.params.discovery_complete.handles);
	ble_nus_client_on_db_disc_evt(&ble_nus_client, &db_evt);

	nrf_err = ble_nus_client_string_send(
		&ble_nus_client, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890",
		sizeof("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890") - 1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_nus_client_string_send_error_no_mem(void)
{
	uint32_t nrf_err;

	struct ble_nus_client_config cfg = {.evt_handler = ble_nus_client_evt_handler,
					    .gatt_queue = &m_ble_gatt_queue,
					    .db_discovery = &m_db_disc};

	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_item_add_Stub(stub_ble_gq_item_add_no_mem);
	nus_client_init(&cfg);
	struct ble_db_discovery_evt db_evt = {
		.conn_handle = test_case_conn_handle,
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.params.discovered_db = {
			.srv_uuid = {.uuid = BLE_UUID_NUS_SERVICE,
				     .type = ble_nus_client.uuid_type},
			.char_count = 2,
			.charateristics = {
				{.characteristic.uuid.uuid = BLE_UUID_NUS_RX_CHARACTERISTIC,
				 .cccd_handle = 0x100},
				{.characteristic.uuid.uuid = BLE_UUID_NUS_TX_CHARACTERISTIC,
				 .cccd_handle = 0x100}}}};

	ble_nus_client_handles_assign(&ble_nus_client, db_evt.conn_handle,
				      &nus_client_evt.params.discovery_complete.handles);
	ble_nus_client_on_db_disc_evt(&ble_nus_client, &db_evt);

	nrf_err = ble_nus_client_string_send(
		&ble_nus_client, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890",
		sizeof("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890") - 1);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_ble_nus_client_string_send_error_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_nus_client_string_send(
		NULL, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890",
		sizeof("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890") - 1);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_nus_client_string_send_error_invalid_param(void)
{
	uint32_t nrf_err;

	struct ble_nus_client_config cfg = {.evt_handler = ble_nus_client_evt_handler,
					    .gatt_queue = &m_ble_gatt_queue,
					    .db_discovery = &m_db_disc};

	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	nus_client_init(&cfg);
	struct ble_db_discovery_evt db_evt = {
		.conn_handle = test_case_conn_handle,
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.params.discovered_db = {
			.srv_uuid = {.uuid = BLE_UUID_NUS_SERVICE,
				     .type = ble_nus_client.uuid_type},
			.char_count = 2,
			.charateristics = {
				{.characteristic.uuid.uuid = BLE_UUID_NUS_RX_CHARACTERISTIC,
				 .cccd_handle = 0x100},
				{.characteristic.uuid.uuid = BLE_UUID_NUS_TX_CHARACTERISTIC,
				 .cccd_handle = 0x100}}}};

	ble_nus_client_handles_assign(&ble_nus_client, db_evt.conn_handle,
				      &nus_client_evt.params.discovery_complete.handles);
	ble_nus_client_on_db_disc_evt(&ble_nus_client, &db_evt);

	char buf[BLE_NUS_MAX_DATA_LEN + 1] = {0};

	nrf_err = ble_nus_client_string_send(&ble_nus_client, buf, sizeof(buf));
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_nus_client_string_send_error_invalid(void)
{
	uint32_t nrf_err;

	struct ble_nus_client_config cfg = {.evt_handler = ble_nus_client_evt_handler,
					    .gatt_queue = &m_ble_gatt_queue,
					    .db_discovery = &m_db_disc};

	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	nus_client_init(&cfg);

	ble_nus_client_handles_assign(&ble_nus_client, BLE_CONN_HANDLE_INVALID,
				      &nus_client_evt.params.discovery_complete.handles);

	nrf_err = ble_nus_client_string_send(
		&ble_nus_client, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890",
		sizeof("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890") - 1);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_nus_client_handles_assign(void)
{
	uint32_t nrf_err;
	struct ble_db_discovery_evt db_evt = {
		.conn_handle = 100,
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.params.discovered_db = {
			.srv_uuid = {.uuid = BLE_UUID_NUS_SERVICE,
				     .type = ble_nus_client.uuid_type},
			.char_count = 2,
			.charateristics = {
				{.characteristic.uuid.uuid = BLE_UUID_NUS_RX_CHARACTERISTIC},
				{.characteristic.uuid.uuid = BLE_UUID_NUS_TX_CHARACTERISTIC}}}};

	struct ble_nus_client_config nus_cfg = {.evt_handler = ble_nus_client_evt_handler,
						.gatt_queue = &m_ble_gatt_queue,
						.db_discovery = &m_db_disc};
	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	nus_client_init(&nus_cfg);

	ble_nus_client_on_db_disc_evt(&ble_nus_client, &db_evt);

	TEST_ASSERT_EQUAL(nus_client_evt.evt_type, BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE);
	nrf_err = ble_nus_client_handles_assign(&ble_nus_client, nus_client_evt.conn_handle,
						&nus_client_evt.params.discovery_complete.handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_nus_client_handles_assign_error_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_nus_client_handles_assign(NULL, 0x01, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_nus_client_on_ble_evt_null(void)
{
	ble_nus_client_on_ble_evt(NULL, NULL);
}

void test_ble_nus_client_on_ble_evt(void)
{
	ble_evt_t ble_evt = {.header.evt_id = BLE_GATTC_EVT_HVX,
			     .evt.gattc_evt.params.hvx = {.data = {0x42},
							  .len = 1,
							  .handle = 0x100,
							  .type = BLE_GATT_HVX_NOTIFICATION}};

	struct ble_nus_client_config cfg = {.evt_handler = ble_nus_client_evt_handler,
					    .gatt_queue = &m_ble_gatt_queue,
					    .db_discovery = &m_db_disc};

	__cmock_ble_db_discovery_service_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_gq_conn_handle_register_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_ble_db_discovery_on_ble_evt_ExpectAnyArgs();
	__cmock_ble_gq_on_ble_evt_ExpectAnyArgs();
	nus_client_init(&cfg);

	struct ble_db_discovery_evt db_evt = {
		.conn_handle = test_case_conn_handle,
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.params.discovered_db = {
			.srv_uuid = {.uuid = BLE_UUID_NUS_SERVICE,
				     .type = ble_nus_client.uuid_type},
			.char_count = 2,
			.charateristics = {
				{.characteristic.uuid.uuid = BLE_UUID_NUS_RX_CHARACTERISTIC,
				 .characteristic.handle_value = 0x100},
				{.characteristic.uuid.uuid = BLE_UUID_NUS_TX_CHARACTERISTIC,
				 .cccd_handle = 0x200,
				 .characteristic.handle_value = 0x100}}}};

	ble_nus_client_on_db_disc_evt(&ble_nus_client, &db_evt);
	ble_nus_client_handles_assign(&ble_nus_client, db_evt.conn_handle,
				      &nus_client_evt.params.discovery_complete.handles);

	__cmock_ble_db_discovery_on_ble_evt_ExpectAnyArgs();
	__cmock_ble_gq_on_ble_evt_ExpectAnyArgs();
	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_NUS_CLIENT_EVT_NUS_TX_EVT, nus_client_evt.evt_type);
	TEST_ASSERT_EQUAL(ble_evt.evt.gattc_evt.params.hvx.data[0],
			  nus_client_evt.params.nus_tx_evt.data[0]);
	TEST_ASSERT_EQUAL(ble_evt.evt.gattc_evt.params.hvx.len,
			  nus_client_evt.params.nus_tx_evt.data_len);

	ble_evt.header.evt_id = BLE_GAP_EVT_DISCONNECTED;
	ble_evt.evt.gap_evt.conn_handle = test_case_conn_handle;
	ble_evt.evt.gap_evt.params.disconnected.reason = BLE_HCI_LOCAL_HOST_TERMINATED_CONNECTION;
	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_NUS_CLIENT_EVT_DISCONNECTED, nus_client_evt.evt_type);
	TEST_ASSERT_EQUAL(ble_evt.evt.gap_evt.params.disconnected.reason,
			  nus_client_evt.params.disconnected.reason);
	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, ble_nus_client.conn_handle);
}

void setUp(void)
{
	evt_handler_called = false;
	test_case_conn_handle++;
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
