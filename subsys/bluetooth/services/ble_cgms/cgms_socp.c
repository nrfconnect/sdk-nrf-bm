/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdint.h>
#include <string.h>
#include <zephyr/sys/byteorder.h>
#include <ble.h>
#include <bluetooth/services/common.h>
#include <bluetooth/services/uuid.h>
#include <ble_gq.h>
#include "cgms_db.h"
#include "cgms_sst.h"
#include "cgms_socp.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ble_cgms, CONFIG_BLE_CGMS_LOG_LEVEL);

/* Medfloat16 value. Represent a positive infinite value. */
#define BLE_CGMS_PLUS_INFINITE 0x07FE
/* Medfloat16 value. Represent a negative infinite value. */
#define BLE_CGMS_MINUS_INFINITE 0x0802

/* Specific Ops Control Point opcodes. */

/* Reserved for future use. */
#define SOCP_OPCODE_RESERVED 0x00

/* Set CGM communication interval.
 *
 * Operand: Communication interval in minutes.
 *
 * The response to this control point is SOCP_RESPONSE_CODE.
 */
#define SOCP_SET_CGM_COMMUNICATION_INTERVAL 0x01

/* Get CGM communication interval.
 *
 * The normal response to this control point is SOCP_CGM_COMMUNICATION_INTERVAL_RESPONSE.
 * For error conditions, the response is SOCP_RESPONSE_CODE.
 */
#define SOCP_GET_CGM_COMMUNICATION_INTERVAL 0x02

/* CGM communication interval response.
 *
 * Operand: Communication interval in minutes.
 *
 * This is the normal response to SOCP_GET_CGM_COMMUNICATION_INTERVAL.
 */
#define SOCP_CGM_COMMUNICATION_INTERVAL_RESPONSE 0x03

/* Set glucose calibration value. This feature is not supported.
 *
 * Operand: Calibration Value.
 *
 * The response to this control point is SOCP_RESPONSE_CODE.
 */
#define SOCP_SET_GLUCOSE_CALIBRATION_VALUE 0x04

/* Get glucose calibration value. This feature is not supported.
 *
 * Operand: Calibration Data Record Number.
 *
 * The normal response to this control point is SOCP_GLUCOSE_CALIBRATION_VALUE_RESPONSE.
 * For error conditions, the response is SOCP_RESPONSE_CODE.
 */
#define SOCP_GET_GLUCOSE_CALIBRATION_VALUE 0x05

/* Glucose calibration value response.
 *
 * Operand: Calibration Data.
 *
 * This is the normal response to SOCP_GET_GLUCOSE_CALIBRATION_VALUE.
 */
#define SOCP_GLUCOSE_CALIBRATION_VALUE_RESPONSE 0x06

/* Set patient high alert level. This feature is not supported.
 *
 * Operand: Patient high blood glucose value in mg/dL.
 *
 * The response to this control point is SOCP_RESPONSE_CODE.
 */
#define SOCP_SET_PATIENT_HIGH_ALERT_LEVEL 0x07

/* Get patient high alert level. This feature is not supported.
 *
 * The normal response to this control point is SOCP_PATIENT_HIGH_ALERT_LEVEL_RESPONSE.
 * For error conditions, the response is SOCP_RESPONSE_CODE.
 */
#define SOCP_GET_PATIENT_HIGH_ALERT_LEVEL 0x08

/* Patient high alert level response.
 *
 * Operand: Patient high blood glucose value in mg/dL.
 *
 * This is the normal response to SOCP_GET_PATIENT_HIGH_ALERT_LEVEL.
 */
#define SOCP_PATIENT_HIGH_ALERT_LEVEL_RESPONSE 0x09

/* Set patient low alert level. This feature is not supported.
 *
 * Operand: Patient low blood glucose value in mg/dL.
 *
 * The response to this control point is SOCP_RESPONSE_CODE.
 */
#define SOCP_SET_PATIENT_LOW_ALERT_LEVEL 0x0A

/* Get patient low alert level. This feature is not supported.
 *
 * The normal response to this control point is SOCP_PATIENT_LOW_ALERT_LEVEL_RESPONSE.
 * For error conditions, the response is SOCP_RESPONSE_CODE.
 */
#define SOCP_GET_PATIENT_LOW_ALERT_LEVEL 0x0B

/* Patient low alert level response.
 *
 * Operand: Patient low blood glucose value in mg/dL.
 *
 * This is the normal response to SOCP_GET_PATIENT_LOW_ALERT_LEVEL.
 */
#define SOCP_PATIENT_LOW_ALERT_LEVEL_RESPONSE 0x0C

/* Set Hypo Alert Level. This feature is not supported.
 *
 * Operand: Hypo Alert Level value in mg/dL.
 *
 * The response to this control point is SOCP_RESPONSE_CODE.
 */
#define SOCP_SET_HYPO_ALERT_LEVEL 0x0D

/* Get Hypo Alert Level. This feature is not supported.
 *
 * The normal response to this control point is SOCP_HYPO_ALERT_LEVEL_RESPONSE.
 * For error conditions, the response is SOCP_RESPONSE_CODE.
 */
#define SOCP_GET_HYPO_ALERT_LEVEL 0x0E

/* Hypo Alert Level Response.
 *
 * Operand: Hypo Alert Level value in mg/dL.
 *
 * This is the normal response to SOCP_GET_HYPO_ALERT_LEVEL.
 */
#define SOCP_HYPO_ALERT_LEVEL_RESPONSE 0x0F

/* Set Hyper Alert Level. This feature is not supported.
 *
 * Operand: Hyper Alert Level value in mg/dL.
 *
 * The response to this control point is SOCP_RESPONSE_CODE.
 */
#define SOCP_SET_HYPER_ALERT_LEVEL 0x10

/* Get Hyper Alert Level. This feature is not supported.
 *
 * The normal response to this control point is Op Code SOCP_HYPER_ALERT_LEVEL_RESPONSE.
 * For error conditions, the response is SOCP_RESPONSE_CODE.
 */
#define SOCP_GET_HYPER_ALERT_LEVEL 0x11

/* Hyper Alert Level Response.
 *
 * Operand: Hyper Alert Level value in mg/dL.
 *
 * This is the normal response to SOCP_GET_HYPER_ALERT_LEVEL.
 */
#define SOCP_HYPER_ALERT_LEVEL_RESPONSE 0x12

/* Set Rate of Decrease Alert Level. This feature is not supported.
 *
 * Operand: Rate of Decrease Alert Level value in mg/dL/min.
 *
 * The response to this control point is SOCP_RESPONSE_CODE.
 */
#define SOCP_SET_RATE_OF_DECREASE_ALERT_LEVEL 0x13

/* Get Rate of Decreasmg/dLe Alert Level. This feature is not supported.
 *
 * The normal response to this control point is SOCP_RATE_OF_DECREASE_ALERT_LEVEL_RESPONSE.
 * For error conditions, the response is SOCP_RESPONSE_CODE.
 */
#define SOCP_GET_RATE_OF_DECREASE_ALERT_LEVEL 0x14

/* Rate of Decrease Alert Level Response.
 *
 * Operand: Rate of Decrease Alert Level value in mg/dL/min.
 *
 * This is the normal response to SOCP_GET_RATE_OF_DECREASE_ALERT_LEVEL.
 */
#define SOCP_RATE_OF_DECREASE_ALERT_LEVEL_RESPONSE 0x15

/* Set Rate of Increase Alert Level. This feature is not supported.
 *
 * Operand: Rate of Increase Alert Level value in mg/dL/min.
 *
 * The response to this control point is SOCP_RESPONSE_CODE.
 */
#define SOCP_SET_RATE_OF_INCREASE_ALERT_LEVEL 0x16

/* Get Rate of Increase Alert Level. This feature is not supported.
 *
 * The normal response to this control point is SOCP_RATE_OF_INCREASE_ALERT_LEVEL_RESPONSE.
 * For error conditions, the response is SOCP_RESPONSE_CODE.
 */
#define SOCP_GET_RATE_OF_INCREASE_ALERT_LEVEL 0x17

/* Rate of Increase Alert Level Response.
 *
 * Operand: Rate of Increase Alert Level value in mg/dL/min.
 *
 * This is the normal response to SOCP_GET_RATE_OF_INCREASE_ALERT_LEVEL.
 */
#define SOCP_RATE_OF_INCREASE_ALERT_LEVEL_RESPONSE 0x18

/* Reset Device Specific Alert. This feature is not supported.
 *
 * The response to this control point is SOCP_RESPONSE_CODE.
 */
#define SOCP_RESET_DEVICE_SPECIFIC_ALERT 0x19

/* Start the Session.
 *
 * The response to this control point is SOCP_RESPONSE_CODE.
 */
#define SOCP_START_THE_SESSION 0x1A

/* Stop the Session.
 *
 * The response to this control point is SOCP_RESPONSE_CODE.
 */
#define SOCP_STOP_THE_SESSION 0x1B

/* Response Code.
 *
 * Operand: Request Op Code, Response Code Value.
 */
#define SOCP_RESPONSE_CODE 0x1C

/* Response Code Values. */
#define SOCP_RSP_RESERVED_FOR_FUTURE_USE 0x00
#define SOCP_RSP_SUCCESS 0x01
#define SOCP_RSP_OP_CODE_NOT_SUPPORTED 0x02
#define SOCP_RSP_INVALID_OPERAND 0x03
#define SOCP_RSP_PROCEDURE_NOT_COMPLETED 0x04
#define SOCP_RSP_OUT_OF_RANGE 0x05

/* Specific Ops Control Point value. */
struct ble_cgms_socp_value {
	/* Opcode. */
	uint8_t opcode;
	/* Length of the operand. */
	uint8_t operand_len;
	/* Pointer to the operand. */
	const uint8_t *operand;
};

static void ble_socp_decode(uint8_t data_len, const uint8_t *data,
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
		socp_val->operand = &data[1];
	}
}

static uint8_t ble_socp_encode(const struct ble_socp_rsp *socp_rsp, uint8_t *data)
{
	uint8_t len = 0;
	int i;

	if (data) {
		data[len++] = socp_rsp->opcode;

		if ((socp_rsp->opcode != SOCP_CGM_COMMUNICATION_INTERVAL_RESPONSE) &&
		    (socp_rsp->opcode != SOCP_PATIENT_HIGH_ALERT_LEVEL_RESPONSE) &&
		    (socp_rsp->opcode != SOCP_PATIENT_LOW_ALERT_LEVEL_RESPONSE) &&
		    (socp_rsp->opcode != SOCP_HYPO_ALERT_LEVEL_RESPONSE) &&
		    (socp_rsp->opcode != SOCP_HYPER_ALERT_LEVEL_RESPONSE) &&
		    (socp_rsp->opcode != SOCP_RATE_OF_DECREASE_ALERT_LEVEL_RESPONSE) &&
		    (socp_rsp->opcode != SOCP_RATE_OF_INCREASE_ALERT_LEVEL_RESPONSE) &&
		    (socp_rsp->opcode != SOCP_GLUCOSE_CALIBRATION_VALUE_RESPONSE)) {
			data[len++] = socp_rsp->req_opcode;
			data[len++] = socp_rsp->rsp_code;
		}

		for (i = 0; i < socp_rsp->size_val; i++) {
			data[len++] = socp_rsp->resp_val[i];
		}
	}

	return len;
}

/* Add the Specific Ops Control Point characteristic. */
uint32_t cgms_socp_char_add(struct ble_cgms *cgms)
{
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

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

	return sd_ble_gatts_characteristic_add(cgms->service_handle, &char_md, &attr_char_value,
					       &cgms->char_handles.socp);
}

/* Send a Specific Ops Control Point response. */
static void socp_send(struct ble_cgms *cgms)
{
	uint32_t err;
	uint8_t encoded_resp[BLE_CGMS_SOCP_LEN];
	uint16_t len;
	struct ble_cgms_evt cgms_evt = {
		.evt_type = BLE_CGMS_EVT_ERROR,
	};

	/* Send indication. */
	len = ble_socp_encode(&(cgms->socp_response), encoded_resp);

	struct ble_gq_req cgms_req = {
		.type = BLE_GQ_REQ_GATTS_HVX,
		.error_handler.cb = cgms->gatt_err_handler,
		.error_handler.ctx = cgms,
		.gatts_hvx.type = BLE_GATT_HVX_INDICATION,
		.gatts_hvx.handle = cgms->char_handles.socp.value_handle,
		.gatts_hvx.offset = 0,
		.gatts_hvx.p_data = encoded_resp,
		.gatts_hvx.p_len = &len,
	};

	err = ble_gq_item_add(cgms->gatt_queue, &cgms_req, cgms->conn_handle);

	/* Report error to application. */
	if ((cgms->evt_handler != NULL) &&
	    (err != NRF_SUCCESS) &&
	    (err != NRF_ERROR_INVALID_STATE)) {
		cgms_evt.error.reason = err;
		cgms->evt_handler(cgms, &cgms_evt);
	}
}

static bool is_feature_present(struct ble_cgms *cgms, uint32_t feature)
{
	return (cgms->feature.feature & feature);
}

/* Specific Ops Control Point write event handler. */
static void on_socp_value_write(struct ble_cgms *cgms, const ble_gatts_evt_write_t *evt_write)
{
	uint32_t err;
	struct ble_cgms_socp_value socp_request;
	struct ble_cgms_evt evt;
	struct ble_cgms_sst sst = {};

	/* Decode request. */
	ble_socp_decode(evt_write->len, evt_write->data, &socp_request);

	cgms->socp_response.opcode = SOCP_RESPONSE_CODE;
	cgms->socp_response.req_opcode = socp_request.opcode;
	cgms->socp_response.rsp_code = SOCP_RSP_OP_CODE_NOT_SUPPORTED;
	cgms->socp_response.size_val = 0;

	switch (socp_request.opcode) {
	case SOCP_SET_CGM_COMMUNICATION_INTERVAL:
		cgms->socp_response.rsp_code = SOCP_RSP_SUCCESS;
		cgms->comm_interval = socp_request.operand[0];
		evt.evt_type = BLE_CGMS_EVT_WRITE_COMM_INTERVAL;
		cgms->evt_handler(cgms, &evt);
		break;
	case SOCP_GET_CGM_COMMUNICATION_INTERVAL:
		cgms->socp_response.opcode = SOCP_CGM_COMMUNICATION_INTERVAL_RESPONSE;
		cgms->socp_response.resp_val[0] = cgms->comm_interval;
		cgms->socp_response.size_val++;
		break;
	case SOCP_START_THE_SESSION:
		if (cgms->is_session_started) {
			cgms->socp_response.rsp_code = SOCP_RSP_PROCEDURE_NOT_COMPLETED;
		} else if ((cgms->nb_run_session != 0) &&
			   !(is_feature_present(cgms,
						BLE_CGMS_FEAT_MULTIPLE_SESSIONS_SUPPORTED))) {
			cgms->socp_response.rsp_code = SOCP_RSP_PROCEDURE_NOT_COMPLETED;
		} else {
			cgms->socp_response.rsp_code = SOCP_RSP_SUCCESS;
			cgms->is_session_started     = true;
			cgms->nb_run_session++;

			if (cgms->evt_handler != NULL) {
				evt.evt_type = BLE_CGMS_EVT_START_SESSION;
				cgms->evt_handler(cgms, &evt);
			}

			err = cgms_sst_set(cgms, &sst);
			if (err != NRF_SUCCESS) {
				if (cgms->evt_handler != NULL) {
					evt.evt_type = BLE_CGMS_EVT_ERROR;
					evt.error.reason = err;
					cgms->evt_handler(cgms, &evt);
				}
			}

			/* Reset database */
			(void)cgms_db_init();

			cgms->sensor_status.time_offset    = 0;
			cgms->sensor_status.status.status &= (~BLE_CGMS_STATUS_SESSION_STOPPED);
			err = ble_cgms_update_status(cgms, &cgms->sensor_status);
			if (err != NRF_SUCCESS) {
				if (cgms->evt_handler != NULL) {
					evt.evt_type = BLE_CGMS_EVT_ERROR;
					evt.error.reason = err;
					cgms->evt_handler(cgms, &evt);
				}
			}
		}
		break;
	case SOCP_STOP_THE_SESSION:
	{
		struct ble_cgms_status status = {
			.time_offset = cgms->sensor_status.time_offset,
			.status.status = cgms->sensor_status.status.status |
					 BLE_CGMS_STATUS_SESSION_STOPPED,
		};

		cgms->evt_handler(cgms, &evt);
		cgms->socp_response.rsp_code = SOCP_RSP_SUCCESS;
		cgms->is_session_started = false;

		if (cgms->evt_handler != NULL) {
			evt.evt_type = BLE_CGMS_EVT_STOP_SESSION;
			cgms->evt_handler(cgms, &evt);
		}
		err = ble_cgms_update_status(cgms, &status);
		if (err != NRF_SUCCESS) {
			if (cgms->evt_handler != NULL) {
				evt.evt_type = BLE_CGMS_EVT_ERROR;
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


void cgms_socp_on_rw_auth_req(struct ble_cgms *cgms,
			      const ble_gatts_evt_rw_authorize_request_t *auth_req)
{
	uint32_t err;
	struct ble_cgms_evt cgms_evt = {
		.evt_type = BLE_CGMS_EVT_ERROR,
	};
	ble_gatts_rw_authorize_reply_params_t auth_reply = {
		.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE,
		.params = {
			.write = {
				.gatt_status = BLE_GATT_STATUS_SUCCESS,
				.update = 1,
			},
		},
	};

	uint8_t cccd_value[2];
	ble_gatts_value_t gatts_val = {
		.p_value = cccd_value,
		.len = sizeof(cccd_value),
		.offset = 0,
	};

	err = sd_ble_gatts_value_get(cgms->conn_handle, cgms->char_handles.socp.cccd_handle,
				     &gatts_val);
	if ((err != NRF_SUCCESS) || !is_indication_enabled(gatts_val.p_value)) {
		auth_reply.params.write.gatt_status = BLE_GATT_STATUS_ATTERR_CPS_CCCD_CONFIG_ERROR;
	}

	err = sd_ble_gatts_rw_authorize_reply(cgms->conn_handle, &auth_reply);
	if (err != NRF_SUCCESS) {
		if (cgms->evt_handler != NULL) {
			cgms_evt.error.reason = err;
			cgms_evt.evt_type = BLE_CGMS_EVT_ERROR;
			cgms->evt_handler(cgms, &cgms_evt);
		}
		return;
	}

	if (auth_reply.params.write.gatt_status == BLE_GATT_STATUS_ATTERR_CPS_CCCD_CONFIG_ERROR) {
		return;
	}

	on_socp_value_write(cgms, &auth_req->request.write);

}
