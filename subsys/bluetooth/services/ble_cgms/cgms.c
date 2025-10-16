/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <stdint.h>

#include <bm/bluetooth/ble_racp.h>
#include <bm/bluetooth/services/ble_date_time.h>
#include <bm/bluetooth/services/ble_cgms.h>
#include <bm/bluetooth/services/uuid.h>
#include "cgms_db.h"
#include "cgms_meas.h"
#include "cgms_racp.h"
#include "cgms_socp.h"
#include "cgms_sst.h"

#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_cgms, CONFIG_BLE_CGMS_LOG_LEVEL);

/* GATT errors and nrf_ble_gq errors event handler. */
static void gatt_error_handler(uint16_t conn_handle, uint32_t nrf_error, void *ctx)
{
	struct ble_cgms_evt evt = {
		.evt_type = BLE_CGMS_EVT_ERROR,
		.error.reason = nrf_error,
	};
	struct ble_cgms *cgms = (struct ble_cgms *)ctx;

	if (cgms->evt_handler && (nrf_error != NRF_ERROR_INVALID_STATE)) {
		cgms->evt_handler(cgms, &evt);
	}
}

static uint8_t encode_feature_location_type(uint8_t *buf_out, struct ble_cgms_feature *feature)
{
	uint8_t len = 0;

	sys_put_le24(feature->feature, &buf_out[len]);
	len += 3;
	buf_out[len++] = (feature->sample_location << 4) | (feature->type & 0x0F);
	sys_put_le16(0xFFFF, &buf_out[len]);
	len += sizeof(uint16_t);

	return len;
}

/* Add the glucose feature characteristic. */
static uint32_t feature_char_add(struct ble_cgms *cgms)
{
	uint8_t init_value_len;
	uint8_t encoded_initial_feature[BLE_CGMS_FEATURE_LEN];

	init_value_len = encode_feature_location_type(encoded_initial_feature, &(cgms->feature));

	ble_uuid_t char_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_CGM_FEATURE,
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
		.p_value = encoded_initial_feature,
		.init_len = init_value_len,
		.max_len = init_value_len,
	};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

	return sd_ble_gatts_characteristic_add(cgms->service_handle, &char_md, &attr_char_value,
					       &cgms->char_handles.feature);
}

static uint8_t encode_status(uint8_t *buf_out, struct ble_cgms *cgms)
{
	uint8_t len = 0;

	sys_put_le16(cgms->sensor_status.time_offset, &buf_out[len]);
	len += sizeof(uint16_t);

	buf_out[len++] = cgms->sensor_status.status.status;
	buf_out[len++] = cgms->sensor_status.status.calib_temp;
	buf_out[len++] = cgms->sensor_status.status.warning;

	return len;
}

/* Add the CGMS status characteristic. */
static uint32_t status_char_add(struct ble_cgms *cgms)
{
	uint8_t init_value_len;
	uint8_t encoded_initial_status[BLE_CGMS_STATUS_LEN];

	init_value_len = encode_status(encoded_initial_status, cgms);

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

	/* Add Nordic UART Service characteristic RX declaration. */
	return sd_ble_gatts_characteristic_add(cgms->service_handle, &char_md, &attr_char_value,
					      &cgms->char_handles.status);
}

/* Add the Session Run Time characteristic. */
static uint32_t srt_char_add(struct ble_cgms *cgms)
{
	uint8_t len = 0;
	uint8_t encoded_initial_srt[BLE_CGMS_SRT_LEN];

	sys_put_le16(cgms->session_run_time, &(encoded_initial_srt[len]));
	len += sizeof(uint16_t);

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
		.max_len = BLE_CGMS_SRT_LEN,
	};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);

	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

	return sd_ble_gatts_characteristic_add(cgms->service_handle, &char_md, &attr_char_value,
					       &cgms->char_handles.srt);
}

uint32_t ble_cgms_init(struct ble_cgms *cgms, const struct ble_cgms_config *cgms_init)
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

	/* Initialize data base. */
	err = cgms_db_init();
	if (err != NRF_SUCCESS) {
		return err;
	}

	/* Initialize service structure. */
	cgms->evt_handler = cgms_init->evt_handler;
	cgms->gatt_queue = cgms_init->gatt_queue;
	cgms->feature = cgms_init->feature;
	cgms->sensor_status = cgms_init->initial_sensor_status;
	cgms->session_run_time = cgms_init->initial_run_time;
	cgms->is_session_started = false;
	cgms->nb_run_session = 0;
	cgms->conn_handle = BLE_CONN_HANDLE_INVALID;
	cgms->gatt_err_handler = gatt_error_handler;

	memcpy(cgms->calibration_val[0].value, init_calib_val, BLE_CGMS_MAX_CALIB_LEN);

	/* Add service. */
	BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_CGM_SERVICE);

	err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
				       &ble_uuid, &cgms->service_handle);
	if (err != NRF_SUCCESS) {
		return err;
	}

	/* Add CGM Measurement characteristic. */
	err = cgms_meas_char_add(cgms);
	if (err != NRF_SUCCESS) {
		LOG_ERR("Failed to add CGMS measurement characteristic, nrf_error %#x", err);
		return err;
	}

	/* Add CGM Feature characteristic. */
	err = feature_char_add(cgms);
	if (err != NRF_SUCCESS) {
		LOG_ERR("Failed to add CGMS feature characteristic, nrf_error %#x", err);
		return err;
	}

	/* Add CGM Status characteristic. */
	err = status_char_add(cgms);
	if (err != NRF_SUCCESS) {
		LOG_ERR("Failed to add CGMS status characteristic, nrf_error %#x", err);
		return err;
	}

	/* Add CGM Session Start Time characteristic. */
	err = cgms_sst_char_add(cgms);
	if (err != NRF_SUCCESS) {
		LOG_ERR("Failed to add CGMS SST characteristic, nrf_error %#x", err);
		return err;
	}

	/* Add CGM Session Run Time characteristic. */
	err = srt_char_add(cgms);
	if (err != NRF_SUCCESS) {
		LOG_ERR("Failed to add CGMS SRT characteristic, nrf_error %#x", err);
		return err;
	}

	/* Add CGM Record Access Control Point characteristic. */
	err = cgms_racp_char_add(cgms);
	if (err != NRF_SUCCESS) {
		LOG_ERR("Failed to add CGMS RACP characteristic, nrf_error %#x", err);
		return err;
	}

	/* Add CGM Specific Ops Control Point characteristic. */
	err = cgms_socp_char_add(cgms);
	if (err != NRF_SUCCESS) {
		LOG_ERR("Failed to add CGMS SOCP characteristic, nrf_error %#x", err);
		return err;
	}

	return 0;
}

static void on_write(struct ble_cgms *cgms, const ble_evt_t *ble_evt)
{
	const ble_gatts_evt_write_t *evt_write = &ble_evt->evt.gatts_evt.params.write;

	cgms_meas_on_write(cgms, evt_write);
}

static void on_tx_complete(struct ble_cgms *cgms, const ble_evt_t *ble_evt)
{
	cgms_racp_on_tx_complete(cgms);
}

static void on_rw_authorize_request(struct ble_cgms *cgms, const ble_gatts_evt_t *gatts_evt)
{
	const ble_gatts_evt_rw_authorize_request_t *auth_req =
		&gatts_evt->params.authorize_request;

	if (auth_req->type != BLE_GATTS_AUTHORIZE_TYPE_WRITE) {
		return;
	}

	if (auth_req->request.write.handle == cgms->char_handles.racp.value_handle) {
		cgms_racp_on_rw_auth_req(cgms, auth_req);
	} else if (auth_req->request.write.handle == cgms->char_handles.socp.value_handle) {
		cgms_socp_on_rw_auth_req(cgms, auth_req);
	} else if (auth_req->request.write.handle == cgms->char_handles.sst.value_handle) {
		cgms_sst_on_rw_auth_req(cgms, auth_req);
	}
}

void ble_cgms_on_ble_evt(const ble_evt_t *ble_evt, void *context)
{
	struct ble_cgms *cgms = (struct ble_cgms *)context;

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		cgms->conn_handle = ble_evt->evt.gap_evt.conn_handle;
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

uint32_t ble_cgms_meas_create(struct ble_cgms *cgms, struct ble_cgms_rec *rec)
{
	uint32_t err;
	uint16_t nb_rec_to_send = 1;

	err = cgms_db_record_add(rec);
	if (err != NRF_SUCCESS) {
		return err;
	}

	if ((cgms->conn_handle != BLE_CONN_HANDLE_INVALID) && (cgms->comm_interval != 0)) {
		err = cgms_meas_send(cgms, rec, &nb_rec_to_send);
	}

	return err;
}

uint32_t ble_cgms_update_status(struct ble_cgms *cgms, struct ble_cgms_status *status)
{
	uint8_t encoded_status[BLE_CGMS_STATUS_LEN];
	ble_gatts_value_t status_val = {
		.offset = 0,
		.p_value = encoded_status,
	};

	cgms->sensor_status = *status;
	status_val.len = encode_status(encoded_status, cgms);

	return sd_ble_gatts_value_set(cgms->conn_handle, cgms->char_handles.status.value_handle,
				      &status_val);
}

uint32_t ble_cgms_conn_handle_assign(struct ble_cgms *cgms, uint16_t conn_handle)
{
	if (!cgms) {
		return NRF_ERROR_NULL;
	}

	cgms->conn_handle = conn_handle;

	return ble_gq_conn_handle_register(cgms->gatt_queue, conn_handle);
}

uint32_t ble_cgms_srt_set(struct ble_cgms *cgms, uint16_t run_time)
{
	uint8_t encoded_session_run_time[BLE_CGMS_SRT_LEN];
	ble_gatts_value_t srt_val = {
		.len = sizeof(uint16_t),
		.offset = 0,
		.p_value = encoded_session_run_time,
	};

	sys_put_le16(run_time, encoded_session_run_time);

	return sd_ble_gatts_value_set(cgms->conn_handle, cgms->char_handles.srt.value_handle,
				      &srt_val);
}
