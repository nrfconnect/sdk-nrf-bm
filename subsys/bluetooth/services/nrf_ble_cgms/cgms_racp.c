/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdint.h>
#include <string.h>
#include <ble.h>
#include <ble_gq.h>
#include <bluetooth/services/uuid.h>
#include "cgms_racp.h"
#include "cgms_db.h"
#include "cgms_meas.h"

#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ble_cgms, CONFIG_BLE_CGMS_LOG_LEVEL);

#define OPERAND_LESS_GREATER_FILTER_TYPE_SIZE  1
#define OPERAND_LESS_GREATER_FILTER_PARAM_SIZE 2
#define OPERAND_LESS_GREATER_SIZE OPERAND_LESS_GREATER_FILTER_TYPE_SIZE                            \
				  + OPERAND_LESS_GREATER_FILTER_PARAM_SIZE

/**
 * @brief Function for adding a characteristic for the Record Access Control Point.
 *
 * @param[in] cgms Service instance.
 *
 * @return NRF_SUCCESS if characteristic was successfully added, otherwise an error code.
 */
int cgms_racp_char_add(struct nrf_ble_cgms *cgms)
{
	int err;
	ble_uuid_t char_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_RECORD_ACCESS_CONTROL_POINT_CHAR,
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
					      &cgms->char_handles.racp);
	if (err) {
		LOG_ERR("Failed to add GATT CGMS RAC characteristic, nrf_error %#x", err);
		return err;
	}

	return 0;
}

/**
 * @brief Function for sending response from Specific Operation Control Point.
 *
 * @param[in] cgms Service instance.
 * @param[in] racp_val RACP value to be sent.
 */
static void racp_send(struct nrf_ble_cgms *cgms, struct ble_racp_value *racp_val)
{
	uint32_t err;
	uint8_t encoded_resp[25];
	uint16_t len;

	struct nrf_ble_cgms_evt evt = {
		.evt_type = NRF_BLE_CGMS_EVT_ERROR,
	};

	/* Send indication */
	len = ble_racp_encode(racp_val, encoded_resp);

	struct ble_gq_req cgms_req = {
		.type = BLE_GQ_REQ_GATTS_HVX,
		.error_handler.cb = cgms->gatt_err_handler,
		.error_handler.ctx = cgms,
		.params.gatts_hvx.type = BLE_GATT_HVX_INDICATION,
		.params.gatts_hvx.handle = cgms->char_handles.racp.value_handle,
		.params.gatts_hvx.offset = 0,
		.params.gatts_hvx.p_data = encoded_resp,
		.params.gatts_hvx.p_len = &len,
	};


	err = ble_gq_item_add(cgms->gatt_queue, &cgms_req, cgms->conn_handle);

	/* Report error to application */
	if ((cgms->evt_handler != NULL) &&
	    (err != NRF_SUCCESS) &&
	    (err != NRF_ERROR_INVALID_STATE)) {
		evt.error.reason = err;
		cgms->evt_handler(cgms, &evt);
	}
}

/**
 * @brief Function for sending a RACP response containing a Response Code Op Code and
 *        Response Code Value.
 *
 * @param[in] cgms Service instance.
 * @param[in] opcode RACP Op Code.
 * @param[in] value RACP Response Code Value.
 */
static void racp_response_code_send(struct nrf_ble_cgms *cgms, uint8_t opcode, uint8_t value)
{
	cgms->racp_data.pending_racp_response.opcode = RACP_OPCODE_RESPONSE_CODE;
	cgms->racp_data.pending_racp_response.operator = RACP_OPERATOR_NULL;
	cgms->racp_data.pending_racp_response.operand_len = 2;
	cgms->racp_data.pending_racp_response.operand =
		cgms->racp_data.pending_racp_response_operand;

	cgms->racp_data.pending_racp_response_operand[0] = opcode;
	cgms->racp_data.pending_racp_response_operand[1] = value;

	racp_send(cgms, &cgms->racp_data.pending_racp_response);
}

/**
 * @brief Function for responding to the ALL operation.
 *
 * @param[in] cgms Service instance.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t racp_report_records_all(struct nrf_ble_cgms *cgms)
{
	uint16_t total_records = cgms_db_num_records_get();
	uint16_t cur_num_rec;
	uint8_t i;
	uint8_t recs_to_send;

	if (cgms->racp_data.racp_proc_record_ndx >= total_records) {
		cgms->racp_data.racp_processing_active = false;
	} else {
		uint32_t err;
		struct ble_cgms_rec rec[NRF_BLE_CGMS_MEAS_REC_PER_NOTIF_MAX];

		cur_num_rec = total_records - cgms->racp_data.racp_proc_record_ndx;
		if (cur_num_rec > NRF_BLE_CGMS_MEAS_REC_PER_NOTIF_MAX) {
			cur_num_rec = NRF_BLE_CGMS_MEAS_REC_PER_NOTIF_MAX;
		}

		recs_to_send = (uint8_t)cur_num_rec;

		for (i = 0; i < cur_num_rec; i++) {
			err = cgms_db_record_get(&(rec[i]),
						 cgms->racp_data.racp_proc_record_ndx + i);
			if (err != NRF_SUCCESS) {
				return err;
			}
		}

		err = cgms_meas_send(cgms, rec, &recs_to_send);
		if (err != NRF_SUCCESS) {
			return err;
		}

		cgms->racp_data.racp_proc_record_ndx += recs_to_send;
	}

	return NRF_SUCCESS;
}

/**
 * @brief Function for responding to the FIRST or the LAST operation.
 *
 * @param[in] cgms Service instance.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t racp_report_records_first_last(struct nrf_ble_cgms *cgms)
{
	uint32_t err;
	struct ble_cgms_rec rec;
	uint16_t total_records;
	uint8_t recs_to_send = 1;

	total_records = cgms_db_num_records_get();

	if ((cgms->racp_data.racp_proc_records_reported != 0) || (total_records == 0)) {
		cgms->racp_data.racp_processing_active = false;
	} else {
		if (cgms->racp_data.racp_proc_operator == RACP_OPERATOR_FIRST) {
			err = cgms_db_record_get(&rec, 0);
			if (err != NRF_SUCCESS) {
				return err;
			}
		} else if (cgms->racp_data.racp_proc_operator == RACP_OPERATOR_LAST) {
			err = cgms_db_record_get(&rec, total_records - 1);
			if (err != NRF_SUCCESS) {
				return err;
			}
		}

		err = cgms_meas_send(cgms, &rec, &recs_to_send);
		if (err != NRF_SUCCESS) {
			return err;
		}
		cgms->racp_data.racp_proc_record_ndx++;
	}

	return NRF_SUCCESS;
}

/**
 * @brief Function for responding to the LESS OR EQUAL operation.
 *
 * @param[in] cgms Service instance.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static int racp_report_records_less_equal(struct nrf_ble_cgms *cgms)
{
	uint16_t recs_to_send_total;
	uint16_t recs_to_send_remaining;
	uint8_t  recs_to_send;
	uint16_t i;

	recs_to_send_total = cgms->racp_data.racp_proc_records_ndx_last_to_send + 1;

	if (cgms->racp_data.racp_proc_record_ndx >= recs_to_send_total) {
		cgms->racp_data.racp_processing_active = false;
	} else {
		int err;
		struct ble_cgms_rec rec[NRF_BLE_CGMS_MEAS_REC_PER_NOTIF_MAX];

		recs_to_send_remaining = (recs_to_send_total -
				       cgms->racp_data.racp_proc_records_reported);

		if (recs_to_send_remaining > NRF_BLE_CGMS_MEAS_REC_PER_NOTIF_MAX) {
			recs_to_send = NRF_BLE_CGMS_MEAS_REC_PER_NOTIF_MAX;
		} else {
			recs_to_send = (uint8_t)recs_to_send_remaining;
		}

		for (i = 0; i < recs_to_send; i++) {
			err = cgms_db_record_get(&(rec[i]),
						 cgms->racp_data.racp_proc_record_ndx + i);
			if (err != NRF_SUCCESS) {
				return err;
			}
		}

		err = cgms_meas_send(cgms, rec, &recs_to_send);
		if (err != NRF_SUCCESS) {
			return err;
		}

		cgms->racp_data.racp_proc_record_ndx += recs_to_send;
	}

	return NRF_SUCCESS;
}

/**
 * @brief Function for responding to the GREATER OR EQUAL operation.
 *
 * @param[in] cgms Service instance.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static int racp_report_records_greater_equal(struct nrf_ble_cgms *cgms)
{
	int err;
	uint16_t recs_total = cgms_db_num_records_get();
	uint16_t recs_to_send_remaining;
	uint8_t recs_to_send;
	uint16_t i;


	recs_total = cgms_db_num_records_get();
	if (cgms->racp_data.racp_proc_record_ndx >= recs_total) {
		cgms->racp_data.racp_processing_active = false;

		return NRF_SUCCESS;
	}

	struct ble_cgms_rec rec[NRF_BLE_CGMS_MEAS_REC_PER_NOTIF_MAX];

	recs_to_send_remaining = recs_total - cgms->racp_data.racp_proc_record_ndx;
	if (recs_to_send_remaining > NRF_BLE_CGMS_MEAS_REC_PER_NOTIF_MAX) {
		recs_to_send = NRF_BLE_CGMS_MEAS_REC_PER_NOTIF_MAX;
	} else {
		recs_to_send = (uint8_t)recs_to_send_remaining;
	}

	for (i = 0; i < recs_to_send; i++) {
		err = cgms_db_record_get(&(rec[i]), cgms->racp_data.racp_proc_record_ndx + i);
		if (err != NRF_SUCCESS) {
			return err;
		}
	}
	err = cgms_meas_send(cgms, rec, &recs_to_send);
	if (err != NRF_SUCCESS) {
		return err;
	}
	cgms->racp_data.racp_proc_record_ndx += recs_to_send;

	return NRF_SUCCESS;
}

/**
 * @brief Function for informing that the REPORT RECORDS procedure is completed.
 *
 * @param[in] cgms Service instance.
 */
static void racp_report_records_completed(struct nrf_ble_cgms *cgms)
{
	uint8_t resp_code_value;

	if (cgms->racp_data.racp_proc_records_reported > 0) {
		resp_code_value = RACP_RESPONSE_SUCCESS;
	} else {
		resp_code_value = RACP_RESPONSE_NO_RECORDS_FOUND;
	}

	racp_response_code_send(cgms, RACP_OPCODE_REPORT_RECS, resp_code_value);
}

/**
 * @brief Function for the RACP report records procedure.
 *
 * @param[in]   cgms   Service instance.
 */
static void racp_report_records_procedure(struct nrf_ble_cgms *cgms)
{
	int err = NRF_SUCCESS;
	struct nrf_ble_cgms_evt evt = {
		.evt_type = NRF_BLE_CGMS_EVT_ERROR,
	};

	while (cgms->racp_data.racp_processing_active) {
		/* Execute requested procedure */
		switch (cgms->racp_data.racp_proc_operator) {
		case RACP_OPERATOR_ALL:
			err = racp_report_records_all(cgms);
			break;

		case RACP_OPERATOR_FIRST:
			/* Fall through. */
		case RACP_OPERATOR_LAST:
			err = racp_report_records_first_last(cgms);
			break;
		case RACP_OPERATOR_GREATER_OR_EQUAL:
			err = racp_report_records_greater_equal(cgms);
			break;
		case RACP_OPERATOR_LESS_OR_EQUAL:
			err = racp_report_records_less_equal(cgms);
			break;
		default:
			/* Report error to application */
			if (cgms->evt_handler != NULL) {
				evt.error.reason = NRF_ERROR_INTERNAL;
				cgms->evt_handler(cgms, &evt);
			}

			cgms->racp_data.racp_processing_active = false;
			return;
		}

		/* Error handling */
		switch (err) {
		case NRF_SUCCESS:
			if (!cgms->racp_data.racp_processing_active) {
				racp_report_records_completed(cgms);
			}
			break;

		case NRF_ERROR_RESOURCES:
			/* Wait for TX_COMPLETE event to resume transmission. */
			return;

		case NRF_ERROR_INVALID_STATE:
			/* Notification is probably not enabled. Ignore request. */
			cgms->racp_data.racp_processing_active = false;
			return;

		default:
			/* Report error to application. */
			if (cgms->evt_handler != NULL) {
				evt.error.reason = NRF_ERROR_INTERNAL;
				cgms->evt_handler(cgms, &evt);
			}

			/* Make sure state machine returns to the default state. */
			cgms->racp_data.racp_processing_active = false;
			return;
		}
	}
}

/**
 * @brief Function for testing if the received request is to be executed.
 *
 * @param[in] racp_request Request to be checked.
 * @param[out] response_code Response code to be sent in case the request is rejected.
 *                           RACP_RESPONSE_RESERVED is returned if the received message is
 *                           to be rejected without sending a respone.
 *
 * @return TRUE if the request is to be executed, FALSE if it is to be rejected.
 *         If it is to be rejected, response_code will contain the response code to be
 *         returned to the central.
 */
static bool is_request_to_be_executed(struct nrf_ble_cgms *cgms,
				      const struct ble_racp_value *racp_request,
				      uint8_t *response_code)
{
	*response_code = RACP_RESPONSE_RESERVED;

	if (racp_request->opcode == RACP_OPCODE_ABORT_OPERATION) {
		if (cgms->racp_data.racp_processing_active) {
			if (racp_request->operator != RACP_OPERATOR_NULL) {
				*response_code = RACP_RESPONSE_INVALID_OPERATOR;
			} else if (racp_request->operand_len != 0) {
				*response_code = RACP_RESPONSE_INVALID_OPERAND;
			} else {
				*response_code = RACP_RESPONSE_SUCCESS;
			}
		} else {
			*response_code = RACP_RESPONSE_ABORT_FAILED;
		}
	} else if (cgms->racp_data.racp_processing_active) {
		return false;
	} else if ((racp_request->opcode == RACP_OPCODE_REPORT_RECS) ||
		   (racp_request->opcode == RACP_OPCODE_REPORT_NUM_RECS)) {
		/* Known opcodes. */
		switch (racp_request->operator) {
		/* Operators without a filter. */
		case RACP_OPERATOR_ALL:
			/* Fall through. */
		case RACP_OPERATOR_FIRST:
			/* Fall through.*/
		case RACP_OPERATOR_LAST:
			if (racp_request->operand_len != 0) {
				*response_code = RACP_RESPONSE_INVALID_OPERAND;
			}
			break;

		/* Operators with a filter as part of the operand. */
		case RACP_OPERATOR_LESS_OR_EQUAL:
			/* Fall Through. */
		case RACP_OPERATOR_GREATER_OR_EQUAL:
			if (*(racp_request->operand) == RACP_OPERAND_FILTER_TYPE_FACING_TIME) {
				*response_code = RACP_RESPONSE_PROCEDURE_NOT_DONE;
			}

			if (racp_request->operand_len != OPERAND_LESS_GREATER_SIZE) {
				*response_code = RACP_RESPONSE_INVALID_OPERAND;
			}
			break;

		case RACP_OPERATOR_RANGE:
			*response_code = RACP_RESPONSE_OPERATOR_UNSUPPORTED;
			break;

		/* Invalid operators. */
		case RACP_OPERATOR_NULL:
			/* Fall through. */
		default:
			*response_code = RACP_RESPONSE_INVALID_OPERATOR;
			break;
		}
	} else if (racp_request->opcode == RACP_OPCODE_DELETE_RECS) {
		/* Unsupported opcodes. */
		*response_code = RACP_RESPONSE_OPCODE_UNSUPPORTED;
	} else {
		/* Unknown opcodes. */
		*response_code = RACP_RESPONSE_OPCODE_UNSUPPORTED;
	}

	return (*response_code == RACP_RESPONSE_RESERVED);
}

/**
 * @brief Function for getting a record with time offset less or equal to the input param.
 *
 * @param[in] offset The maximum time offset of the record returned.
 * @param[out] record_num The record index of the record that has the desired time offset.
 *
 * @retval NRF_SUCCESS If the record was successfully retrieved.
 * @retval NRF_ERROR_NOT_FOUND A record with the desired offset does not exist in the database.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error are propagated.
 */
static int record_index_offset_less_or_equal_get(uint16_t offset, uint16_t *record_num)
{
	int err;
	struct ble_cgms_rec rec;
	uint16_t upper_bound = cgms_db_num_records_get();

	for ((*record_num) = upper_bound; ((*record_num)-- > 0);) {
		err = cgms_db_record_get(&rec, *record_num);
		if (err != NRF_SUCCESS) {
			return err;
		}

		if (rec.meas.time_offset <= offset) {
			return NRF_SUCCESS;
		}
	}

	return NRF_ERROR_NOT_FOUND;
}

/**
 * @brief Function for getting a record with time offset greater or equal to the input param.
 *
 * @param[in] offset The minimum time offset of the record returned.
 * @param[out] record_num Record index of the record that has the desired time offset.
 *
 * @retval NRF_SUCCESS If the record was successfully retrieved.
 * @retval NRF_ERROR_NOT_FOUND A record with the desired offset does not exist in the database.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error are propagated.
 */
static int record_index_offset_greater_or_equal_get(uint16_t offset, uint16_t *record_num)
{
	int err;
	struct ble_cgms_rec rec;
	uint16_t upper_bound = cgms_db_num_records_get();

	for (*record_num = 0; *record_num < upper_bound; (*record_num)++) {
		err = cgms_db_record_get(&rec, *record_num);
		if (err != NRF_SUCCESS) {
			return err;
		}

		if (rec.meas.time_offset >= offset) {
			return NRF_SUCCESS;
		}
	}

	return NRF_ERROR_NOT_FOUND;
}

/**
 * @brief Function for processing a REPORT RECORDS request.
 *
 * @details Set initial values before entering the state machine of racp_report_records_procedure().
 *
 * @param[in] cgms Service instance.
 * @param[in] racp_request Request to be executed.
 */
static void report_records_request_execute(struct nrf_ble_cgms *cgms,
					   struct ble_racp_value *racp_request)
{
	int err;
	uint16_t offset_requested;
	uint16_t *record_num;
	const uint8_t *op;

	cgms->racp_data.racp_processing_active = true;

	cgms->racp_data.racp_proc_record_ndx = 0;
	cgms->racp_data.racp_proc_operator = racp_request->operator;
	cgms->racp_data.racp_proc_records_reported = 0;
	cgms->racp_data.racp_proc_records_ndx_last_to_send = 0;

	if (cgms->racp_data.racp_proc_operator == RACP_OPERATOR_GREATER_OR_EQUAL) {
		op = &cgms->racp_data.racp_request.operand[OPERAND_LESS_GREATER_FILTER_TYPE_SIZE];
		offset_requested = sys_get_le16(op);
		record_num = &cgms->racp_data.racp_proc_record_ndx;
		err = record_index_offset_greater_or_equal_get(offset_requested, record_num);
		if (err != NRF_SUCCESS) {
			racp_report_records_completed(cgms);
		}
	}

	if (cgms->racp_data.racp_proc_operator == RACP_OPERATOR_LESS_OR_EQUAL) {
		op = &cgms->racp_data.racp_request.operand[OPERAND_LESS_GREATER_FILTER_TYPE_SIZE];
		offset_requested = sys_get_le16(op);
		record_num = &cgms->racp_data.racp_proc_records_ndx_last_to_send;
		err = record_index_offset_less_or_equal_get(offset_requested, record_num);
		if (err != NRF_SUCCESS) {
			racp_report_records_completed(cgms);
		}
	}

	racp_report_records_procedure(cgms);
}

/**
 * @brief Function for processing a REPORT NUM RECORDS request.
 *
 * @param[in] cgms Service instance.
 * @param[in] racp_request Request to be executed.
 */
static void report_num_records_request_execute(struct nrf_ble_cgms   *cgms,
					       struct ble_racp_value *racp_request)
{
	int err;
	uint16_t total_records;
	uint16_t num_records;

	total_records = cgms_db_num_records_get();
	num_records = 0;

	if (racp_request->operator == RACP_OPERATOR_ALL) {
		num_records = total_records;
	} else if ((racp_request->operator == RACP_OPERATOR_FIRST) ||
		   (racp_request->operator == RACP_OPERATOR_LAST)) {
		if (total_records > 0) {
			num_records = 1;
		}
	} else if (racp_request->operator == RACP_OPERATOR_GREATER_OR_EQUAL) {
		uint16_t index_of_offset;
		uint8_t *operand = cgms->racp_data.racp_request.operand;
		uint16_t offset_requested =
			sys_get_le16(&operand[OPERAND_LESS_GREATER_FILTER_TYPE_SIZE]);
		err = record_index_offset_greater_or_equal_get(offset_requested, &index_of_offset);

		if (err != NRF_SUCCESS) {
			num_records = 0;
		} else {
			num_records = total_records - index_of_offset;
		}
	}

	cgms->racp_data.pending_racp_response.opcode = RACP_OPCODE_NUM_RECS_RESPONSE;
	cgms->racp_data.pending_racp_response.operator = RACP_OPERATOR_NULL;
	cgms->racp_data.pending_racp_response.operand_len = sizeof(uint16_t);
	cgms->racp_data.pending_racp_response.operand =
		cgms->racp_data.pending_racp_response_operand;

	cgms->racp_data.pending_racp_response_operand[0] = num_records & 0xFF;
	cgms->racp_data.pending_racp_response_operand[1] = num_records >> 8;

	racp_send(cgms, &cgms->racp_data.pending_racp_response);
}

/**
 * @brief Function for handling a write event to the Record Access Control Point.
 *
 * @param[in] cgms Service instance.
 * @param[in] evt_write WRITE event to be handled.
 */
static void on_racp_value_write(struct nrf_ble_cgms *cgms, const ble_gatts_evt_write_t *evt_write)
{
	uint8_t response_code;
	struct nrf_ble_cgms_evt cgms_evt = {
		.evt_type = NRF_BLE_CGMS_EVT_ERROR,
	};

	/* set up reply to authorized write. */
	ble_gatts_rw_authorize_reply_params_t auth_reply;
	uint32_t err;

	auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
	auth_reply.params.write.offset = 0;
	auth_reply.params.write.len    = 0;
	auth_reply.params.write.p_data = NULL;

	/* Decode request. */
	ble_racp_decode(evt_write->len, evt_write->data, &cgms->racp_data.racp_request);

	/* Check if request is to be executed */
	if (is_request_to_be_executed(cgms, &cgms->racp_data.racp_request, &response_code)) {
		auth_reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
		auth_reply.params.write.update = 1;

		err = sd_ble_gatts_rw_authorize_reply(cgms->conn_handle, &auth_reply);
		if (err != NRF_SUCCESS) {
			if (cgms->evt_handler != NULL) {
				cgms_evt.error.reason = err;
				cgms->evt_handler(cgms, &cgms_evt);
			}
			return;
		}

		/* Execute request */
		if (cgms->racp_data.racp_request.opcode == RACP_OPCODE_REPORT_RECS) {
			report_records_request_execute(cgms, &cgms->racp_data.racp_request);
		} else if (cgms->racp_data.racp_request.opcode == RACP_OPCODE_REPORT_NUM_RECS) {
			report_num_records_request_execute(cgms, &cgms->racp_data.racp_request);
		}
	} else if (response_code != RACP_RESPONSE_RESERVED) {
		auth_reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
		auth_reply.params.write.update = 1;
		err = sd_ble_gatts_rw_authorize_reply(cgms->conn_handle, &auth_reply);

		if (err != NRF_SUCCESS) {
			if (cgms->evt_handler != NULL) {
				cgms_evt.error.reason = err;
				cgms->evt_handler(cgms, &cgms_evt);
			}
			return;
		}
		/* Abort any running procedure */
		cgms->racp_data.racp_processing_active = false;

		/* Respond with error code */
		racp_response_code_send(cgms, cgms->racp_data.racp_request.opcode, response_code);
	} else {
		/* ignore request */
		auth_reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
		auth_reply.params.write.update      = 1;
		err = sd_ble_gatts_rw_authorize_reply(cgms->conn_handle, &auth_reply);

		if (err != NRF_SUCCESS) {
			if (cgms->evt_handler != NULL) {
				cgms_evt.error.reason = err;
				cgms->evt_handler(cgms, &cgms_evt);
			}
			return;
		}
	}
}

void cgms_racp_on_rw_auth_req(struct nrf_ble_cgms *cgms,
			      const ble_gatts_evt_rw_authorize_request_t *auth_req)
{
	if (auth_req->type == BLE_GATTS_AUTHORIZE_TYPE_WRITE) {
		if (auth_req->request.write.handle == cgms->char_handles.racp.value_handle) {
			on_racp_value_write(cgms, &auth_req->request.write);
		}
	}
}

/**
 * @brief Function for handling BLE_GATTS_EVT_HVN_TX_COMPLETE events.
 *
 * @param[in] cgms Glucose Service structure.
 */
void cgms_racp_on_tx_complete(struct nrf_ble_cgms *cgms)
{
	if (cgms->racp_data.racp_processing_active) {
		racp_report_records_procedure(cgms);
	}
}
