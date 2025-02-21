/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */


#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <bluetooth/services/ble_bms.h>
#include <bluetooth/services/common.h>
#include <bluetooth/services/uuid.h>

#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_bms, CONFIG_BLE_BMS_LOG_LEVEL);

/**
 * @brief Function for adding the Bond Management Control Point characteristic.
 *
 * @param[in] bms Bond Management Service structure.
 * @param[in] bms_config Information needed to initialize the service.
 *
 * @return 0 on success, otherwise an error code returned by @ref characteristic_add.
 */
static int ctrlpt_char_add(struct nrf_ble_bms *bms, struct nrf_ble_bms_config const *bms_config)
{
	int err;
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
		.write_perm = bms_config->bms_ctrlpt_sec,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = 0,
		.init_len = 0,
		.max_len = NRF_BLE_BMS_CTRLPT_MAX_LEN,
	};

	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.read_perm);

	err = sd_ble_gatts_characteristic_add(bms->service_handle, &char_md, &attr_char_value,
					      &bms->ctrlpt_handles);
	if (err) {
		LOG_ERR("Failed to add GATT CGMS SST characteristic, nrf_error %#x", err);
		return err;
	}

	return 0;
}

/**
 * @brief Forward an authorization request to the application, if necessary.
 *
 * @param[in] bms Bond Management Service structure.
 * @param[in] ctrlpt Pointer to the received Control Point value.
 */
static void ctrlpt_auth(struct nrf_ble_bms *bms, struct nrf_ble_bms_ctrlpt *ctrlpt)
{
	struct nrf_ble_bms_features *feature = &bms->feature;

	bms->auth_status = NRF_BLE_BMS_AUTH_STATUS_ALLOWED;

	/* Check if the authorization feature is enabled for this op code. */
	if (((feature->delete_requesting_auth) &&
	     (ctrlpt->op_code == NRF_BLE_BMS_OP_DEL_BOND_REQ_DEVICE_LE_ONLY)) ||
	    ((feature->delete_all_auth) &&
	     (ctrlpt->op_code == NRF_BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_LE_ONLY)) ||
	    ((feature->delete_all_but_requesting_auth) &&
	     (ctrlpt->op_code == NRF_BLE_BMS_OP_DEL_ALL_BUT_ACTIVE_BOND_LE_ONLY))) {
		if (bms->evt_handler != NULL) {
			struct nrf_ble_bms_evt bms_evt = {
				.evt_type = NRF_BLE_BMS_EVT_AUTH,
				.auth_code.len = ctrlpt->auth_code.len,
			};

			memcpy(bms_evt.auth_code.code,
			       ctrlpt->auth_code.code, ctrlpt->auth_code.len);

			bms->auth_status = NRF_BLE_BMS_AUTH_STATUS_PENDING;

			bms->evt_handler(bms, &bms_evt);
		} else {
			bms->auth_status = NRF_BLE_BMS_AUTH_STATUS_DENIED;
		}
	}
}

/**
 * @brief Decode an incoming Control Point write.
 *
 * @param[in] rcvd_val Received write value.
 * @param[in] len Value length.
 * @param[out] ctrlpt Decoded control point structure.
 *
 * @retval NRF_ERROR_INVALID_LENGTH The supplied value length is invalid.
 * @retval NRF_ERROR_NOT_SUPPORTED The supplied op code is not supported.
 * @retval 0 Operation successful.
 */
static int ctrlpt_decode(uint8_t const *rcvd_val, uint16_t len,	struct nrf_ble_bms_ctrlpt *ctrlpt)
{
	uint16_t pos = 0;

	if (len < NRF_BLE_BMS_CTRLPT_MIN_LEN || len > NRF_BLE_BMS_CTRLPT_MAX_LEN) {
		return NRF_ERROR_INVALID_LENGTH;
	}

	ctrlpt->op_code = (enum nrf_ble_bms_op)rcvd_val[pos++];
	ctrlpt->auth_code.len = (len - pos);
	memcpy(ctrlpt->auth_code.code, &(rcvd_val[pos]), ctrlpt->auth_code.len);

	return 0;
}

/**
 * @brief Function for performing an operation requested through the Control Point.
 *
 * @param[in] bms Bond Management Service structure.
 * @param[in] op_code Op code to execute.
 */
static void ctrlpt_execute(struct nrf_ble_bms *bms, enum nrf_ble_bms_op op_code)
{
	switch (op_code) {
	case NRF_BLE_BMS_OP_DEL_BOND_REQ_DEVICE_LE_ONLY:
		/* Delete single bond */
		bms->bond_callbacks.delete_requesting(bms);
		break;
	case NRF_BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_LE_ONLY:
		/* Delete all bonds */
		bms->bond_callbacks.delete_all(bms);
		break;
	case NRF_BLE_BMS_OP_DEL_ALL_BUT_ACTIVE_BOND_LE_ONLY:
		/* Delete all but current bond */
		bms->bond_callbacks.delete_all_except_requesting(bms);
		break;
	default:
		/* No implemementation needed. */
		break;
	}
}

/**
 * @brief Validate an incoming Control Point write.
 *
 * @param[in] op_code Received op code.
 * @param[in] feature Supported features.
 *
 * @returns True if the op code is supported, or false.
 */
static bool ctrlpt_validate(struct nrf_ble_bms_ctrlpt *ctrlpt, struct nrf_ble_bms_features *feature)
{
	switch (ctrlpt->op_code) {
	case NRF_BLE_BMS_OP_DEL_BOND_REQ_DEVICE_LE_ONLY:
		if (feature->delete_requesting || feature->delete_requesting_auth) {
			return true;
		}
		break;
	case NRF_BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_LE_ONLY:
		if (feature->delete_all || feature->delete_all_auth) {
			return true;
		}
		break;
	case NRF_BLE_BMS_OP_DEL_ALL_BUT_ACTIVE_BOND_LE_ONLY:
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

/**
 * @brief Function for processing a write to the Control Point.
 *
 * @param[in] bms Bond Management Service structure.
 * @param[in] rcvd_val Received write value.
 * @param[in] len Value length.
 * @param[out] ctrlpt Decoded control point structure.
 */
static uint16_t ctrlpt_process(struct nrf_ble_bms *bms, uint8_t const *rcvd_val, uint16_t len,
			       struct nrf_ble_bms_ctrlpt *ctrlpt)
{
	int err;

	/* Decode operation */
	err = ctrlpt_decode(rcvd_val, len, ctrlpt);
	if (err) {
		LOG_ERR("Control point write: Operation failed.");
		return NRF_BLE_BMS_OPERATION_FAILED;
	}

	/* Verify that the operation is allowed. */
	if (!ctrlpt_validate(ctrlpt, &bms->feature)) {
		LOG_ERR("Control point write: Invalid op code.");
		return NRF_BLE_BMS_OPCODE_NOT_SUPPORTED;
	}

	/* Request authorization */
	ctrlpt_auth(bms, ctrlpt);
	if (bms->auth_status != NRF_BLE_BMS_AUTH_STATUS_ALLOWED) {
		LOG_ERR("Control point long write: Invalid auth.");
		return BLE_GATT_STATUS_ATTERR_INSUF_AUTHORIZATION;
	}

	return BLE_GATT_STATUS_SUCCESS;
}

/**
 * @brief Function for encoding the Bond Management Feature characteristic.
 *
 * @param[in] bms Bond Management Service structure.
 * @param[out] encoded_feature Encoded features.
 *
 * @return Size of the encoded feature.
 */
static uint8_t feature_encode(struct nrf_ble_bms_features const *feature, uint8_t *encoded_feature)
{
	uint32_t data = 0;

	if (feature->delete_all_auth) {
		data |= NRF_BLE_BMS_ALL_BONDS_LE_AUTH_CODE;
	}

	if (feature->delete_all_but_requesting_auth) {
		data |= NRF_BLE_BMS_ALL_EXCEPT_REQUESTING_DEVICE_LE_AUTH_CODE;
	}

	if (feature->delete_all_but_requesting) {
		data |= NRF_BLE_BMS_ALL_EXCEPT_REQUESTING_DEVICE_LE;
	}

	if (feature->delete_all) {
		data |= NRF_BLE_BMS_ALL_BONDS_LE;
	}

	if (feature->delete_requesting_auth) {
		data |= NRF_BLE_BMS_REQUESTING_DEVICE_LE_AUTH_CODE;
	}

	if (feature->delete_requesting) {
		data |= NRF_BLE_BMS_REQUESTING_DEVICE_LE;
	}

	sys_put_le24(data, encoded_feature);
	return 3;
}

/**
 * @brief Function for adding the Bond Management Feature characteristic.
 *
 * @param[in] bms Bond Management Service structure.
 * @param[in] bms_config Information needed to initialize the service.
 *
 * @return 0 on success, otherwise an error code returned by @ref characteristic_add.
 */
static int feature_char_add(struct nrf_ble_bms *bms, struct nrf_ble_bms_config const *bms_config)
{
	uint8_t encoded_feature[NRF_BLE_BMS_FEATURE_LEN];
	uint8_t init_value_len;
	struct nrf_ble_bms_features *feature = &bms->feature;

	if ((feature->delete_all_auth) ||
	    (feature->delete_all_but_requesting_auth) ||
	    (feature->delete_requesting_auth)) {
		if (!bms_config->evt_handler) {
			return NRF_ERROR_NULL;
		}
	}

	if ((feature->delete_requesting_auth) || (feature->delete_requesting)) {
		if (!bms_config->bond_callbacks.delete_requesting) {
			return NRF_ERROR_NULL;
		}
	}

	if ((feature->delete_all) || (feature->delete_all_auth)) {
		if (!bms_config->bond_callbacks.delete_all) {
			return NRF_ERROR_NULL;
		}
	}

	if ((feature->delete_all_but_requesting) || (feature->delete_all_but_requesting_auth)) {
		if (!bms_config->bond_callbacks.delete_all_except_requesting) {
			return NRF_ERROR_NULL;
		}
	}

	init_value_len = feature_encode(&bms->feature, encoded_feature);

	int err;
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
		.read_perm = bms_config->bms_feature_sec,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = encoded_feature,
		.init_len = init_value_len,
		.max_len = init_value_len,
	};

	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

	err = sd_ble_gatts_characteristic_add(bms->service_handle, &char_md, &attr_char_value,
					      &bms->feature_handles);
	if (err) {
		LOG_ERR("Failed to add GATT CGMS SST characteristic, nrf_error %#x", err);
		return err;
	}

	return 0;
}

/**
 * @brief Handle a write event to the Bond Management Service Control Point.
 *
 * @param[in] bms Bond Management Service structure.
 * @param[in] evt_write WRITE event to be handled.
 */
static void on_ctrlpt_write(struct nrf_ble_bms *bms, ble_gatts_evt_write_t const *evt_write,
			    ble_gatts_authorize_params_t *auth_params)
{
	int err;
	struct nrf_ble_bms_ctrlpt ctrlpt;

	err = ctrlpt_process(bms, evt_write->data, evt_write->len, &ctrlpt);
	if (err) {
		auth_params->gatt_status = err;
		auth_params->update = 0;

		return;
	}

	auth_params->gatt_status = BLE_GATT_STATUS_SUCCESS;
	auth_params->update = 1;

	LOG_INF("Control point write: Success");

	/* Execute the requested operation. */
	ctrlpt_execute(bms, ctrlpt.op_code);
}

/**
 * @brief Authorize WRITE request event handler.
 *
 * @details Handles WRITE events from the BLE stack.
 *
 * @param[in] bms Bond Management Service structure.
 * @param[in] gatts_evt GATTS Event received from the BLE stack.
 *
 */
static void on_rw_auth_req(struct nrf_ble_bms *bms, ble_gatts_evt_t const *gatts_evt)
{
	int err;
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
		err = sd_ble_gatts_rw_authorize_reply(bms->conn_handle, &auth_reply);
		if (err) {
			bms->error_handler(err);
		}
	}
}

/**
 * @brief Handle authorization request events from the Queued Write module.
 *
 * @param[in] bms Bond Management Service structure.
 * @param[in] qwr Queued Write Structure.
 * @param[in] evt Event received from the Queued Writes module.
 *
 * @retval BLE_GATT_STATUS_SUCCESS If the received event is accepted.
 * @retval BLE_BMS_OPCODE_OPERATION_FAILED If the received event is not relevant for any of this
 *                                         module's attributes.
 * @retval BLE_BMS_OPCODE_NOT_SUPPORTED If the received opcode is not supported.
 * @retval BLE_GATT_STATUS_ATTERR_INSUF_AUTHORIZATION If the application handler returns that the
 *                                                    authorization code is not valid.
 */
uint16_t on_qwr_auth_req(struct nrf_ble_bms *bms, struct nrf_ble_qwr *qwr,
			 struct nrf_ble_qwr_evt *evt)
{
	int err;
	uint16_t len = NRF_BLE_BMS_CTRLPT_MAX_LEN;
	uint8_t mem_buffer[NRF_BLE_BMS_CTRLPT_MAX_LEN];
	struct nrf_ble_bms_ctrlpt ctrlpt;

	err = nrf_ble_qwr_value_get(qwr, evt->attr_handle, mem_buffer, &len);
	if (err) {
		LOG_ERR("Control point write: Operation failed.");
		return NRF_BLE_BMS_OPERATION_FAILED;
	}

	return ctrlpt_process(bms, mem_buffer, len, &ctrlpt);
}

/**
 * @brief Handle execute write events to from the Queued Write module.
 *
 * @param[in] bms Bond Management Service structure.
 * @param[in] qwr Queued Write Structure.
 * @param[in] evt Event received from the Queued Writes module.
 *
 * @retval BLE_GATT_STATUS_SUCCESS If the received event is accepted.
 * @retval BLE_BMS_OPCODE_OPERATION_FAILED If the received event is not relevant for any of this
 *                                         module's attributes.
 * @retval BLE_BMS_OPCODE_NOT_SUPPORTED If the received opcode is not supported.
 */
uint16_t on_qwr_exec_write(struct nrf_ble_bms *bms, struct nrf_ble_qwr *qwr,
			   struct nrf_ble_qwr_evt *evt)
{
	int err;
	uint16_t len = NRF_BLE_BMS_CTRLPT_MAX_LEN;
	uint8_t mem_buffer[NRF_BLE_BMS_CTRLPT_MAX_LEN];
	struct nrf_ble_bms_ctrlpt ctrlpt;
	const uint16_t ctrlpt_handle = bms->ctrlpt_handles.value_handle;
	ble_gatts_value_t ctrlpt_value = {
		.len = NRF_BLE_BMS_CTRLPT_MAX_LEN,
		.offset = 0,
		.p_value = mem_buffer,
	};

	err = sd_ble_gatts_value_get(bms->conn_handle, ctrlpt_handle, &ctrlpt_value);
	if (err) {
		LOG_ERR("Control point write: Operation failed.");
		return NRF_BLE_BMS_OPERATION_FAILED;
	}

	/* Decode operation */
	err = ctrlpt_decode(ctrlpt_value.p_value, len, &ctrlpt);
	if (err) {
		LOG_ERR("Control point write: Operation failed.");
		return NRF_BLE_BMS_OPERATION_FAILED;
	}

	/* Execute the requested operation. */
	ctrlpt_execute(bms, ctrlpt.op_code);

	/* Reset authorization status */
	bms->auth_status = NRF_BLE_BMS_AUTH_STATUS_DENIED;

	return BLE_GATT_STATUS_SUCCESS;
}

uint16_t nrf_ble_bms_on_qwr_evt(struct nrf_ble_bms *bms, struct nrf_ble_qwr *qwr,
				struct nrf_ble_qwr_evt *evt)
{
	if (!bms || !qwr || !evt) {
		return NRF_BLE_QWR_REJ_REQUEST_ERR_CODE;
	}

	if (evt->attr_handle != bms->ctrlpt_handles.value_handle) {
		return NRF_BLE_QWR_REJ_REQUEST_ERR_CODE;
	}

	bms->conn_handle = qwr->conn_handle;

	if (evt->evt_type == NRF_BLE_QWR_EVT_AUTH_REQUEST) {
		return on_qwr_auth_req(bms, qwr, evt);
	} else if ((evt->evt_type == NRF_BLE_QWR_EVT_EXECUTE_WRITE) &&
			 (bms->auth_status == NRF_BLE_BMS_AUTH_STATUS_ALLOWED)) {
		return on_qwr_exec_write(bms, qwr, evt);
	}

	return BLE_GATT_STATUS_SUCCESS;
}

void nrf_ble_bms_on_ble_evt(ble_evt_t const *ble_evt, void *context)
{
	struct nrf_ble_bms *bms;

	if (!context || !ble_evt) {
		return;
	}

	bms = (struct nrf_ble_bms *)context;

	switch (ble_evt->header.evt_id) {
	case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
		bms->conn_handle = ble_evt->evt.gatts_evt.conn_handle;
		on_rw_auth_req(bms, &ble_evt->evt.gatts_evt);
		break;
	default:
		break;
	}
}

int nrf_ble_bms_set_conn_handle(struct nrf_ble_bms *bms, uint16_t conn_handle)
{
	if (!bms) {
		return NRF_ERROR_NULL;
	}

	bms->conn_handle = conn_handle;
	return 0;
}

int nrf_ble_bms_init(struct nrf_ble_bms *bms, struct nrf_ble_bms_config *bms_config)
{
	int err;
	ble_uuid_t ble_uuid;

	if (!bms || !bms_config) {
		return NRF_ERROR_NULL;
	}

	/* Add service */
	BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_BMS_SERVICE);

	bms->evt_handler = bms_config->evt_handler;
	bms->error_handler = bms_config->error_handler;
	bms->feature = bms_config->feature;
	bms->bond_callbacks = bms_config->bond_callbacks;
	bms->conn_handle = BLE_CONN_HANDLE_INVALID;

	err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
				       &ble_uuid, &bms->service_handle);
	if (err) {
		return err;
	}

	err = feature_char_add(bms, bms_config);
	if (err) {
		return err;
	}

	err = ctrlpt_char_add(bms, bms_config);
	if (err) {
		return err;
	}

	/* Allow this for backward compatibility */
	if ((bms_config->qwr != NULL) && (bms_config->qwr_count == 0)) {
		err = nrf_ble_qwr_attr_register(bms_config->qwr, bms->ctrlpt_handles.value_handle);
		if (err) {
			return err;
		}
	} else if (bms_config->qwr && (bms_config->qwr_count > 0)) {
		for (uint32_t i = 0; i < bms_config->qwr_count; i++) {
			err = nrf_ble_qwr_attr_register(&bms_config->qwr[i],
							bms->ctrlpt_handles.value_handle);
			if (err) {
				return err;
			}
		}
	} else {
		/* Do nothing */
	}

	return 0;
}

int nrf_ble_bms_auth_response(struct nrf_ble_bms *bms, bool authorize)
{
	if (!bms) {
		return NRF_ERROR_NULL;
	}

	if (bms->auth_status != NRF_BLE_BMS_AUTH_STATUS_PENDING) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (authorize) {
		bms->auth_status = NRF_BLE_BMS_AUTH_STATUS_ALLOWED;
	} else {
		bms->auth_status = NRF_BLE_BMS_AUTH_STATUS_DENIED;
	}

	return 0;
}
