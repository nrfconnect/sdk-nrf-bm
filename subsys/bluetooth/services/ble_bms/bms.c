/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <bm/bluetooth/services/ble_bms.h>
#include <bm/bluetooth/services/common.h>
#include <bm/bluetooth/services/uuid.h>
#include <bm/bluetooth/ble_qwr.h>

#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

/** Length of the Feature Characteristic (in bytes). */
#define BLE_BMS_FEATURE_LEN 3
/** Minimum length of the Bond Management Control Point Characteristic (in bytes). */
#define BLE_BMS_CTRLPT_MIN_LEN 1

LOG_MODULE_REGISTER(ble_bms, CONFIG_BLE_BMS_LOG_LEVEL);

static uint32_t ctrlpt_char_add(struct ble_bms *bms, struct ble_bms_config const *bms_config)
{
	uint32_t nrf_err;
	ble_uuid_t char_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_BMS_CTRLPT,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.write = true,
		},
		.char_ext_props = {
			.reliable_wr = bms_config->qwr ? true : false,
		},
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.wr_auth = true,
		.vlen = true,
		.write_perm = bms_config->ctrlpt_sec,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = NULL,
		.init_len = 0,
		.max_len = BLE_BMS_CTRLPT_MAX_LEN,
	};

	nrf_err = sd_ble_gatts_characteristic_add(bms->service_handle, &char_md, &attr_char_value,
					      &bms->ctrlpt_handles);
	if (nrf_err) {
		LOG_ERR("Failed to add GATT CGMS SST characteristic, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static uint8_t feature_encode(struct ble_bms_features const *feature, uint8_t *encoded_feature)
{
	uint32_t data = 0;

	if (feature->delete_all_auth) {
		data |= BLE_BMS_ALL_BONDS_LE_AUTH_CODE;
	}

	if (feature->delete_all_but_requesting_auth) {
		data |= BLE_BMS_ALL_EXCEPT_REQUESTING_DEVICE_LE_AUTH_CODE;
	}

	if (feature->delete_all_but_requesting) {
		data |= BLE_BMS_ALL_EXCEPT_REQUESTING_DEVICE_LE;
	}

	if (feature->delete_all) {
		data |= BLE_BMS_ALL_BONDS_LE;
	}

	if (feature->delete_requesting_auth) {
		data |= BLE_BMS_REQUESTING_DEVICE_LE_AUTH_CODE;
	}

	if (feature->delete_requesting) {
		data |= BLE_BMS_REQUESTING_DEVICE_LE;
	}

	sys_put_le24(data, encoded_feature);
	return 3;
}

static uint32_t feature_char_add(struct ble_bms *bms, struct ble_bms_config const *bms_config)
{
	uint32_t nrf_err;
	uint8_t encoded_feature[BLE_BMS_FEATURE_LEN];
	uint8_t init_value_len;

	ble_uuid_t char_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_BMS_FEATURE,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
		},
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = encoded_feature,
	};

	__ASSERT_NO_MSG(bms != NULL);
	__ASSERT_NO_MSG(bms_config != NULL);

	init_value_len = feature_encode(&bms->feature, encoded_feature);
	attr_char_value.init_len = init_value_len;
	attr_char_value.max_len = init_value_len;
	attr_md.read_perm = bms_config->feature_sec;

	nrf_err = sd_ble_gatts_characteristic_add(bms->service_handle, &char_md, &attr_char_value,
						  &bms->feature_handles);
	if (nrf_err) {
		LOG_ERR("Failed to add GATT CGMS SST characteristic, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static void ctrlpt_auth(struct ble_bms *bms, struct ble_bms_ctrlpt *ctrlpt)
{
	__ASSERT_NO_MSG(bms != NULL);
	__ASSERT_NO_MSG(ctrlpt != NULL);

	bms->auth_status = BLE_BMS_AUTH_STATUS_ALLOWED;

	/* Check if the authorization feature is enabled for this op code. */
	if (((bms->feature.delete_requesting_auth) &&
	     (ctrlpt->op_code == BLE_BMS_OP_DEL_BOND_REQ_DEVICE_LE_ONLY)) ||
	    ((bms->feature.delete_all_auth) &&
	     (ctrlpt->op_code == BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_LE_ONLY)) ||
	    ((bms->feature.delete_all_but_requesting_auth) &&
	     (ctrlpt->op_code == BLE_BMS_OP_DEL_ALL_BUT_ACTIVE_BOND_LE_ONLY))) {
		if (bms->evt_handler != NULL) {
			struct ble_bms_evt bms_evt = {
				.evt_type = BLE_BMS_EVT_AUTH,
				.auth.auth_code.len = ctrlpt->auth_code.len,
			};

			memcpy(bms_evt.auth.auth_code.code,
			       ctrlpt->auth_code.code, ctrlpt->auth_code.len);

			bms->auth_status = BLE_BMS_AUTH_STATUS_PENDING;

			bms->evt_handler(bms, &bms_evt);
		} else {
			bms->auth_status = BLE_BMS_AUTH_STATUS_DENIED;
		}
	}
}

static uint32_t ctrlpt_decode(uint8_t const *rcvd_val, uint16_t len, struct ble_bms_ctrlpt *ctrlpt)
{
	uint16_t pos = 0;

	__ASSERT_NO_MSG(rcvd_val != NULL);
	__ASSERT_NO_MSG(ctrlpt != NULL);

	if (len < BLE_BMS_CTRLPT_MIN_LEN || len > BLE_BMS_CTRLPT_MAX_LEN) {
		return NRF_ERROR_INVALID_LENGTH;
	}

	ctrlpt->op_code = (enum ble_bms_op)rcvd_val[pos++];
	ctrlpt->auth_code.len = (len - pos);
	memcpy(ctrlpt->auth_code.code, &(rcvd_val[pos]), ctrlpt->auth_code.len);

	return NRF_SUCCESS;
}

static void ctrlpt_execute(struct ble_bms *bms, enum ble_bms_op op_code)
{
	__ASSERT_NO_MSG(bms != NULL);

	struct ble_bms_evt evt = {};

	switch (op_code) {
	case BLE_BMS_OP_DEL_BOND_REQ_DEVICE_LE_ONLY:
		/* Delete single bond */
		evt.evt_type = BLE_BMS_EVT_BOND_DELETE_REQUESTING;
		break;
	case BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_LE_ONLY:
		/* Delete all bonds */
		evt.evt_type = BLE_BMS_EVT_BOND_DELETE_ALL;
		break;
	case BLE_BMS_OP_DEL_ALL_BUT_ACTIVE_BOND_LE_ONLY:
		/* Delete all but current bond */
		evt.evt_type = BLE_BMS_EVT_BOND_DELETE_ALL_EXCEPT_REQUESTING;
		break;
	default:
		/* No event, return. */
		return;
	}

	bms->evt_handler(bms, &evt);
}

static bool ctrlpt_validate(struct ble_bms_ctrlpt *ctrlpt, struct ble_bms_features *feature)
{
	__ASSERT_NO_MSG(ctrlpt != NULL);
	__ASSERT_NO_MSG(feature != NULL);

	switch (ctrlpt->op_code) {
	case BLE_BMS_OP_DEL_BOND_REQ_DEVICE_LE_ONLY:
		if (feature->delete_requesting || feature->delete_requesting_auth) {
			return true;
		}
		break;
	case BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_LE_ONLY:
		if (feature->delete_all || feature->delete_all_auth) {
			return true;
		}
		break;
	case BLE_BMS_OP_DEL_ALL_BUT_ACTIVE_BOND_LE_ONLY:
		if (feature->delete_all_but_requesting || feature->delete_all_but_requesting_auth) {
			return true;
		}
		break;
	default:
		/* No implementation needed. */
		break;
	}

	return false;
}

static uint16_t ctrlpt_process(struct ble_bms *bms, uint8_t const *rcvd_val, uint16_t len,
			       struct ble_bms_ctrlpt *ctrlpt)
{
	uint32_t nrf_err;

	__ASSERT_NO_MSG(bms != NULL);
	__ASSERT_NO_MSG(rcvd_val != NULL);
	__ASSERT_NO_MSG(ctrlpt != NULL);

	/* Decode operation */
	nrf_err = ctrlpt_decode(rcvd_val, len, ctrlpt);
	if (nrf_err) {
		LOG_ERR("Control point write: Operation failed.");
		return BLE_BMS_OPERATION_FAILED;
	}

	/* Verify that the operation is allowed. */
	if (!ctrlpt_validate(ctrlpt, &bms->feature)) {
		LOG_ERR("Control point write: Invalid op code.");
		return BLE_BMS_OPCODE_NOT_SUPPORTED;
	}

	/* Request authorization */
	ctrlpt_auth(bms, ctrlpt);
	if (bms->auth_status != BLE_BMS_AUTH_STATUS_ALLOWED) {
		LOG_ERR("Control point long write: Invalid auth.");
		return BLE_GATT_STATUS_ATTERR_INSUF_AUTHORIZATION;
	}

	return BLE_GATT_STATUS_SUCCESS;
}

static void on_ctrlpt_write(struct ble_bms *bms, ble_gatts_evt_write_t const *evt_write,
			    ble_gatts_authorize_params_t *auth_params)
{
	uint32_t nrf_err;
	struct ble_bms_ctrlpt ctrlpt;

	__ASSERT_NO_MSG(bms != NULL);
	__ASSERT_NO_MSG(evt_write != NULL);
	__ASSERT_NO_MSG(auth_params != NULL);

	nrf_err = ctrlpt_process(bms, evt_write->data, evt_write->len, &ctrlpt);
	if (nrf_err) {
		auth_params->gatt_status = nrf_err;
		auth_params->update = 0;

		return;
	}

	auth_params->gatt_status = BLE_GATT_STATUS_SUCCESS;
	auth_params->update = 1;

	LOG_INF("Control point write: Success");

	/* Execute the requested operation. */
	ctrlpt_execute(bms, ctrlpt.op_code);
}

static void on_rw_auth_req(struct ble_bms *bms, ble_gatts_evt_t const *gatts_evt)
{
	uint32_t nrf_err;

	__ASSERT_NO_MSG(bms != NULL);
	__ASSERT_NO_MSG(gatts_evt != NULL);

	ble_gatts_rw_authorize_reply_params_t auth_reply = {
		.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE,
	};

	ble_gatts_evt_rw_authorize_request_t const *auth_req =
		&gatts_evt->params.authorize_request;

	if ((auth_req->type == BLE_GATTS_AUTHORIZE_TYPE_WRITE) &&
		(auth_req->request.write.op == BLE_GATTS_OP_WRITE_REQ) &&
		(auth_req->request.write.handle == bms->ctrlpt_handles.value_handle)) {
		on_ctrlpt_write(bms, &auth_req->request.write, &auth_reply.params.write);

		/* Send authorization reply */
		nrf_err = sd_ble_gatts_rw_authorize_reply(bms->conn_handle, &auth_reply);
		if (nrf_err) {
			struct ble_bms_evt bms_evt = {
				.evt_type = BLE_BMS_EVT_ERROR,
				.error.reason = nrf_err,
			};

			bms->evt_handler(bms, &bms_evt);
		}
	}
}

static uint16_t on_qwr_auth_req(struct ble_bms *bms, struct ble_qwr *qwr,
			 const struct ble_qwr_evt *evt)
{
	uint32_t nrf_err;
	uint16_t len = BLE_BMS_CTRLPT_MAX_LEN;
	uint8_t mem_buffer[BLE_BMS_CTRLPT_MAX_LEN];
	struct ble_bms_ctrlpt ctrlpt;

	__ASSERT_NO_MSG(bms != NULL);
	__ASSERT_NO_MSG(qwr != NULL);
	__ASSERT_NO_MSG(evt != NULL);

	nrf_err = ble_qwr_value_get(qwr, evt->auth_req.attr_handle, mem_buffer, &len);
	if (nrf_err) {
		LOG_ERR("Control point write: Operation failed.");
		return BLE_BMS_OPERATION_FAILED;
	}

	return ctrlpt_process(bms, mem_buffer, len, &ctrlpt);
}

static uint16_t on_qwr_exec_write(struct ble_bms *bms, struct ble_qwr *qwr,
			   const struct ble_qwr_evt *evt)
{
	uint32_t nrf_err;
	uint16_t len = BLE_BMS_CTRLPT_MAX_LEN;
	uint8_t mem_buffer[BLE_BMS_CTRLPT_MAX_LEN];
	struct ble_bms_ctrlpt ctrlpt;
	ble_gatts_value_t ctrlpt_value = {
		.len = BLE_BMS_CTRLPT_MAX_LEN,
		.offset = 0,
		.p_value = mem_buffer,
	};

	__ASSERT_NO_MSG(bms != NULL);
	__ASSERT_NO_MSG(qwr != NULL);
	__ASSERT_NO_MSG(evt != NULL);

	const uint16_t ctrlpt_handle = bms->ctrlpt_handles.value_handle;

	nrf_err = sd_ble_gatts_value_get(bms->conn_handle, ctrlpt_handle, &ctrlpt_value);
	if (nrf_err) {
		LOG_ERR("Control point write: Operation failed.");
		return BLE_BMS_OPERATION_FAILED;
	}

	/* Decode operation */
	nrf_err = ctrlpt_decode(ctrlpt_value.p_value, len, &ctrlpt);
	if (nrf_err) {
		LOG_ERR("Control point write: Operation failed.");
		return BLE_BMS_OPERATION_FAILED;
	}

	/* Execute the requested operation. */
	ctrlpt_execute(bms, ctrlpt.op_code);

	/* Reset authorization status */
	bms->auth_status = BLE_BMS_AUTH_STATUS_DENIED;

	return BLE_GATT_STATUS_SUCCESS;
}

uint16_t ble_bms_on_qwr_evt(struct ble_bms *bms, struct ble_qwr *qwr,
			    const struct ble_qwr_evt *evt)
{
	if (!bms || !qwr || !evt) {
		return BLE_QWR_REJ_REQUEST_ERR_CODE;
	}

	if (evt->auth_req.attr_handle != bms->ctrlpt_handles.value_handle) {
		return BLE_QWR_REJ_REQUEST_ERR_CODE;
	}

	bms->conn_handle = qwr->conn_handle;

	if (evt->evt_type == BLE_QWR_EVT_AUTH_REQUEST) {
		return on_qwr_auth_req(bms, qwr, evt);
	} else if ((evt->evt_type == BLE_QWR_EVT_EXECUTE_WRITE) &&
			 (bms->auth_status == BLE_BMS_AUTH_STATUS_ALLOWED)) {
		return on_qwr_exec_write(bms, qwr, evt);
	}

	return BLE_GATT_STATUS_SUCCESS;
}

void ble_bms_on_ble_evt(ble_evt_t const *ble_evt, void *context)
{
	struct ble_bms *bms;

	if (!context || !ble_evt) {
		return;
	}

	bms = (struct ble_bms *)context;

	switch (ble_evt->header.evt_id) {
	case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
		bms->conn_handle = ble_evt->evt.gatts_evt.conn_handle;
		on_rw_auth_req(bms, &ble_evt->evt.gatts_evt);
		break;
	default:
		break;
	}
}

uint32_t ble_bms_init(struct ble_bms *bms, struct ble_bms_config *bms_config)
{
	uint32_t nrf_err;
	ble_uuid_t ble_uuid;

	if (!bms || !bms_config) {
		return NRF_ERROR_NULL;
	}

	if (!bms_config->evt_handler) {
		return NRF_ERROR_INVALID_ADDR;
	}

	/* Add service */
	BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_BMS_SERVICE);

	bms->evt_handler = bms_config->evt_handler;
	bms->feature = bms_config->feature;
	bms->conn_handle = BLE_CONN_HANDLE_INVALID;

	nrf_err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
				       &ble_uuid, &bms->service_handle);
	if (nrf_err) {
		return nrf_err;
	}

	nrf_err = feature_char_add(bms, bms_config);
	if (nrf_err) {
		return nrf_err;
	}

	nrf_err = ctrlpt_char_add(bms, bms_config);
	if (nrf_err) {
		return nrf_err;
	}

	/* Allow this for backward compatibility */
	if (bms_config->qwr && (bms_config->qwr_count == 0)) {
		nrf_err = ble_qwr_attr_register(bms_config->qwr, bms->ctrlpt_handles.value_handle);
		if (nrf_err) {
			return nrf_err;
		}
	} else if (bms_config->qwr && (bms_config->qwr_count > 0)) {
		for (uint32_t i = 0; i < bms_config->qwr_count; i++) {
			nrf_err = ble_qwr_attr_register(&bms_config->qwr[i],
							bms->ctrlpt_handles.value_handle);
			if (nrf_err) {
				return nrf_err;
			}
		}
	} else {
		/* Do nothing */
	}

	return NRF_SUCCESS;
}

uint32_t ble_bms_auth_response(struct ble_bms *bms, bool authorize)
{
	if (!bms) {
		return NRF_ERROR_NULL;
	}

	if (bms->auth_status != BLE_BMS_AUTH_STATUS_PENDING) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (authorize) {
		bms->auth_status = BLE_BMS_AUTH_STATUS_ALLOWED;
	} else {
		bms->auth_status = BLE_BMS_AUTH_STATUS_DENIED;
	}

	return NRF_SUCCESS;
}
