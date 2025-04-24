/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdint.h>
#include <string.h>
#include <zephyr/sys/byteorder.h>
#include <ble.h>
#include <bluetooth/services/ble_cgms.h>
#include <bluetooth/services/uuid.h>
#include "cgms_meas.h"
#include "cgms_db.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ble_cgms, CONFIG_BLE_CGMS_LOG_LEVEL);

/**
 * @brief Function for encoding a Glucose measurement.
 *
 * @param[in] meas Measurement to be encoded.
 * @param[out] encoded_buffer Pointer to buffer where the encoded measurement is to be stored.
 *
 * @return Size of encoded measurement.
 */
static uint8_t cgms_meas_encode(struct nrf_ble_cgms *cgms,
				const struct nrf_ble_cgms_meas *meas,
				uint8_t *encoded_buffer)
{
	uint8_t len = 2;

	uint8_t flags = meas->flags;

	sys_put_le16(meas->glucose_concentration, &encoded_buffer[len]);
	len += sizeof(uint16_t);
	sys_put_le16(meas->time_offset, &encoded_buffer[len]);
	len += sizeof(uint16_t);

	if (meas->sensor_status_annunciation.warning != 0) {
		encoded_buffer[len++] = meas->sensor_status_annunciation.warning;
		flags |= NRF_BLE_CGMS_STATUS_FLAGS_WARNING_OCT_PRESENT;
	}

	if (meas->sensor_status_annunciation.calib_temp != 0) {
		encoded_buffer[len++] = meas->sensor_status_annunciation.calib_temp;
		flags |= NRF_BLE_CGMS_STATUS_FLAGS_CALTEMP_OCT_PRESENT;
	}

	if (meas->sensor_status_annunciation.status != 0) {
		encoded_buffer[len++] = meas->sensor_status_annunciation.status;
		flags |= NRF_BLE_CGMS_STATUS_FLAGS_STATUS_OCT_PRESENT;
	}

	/* Trend field */
	if (cgms->feature.feature & NRF_BLE_CGMS_FEAT_CGM_TREND_INFORMATION_SUPPORTED) {
		if (flags & NRF_BLE_CGMS_FLAG_TREND_INFO_PRESENT) {
			sys_put_le16(meas->trend, &encoded_buffer[len]);
			len += sizeof(uint16_t);
		}
	}

	/* Quality field */
	if (cgms->feature.feature & NRF_BLE_CGMS_FEAT_CGM_QUALITY_SUPPORTED) {
		if (flags & NRF_BLE_CGMS_FLAGS_QUALITY_PRESENT) {
			sys_put_le16(meas->quality, &encoded_buffer[len]);
			len += sizeof(uint16_t);
		}
	}

	encoded_buffer[1] = flags;
	encoded_buffer[0] = len;
	return len;
}

/**
 * @brief Function for adding a characteristic for the Continuous Glucose Meter Measurement.
 *
 * @param[in] cgms Service instance.
 *
 * @return NRF_SUCCESS if characteristic was successfully added, otherwise an error code.
 */
int cgms_meas_char_add(struct nrf_ble_cgms *cgms)
{
	int err;
	uint8_t num_recs;
	uint8_t encoded_cgms_meas[NRF_BLE_CGMS_MEAS_LEN_MAX];
	struct ble_cgms_rec initial_cgms_rec_value;

	memset(&initial_cgms_rec_value, 0, sizeof(struct ble_cgms_rec));

	num_recs = cgms_db_num_records_get();
	if (num_recs > 0) {
		err = cgms_db_record_get(&initial_cgms_rec_value, num_recs - 1);
		if (err != NRF_SUCCESS) {
			return err;
		}
	}

	uint8_t init_len = cgms_meas_encode(cgms, &initial_cgms_rec_value.meas, encoded_cgms_meas);

	ble_uuid_t char_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_CGM_MEASUREMENT,
	};
	ble_gatts_attr_md_t cccd_md = {
		.vloc = BLE_GATTS_VLOC_STACK
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.notify = true,
			.read = true, /* TODO remove? */
		},
		.p_cccd_md = &cccd_md,
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.vlen = true,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = encoded_cgms_meas,
		.init_len = init_len,
		.max_len = NRF_BLE_CGMS_MEAS_LEN_MAX,
	};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm); /* Remove? */

	/* Setup CCCD */
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

	err = sd_ble_gatts_characteristic_add(cgms->service_handle, &char_md, &attr_char_value,
					      &cgms->char_handles.measurment);
	if (err) {
		LOG_ERR("Failed to add GATT CGMS MEAS characteristic, nrf_error %#x", err);
		return err;
	}

	return 0;
}

int cgms_meas_send(struct nrf_ble_cgms *cgms, struct ble_cgms_rec *rec, uint8_t *count)
{

	uint32_t err;
	uint8_t encoded_meas[NRF_BLE_CGMS_MEAS_LEN_MAX + NRF_BLE_CGMS_MEAS_REC_LEN_MAX];
	uint16_t len = 0;
	uint16_t hvx_len = NRF_BLE_CGMS_MEAS_LEN_MAX;
	ble_gatts_hvx_params_t hvx_params;
	uint8_t meas_len;
	int i;

	for (i = 0; i < *count; i++) {
		meas_len = cgms_meas_encode(cgms, &(rec[i].meas), (encoded_meas + len));
		if (len + meas_len >= NRF_BLE_CGMS_MEAS_LEN_MAX) {
			break;
		}

		len += meas_len;
	}

	*count = i;
	hvx_len  = len;

	memset(&hvx_params, 0, sizeof(hvx_params));

	hvx_params.handle = cgms->char_handles.measurment.value_handle;
	hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
	hvx_params.offset = 0;
	hvx_params.p_len = &hvx_len;
	hvx_params.p_data = encoded_meas;

	err = sd_ble_gatts_hvx(cgms->conn_handle, &hvx_params);
	if (err == NRF_SUCCESS) {
		if (hvx_len != len) {
			err = NRF_ERROR_DATA_SIZE;
		} else {
			/* Measurement successfully sent */
			cgms->racp_data.racp_proc_records_reported += *count;
		}
	}

	return err;
}

/**
 * @brief Function for handling the Glucose measurement CCCD write event.
 *
 * @param[in] cgms Service instance.
 * @param[in] evt_write WRITE event to be handled.
 */
static void on_meas_cccd_write(struct nrf_ble_cgms *cgms, const ble_gatts_evt_write_t *evt_write)
{
	if (evt_write->len != 2) {
		return;
	}

	/* CCCD written, update notification state */
	if (cgms->evt_handler != NULL) {
		struct nrf_ble_cgms_evt evt;

		if (sys_get_le16(evt_write->data) & BLE_GATT_HVX_NOTIFICATION) {
			evt.evt_type = NRF_BLE_CGMS_EVT_NOTIFICATION_ENABLED;
		} else {
			evt.evt_type = NRF_BLE_CGMS_EVT_NOTIFICATION_DISABLED;
		}

		cgms->evt_handler(cgms, &evt);
	}

}

/**
 * @brief Function for handling the WRITE event.
 *
 * @details Handles WRITE events from the BLE stack.
 *
 * @param[in] cgms Glucose Service structure.
 * @param[in] evt_write Event received from the BLE stack.
 */
void cgms_meas_on_write(struct nrf_ble_cgms *cgms, const ble_gatts_evt_write_t *evt_write)
{

	if (evt_write->handle == cgms->char_handles.measurment.cccd_handle) {
		on_meas_cccd_write(cgms, evt_write);
	}
}
