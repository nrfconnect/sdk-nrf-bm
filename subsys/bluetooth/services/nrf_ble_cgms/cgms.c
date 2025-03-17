/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <ble_racp.h>
#include <bluetooth/services/ble_date_time.h>
#include <bluetooth/services/ble_cgms.h>
#include <bluetooth/services/uuid.h>
#include "cgms_db.h"
#include "cgms_meas.h"
#include "cgms_racp.h"
#include "cgms_socp.h"
#include "cgms_sst.h"

#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_cgms, CONFIG_BLE_CGMS_LOG_LEVEL);

/** Filter type value reserved for future use. */
#define OPERAND_FILTER_TYPE_RESV 0x00
/** Filter data using Sequence Number criteria. */
#define OPERAND_FILTER_TYPE_SEQ_NUM 0x01
/** Filter data using User Facing Time criteria. */
#define OPERAND_FILTER_TYPE_FACING_TIME 0x02

/**
 * @brief Function for interception of GATT errors and @ref nrf_ble_gq errors.
 *
 * @param[in] err Error code.
 * @param[in] ctx Parameter from the event handler.
 * @param[in] conn_handle Connection handle.
 */
static void gatt_error_handler(void *ctx, uint16_t conn_handle, uint32_t err)
{
	struct nrf_ble_cgms_evt evt = {
		.evt_type = NRF_BLE_CGMS_EVT_ERROR,
		.error.reason = err,
	};
	struct nrf_ble_cgms *cgms = (struct nrf_ble_cgms *)ctx;

	if (cgms->evt_handler && (err != NRF_ERROR_INVALID_STATE)) {
		cgms->evt_handler(cgms, &evt);
	}
}

/**
 * @brief Function for setting next sequence number by reading the last record in the data base.
 *
 * @return NRF_SUCCESS on successful initialization of service, otherwise an error code.
 */
static uint32_t next_sequence_number_set(void)
{
	int err;
	uint16_t num_records;
	struct ble_cgms_rec rec;

	num_records = cgms_db_num_records_get();
	if (num_records > 0) {
		/* Get last record */
		err = cgms_db_record_get(&rec, num_records - 1);
		if (err != NRF_SUCCESS) {
			return err;
		}
	}

	return NRF_SUCCESS;
}

uint8_t encode_feature_location_type(uint8_t *buf_out, struct nrf_ble_cgms_feature *feature)
{
	uint8_t len = 0;

	sys_put_le24(feature->feature, &buf_out[len]);
	len += 3;
	buf_out[len++] = (feature->sample_location << 4) | (feature->type & 0x0F);
	sys_put_le16(0xFFFF, &buf_out[len]);
	len += sizeof(uint16_t);

	return len;
}

/**
 * @brief Function for adding a characteristic for the glucose feature.
 *
 * @param[in] cgms Service instance.
 *
 * @return NRF_SUCCESS if characteristic was successfully added, otherwise an error code.
 */
static uint32_t glucose_feature_char_add(struct nrf_ble_cgms *cgms)
{
	int err;
	uint8_t init_value_len;
	uint8_t encoded_initial_feature[NRF_BLE_CGMS_FEATURE_LEN];

	init_value_len = encode_feature_location_type(encoded_initial_feature, &(cgms->feature));

	ble_uuid_t char_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_CGM_FEATURE,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
			.indicate = false, /* TODO: true? Or able to configure to true? */
		},
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = encoded_initial_feature,
		.init_len = init_value_len,
		.max_len = init_value_len,
	};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);

	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

	err = sd_ble_gatts_characteristic_add(cgms->service_handle, &char_md, &attr_char_value,
					      &cgms->char_handles.feature);
	if (err) {
		LOG_ERR("Failed to add GATT CGMS Feature characteristic, err %#x", err);
		return err;
	}

	return 0;
}

uint8_t encode_status(uint8_t *buf_out, struct nrf_ble_cgms *cgms)
{
	uint8_t len = 0;

	sys_put_le16(cgms->sensor_status.time_offset, &buf_out[len]);
	len += sizeof(uint16_t);

	buf_out[len++] = cgms->sensor_status.status.status;
	buf_out[len++] = cgms->sensor_status.status.calib_temp;
	buf_out[len++] = cgms->sensor_status.status.warning;

	return len;
}

/**
 * @brief Function for adding a status characteristic for the CGMS.
 *
 * @param[in] cgms Service instance.
 *
 * @return NRF_SUCCESS if characteristic was successfully added, otherwise an error code.
 */
static uint32_t status_char_add(struct nrf_ble_cgms *cgms)
{
	uint8_t init_value_len;
	uint8_t encoded_initial_status[NRF_BLE_CGMS_STATUS_LEN];

	init_value_len = encode_status(encoded_initial_status, cgms);

	int err;
	ble_uuid_t char_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_CGM_STATUS,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
		},
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.vlen = true,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = encoded_initial_status,
		.init_len = init_value_len,
		.max_len = init_value_len,
	};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

	/* Add Nordic UART Service characteristic RX declaration */
	err = sd_ble_gatts_characteristic_add(cgms->service_handle, &char_md, &attr_char_value,
					      &cgms->char_handles.status);
	if (err) {
		LOG_ERR("Failed to add GATT CGMS Status characteristic, err %#x", err);
		return err;
	}

	return 0;
}

/**
 * @brief Function for adding a characteristic for the Session Run Time.
 *
 * @param[in] cgms Service instance.
 *
 * @return NRF_SUCCESS if characteristic was successfully added, otherwise an error code.
 */
static uint32_t srt_char_add(struct nrf_ble_cgms *cgms)
{
	uint8_t len = 0;
	uint8_t encoded_initial_srt[NRF_BLE_CGMS_SRT_LEN];

	sys_put_le16(cgms->session_run_time, &(encoded_initial_srt[len]));
	len += sizeof(uint16_t);

	int err;
	ble_uuid_t char_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_CGM_SESSION_RUN_TIME,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
		},
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.vlen = true,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = encoded_initial_srt,
		.init_len = len,
		.max_len = NRF_BLE_CGMS_SRT_LEN,
	};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);

	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

	err = sd_ble_gatts_characteristic_add(cgms->service_handle, &char_md, &attr_char_value,
					      &cgms->char_handles.srt);
	if (err) {
		LOG_ERR("Failed to add GATT CGMS SRT characteristic, err %#x", err);
		return err;
	}

	return 0;
}

int nrf_ble_cgms_init(struct nrf_ble_cgms *cgms, const struct nrf_ble_cgms_config *cgms_init)
{
	if (!cgms || !cgms_init || !cgms_init->evt_handler || !cgms_init->gatt_queue) {
		return NRF_ERROR_NULL;
	}

	uint32_t err;
	ble_uuid_t ble_uuid;
	const uint8_t init_calib_val[] = {
		0x3E, 0x00, 0x07, 0x00,
		0x06, 0x07, 0x00, 0x00,
		0x00, 0x00
	};

	/* Initialize data base */
	err = cgms_db_init();
	if (err) {
		return err;
	}

	err = next_sequence_number_set();
	if (err) {
		return err;
	}



	/* Initialize service structure */
	cgms->evt_handler = cgms_init->evt_handler;
	cgms->gatt_queue = cgms_init->gatt_queue;
	cgms->feature = cgms_init->feature;
	cgms->sensor_status = cgms_init->initial_sensor_status;
	cgms->session_run_time = cgms_init->initial_run_time;
	cgms->is_session_started = false;
	cgms->nb_run_session = 0;
	cgms->conn_handle = BLE_CONN_HANDLE_INVALID;
	cgms->gatt_err_handler = gatt_error_handler;

	cgms->feature.feature = (NRF_BLE_CGMS_FEAT_MULTIPLE_BOND_SUPPORTED |
				 NRF_BLE_CGMS_FEAT_MULTIPLE_SESSIONS_SUPPORTED);
	cgms->feature.type = NRF_BLE_CGMS_MEAS_TYPE_VEN_BLOOD;
	cgms->feature.sample_location = NRF_BLE_CGMS_MEAS_LOC_AST;

	memcpy(cgms->calibration_val[0].value, init_calib_val, NRF_BLE_CGMS_MAX_CALIB_LEN);

	/* Add service */
	BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_CGM_SERVICE);

	err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
				       &ble_uuid, &cgms->service_handle);
	if (err) {
		return err;
	}

	/* Add glucose measurement characteristic */
	err = cgms_meas_char_add(cgms);
	if (err) {
		return err;
	}

	/* Add glucose measurement feature characteristic */
	err = glucose_feature_char_add(cgms);
	if (err) {
		return err;
	}

	/* Add glucose measurement status characteristic */
	err = status_char_add(cgms);
	if (err) {
		return err;
	}

	/* Add record control access point characteristic */
	err = cgms_racp_char_add(cgms);
	if (err) {
		return err;
	}

	/* Add Start Session Time characteristic */
	err = cgms_sst_char_add(cgms);
	if (err) {
		return err;
	}

	/* Add Session Run Time characteristic */
	err = srt_char_add(cgms);
	if (err) {
		return err;
	}

	/* Add Specific Operations Control Point characteristic */
	err = cgms_socp_char_add(cgms);
	if (err) {
		return err;
	}

	return 0;
}

/**
 * @brief Function for handling the WRITE event.
 *
 * @details Handles WRITE events from the BLE stack.
 *
 * @param[in] cgms Glucose Service structure.
 * @param[in] ble_evt Event received from the BLE stack.
 */
static void on_write(struct nrf_ble_cgms *cgms, ble_evt_t const *ble_evt)
{
	ble_gatts_evt_write_t const *evt_write = &ble_evt->evt.gatts_evt.params.write;

	cgms_meas_on_write(cgms, evt_write);
}

/**
 * @brief Function for handling the TX_COMPLETE event.
 *
 * @details Handles TX_COMPLETE events from the BLE stack.
 *
 * @param[in] cgms Glucose Service structure.
 * @param[in] ble_evt Event received from the BLE stack.
 */
static void on_tx_complete(struct nrf_ble_cgms *cgms, ble_evt_t const *ble_evt)
{
	cgms_racp_on_tx_complete(cgms);
}

static void on_rw_authorize_request(struct nrf_ble_cgms *cgms, ble_gatts_evt_t const *gatts_evt)
{
	ble_gatts_evt_rw_authorize_request_t const *auth_req =
		&gatts_evt->params.authorize_request;

	cgms_racp_on_rw_auth_req(cgms, auth_req);
	cgms_socp_on_rw_auth_req(cgms, auth_req);
	cgms_sst_on_rw_auth_req(cgms, auth_req);
}

void nrf_ble_cgms_on_ble_evt(ble_evt_t const *ble_evt, void *context)
{
	struct nrf_ble_cgms *cgms = (struct nrf_ble_cgms *)context;

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		cgms->conn_handle    = ble_evt->evt.gap_evt.conn_handle;
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		cgms->conn_handle = BLE_CONN_HANDLE_INVALID;
		break;

	case BLE_GATTS_EVT_WRITE:
		on_write(cgms, ble_evt);
		break;

	case BLE_GATTS_EVT_HVN_TX_COMPLETE:
		on_tx_complete(cgms, ble_evt);
		break;

	case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
		on_rw_authorize_request(cgms, &ble_evt->evt.gatts_evt);
		break;

	default:
		/* No implementation needed. */
		break;
	}
}

int nrf_ble_cgms_meas_create(struct nrf_ble_cgms *cgms, struct ble_cgms_rec *rec)
{
	uint32_t err;
	uint8_t nb_rec_to_send = 1;

	err = cgms_db_record_add(rec);
	if (err) {
		return err;
	}

	if ((cgms->conn_handle != BLE_CONN_HANDLE_INVALID) && (cgms->comm_interval != 0)) {
		err = cgms_meas_send(cgms, rec, &nb_rec_to_send);
	}

	return err;
}

int nrf_ble_cgms_update_status(struct nrf_ble_cgms *cgms, struct nrf_ble_cgm_status *status)
{
	uint8_t encoded_status[NRF_BLE_CGMS_STATUS_LEN];
	ble_gatts_value_t status_val;

	memset(&status_val, 0, sizeof(status_val));
	cgms->sensor_status = *status;
	status_val.len = encode_status(encoded_status, cgms);
	status_val.p_value = encoded_status;
	status_val.offset = 0;

	return sd_ble_gatts_value_set(cgms->conn_handle,
				      cgms->char_handles.status.value_handle,
				      &status_val);
}

int nrf_ble_cgms_conn_handle_assign(struct nrf_ble_cgms *cgms, uint16_t conn_handle)
{
	if (!cgms) {
		return NRF_ERROR_NULL;
	}

	cgms->conn_handle = conn_handle;

	return ble_gq_conn_handle_register(cgms->gatt_queue, conn_handle);
}

int nrf_ble_cgms_srt_set(struct nrf_ble_cgms *cgms, uint16_t run_time)
{
	ble_gatts_value_t srt_val;
	uint8_t encoded_session_run_time[NRF_BLE_CGMS_SRT_LEN];
	uint8_t gatts_value_set_len = sizeof(uint16_t);

	sys_put_le16(run_time, encoded_session_run_time);

	memset(&srt_val, 0, sizeof(ble_gatts_value_t));
	srt_val.len = gatts_value_set_len;
	srt_val.p_value = encoded_session_run_time;
	srt_val.offset = 0;

	return sd_ble_gatts_value_set(cgms->conn_handle,
				      cgms->char_handles.srt.value_handle,
				      &srt_val);
}
