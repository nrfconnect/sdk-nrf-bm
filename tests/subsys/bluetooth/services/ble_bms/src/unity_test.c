/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unity.h>
#include <ble.h>
#include <ble_err.h>
#include <ble_gatt.h>
#include <nrf_error.h>
#include <bm/bluetooth/services/common.h>
#include <bm/bluetooth/services/ble_bms.h>
#include <bm/bluetooth/services/uuid.h>

#include "cmock_ble_gatts.h"
#include "cmock_ble_qwr.h"

#define SERVICE_HANDLE 0x1234

#define FEATURE_SEC_LV 1
#define FEATURE_SEC_SM 2
#define CTRLPT_SEC_LV 1
#define CTRLPT_SEC_SM 3

#define BLE_BMS_FEATURE_LEN 3
#define CTRLPT_VALUE_HANDLE 2

#define CONN_HANDLE 10

BLE_BMS_DEF(ble_bms);
BLE_QWR_DEF(ble_qwr);

#define AUTH_CODE_VALID "abc"

struct ble_bms_evt last_evt;

void bms_evt_handler(struct ble_bms *bms, struct ble_bms_evt *evt)
{
	uint32_t nrf_err;
	bool is_authorized = true;

	last_evt = *evt;

	switch (evt->evt_type) {
	case BLE_BMS_EVT_ERROR:
	TEST_ASSERT_EQUAL(NRF_ERROR_BUSY, evt->error.reason);
		break;
	case BLE_BMS_EVT_AUTH:
		if ((evt->auth.auth_code.len != strlen(AUTH_CODE_VALID)) ||
		    (memcmp(AUTH_CODE_VALID, evt->auth.auth_code.code,
			    strlen(AUTH_CODE_VALID)) != 0)) {
			is_authorized = false;
		}
		nrf_err = ble_bms_auth_response(&ble_bms, is_authorized);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
		break;
	case BLE_BMS_EVT_BOND_DELETE_REQUESTING:
		break;
	case BLE_BMS_EVT_BOND_DELETE_ALL:
		break;
	case BLE_BMS_EVT_BOND_DELETE_ALL_EXCEPT_REQUESTING:
		break;
	}
}

uint32_t stub_sd_ble_gatts_service_add(uint8_t type, ble_uuid_t const *p_uuid, uint16_t *p_handle,
				       int cmock_calls)
{
	TEST_ASSERT_EQUAL(BLE_GATTS_SRVC_TYPE_PRIMARY, type);
	TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_uuid->type);
	TEST_ASSERT_EQUAL(BLE_UUID_BMS_SERVICE, p_uuid->uuid);

	*p_handle = SERVICE_HANDLE;

	return NRF_SUCCESS;
}

uint32_t stub_sd_ble_gatts_characteristic_add_feature_char_error_no_mem(
	uint16_t service_handle, ble_gatts_char_md_t const *p_char_md,
	ble_gatts_attr_t const *p_attr_char_value, ble_gatts_char_handles_t *p_handles,
	int cmock_calls)
{
	/* Encoded expected feature of delete_all_auth, delete_requesting and
	 * delete_all_but_requesting_auth.
	 */
	const uint8_t encoded_feature_expected[BLE_BMS_FEATURE_LEN] = { 0x10, 0x08, 0x02 };

	TEST_ASSERT_EQUAL(SERVICE_HANDLE, service_handle);
	TEST_ASSERT_TRUE(p_char_md->char_props.read);

	TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_attr_char_value->p_uuid->type);
	TEST_ASSERT_EQUAL(BLE_UUID_BMS_FEATURE, p_attr_char_value->p_uuid->uuid);
	TEST_ASSERT_EQUAL(BLE_GATTS_VLOC_STACK, p_attr_char_value->p_attr_md->vloc);
	TEST_ASSERT_EQUAL(FEATURE_SEC_LV, p_attr_char_value->p_attr_md->read_perm.lv);
	TEST_ASSERT_EQUAL(FEATURE_SEC_SM, p_attr_char_value->p_attr_md->read_perm.sm);
	TEST_ASSERT_EQUAL(BLE_BMS_FEATURE_LEN, p_attr_char_value->init_len);
	TEST_ASSERT_EQUAL_MEMORY(encoded_feature_expected, p_attr_char_value->p_value,
				 p_attr_char_value->init_len);

	TEST_ASSERT_EQUAL(BLE_BMS_FEATURE_LEN, p_attr_char_value->max_len);

	return NRF_ERROR_NO_MEM;
}

uint32_t stub_sd_ble_gatts_characteristic_add_ctrlpt_char_error_no_mem(
	uint16_t service_handle, ble_gatts_char_md_t const *p_char_md,
	ble_gatts_attr_t const *p_attr_char_value, ble_gatts_char_handles_t *p_handles,
	int cmock_calls)
{
	if (cmock_calls < 1) {
		/* Earlier calls verified by other tests, return successfully. */
		return NRF_SUCCESS;
	}

	TEST_ASSERT_EQUAL(SERVICE_HANDLE, service_handle);
	TEST_ASSERT_TRUE(p_char_md->char_props.write);
	TEST_ASSERT_FALSE(p_char_md->char_ext_props.reliable_wr);

	TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_attr_char_value->p_uuid->type);
	TEST_ASSERT_EQUAL(BLE_UUID_BMS_CTRLPT, p_attr_char_value->p_uuid->uuid);
	TEST_ASSERT_EQUAL(BLE_GATTS_VLOC_STACK, p_attr_char_value->p_attr_md->vloc);
	TEST_ASSERT_TRUE(p_attr_char_value->p_attr_md->wr_auth);
	TEST_ASSERT_TRUE(p_attr_char_value->p_attr_md->vlen);
	TEST_ASSERT_EQUAL(CTRLPT_SEC_LV, p_attr_char_value->p_attr_md->write_perm.lv);
	TEST_ASSERT_EQUAL(CTRLPT_SEC_SM, p_attr_char_value->p_attr_md->write_perm.sm);
	TEST_ASSERT_EQUAL(0, p_attr_char_value->init_len);
	TEST_ASSERT_NULL(p_attr_char_value->p_value);
	TEST_ASSERT_EQUAL(BLE_BMS_CTRLPT_MAX_LEN, p_attr_char_value->max_len);

	return NRF_ERROR_NO_MEM;
}

uint32_t stub_sd_ble_gatts_characteristic_add(
	uint16_t service_handle, ble_gatts_char_md_t const *p_char_md,
	ble_gatts_attr_t const *p_attr_char_value, ble_gatts_char_handles_t *p_handles,
	int cmock_calls)
{
	if (cmock_calls < 1) {
		/* Earlier calls verified by other tests, return successfully. */
		return NRF_SUCCESS;
	}

	p_handles->value_handle = CTRLPT_VALUE_HANDLE;

	return NRF_SUCCESS;
}

uint32_t stub_sd_ble_gatts_rw_authorize_reply_accepted(
	uint16_t conn_handle,
	ble_gatts_rw_authorize_reply_params_t const *p_rw_authorize_reply_params, int cmock_calls)
{
	TEST_ASSERT_EQUAL(1, p_rw_authorize_reply_params->params.write.update);

	return NRF_SUCCESS;
}

uint32_t stub_sd_ble_gatts_rw_authorize_reply_rejected(
	uint16_t conn_handle,
	ble_gatts_rw_authorize_reply_params_t const *p_rw_authorize_reply_params, int cmock_calls)
{
	TEST_ASSERT_EQUAL(0, p_rw_authorize_reply_params->params.write.update);

	return NRF_SUCCESS;
}

uint32_t stub_sd_ble_gatts_rw_authorize_reply_error(
	uint16_t conn_handle,
	ble_gatts_rw_authorize_reply_params_t const *p_rw_authorize_reply_params, int cmock_calls)
{
	return NRF_ERROR_BUSY;
}

uint32_t stub_ble_qwr_value_get_auth_req_accepted(
	struct ble_qwr *qwr, uint16_t attr_handle, uint8_t *mem, uint16_t *len, int cmock_calls)
{
	TEST_ASSERT_EQUAL_PTR(&ble_qwr, qwr);
	TEST_ASSERT_EQUAL(CTRLPT_VALUE_HANDLE, attr_handle);

	uint8_t data_val[] = { BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_LE_ONLY, 'a', 'b', 'c' };

	memcpy(mem, data_val, sizeof(data_val));
	*len = sizeof(data_val);

	return NRF_SUCCESS;
}

uint32_t stub_ble_qwr_value_get_auth_req_no_data(
	struct ble_qwr *qwr, uint16_t attr_handle, uint8_t *mem, uint16_t *len, int cmock_calls)
{
	TEST_ASSERT_EQUAL_PTR(&ble_qwr, qwr);
	TEST_ASSERT_EQUAL(CTRLPT_VALUE_HANDLE, attr_handle);

	*len = 0;

	return NRF_SUCCESS;
}

uint32_t stub_ble_qwr_value_get_auth_req_rejected(
	struct ble_qwr *qwr, uint16_t attr_handle, uint8_t *mem, uint16_t *len, int cmock_calls)
{
	TEST_ASSERT_EQUAL_PTR(&ble_qwr, qwr);
	TEST_ASSERT_EQUAL(CTRLPT_VALUE_HANDLE, attr_handle);

	uint8_t data_inval[] = { BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_LE_ONLY, 'b', 'a', 'd' };

	memcpy(mem, data_inval, sizeof(data_inval));
	*len = sizeof(data_inval);

	return NRF_SUCCESS;
}

uint32_t stub_ble_qwr_value_get_error_no_mem(
	struct ble_qwr *qwr, uint16_t attr_handle, uint8_t *mem, uint16_t *len, int cmock_calls)
{
	return NRF_ERROR_NO_MEM;
}

uint32_t stub_sd_ble_gatts_value_get_accepted(uint16_t conn_handle, uint16_t handle,
					      ble_gatts_value_t *p_value, int cmock_calls)
{
	TEST_ASSERT_EQUAL(CONN_HANDLE, conn_handle);
	TEST_ASSERT_EQUAL(CTRLPT_VALUE_HANDLE, handle);

	uint8_t data_val[] = { BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_LE_ONLY, 'a', 'b', 'c' };

	memcpy(p_value->p_value, data_val, sizeof(data_val));
	p_value->len = sizeof(data_val);

	return NRF_SUCCESS;
}

uint32_t stub_sd_ble_gatts_value_get_no_data(uint16_t conn_handle, uint16_t handle,
					     ble_gatts_value_t *p_value, int cmock_calls)
{
	p_value->len = 0;

	return NRF_SUCCESS;
}

uint32_t stub_sd_ble_gatts_value_get_error_not_found(uint16_t conn_handle, uint16_t handle,
						     ble_gatts_value_t *p_value, int cmock_calls)
{
	return NRF_ERROR_NOT_FOUND;
}

void test_ble_bms_init_error_null(void)
{
	uint32_t nrf_err;
	struct ble_bms_config bms_cfg = {0};

	nrf_err = ble_bms_init(NULL, &bms_cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_bms_init(&ble_bms, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_bms_init_error_invalid_addr(void)
{
	uint32_t nrf_err;
	struct ble_bms_config bms_cfg = {0};

	nrf_err = ble_bms_init(&ble_bms, &bms_cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_ADDR, nrf_err);
}

void test_ble_bms_init_error_no_mem(void)
{
	uint32_t nrf_err;
	struct ble_bms_config bms_cfg = {
		.evt_handler = bms_evt_handler,
	};

	__cmock_sd_ble_gatts_service_add_ExpectAndReturn(BLE_GATTS_SRVC_TYPE_PRIMARY,
							 NULL, &ble_bms.service_handle,
							 NRF_ERROR_NO_MEM);
	__cmock_sd_ble_gatts_service_add_IgnoreArg_p_uuid();

	nrf_err = ble_bms_init(&ble_bms, &bms_cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_ble_bms_init_error_feature_add_no_mem(void)
{
	uint32_t nrf_err;
	struct ble_bms_config bms_cfg = {
		.evt_handler = bms_evt_handler,
		.feature_sec.lv = FEATURE_SEC_LV,
		.feature_sec.sm = FEATURE_SEC_SM,
		.feature = {
			.delete_all_auth = true,
			.delete_requesting  = true,
			.delete_all_but_requesting_auth = true,
		},
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);

	__cmock_sd_ble_gatts_characteristic_add_Stub(
		stub_sd_ble_gatts_characteristic_add_feature_char_error_no_mem);

	nrf_err = ble_bms_init(&ble_bms, &bms_cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_ble_bms_init_error_ctrlpt_add_no_mem(void)
{
	uint32_t nrf_err;
	struct ble_bms_config bms_cfg = {
		.evt_handler = bms_evt_handler,
		.feature_sec.lv = FEATURE_SEC_LV,
		.feature_sec.sm = FEATURE_SEC_SM,
		.ctrlpt_sec.lv = CTRLPT_SEC_LV,
		.ctrlpt_sec.sm = CTRLPT_SEC_SM,
		.feature = {
			.delete_all_auth = true,
			.delete_requesting  = true,
			.delete_all_but_requesting_auth = true,
		},
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);

	__cmock_sd_ble_gatts_characteristic_add_Stub(
		stub_sd_ble_gatts_characteristic_add_ctrlpt_char_error_no_mem);

	nrf_err = ble_bms_init(&ble_bms, &bms_cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_ble_bms_init_error_qwr_attr_register_no_mem(void)
{
	uint32_t nrf_err;
	struct ble_bms_config bms_cfg = {
		.evt_handler = bms_evt_handler,
		.feature_sec.lv = FEATURE_SEC_LV,
		.feature_sec.sm = FEATURE_SEC_SM,
		.ctrlpt_sec.lv = CTRLPT_SEC_LV,
		.ctrlpt_sec.sm = CTRLPT_SEC_SM,
		.feature = {
			.delete_all_auth = true,
			.delete_requesting  = true,
			.delete_all_but_requesting_auth = true,
		},
		.qwr = &ble_qwr,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);

	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add);

	__cmock_ble_qwr_attr_register_ExpectAndReturn(
		&ble_qwr, CTRLPT_VALUE_HANDLE, NRF_ERROR_NO_MEM);

	nrf_err = ble_bms_init(&ble_bms, &bms_cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);

	bms_cfg.qwr_count = 1;

	/* TODO reset stub */

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);

	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add);

	__cmock_ble_qwr_attr_register_ExpectAndReturn(
		&ble_qwr, CTRLPT_VALUE_HANDLE, NRF_ERROR_NO_MEM);

	nrf_err = ble_bms_init(&ble_bms, &bms_cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_ble_bms_init(void)
{
	uint32_t nrf_err;
	struct ble_bms_config bms_cfg = {
		.evt_handler = bms_evt_handler,
		.feature_sec.lv = FEATURE_SEC_LV,
		.feature_sec.sm = FEATURE_SEC_SM,
		.ctrlpt_sec.lv = CTRLPT_SEC_LV,
		.ctrlpt_sec.sm = CTRLPT_SEC_SM,
		.feature = {
			.delete_all_auth = true,
			.delete_requesting  = true,
			.delete_all_but_requesting_auth = true,
		},
		.qwr = &ble_qwr,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);

	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add);

	__cmock_ble_qwr_attr_register_ExpectAndReturn(
		&ble_qwr, CTRLPT_VALUE_HANDLE, NRF_SUCCESS);

	nrf_err = ble_bms_init(&ble_bms, &bms_cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_bms_init_support_none(void)
{
	uint32_t nrf_err;
	struct ble_bms_config bms_cfg = {
		.evt_handler = bms_evt_handler,
		.feature_sec.lv = FEATURE_SEC_LV,
		.feature_sec.sm = FEATURE_SEC_SM,
		.ctrlpt_sec.lv = CTRLPT_SEC_LV,
		.ctrlpt_sec.sm = CTRLPT_SEC_SM,
		.feature = {
		},
		.qwr = &ble_qwr,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);

	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add);

	__cmock_ble_qwr_attr_register_ExpectAndReturn(
		&ble_qwr, CTRLPT_VALUE_HANDLE, NRF_SUCCESS);

	nrf_err = ble_bms_init(&ble_bms, &bms_cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_bms_init_support_all(void)
{
	uint32_t nrf_err;
	struct ble_bms_config bms_cfg = {
		.evt_handler = bms_evt_handler,
		.feature_sec.lv = FEATURE_SEC_LV,
		.feature_sec.sm = FEATURE_SEC_SM,
		.ctrlpt_sec.lv = CTRLPT_SEC_LV,
		.ctrlpt_sec.sm = CTRLPT_SEC_SM,
		.feature = {
			.delete_all = true,
			.delete_all_auth = true,
			.delete_requesting  = true,
			.delete_requesting_auth = true,
			.delete_all_but_requesting = true,
			.delete_all_but_requesting_auth = true,
		},
		.qwr = &ble_qwr,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);

	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add);

	__cmock_ble_qwr_attr_register_ExpectAndReturn(
		&ble_qwr, CTRLPT_VALUE_HANDLE, NRF_SUCCESS);

	nrf_err = ble_bms_init(&ble_bms, &bms_cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_bms_auth_response_error_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_bms_auth_response(NULL, true);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_bms_auth_response_error_invalid_state(void)
{
	uint32_t nrf_err;

	nrf_err = ble_bms_auth_response(&ble_bms, true);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_bms_on_ble_evt_rw_authorize_req_uninitialized(void)
{
	ble_evt_t evt = {
		.header = {
			.evt_id = BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST,
		},
		.evt.gatts_evt = {
			.conn_handle = CONN_HANDLE,
			.params.authorize_request = {
				.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE,
				.request.write = {
					.op = BLE_GATTS_OP_WRITE_REQ,
					.handle = CTRLPT_VALUE_HANDLE,
					.len = 4,
				},
			},
		},
	};

	/* evt.evt.gatts_evt.params.authorize_request.request.write.data is set as an array of
	 * length 1, so the variable data cannot be set above.
	 */
	uint8_t data[] = { BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_LE_ONLY, 'a', 'b', 'c' };

	memcpy(evt.evt.gatts_evt.params.authorize_request.request.write.data,
	       data, sizeof(data));

	/* These should return immediately. */
	ble_bms_on_ble_evt(&evt, NULL);
	ble_bms_on_ble_evt(NULL, &ble_bms);

	/* Unhandled as ctrlpt handle is not registered. */
	ble_bms_on_ble_evt(&evt, &ble_bms);
}

void test_ble_bms_on_ble_evt_rw_authorize_req_error(void)
{
	ble_evt_t evt = {
		.header = {
			.evt_id = BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST,
		},
		.evt.gatts_evt = {
			.conn_handle = CONN_HANDLE,
			.params.authorize_request = {
				.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE,
				.request.write = {
					.op = BLE_GATTS_OP_WRITE_REQ,
					.handle = CTRLPT_VALUE_HANDLE,
				},
			},
		},
	};

	test_ble_bms_init();

	/* evt.evt.gatts_evt.params.authorize_request.request.write.data is set as an array of
	 * length 1, so the variable data cannot be set above.
	 */
	uint8_t data[] = { BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_LE_ONLY, 'a', 'b', 'c' };

	memcpy(evt.evt.gatts_evt.params.authorize_request.request.write.data,
	       data, sizeof(data));
	evt.evt.gatts_evt.params.authorize_request.request.write.len = sizeof(data);

	__cmock_sd_ble_gatts_rw_authorize_reply_Stub(stub_sd_ble_gatts_rw_authorize_reply_error);

	ble_bms_on_ble_evt(&evt, &ble_bms);

	TEST_ASSERT_EQUAL(BLE_BMS_EVT_ERROR, last_evt.evt_type);
}

void test_ble_bms_on_ble_evt_rw_authorize_req(void)
{
	ble_evt_t evt = {
		.header = {
			.evt_id = BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST,
		},
		.evt.gatts_evt = {
			.conn_handle = CONN_HANDLE,
			.params.authorize_request = {
				.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE,
				.request.write = {
					.op = BLE_GATTS_OP_WRITE_REQ,
					.handle = CTRLPT_VALUE_HANDLE,
				},
			},
		},
	};

	test_ble_bms_init();

	/* These should return immediately. */
	ble_bms_on_ble_evt(&evt, NULL);
	ble_bms_on_ble_evt(NULL, &ble_bms);

	/* Empty data is rejected */
	__cmock_sd_ble_gatts_rw_authorize_reply_Stub(stub_sd_ble_gatts_rw_authorize_reply_rejected);
	ble_bms_on_ble_evt(&evt, &ble_bms);

	/* evt.evt.gatts_evt.params.authorize_request.request.write.data is set as an array of
	 * length 1, so the variable data cannot be set above.
	 */
	uint8_t data_bad_op[] = { 0xba, 'a', 'b', 'c' };

	memcpy(evt.evt.gatts_evt.params.authorize_request.request.write.data,
	       data_bad_op, sizeof(data_bad_op));
	evt.evt.gatts_evt.params.authorize_request.request.write.len = sizeof(data_bad_op);

	__cmock_sd_ble_gatts_rw_authorize_reply_Stub(stub_sd_ble_gatts_rw_authorize_reply_rejected);

	ble_bms_on_ble_evt(&evt, &ble_bms);

	/* evt.evt.gatts_evt.params.authorize_request.request.write.data is set as an array of
	 * length 1, so the variable data cannot be set above.
	 */
	uint8_t data[] = { BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_LE_ONLY, 'a', 'b', 'c' };

	memcpy(evt.evt.gatts_evt.params.authorize_request.request.write.data,
	       data, sizeof(data));
	evt.evt.gatts_evt.params.authorize_request.request.write.len = sizeof(data);

	__cmock_sd_ble_gatts_rw_authorize_reply_Stub(stub_sd_ble_gatts_rw_authorize_reply_accepted);

	ble_bms_on_ble_evt(&evt, &ble_bms);

	__cmock_sd_ble_gatts_rw_authorize_reply_Stub(stub_sd_ble_gatts_rw_authorize_reply_accepted);

	ble_bms_on_ble_evt(&evt, &ble_bms);

	/* This is allowed without valid passkey in init config */
	uint8_t data_device_only[] = { BLE_BMS_OP_DEL_BOND_REQ_DEVICE_LE_ONLY };

	memcpy(evt.evt.gatts_evt.params.authorize_request.request.write.data,
	       data_device_only, sizeof(data_device_only));
	evt.evt.gatts_evt.params.authorize_request.request.write.len = sizeof(data);

	__cmock_sd_ble_gatts_rw_authorize_reply_Stub(stub_sd_ble_gatts_rw_authorize_reply_accepted);

	ble_bms_on_ble_evt(&evt, &ble_bms);

	uint8_t data_other_only[] = { BLE_BMS_OP_DEL_ALL_BUT_ACTIVE_BOND_LE_ONLY, 'a', 'b', 'c'};

	memcpy(evt.evt.gatts_evt.params.authorize_request.request.write.data,
	       data_other_only, sizeof(data_other_only));
	evt.evt.gatts_evt.params.authorize_request.request.write.len = sizeof(data);
	__cmock_sd_ble_gatts_rw_authorize_reply_Stub(stub_sd_ble_gatts_rw_authorize_reply_accepted);

	ble_bms_on_ble_evt(&evt, &ble_bms);

	uint8_t data_inval[] = { BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_LE_ONLY, 'b', 'a', 'd' };

	memcpy(evt.evt.gatts_evt.params.authorize_request.request.write.data,
	       data_inval, sizeof(data_inval));
	evt.evt.gatts_evt.params.authorize_request.request.write.len = sizeof(data);
	__cmock_sd_ble_gatts_rw_authorize_reply_Stub(stub_sd_ble_gatts_rw_authorize_reply_rejected);

	ble_bms_on_ble_evt(&evt, &ble_bms);
}

void test_ble_bms_on_qwr_evt_authorize_req_error_op_failed(void)
{
	uint32_t nrf_err;
	struct ble_qwr_evt evt = {
		.evt_type = BLE_QWR_EVT_AUTH_REQUEST,
		.auth_req = {
			.attr_handle = CTRLPT_VALUE_HANDLE,
		},
	};

	ble_qwr.conn_handle = CONN_HANDLE;

	test_ble_bms_init();

	__cmock_ble_qwr_value_get_Stub(stub_ble_qwr_value_get_error_no_mem);

	nrf_err = ble_bms_on_qwr_evt(&ble_bms, &ble_qwr, &evt);
	TEST_ASSERT_EQUAL(BLE_BMS_OPERATION_FAILED, nrf_err);
}

void test_ble_bms_on_qwr_evt_authorize_req_error(void)
{
	uint32_t nrf_err;
	struct ble_qwr_evt evt = {
		.evt_type = BLE_QWR_EVT_AUTH_REQUEST,
		.auth_req = {
			.attr_handle = CTRLPT_VALUE_HANDLE,
		},
	};

	ble_qwr.conn_handle = CONN_HANDLE;

	test_ble_bms_init();

	evt.evt_type = BLE_QWR_EVT_EXECUTE_WRITE;

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_error_not_found);

	nrf_err = ble_bms_on_qwr_evt(&ble_bms, &ble_qwr, &evt);
	TEST_ASSERT_EQUAL(BLE_BMS_OPERATION_FAILED, nrf_err);

	evt.evt_type = BLE_QWR_EVT_AUTH_REQUEST;

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_accepted);
	__cmock_ble_qwr_value_get_Stub(stub_ble_qwr_value_get_auth_req_no_data);

	nrf_err = ble_bms_on_qwr_evt(&ble_bms, &ble_qwr, &evt);
	TEST_ASSERT_EQUAL(BLE_BMS_OPERATION_FAILED, nrf_err);

	/* Successful request before execute write*/
	__cmock_ble_qwr_value_get_Stub(stub_ble_qwr_value_get_auth_req_accepted);

	nrf_err = ble_bms_on_qwr_evt(&ble_bms, &ble_qwr, &evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt.evt_type = BLE_QWR_EVT_EXECUTE_WRITE;

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_accepted);

	nrf_err = ble_bms_on_qwr_evt(&ble_bms, &ble_qwr, &evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_bms_on_qwr_evt_error_failed(void)
{
	uint32_t nrf_err;
	struct ble_qwr_evt evt = {
		.evt_type = BLE_QWR_EVT_AUTH_REQUEST,
		.auth_req = {
			.attr_handle = CTRLPT_VALUE_HANDLE,
		},
	};

	ble_qwr.conn_handle = CONN_HANDLE;

	test_ble_bms_init();

	__cmock_ble_qwr_value_get_Stub(stub_ble_qwr_value_get_auth_req_accepted);

	nrf_err = ble_bms_on_qwr_evt(&ble_bms, &ble_qwr, &evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt.evt_type = BLE_QWR_EVT_EXECUTE_WRITE;

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_error_not_found);

	nrf_err = ble_bms_on_qwr_evt(&ble_bms, &ble_qwr, &evt);
	TEST_ASSERT_EQUAL(BLE_BMS_OPERATION_FAILED, nrf_err);
}

void test_ble_bms_on_qwr_evt_authorize_req(void)
{
	uint32_t nrf_err;
	struct ble_qwr_evt evt = {
		.evt_type = BLE_QWR_EVT_AUTH_REQUEST,
		.auth_req = {
			.attr_handle = CTRLPT_VALUE_HANDLE,
		},
	};

	ble_qwr.conn_handle = CONN_HANDLE;

	test_ble_bms_init();

	/* These should return immediately. */
	nrf_err = ble_bms_on_qwr_evt(NULL, &ble_qwr, &evt);
	TEST_ASSERT_EQUAL(BLE_QWR_REJ_REQUEST_ERR_CODE, nrf_err);
	nrf_err = ble_bms_on_qwr_evt(&ble_bms, NULL, &evt);
	TEST_ASSERT_EQUAL(BLE_QWR_REJ_REQUEST_ERR_CODE, nrf_err);
	nrf_err = ble_bms_on_qwr_evt(&ble_bms, &ble_qwr, NULL);
	TEST_ASSERT_EQUAL(BLE_QWR_REJ_REQUEST_ERR_CODE, nrf_err);

	__cmock_ble_qwr_value_get_Stub(stub_ble_qwr_value_get_auth_req_accepted);

	nrf_err = ble_bms_on_qwr_evt(&ble_bms, &ble_qwr, &evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evt.evt_type = BLE_QWR_EVT_EXECUTE_WRITE;

	__cmock_sd_ble_gatts_value_get_Stub(stub_sd_ble_gatts_value_get_accepted);
	__cmock_sd_ble_gatts_rw_authorize_reply_Stub(stub_sd_ble_gatts_rw_authorize_reply_accepted);

	nrf_err = ble_bms_on_qwr_evt(&ble_bms, &ble_qwr, &evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void setUp(void)
{
	memset(&ble_bms, 0, sizeof(struct ble_bms));
	memset(&last_evt, 0, sizeof(struct ble_bms_evt));
	last_evt.evt_type = 100; /* 0 is a valid event type */
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
