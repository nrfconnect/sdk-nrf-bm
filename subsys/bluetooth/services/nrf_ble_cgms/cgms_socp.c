/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdint.h>
#include <string.h>
#include <zephyr/sys/byteorder.h>
#include <ble.h>
#include <bluetooth/services/uuid.h>
#include <ble_gq.h>
#include "cgms_sst.h"
#include "cgms_socp.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ble_cgms, CONFIG_BLE_CGMS_LOG_LEVEL);

#define NRF_BLE_CGMS_PLUS_INFINTE                     0x07FE
#define NRF_BLE_CGMS_MINUS_INFINTE                    0x0802

/** @brief Specific Operation Control Point opcodes. */
/** Specific Operation Control Point opcode - Reserved for future use. */
#define SOCP_OPCODE_RESERVED                          0x00
#define SOCP_WRITE_CGM_COMMUNICATION_INTERVAL         0x01
#define SOCP_READ_CGM_COMMUNICATION_INTERVAL          0x02
#define SOCP_READ_CGM_COMMUNICATION_INTERVAL_RESPONSE 0x03
#define SOCP_WRITE_GLUCOSE_CALIBRATION_VALUE          0x04
#define SOCP_READ_GLUCOSE_CALIBRATION_VALUE           0x05
#define SOCP_READ_GLUCOSE_CALIBRATION_VALUE_RESPONSE  0x06
#define SOCP_WRITE_PATIENT_HIGH_ALERT_LEVEL           0x07
#define SOCP_READ_PATIENT_HIGH_ALERT_LEVEL            0x08
#define SOCP_READ_PATIENT_HIGH_ALERT_LEVEL_RESPONSE   0x09
#define SOCP_WRITE_PATIENT_LOW_ALERT_LEVEL            0x0A
#define SOCP_READ_PATIENT_LOW_ALERT_LEVEL             0x0B
#define SOCP_READ_PATIENT_LOW_ALERT_LEVEL_RESPONSE    0x0C
/** Set Hypo Alert Level
 *  Hypo Alert Level value in mg/dL
 *  The response to this control point is Response Code.
 */
#define SOCP_SET_HYPO_ALERT_LEVEL                     0x0D
/** Get Hypo Alert Level
 *  N/A
 *  The normal response to this control point is Op Code 0x0F.
 *  For error conditions, the response is Response Code
 */
#define SOCP_GET_HYPO_ALERT_LEVEL                     0x0E
/** Hypo Alert Level Response
 *  Hypo Alert Level value in mg/dL
 *  This is the normal response to Op Code 0x0E
 */
#define SOCP_HYPO_ALERT_LEVEL_RESPONSE                0x0F
/** Set Hyper Alert Level
 *  Hyper Alert Level value in mg/dL
 *  The response to this control point is Response Code.
 */
#define SOCP_SET_HYPER_ALERT_LEVEL                    0x10
/** Get Hyper Alert Level
 *  N/A
 *  The normal response to this control point is Op Code 0x12.
 *  For error conditions, the response is Response Code
 */
#define SOCP_GET_HYPER_ALERT_LEVEL                    0x11
/** Hyper Alert Level Response
 *  Hyper Alert Level value in mg/dL
 *  This is the normal response to Op Code 0x11
 */
#define SOCP_HYPER_ALERT_LEVEL_RESPONSE               0x12
/** Set Rate of Decrease Alert Level
 *  Rate of Decrease Alert Level value in mg/dL/min
 *  The response to this control point is Response Code.
 */
#define SOCP_SET_RATE_OF_DECREASE_ALERT_LEVEL         0x13
/** Get Rate of Decrease Alert Level
 *  N/A
 *  The normal response to this control point is Op Code 0x15.
 *  For error conditions, the response is Response Code
 */
#define SOCP_GET_RATE_OF_DECREASE_ALERT_LEVEL         0x14
/** Rate of Decrease Alert Level Response
 *  Rate of Decrease Alert Level value in mg/dL/min
 *  This is the normal response to Op Code 0x14
 */
#define SOCP_RATE_OF_DECREASE_ALERT_LEVEL_RESPONSE    0x15
/** Set Rate of Increase Alert Level
 *  Rate of Increase Alert Level value in mg/dL/min
 *  The response to this control point is Response Code.
 */
#define SOCP_SET_RATE_OF_INCREASE_ALERT_LEVEL         0x16
/** Get Rate of Increase Alert Level
 *  N/A
 *  The normal response to this control point is Op Code 0x18.
 *  For error conditions, the response is Response Code
 */
#define SOCP_GET_RATE_OF_INCREASE_ALERT_LEVEL         0x17
/** Rate of Increase Alert Level Response
 *  Rate of Increase Alert Level value in mg/dL/min
 *  This is the normal response to Op Code 0x17
 */
#define SOCP_RATE_OF_INCREASE_ALERT_LEVEL_RESPONSE    0x18
/** Reset Device Specific Alert
 *  N/A
 *  The response to this control point is Response Code.
 */
#define SOCP_RESET_DEVICE_SPECIFIC_ALERT              0x19

#define SOCP_START_THE_SESSION                        0x1A
#define SOCP_STOP_THE_SESSION                         0x1B
#define SOCP_RESPONSE_CODE                            0x1C

#define SOCP_RSP_RESERVED_FOR_FUTURE_USE              0x00
#define SOCP_RSP_SUCCESS                              0x01
#define SOCP_RSP_OP_CODE_NOT_SUPPORTED                0x02
#define SOCP_RSP_INVALID_OPERAND                      0x03
#define SOCP_RSP_PROCEDURE_NOT_COMPLETED              0x04
#define SOCP_RSP_OUT_OF_RANGE                         0x05

static void ble_socp_decode(uint8_t data_len, uint8_t const *data,
			    struct ble_cgms_socp_value *socp_val)
{
	socp_val->opcode = 0xFF;
	socp_val->operand_len = 0;
	socp_val->operand = NULL;

	if (data_len > 0) {
		socp_val->opcode = data[0];
	}

	if (data_len > 1) {
		socp_val->operand_len = data_len - 1;
		socp_val->operand = (uint8_t *)&data[1];
	}
}


uint8_t ble_socp_encode(const struct ble_socp_rsp *socp_rsp, uint8_t *data)
{
	uint8_t len = 0;
	int i;


	if (data) {
		data[len++] = socp_rsp->opcode;

		if ((socp_rsp->opcode != SOCP_READ_CGM_COMMUNICATION_INTERVAL_RESPONSE) &&
		    (socp_rsp->opcode != SOCP_READ_PATIENT_HIGH_ALERT_LEVEL_RESPONSE) &&
		    (socp_rsp->opcode != SOCP_READ_PATIENT_LOW_ALERT_LEVEL_RESPONSE) &&
		    (socp_rsp->opcode != SOCP_HYPO_ALERT_LEVEL_RESPONSE) &&
		    (socp_rsp->opcode != SOCP_HYPER_ALERT_LEVEL_RESPONSE) &&
		    (socp_rsp->opcode != SOCP_RATE_OF_DECREASE_ALERT_LEVEL_RESPONSE) &&
		    (socp_rsp->opcode != SOCP_RATE_OF_INCREASE_ALERT_LEVEL_RESPONSE) &&
		    (socp_rsp->opcode != SOCP_READ_GLUCOSE_CALIBRATION_VALUE_RESPONSE)) {
			data[len++] = socp_rsp->req_opcode;
			data[len++] = socp_rsp->rsp_code;
		}

		for (i = 0; i < socp_rsp->size_val; i++) {
			data[len++] = socp_rsp->resp_val[i];
		}
	}

	return len;
}


/**
 * @brief Function for adding a characteristic for the Specific Operations Control Point.
 *
 * @param[in] cgms Service instance.
 *
 * @return NRF_SUCCESS if characteristic was successfully added, otherwise an error code.
 */
int cgms_socp_char_add(struct nrf_ble_cgms *cgms)
{
	int err;
	ble_uuid_t char_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_CGM_SPECIFIC_OPS_CTRLPT,
	};
	ble_gatts_attr_md_t cccd_md = {
		.vloc = BLE_GATTS_VLOC_STACK
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.indicate = true,
			.write = true,
		},
		.p_cccd_md = &cccd_md,
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.wr_auth = true,
		.vlen = true,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = 0,
		.init_len = 0,
		.max_len = BLE_GATT_ATT_MTU_DEFAULT,
	};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);

	err = sd_ble_gatts_characteristic_add(cgms->service_handle, &char_md, &attr_char_value,
					      &cgms->char_handles.socp);
	if (err) {
		LOG_ERR("Failed to add GATT CGMS CTRLPT characteristic, nrf_error %#x", err);
		return err;
	}

	return 0;
}

/**
 * @brief Function for sending a response from the Specific Operation Control Point.
 *
 * @param[in] cgms Service instance.
 */
static void socp_send(struct nrf_ble_cgms *cgms)
{
	uint32_t err;
	uint8_t encoded_resp[NRF_BLE_CGMS_SOCP_RESP_LEN + 3];
	uint16_t len;
	struct nrf_ble_cgms_evt cgms_evt = {
		.evt_type = NRF_BLE_CGMS_EVT_ERROR,
	};

	/* Send indication */
	len = ble_socp_encode(&(cgms->socp_response), encoded_resp);

	struct ble_gq_req cgms_req = {
		.type = BLE_GQ_REQ_GATTS_HVX,
		.error_handler.cb = cgms->gatt_err_handler,
		.error_handler.ctx = cgms,
		.params.gatts_hvx.type = BLE_GATT_HVX_INDICATION,
		.params.gatts_hvx.handle = cgms->char_handles.socp.value_handle,
		.params.gatts_hvx.offset = 0,
		.params.gatts_hvx.p_data = encoded_resp,
		.params.gatts_hvx.p_len = &len,
	};

	err = ble_gq_item_add(cgms->gatt_queue, &cgms_req, cgms->conn_handle);

	/* Report error to application */
	if ((cgms->evt_handler != NULL) &&
	    (err != NRF_SUCCESS) &&
	    (err != NRF_ERROR_INVALID_STATE)) {
		cgms_evt.error.reason = err;
		cgms->evt_handler(cgms, &cgms_evt);
	}
}

void encode_get_response(uint8_t rsp_code, struct ble_socp_rsp *rsp, uint16_t in_val)
{
	rsp->opcode    = rsp_code;
	rsp->rsp_code  = SOCP_RSP_SUCCESS;
	sys_put_le16(in_val, &(rsp->resp_val[rsp->size_val]));
	rsp->size_val += sizeof(uint16_t);
}

void decode_set_opcode(struct nrf_ble_cgms *cgms, struct ble_cgms_socp_value *rcv_val,
		       uint16_t min, uint16_t max, uint16_t *val)
{
	uint16_t rcvd_val = sys_get_le16(rcv_val->operand);

	if ((rcvd_val == NRF_BLE_CGMS_PLUS_INFINTE) ||
	    (rcvd_val == NRF_BLE_CGMS_MINUS_INFINTE) ||
	    (rcvd_val > max) ||
	    (rcvd_val < min)) {
		cgms->socp_response.rsp_code = SOCP_RSP_OUT_OF_RANGE;
	} else {
		cgms->socp_response.rsp_code = SOCP_RSP_SUCCESS;
		*val                         = rcvd_val;
	}
}

static bool is_feature_present(struct nrf_ble_cgms *cgms, uint32_t feature)
{
	return (cgms->feature.feature & feature);
}


/**
 * @brief Function for handling a write event to the Specific Operation Control Point.
 *
 * @param[in] cgms Service instance.
 * @param[in] evt_write WRITE event to be handled.
 */
static void on_socp_value_write(struct nrf_ble_cgms *cgms, ble_gatts_evt_write_t const *evt_write)
{
	int err;
	struct ble_cgms_socp_value socp_request;
	struct nrf_ble_cgms_evt evt;
	ble_gatts_rw_authorize_reply_params_t auth_reply = {
		.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE,
		.params = {
			.write = {
				.gatt_status = BLE_GATT_STATUS_SUCCESS,
				.update = 1,
			},
		},
	};
	struct ble_cgms_sst sst;

	err = sd_ble_gatts_rw_authorize_reply(cgms->conn_handle, &auth_reply);
	if (err != NRF_SUCCESS) {
		if (cgms->evt_handler != NULL) {
			evt.error.reason = err;
			evt.evt_type = NRF_BLE_CGMS_EVT_ERROR;
			cgms->evt_handler(cgms, &evt);
		}
		return;
	}

	/* Decode request */
	ble_socp_decode(evt_write->len, evt_write->data, &socp_request);

	cgms->socp_response.opcode = SOCP_RESPONSE_CODE;
	cgms->socp_response.req_opcode = socp_request.opcode;
	cgms->socp_response.rsp_code = SOCP_RSP_OP_CODE_NOT_SUPPORTED;
	cgms->socp_response.size_val = 0;

	switch (socp_request.opcode) {
	case SOCP_WRITE_CGM_COMMUNICATION_INTERVAL:
		cgms->socp_response.rsp_code = SOCP_RSP_SUCCESS;
		cgms->comm_interval = socp_request.operand[0];
		evt.evt_type = NRF_BLE_CGMS_EVT_WRITE_COMM_INTERVAL;
		cgms->evt_handler(cgms, &evt);
		break;
	case SOCP_READ_CGM_COMMUNICATION_INTERVAL:
		cgms->socp_response.opcode = SOCP_READ_CGM_COMMUNICATION_INTERVAL_RESPONSE;
		cgms->socp_response.resp_val[0] = cgms->comm_interval;
		cgms->socp_response.size_val++;
		break;
	case SOCP_START_THE_SESSION:
		if (cgms->is_session_started) {
			cgms->socp_response.rsp_code = SOCP_RSP_PROCEDURE_NOT_COMPLETED;
		} else if (cgms->nb_run_session != 0) {
			if (!is_feature_present(cgms,
						NRF_BLE_CGMS_FEAT_MULTIPLE_SESSIONS_SUPPORTED)) {
				cgms->socp_response.rsp_code = SOCP_RSP_PROCEDURE_NOT_COMPLETED;
			}
		} else {
			cgms->socp_response.rsp_code = SOCP_RSP_SUCCESS;
			cgms->is_session_started     = true;
			cgms->nb_run_session++;

			if (cgms->evt_handler != NULL) {
				evt.evt_type = NRF_BLE_CGMS_EVT_START_SESSION;
				cgms->evt_handler(cgms, &evt);
			}

			memset(&sst, 0, sizeof(struct ble_cgms_sst));

			err = cgms_sst_set(cgms, &sst);
			if (err != NRF_SUCCESS) {
				if (cgms->evt_handler != NULL) {
					evt.evt_type = NRF_BLE_CGMS_EVT_ERROR;
					evt.error.reason = err;
					cgms->evt_handler(cgms, &evt);
				}
			}
			cgms->sensor_status.time_offset    = 0;
			cgms->sensor_status.status.status &= (~NRF_BLE_CGMS_STATUS_SESSION_STOPPED);
			err = nrf_ble_cgms_update_status(cgms, &cgms->sensor_status);
			if (err != NRF_SUCCESS) {
				if (cgms->evt_handler != NULL) {
					evt.evt_type = NRF_BLE_CGMS_EVT_ERROR;
					evt.error.reason = err;
					cgms->evt_handler(cgms, &evt);
				}
			}
		}
		break;
	case SOCP_STOP_THE_SESSION:
	{
		struct nrf_ble_cgm_status status = {
			.time_offset = cgms->sensor_status.time_offset,
			.status.status = cgms->sensor_status.status.status |
					 NRF_BLE_CGMS_STATUS_SESSION_STOPPED,
		};

		cgms->evt_handler(cgms, &evt);
		cgms->socp_response.rsp_code = SOCP_RSP_SUCCESS;
		cgms->is_session_started = false;

		if (cgms->evt_handler != NULL) {
			evt.evt_type = NRF_BLE_CGMS_EVT_STOP_SESSION;
			cgms->evt_handler(cgms, &evt);
		}
		err = nrf_ble_cgms_update_status(cgms, &status);
		if (err != NRF_SUCCESS) {
			if (cgms->evt_handler != NULL) {
				evt.evt_type = NRF_BLE_CGMS_EVT_ERROR;
				evt.error.reason = err;
				cgms->evt_handler(cgms, &evt);
			}
		}
		break;
	}
	default:
		cgms->socp_response.rsp_code = SOCP_RSP_OP_CODE_NOT_SUPPORTED;
		break;
	}

	socp_send(cgms);
}


void cgms_socp_on_rw_auth_req(struct nrf_ble_cgms *cgms,
			      ble_gatts_evt_rw_authorize_request_t const *auth_req)
{
	if (auth_req->type == BLE_GATTS_AUTHORIZE_TYPE_WRITE) {
		if (auth_req->request.write.handle == cgms->char_handles.socp.value_handle) {
			on_socp_value_write(cgms, &auth_req->request.write);
		}
	}
}
