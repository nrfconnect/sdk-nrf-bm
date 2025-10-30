/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <unity.h>
#include <stdint.h>
#include <string.h>

#include <bm/bluetooth/services/ble_nus.h>
#include <nrf_error.h>

#include "cmock_ble_gatts.h"
#include "cmock_ble.h"
#include "cmock_nrf_sdh_ble.h"

BLE_NUS_DEF(ble_nus);
uint16_t test_case_conn_handle = 0x1000;
bool evt_handler_called;
struct ble_nus_client_context *last_link_ctx;

static uint32_t stub_sd_ble_gatts_service_add(uint8_t type, ble_uuid_t const *p_uuid,
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

static void ble_nus_evt_handler_on_connect(const struct ble_nus_evt *evt)
{
	last_link_ctx = evt->link_ctx;
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_COMM_STARTED, evt->type);
	TEST_ASSERT_TRUE(evt->link_ctx->is_notification_enabled);
	evt_handler_called = true;
}

static void ble_nus_evt_handler_on_connect_null_ctx(const struct ble_nus_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_COMM_STARTED, evt->type);
	TEST_ASSERT_NULL(evt->link_ctx);
	evt_handler_called = true;
}

static void ble_nus_evt_handler_on_write_notif(const struct ble_nus_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_COMM_STARTED, evt->type);
	TEST_ASSERT_TRUE(evt->link_ctx->is_notification_enabled);
	evt_handler_called = true;
}

static void ble_nus_evt_handler_on_write_indica(const struct ble_nus_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_COMM_STOPPED, evt->type);
	TEST_ASSERT_FALSE(evt->link_ctx->is_notification_enabled);
	evt_handler_called = true;
}

static void ble_nus_evt_handler_on_write_value(const struct ble_nus_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_RX_DATA, evt->type);
	TEST_ASSERT_EQUAL(0xAB, evt->params.rx_data.data[0]);
	TEST_ASSERT_EQUAL(0xCD, evt->params.rx_data.data[1]);
	TEST_ASSERT_EQUAL(2, evt->params.rx_data.length);
	evt_handler_called = true;
}

static void ble_nus_evt_handler_on_hvx_tx_complete(const struct ble_nus_evt *evt)
{
	TEST_ASSERT_EQUAL(BLE_NUS_EVT_TX_RDY, evt->type);
	TEST_ASSERT_EQUAL_PTR(last_link_ctx, evt->link_ctx);
	evt_handler_called = true;
}

static void nus_init(struct ble_nus_config *nus_cfg)
{
	int ret;
	uint8_t expected_uuid_type = 123;

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_uuid_vs_add_ReturnThruPtr_p_uuid_type(&expected_uuid_type);
	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add);

	ret = ble_nus_init(&ble_nus, nus_cfg);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_EQUAL_PTR(nus_cfg->evt_handler, ble_nus.evt_handler);
}

static void setup_with_notif_enabled(uint16_t conn_handle)
{
	ble_evt_t const ble_evt = {
		.evt.gap_evt.conn_handle = conn_handle,
		.header.evt_id = BLE_GAP_EVT_CONNECTED
	};

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(conn_handle, 0);
	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	__cmock_sd_ble_gatts_value_get_Stub(NULL);

	TEST_ASSERT_TRUE(evt_handler_called);
}

void test_ble_nus_init_efault(void)
{
	int ret;
	struct ble_nus_config nus_cfg = {0};

	ret = ble_nus_init(NULL, &nus_cfg);
	TEST_ASSERT_EQUAL(-EFAULT, ret);

	ret = ble_nus_init(&ble_nus, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, ret);
}

void test_ble_nus_init_einval(void)
{
	int ret;
	struct ble_nus_config nus_cfg = {0};

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	ret = ble_nus_init(&ble_nus, &nus_cfg);
	TEST_ASSERT_EQUAL(-EINVAL, ret);

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	ret = ble_nus_init(&ble_nus, &nus_cfg);
	TEST_ASSERT_EQUAL(-EINVAL, ret);

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	ret = ble_nus_init(&ble_nus, &nus_cfg);
	TEST_ASSERT_EQUAL(-EINVAL, ret);

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	ret = ble_nus_init(&ble_nus, &nus_cfg);
	TEST_ASSERT_EQUAL(-EINVAL, ret);
}

void test_ble_nus_init_success(void)
{
	int ret;
	struct ble_nus_config nus_cfg = {0};
	uint8_t expected_uuid_type = 123;

	__cmock_sd_ble_uuid_vs_add_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_uuid_vs_add_ReturnThruPtr_p_uuid_type(&expected_uuid_type);

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add);

	ret = ble_nus_init(&ble_nus, &nus_cfg);
}

void test_ble_nus_on_ble_evt_gap_evt_do_nothing(void)
{
	ble_evt_t const ble_evt = {0};
	struct ble_nus nus_ctx = {0};
	ble_evt_t empty_ble_evt = {0};
	struct ble_nus empty_nus_ctx = {0};

	ble_nus_on_ble_evt(NULL, &nus_ctx);
	ble_nus_on_ble_evt(&ble_evt, NULL);
	ble_nus_on_ble_evt(&ble_evt, &nus_ctx);

	TEST_ASSERT_EQUAL_MEMORY(&empty_ble_evt, &ble_evt, sizeof(ble_evt_t));
	TEST_ASSERT_EQUAL_MEMORY(&empty_nus_ctx, &nus_ctx, sizeof(struct ble_nus));
}

void test_ble_nus_on_ble_evt_gap_evt_on_connect_readiness(void)
{
	ble_evt_t const ble_evt = {.evt.gap_evt.conn_handle = test_case_conn_handle,
				   .header.evt_id = BLE_GAP_EVT_CONNECTED};
	struct ble_nus_config nus_cfg = { .evt_handler = NULL };

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);
	ble_nus_on_ble_evt(&ble_evt, &ble_nus);

	TEST_ASSERT_FALSE(evt_handler_called);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_err);
	ble_nus.evt_handler = ble_nus_evt_handler_on_connect;
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);

	TEST_ASSERT_FALSE(evt_handler_called);

	ble_nus.evt_handler = ble_nus_evt_handler_on_connect;
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);

	TEST_ASSERT_FALSE(evt_handler_called);
}

void test_ble_nus_on_ble_evt_gap_evt_on_connect(void)
{
	ble_evt_t const ble_evt = {.evt.gap_evt.conn_handle = test_case_conn_handle,
				   .header.evt_id = BLE_GAP_EVT_CONNECTED};
	struct ble_nus_config nus_cfg = { .evt_handler = ble_nus_evt_handler_on_connect };

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);
	ble_nus_on_ble_evt(&ble_evt, &ble_nus);

	TEST_ASSERT_TRUE(evt_handler_called);
}

void test_ble_nus_on_ble_evt_gap_evt_on_connect_null_ctx(void)
{
	ble_evt_t const ble_evt = {.evt.gap_evt.conn_handle = test_case_conn_handle,
				   .header.evt_id = BLE_GAP_EVT_CONNECTED};
	struct ble_nus_config nus_cfg = { .evt_handler = ble_nus_evt_handler_on_connect_null_ctx };

	nus_init(&nus_cfg);

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, -1);
	ble_nus_on_ble_evt(&ble_evt, &ble_nus);

	TEST_ASSERT_TRUE(evt_handler_called);
}

void test_ble_nus_on_ble_evt_gap_evt_on_write(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_WRITE,
		.evt.gatts_evt = {
			.conn_handle = test_case_conn_handle,
			.params.write = {
				.handle = 0x101,
				.len = 2,
			},
		},
	};
	struct ble_nus_config nus_cfg = { .evt_handler = ble_nus_evt_handler_on_write_notif };
	uint16_t *const data_notif_enable = (uint16_t *)ble_evt.evt.gatts_evt.params.write.data;

	nus_init(&nus_cfg);

	*data_notif_enable = BLE_GATT_HVX_NOTIFICATION;
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	TEST_ASSERT_TRUE(evt_handler_called);

	evt_handler_called = false;
	*data_notif_enable = BLE_GATT_HVX_INDICATION;
	ble_nus.evt_handler = ble_nus_evt_handler_on_write_indica;
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	TEST_ASSERT_TRUE(evt_handler_called);

	evt_handler_called = false;
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, -1);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	TEST_ASSERT_FALSE(evt_handler_called);

	uint8_t *const data_ptr = ble_evt.evt.gatts_evt.params.write.data;

	evt_handler_called = false;
	ble_evt.evt.gatts_evt.params.write.handle = 0x102;
	ble_nus.evt_handler = ble_nus_evt_handler_on_write_value;
	data_ptr[0] = 0xAB;
	data_ptr[1] = 0xCD;
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	TEST_ASSERT_TRUE(evt_handler_called);
}

void test_ble_nus_on_hvx_tx_complete(void)
{
	ble_evt_t ble_evt = {.evt.gap_evt.conn_handle = test_case_conn_handle,
			     .header.evt_id = BLE_GAP_EVT_CONNECTED};
	struct ble_nus_config nus_cfg = { .evt_handler = ble_nus_evt_handler_on_connect };

	nus_init(&nus_cfg);

	/* Setup context */
	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);
	ble_nus_on_ble_evt(&ble_evt, &ble_nus);

	TEST_ASSERT_TRUE(evt_handler_called);

	/* Test a non relevant event */
	evt_handler_called = false;
	ble_evt.header.evt_id = BLE_GATTS_EVT_HVN_TX_COMPLETE;
	ble_nus.evt_handler = NULL;
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	TEST_ASSERT_FALSE(evt_handler_called);

	/* Test a relevant event */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);
	ble_nus.evt_handler = ble_nus_evt_handler_on_hvx_tx_complete;

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	TEST_ASSERT_TRUE(evt_handler_called);
}

void test_ble_nus_data_send_efault(void)
{
	int ret;
	uint8_t data[2];
	uint16_t length = sizeof(data);

	ret = ble_nus_data_send(NULL, NULL, NULL, test_case_conn_handle);
	TEST_ASSERT_EQUAL(-EFAULT, ret);

	ret = ble_nus_data_send(&ble_nus, data, NULL, test_case_conn_handle);
	TEST_ASSERT_EQUAL(-EFAULT, ret);

	ret = ble_nus_data_send(&ble_nus, NULL, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(-EFAULT, ret);

	ret = ble_nus_data_send(NULL, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(-EFAULT, ret);
}

void test_ble_nus_data_send_einval(void)
{
	int ret;
	uint8_t data[2];
	uint16_t length = sizeof(data);
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_WRITE,
		.evt.gatts_evt = {
			.conn_handle = test_case_conn_handle,
			.params.write = {
				.handle = 0x101,
				.len = 2,
			},
		},
	};
	struct ble_nus_config nus_cfg = { .evt_handler = ble_nus_evt_handler_on_write_notif };
	uint16_t *const data_notif_enable = (uint16_t *)ble_evt.evt.gatts_evt.params.write.data;

	nus_init(&nus_cfg);

	/* Set context is_notification_enabled to `false` */
	evt_handler_called = false;
	*data_notif_enable = BLE_GATT_HVX_INDICATION;
	ble_nus.evt_handler = ble_nus_evt_handler_on_write_indica;
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);

	ble_nus_on_ble_evt(&ble_evt, &ble_nus);
	TEST_ASSERT_TRUE(evt_handler_called);

	/* Test for -EINVAL */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);
	ret = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(-EINVAL, ret);

	length = (BLE_NUS_MAX_DATA_LEN + 1);
	ret = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(-EINVAL, ret);
}

void test_ble_nus_data_send_enoent(void)
{
	int ret;
	uint8_t data[2];
	uint16_t length = sizeof(data);
	uint16_t conn_handle_inval = BLE_CONN_HANDLE_INVALID;

	ret = ble_nus_data_send(&ble_nus, data, &length, conn_handle_inval);
	TEST_ASSERT_EQUAL(-ENOENT, ret);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, -1);
	ret = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(-ENOENT, ret);
}

void test_ble_nus_data_send_eio(void)
{
	int ret;
	uint8_t data[2];
	uint16_t length = sizeof(data);
	struct ble_nus_config nus_cfg = { .evt_handler = ble_nus_evt_handler_on_connect };

	nus_init(&nus_cfg);
	setup_with_notif_enabled(test_case_conn_handle);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);
	ret = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(-EIO, ret);
}

void test_ble_nus_data_send_enotconn(void)
{
	int ret;
	uint8_t data[2];
	uint16_t length = sizeof(data);
	struct ble_nus_config nus_cfg = { .evt_handler = ble_nus_evt_handler_on_connect };

	nus_init(&nus_cfg);
	setup_with_notif_enabled(test_case_conn_handle);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(BLE_ERROR_INVALID_CONN_HANDLE);
	ret = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(-ENOTCONN, ret);
}

void test_ble_nus_data_send_epipe(void)
{
	int ret;
	uint8_t data[2];
	uint16_t length = sizeof(data);
	struct ble_nus_config nus_cfg = { .evt_handler = ble_nus_evt_handler_on_connect };

	nus_init(&nus_cfg);
	setup_with_notif_enabled(test_case_conn_handle);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_STATE);
	ret = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(-EPIPE, ret);
}

void test_ble_nus_data_send_ebadf(void)
{
	int ret;
	uint8_t data[2];
	uint16_t length = sizeof(data);
	struct ble_nus_config nus_cfg = { .evt_handler = ble_nus_evt_handler_on_connect };

	nus_init(&nus_cfg);
	setup_with_notif_enabled(test_case_conn_handle);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_ERROR_NOT_FOUND);
	ret = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(-EBADF, ret);
}

void test_ble_nus_data_send_eagain(void)
{
	int ret;
	uint8_t data[2];
	uint16_t length = sizeof(data);
	struct ble_nus_config nus_cfg = { .evt_handler = ble_nus_evt_handler_on_connect };

	nus_init(&nus_cfg);
	setup_with_notif_enabled(test_case_conn_handle);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);
	__cmock_sd_ble_gatts_hvx_ExpectAnyArgsAndReturn(NRF_ERROR_RESOURCES);
	ret = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(-EAGAIN, ret);
}

void test_ble_nus_data_send_success(void)
{
	int ret;
	uint8_t data[2] = {0x01, 0x02};
	uint16_t length = sizeof(data);
	ble_gatts_hvx_params_t expected_hvx_params = {
		.p_data = data,
		.p_len = &length,
		.type = BLE_GATT_HVX_NOTIFICATION,
	};
	struct ble_nus_config nus_cfg = { .evt_handler = ble_nus_evt_handler_on_connect };

	nus_init(&nus_cfg);
	setup_with_notif_enabled(test_case_conn_handle);
	expected_hvx_params.handle = ble_nus.tx_handles.value_handle;
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(test_case_conn_handle, 0);
	__cmock_sd_ble_gatts_hvx_ExpectAndReturn(test_case_conn_handle,
					  &expected_hvx_params, NRF_SUCCESS);

	ret = ble_nus_data_send(&ble_nus, data, &length, test_case_conn_handle);
	TEST_ASSERT_EQUAL(0, ret);
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
