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
	//__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);
	//__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add);

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
	nus_client_init(&nus_cfg);
	struct ble_db_discovery_evt db_evt = {
		.evt_type = BLE_DB_DISCOVERY_COMPLETE,
		.conn_handle = test_case_conn_handle,
		.params.discovered_db.srv_uuid.uuid = BLE_UUID_NUS_SERVICE,
		.params.discovered_db.srv_uuid.type = ble_nus_client.uuid_type};
	// struct ble_gatt_db_char *chars = evt.params.discovered_db.charateristics; // TODO: Make
	// fake characteristics

	ble_nus_client_handles_assign(&ble_nus_client, nus_client_evt.conn_handle,
				      &nus_client_evt.params.discovery_complete.handles);
	ble_nus_client_on_db_disc_evt(&ble_nus_client, &db_evt);

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

	nus_client_init(&cfg);

	nrf_err = ble_nus_client_string_send(
		&ble_nus_client, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890",
		sizeof("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890") - 1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
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
	nus_client_init(&nus_cfg);

	ble_nus_client_on_db_disc_evt(&ble_nus_client, &db_evt);

	TEST_ASSERT_EQUAL(nus_client_evt.evt_type, BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE);
	nrf_err = ble_nus_client_handles_assign(&ble_nus_client, nus_client_evt.conn_handle,
						&nus_client_evt.params.discovery_complete.handles);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
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
