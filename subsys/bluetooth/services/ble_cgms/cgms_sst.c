/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <nrf_error.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <ble.h>

#include <bm/bluetooth/services/ble_cgms.h>
#include <bm/bluetooth/services/uuid.h>
#include "cgms_sst.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ble_cgms, CONFIG_BLE_CGMS_LOG_LEVEL);

#define CRC_LEN 2

static uint32_t sst_decode(struct ble_cgms_sst *sst, const uint8_t *data, const uint16_t len)
{
	uint32_t index;

	/* Allow both with and without CRC */
	if (len != BLE_CGMS_SST_LEN && (len != BLE_CGMS_SST_LEN - CRC_LEN)) {
		return NRF_ERROR_DATA_SIZE;
	}

	index = ble_date_time_decode(&sst->date_time, data);

	sst->time_zone = data[index++];
	sst->dst = data[index++];

	return NRF_SUCCESS;
}

static void convert_ble_time_c_time(struct ble_cgms_sst *sst, struct tm *tm)
{
	tm->tm_sec = sst->date_time.seconds;
	tm->tm_min = sst->date_time.minutes;
	tm->tm_hour = sst->date_time.hours;
	tm->tm_mday = sst->date_time.day;
	tm->tm_mon = sst->date_time.month;
	tm->tm_year = sst->date_time.year - 1900;

	/* Ignore daylight saving for this conversion. */
	tm->tm_isdst = 0;
}

static void calc_sst(uint16_t offset, struct tm *tm)
{
	/* Time in seconds. */
	time_t time;

	struct tm ltm = { 0 };

	time  = mktime(tm);
	time  = time - (offset * 60);
	*tm = *(gmtime_r(&time, &ltm));

	if (tm->tm_isdst == 1) {
		/* Daylight saving time is not used and must be removed. */
		tm->tm_hour = tm->tm_hour - 1;
		tm->tm_isdst = 0;
	}
}

static void convert_c_time_ble_time(struct ble_cgms_sst *sst, struct tm *tm)
{
	sst->date_time.seconds = tm->tm_sec;
	sst->date_time.minutes = tm->tm_min;
	sst->date_time.hours = tm->tm_hour;
	sst->date_time.day = tm->tm_mday;
	sst->date_time.month = tm->tm_mon;
	sst->date_time.year = tm->tm_year + 1900;
}

static uint8_t sst_encode(struct ble_cgms_sst *sst, uint8_t *sst_encoded)
{
	uint8_t len;

	len = ble_date_time_encode(&sst->date_time, sst_encoded);

	sst_encoded[len++] = sst->time_zone;
	sst_encoded[len++] = sst->dst;

	return len;
}

static uint32_t cgm_update_sst(struct ble_cgms *cgms, const ble_gatts_evt_write_t *evt_write)
{
	uint32_t nrf_err;
	struct ble_cgms_sst sst = {0};
	struct tm c_time_and_date;

	nrf_err = sst_decode(&sst, evt_write->data, evt_write->len);
	if (nrf_err) {
		/* Silently drop the update */
		return NRF_SUCCESS;
	}
	convert_ble_time_c_time(&sst, &c_time_and_date);
	calc_sst(cgms->sensor_status.time_offset, &c_time_and_date);
	convert_c_time_ble_time(&sst, &c_time_and_date);

	return cgms_sst_set(cgms, &sst);
}

/* Glucose session start time write event handler. */
static void on_sst_value_write(struct ble_cgms *cgms, const ble_gatts_evt_write_t *evt_write)
{
	uint32_t nrf_err;
	ble_gatts_rw_authorize_reply_params_t auth_reply = {
		.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE,
		.params = {
			.write = {
				.gatt_status = BLE_GATT_STATUS_SUCCESS,
				.update = 1,
			},
		},
	};
	struct ble_cgms_evt cgms_evt = {
		.evt_type = BLE_CGMS_EVT_ERROR,
		.conn_handle = cgms->conn_handle,
	};

	nrf_err = sd_ble_gatts_rw_authorize_reply(cgms->conn_handle, &auth_reply);
	if (nrf_err) {
		if (cgms->evt_handler != NULL) {
			cgms_evt.error.reason = nrf_err;
			cgms->evt_handler(cgms, &cgms_evt);
		}
	}

	nrf_err = cgm_update_sst(cgms, evt_write);
	if (nrf_err) {
		LOG_ERR("Failed to update SST, nrf_error %#x", nrf_err);
		if (cgms->evt_handler != NULL) {
			cgms_evt.error.reason = nrf_err;
			cgms->evt_handler(cgms, &cgms_evt);
		}
	}
}

/* Add the Session Start Time characteristic. */
uint32_t cgms_sst_char_add(struct ble_cgms *cgms, const struct ble_cgms_config *cgms_cfg)
{
	uint8_t init_value[BLE_CGMS_SST_LEN] = {0};

	ble_uuid_t char_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_CGM_SESSION_START_TIME,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.write = true,
			.read = true,
		},
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.wr_auth = true,
		.read_perm = cgms_cfg->sec_mode.sst_char.read,
		.write_perm = cgms_cfg->sec_mode.sst_char.write,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = init_value,
		.init_len = (BLE_CGMS_SST_LEN - BLE_CGMS_CRC_LEN),
		.max_len = BLE_CGMS_SST_LEN,
	};

	return sd_ble_gatts_characteristic_add(cgms->service_handle, &char_md, &attr_char_value,
					       &cgms->char_handles.sst);
}

uint32_t cgms_sst_set(struct ble_cgms *cgms, struct ble_cgms_sst *sst)
{
	const uint16_t value_handle = cgms->char_handles.sst.value_handle;
	uint8_t encoded_start_session_time[BLE_CGMS_SST_LEN];
	ble_gatts_value_t sst_val = {
		.offset = 0,
		.p_value = encoded_start_session_time,
		.len = sst_encode(sst, encoded_start_session_time),
	};

	return sd_ble_gatts_value_set(cgms->conn_handle, value_handle, &sst_val);
}

void cgms_sst_on_rw_auth_req(struct ble_cgms *cgms,
			     const ble_gatts_evt_rw_authorize_request_t *auth_req)
{
	if (auth_req->request.write.handle == cgms->char_handles.sst.value_handle) {
		on_sst_value_write(cgms, &auth_req->request.write);
	}
}
