/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
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
#include "cmock_ble.h"
#include "cmock_nrf_sdh_ble.h"

static struct ble_nus ble_nus;

uint16_t test_case_conn_handle = 0x1000;
bool evt_handler_called;
struct ble_nus_client_context *last_link_ctx;

static uint32_t stub_sd_ble_gatts_service_add(uint8_t type, const ble_uuid_t *p_uuid,
					      uint16_t *p_handle, int calls)
{
	ble_uuid_t expected_uuid = {
		.type = 123,
		.uuid = BLE_UUID_NUS_SERVICE,
	};
	uint16_t expected_conn_handle = BLE_CONN_HANDLE_INVALID;

	TEST_ASSERT_EQUAL(BLE_GATTS_SRVC_TYPE_PRIMARY, type);
	TEST_ASSERT_EQUAL(expected_uuid.type, p_uuid->type);
	TEST_ASSERT_EQUAL(expected_uuid.uuid, p_uuid->uuid);
	TEST_ASSERT_EQUAL(expected_conn_handle, *p_handle);
	*p_handle = test_case_conn_handle;

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_characteristic_add(uint16_t service_handle,
						     const ble_gatts_char_md_t *p_char_md,
						     const ble_gatts_attr_t *p_attr_char_value,
						     ble_gatts_char_handles_t *p_handles, int calls)
{
	ble_uuid_t expected_char_uuid = {.type = 123};

	TEST_ASSERT_EQUAL(expected_char_uuid.type, p_attr_char_value->p_uuid->type);
	TEST_ASSERT_EQUAL(test_case_conn_handle, service_handle);

	p_handles->cccd_handle = 0x101;
	p_handles->value_handle = 0x102;

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_value_get(uint16_t conn_handle, uint16_t handle,
					    ble_gatts_value_t *p_value, int calls)
{
	TEST_ASSERT_EQUAL(test_case_conn_handle, conn_handle);
	TEST_ASSERT_EQUAL(0x101, handle);

	*p_value->p_value = BLE_GATT_HVX_NOTIFICATION;

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gatts_value_get_err(uint16_t conn_handle, uint16_t handle,
						ble_gatts_value_t *p_value, int calls)
{
	TEST_ASSERT_EQUAL(test_case_conn_handle, conn_handle);
	TEST_ASSERT_EQUAL(0x101, handle);

	switch (calls) {
	case 0:
		*p_value->p_value = BLE_GATT_HVX_NOTIFICATION;
		return NRF_ERROR_INVALID_PARAM;
	case 1:
		*p_value->p_value = BLE_GATT_HVX_INDICATION;
		return NRF_SUCCESS;
	default:
		return -1;
	}
}

static void ble_nus_evt_handler_on_connect(struct ble_nus *nus, const struct ble_nus_evt *evt)
{
	last_link_ctx = evt->link_ctx;
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_COMM_STARTED, evt->evt_type);
	TEST_ASSERT_TRUE(evt->link_ctx->is_notification_enabled);
	evt_handler_called = true;
}

static void ble_nus_evt_handler_on_connect_null_ctx(struct ble_nus *nus,
						    const struct ble_nus_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_ERROR, evt->evt_type);
	TEST_ASSERT_NULL(evt->link_ctx);
	evt_handler_called = true;
}

static void ble_nus_evt_handler_on_write_notif(struct ble_nus *nus, const struct ble_nus_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_COMM_STARTED, evt->evt_type);
	TEST_ASSERT_TRUE(evt->link_ctx->is_notification_enabled);
	evt_handler_called = true;
}

static void ble_nus_evt_handler_on_write_indica(struct ble_nus *nus, const struct ble_nus_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_COMM_STOPPED, evt->evt_type);
	TEST_ASSERT_FALSE(evt->link_ctx->is_notification_enabled);
	evt_handler_called = true;
}

static void ble_nus_evt_handler_on_write_value(struct ble_nus *nus, const struct ble_nus_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_RX_DATA, evt->evt_type);
	TEST_ASSERT_EQUAL(0xAB, evt->rx_data.data[0]);
	TEST_ASSERT_EQUAL(0xCD, evt->rx_data.data[1]);
	TEST_ASSERT_EQUAL(2, evt->rx_data.length);
	evt_handler_called = true;
}

static void ble_nus_evt_handler_on_hvx_tx_complete(struct ble_nus *nus,
						   const struct ble_nus_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_TX_RDY, evt->evt_type);
	TEST_ASSERT_EQUAL_PTR(last_link_ctx, evt->link_ctx);
	evt_handler_called = true;
}

static void nus_init(struct ble_nus_config *nus_cfg)
{
	uint32_t nrf_err;
	uint8_t expected_uuid_type = 123;

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_uuid_vs_add_ReturnThruPtr_p_uuid_type(&expected_uuid_type);
	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add);

	nrf_err = ble_nus_init(&ble_nus, nus_cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL_PTR(nus_cfg->evt_handler, ble_nus.evt_handler);
}

static void setup_with_notif_enabled(uint16_t conn_handle)
{
	ble_evt_t const ble_evt = {.evt.gap_evt.conn_handle = conn_handle,
				   .header.evt_id = BLE_GAP_EVT_CONNECTED};

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(conn_handle, 0);
	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	__cmock_sd_ble_gatts_value_get_Stub(NULL);

	TEST_ASSERT_TRUE(evt_handler_called);
}

void setUp(void)
{
	memset(&ble_nus, 0, sizeof(ble_nus));
	evt_handler_called = false;
	test_case_conn_handle++;
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
