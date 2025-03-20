/*
 * Copyright (c) 2012 - 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ble_conn_params.h>
#include <bluetooth/services/ble_hrs.h>
#include <bluetooth/services/common.h>
#include <bluetooth/services/uuid.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_hrs, CONFIG_BLE_HRS_LOG_LEVEL);

/* Length of ATT header. Opcode (1 byte) and attribute handle (2 bytes). */
#define ATT_HEADER_LENGTH 3
/* Macro for calculating max ATT data/payload size from max ATT GATT MTU size. */
#define MAX_HRM_LEN_CALC(max_mtu_size) ((max_mtu_size) - ATT_HEADER_LENGTH)

/* Initial Heart Rate Measurement value. */
#define INITIAL_VALUE_HRM 0

/* Heart Rate Measurement flag bits. */

/* Heart Rate Value Format bit. */
#define HRM_FLAG_MASK_HR_VALUE_16BIT		BIT(0)
/* Sensor Contact Detected bit. */
#define HRM_FLAG_MASK_SENSOR_CONTACT_DETECTED	BIT(1)
/* Sensor Contact Supported bit. */
#define HRM_FLAG_MASK_SENSOR_CONTACT_SUPPORTED	BIT(2)
/* Energy Expended Status bit. Feature Not Supported. */
#define HRM_FLAG_MASK_EXPENDED_ENERGY_INCLUDED	BIT(3)
/* RR-Interval bit. */
#define HRM_FLAG_MASK_RR_INTERVAL_INCLUDED	BIT(4)

static uint8_t hrm_encode(struct ble_hrs *hrs, uint16_t heart_rate, uint8_t *encoded_buffer)
{
	uint8_t flags = 0;
	/* Make space for flags. */
	uint8_t len = 1;
	int i;

	/* Set sensor contact related flags. */
	if (hrs->is_sensor_contact_supported) {
		flags |= HRM_FLAG_MASK_SENSOR_CONTACT_SUPPORTED;
	}
	if (hrs->is_sensor_contact_detected) {
		flags |= HRM_FLAG_MASK_SENSOR_CONTACT_DETECTED;
	}

	/* Encode heart rate measurement. */
	encoded_buffer[len++] = (uint8_t)heart_rate;
	if (heart_rate > UINT8_MAX) {
		flags |= HRM_FLAG_MASK_HR_VALUE_16BIT;
		encoded_buffer[len++] = (uint8_t)(heart_rate >> 8);
	}

	/* Encode rr_interval values. */
	if (hrs->rr_interval_count > 0) {
		flags |= HRM_FLAG_MASK_RR_INTERVAL_INCLUDED;
	}
	for (i = 0; i < hrs->rr_interval_count; i++) {
		if (len + sizeof(uint16_t) > hrs->max_hrm_len) {
			/* Not all stored rr_interval values can fit into the encoded hrm,
			 * move the remaining values to the start of the buffer.
			 */
			memmove(&hrs->rr_interval[0], &hrs->rr_interval[1],
				(hrs->rr_interval_count - i) * sizeof(uint16_t));
			break;
		}
		encoded_buffer[len++] = (uint8_t)(hrs->rr_interval[i]);
		encoded_buffer[len++] = (uint8_t)(hrs->rr_interval[i] >> 8);
	}
	hrs->rr_interval_count -= i;

	/* Add flags. */
	encoded_buffer[0] = flags;

	return len;
}

static int heart_rate_measurement_char_add(struct ble_hrs *hrs, const struct ble_hrs_config *cfg)
{
	int err;
	uint8_t encoded_initial_hrm[MAX_HRM_LEN_CALC(CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE)];
	ble_uuid_t char_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_HEART_RATE_MEASUREMENT_CHAR,
	};
	ble_gatts_attr_md_t cccd_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.write_perm = cfg->hrm_cccd_wr_sec,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.notify = true,
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
		.p_value = encoded_initial_hrm,
		.init_len = hrm_encode(hrs, INITIAL_VALUE_HRM, encoded_initial_hrm),
		.max_len = sizeof(encoded_initial_hrm),
	};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);

	/* Add Heart rate measurement characteristic declaration, value, and CCCD attributes. */
	err = sd_ble_gatts_characteristic_add(hrs->service_handle, &char_md, &attr_char_value,
					      &hrs->hrm_handles);
	if (err) {
		LOG_ERR("Failed to add GATT characteristic, nrf_error %#x", err);
		return err;
	}

	return 0;
}

static int body_sensor_location_char_add(struct ble_hrs *hrs, const struct ble_hrs_config *cfg)
{
	int err;
	ble_uuid_t char_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_BODY_SENSOR_LOCATION_CHAR,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
		},
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.read_perm = cfg->bsl_rd_sec,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = cfg->body_sensor_location,
		.init_len = sizeof(uint8_t),
		.max_len = sizeof(uint8_t),
	};

	/* Add Body sensor location characteristic declaration and value attributes. */
	err = sd_ble_gatts_characteristic_add(hrs->service_handle, &char_md, &attr_char_value,
					      &hrs->bsl_handles);
	if (err) {
		LOG_ERR("Failed to add GATT characteristic, nrf_error %#x", err);
		return err;
	}

	return 0;
}

static void on_connect(struct ble_hrs *hrs, const ble_gap_evt_t *gap_evt)
{
	hrs->conn_handle = gap_evt->conn_handle;
}

static void on_disconnect(struct ble_hrs *hrs, const ble_gap_evt_t *gap_evt)
{
	ARG_UNUSED(gap_evt);
	hrs->conn_handle = BLE_CONN_HANDLE_INVALID;
}

static void on_write(struct ble_hrs *hrs, const ble_gatts_evt_t *gatts_evt)
{
	struct ble_hrs_evt hrs_evt;

	if (!hrs->evt_handler) {
		return;
	}

	if ((gatts_evt->params.write.handle != hrs->hrm_handles.cccd_handle) ||
	    (gatts_evt->params.write.len != 2)) {
		/* Nothing to do */
		return;
	}

	if (is_notification_enabled(gatts_evt->params.write.data)) {
		hrs_evt.evt_type = BLE_HRS_EVT_NOTIFICATION_ENABLED;
	} else {
		hrs_evt.evt_type = BLE_HRS_EVT_NOTIFICATION_DISABLED;
	}

	LOG_INF("Heart rate measurement notifications %sabled for peer %#x",
		(hrs_evt.evt_type == BLE_HRS_EVT_NOTIFICATION_ENABLED ? "en" : "dis"),
		gatts_evt->conn_handle);

	hrs->evt_handler(hrs, &hrs_evt);
}

void ble_hrs_on_ble_evt(const ble_evt_t *ble_evt, void *hrs_instance)
{
	__ASSERT(ble_evt, "BLE event is NULL");
	__ASSERT(hrs_instance, "BLE instance is NULL");

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		on_connect(hrs_instance, &ble_evt->evt.gap_evt);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnect(hrs_instance, &ble_evt->evt.gap_evt);
		break;

	case BLE_GATTS_EVT_WRITE:
		on_write(hrs_instance, &ble_evt->evt.gatts_evt);
		break;

	default:
		break;
	}
}

int ble_hrs_init(struct ble_hrs *hrs, const struct ble_hrs_config *cfg)
{
	int err;
	ble_uuid_t ble_uuid;

	if (hrs == NULL || cfg == NULL) {
		return -EFAULT;
	}

	/* Initialize service structure. */
	hrs->evt_handler = cfg->evt_handler;
	hrs->conn_handle = BLE_CONN_HANDLE_INVALID;
	hrs->rr_interval_count = 0;
	hrs->max_hrm_len = MAX_HRM_LEN_CALC(BLE_GATT_ATT_MTU_DEFAULT);
	hrs->is_sensor_contact_supported = cfg->is_sensor_contact_supported;
	hrs->is_sensor_contact_detected = false;

	BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_HEART_RATE_SERVICE);

	/* Add Heart rate service declaration. */
	err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid,
				       &hrs->service_handle);
	if (err) {
		LOG_ERR("Failed to add heart rate service, nrf_error %#x", err);
		return -EINVAL;
	}

	/* Add Heart rate measurement characteristic. */
	err = heart_rate_measurement_char_add(hrs, cfg);
	if (err) {
		return -EINVAL;
	}

	/* Add Body sensor location characteristic. */
	err = body_sensor_location_char_add(hrs, cfg);
	if (err) {
		return -EINVAL;
	}

	return 0;
}

int ble_hrs_heart_rate_measurement_send(struct ble_hrs *hrs, uint16_t heart_rate)
{
	int err;
	uint8_t encoded_hrm[MAX_HRM_LEN_CALC(CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE)];
	uint16_t len;
	uint16_t hvx_len;

	if (!hrs) {
		return -EFAULT;
	}

	LOG_INF("Heart rate: %d bpm", heart_rate);

	/* Prepare heart rate measurement notification data */
	len = hrm_encode(hrs, heart_rate, encoded_hrm);
	hvx_len = len;

	/* Notify */
	ble_gatts_hvx_params_t hvx = {
		.handle = hrs->hrm_handles.value_handle,
		.type = BLE_GATT_HVX_NOTIFICATION,
		.offset = 0,
		.p_len = &hvx_len,
		.p_data = encoded_hrm,
	};

	err = sd_ble_gatts_hvx(hrs->conn_handle, &hvx);
	switch (err) {
	case NRF_SUCCESS:
		if (hvx_len != len) {
			LOG_ERR("Notified %d of %d bytes\n", hvx_len, len);
			return -EINVAL;
		}
		return 0;
	case BLE_ERROR_INVALID_CONN_HANDLE:
		return -ENOTCONN;
	case NRF_ERROR_INVALID_STATE:
		return -EPIPE;
	default:
		LOG_ERR("Failed to notify heart rate measurement, nrf_error %#x", err);
		return -EINVAL;
	}
}

int ble_hrs_rr_interval_add(struct ble_hrs *hrs, uint16_t rr_interval)
{
	if (!hrs) {
		return -EFAULT;
	}

	if (hrs->rr_interval_count == CONFIG_BLE_HRS_MAX_BUFFERED_RR_INTERVALS) {
		memmove(&hrs->rr_interval[0], &hrs->rr_interval[1],
			(CONFIG_BLE_HRS_MAX_BUFFERED_RR_INTERVALS - 1) * sizeof(uint16_t));
		hrs->rr_interval_count--;
	}

	hrs->rr_interval[hrs->rr_interval_count++] = rr_interval;
	return 0;
}

bool ble_hrs_rr_interval_buffer_is_full(struct ble_hrs *hrs)
{
	__ASSERT(hrs, "hrs is NULL");

	return (hrs->rr_interval_count == CONFIG_BLE_HRS_MAX_BUFFERED_RR_INTERVALS);
}

int ble_hrs_sensor_contact_supported_set(struct ble_hrs *hrs, bool is_sensor_contact_supported)
{
	if (!hrs) {
		return -EFAULT;
	}

	/* Check if we are connected to peer. */
	if (hrs->conn_handle != BLE_CONN_HANDLE_INVALID) {
		return -EISCONN;
	}

	hrs->is_sensor_contact_supported = is_sensor_contact_supported;
	return 0;
}

int ble_hrs_sensor_contact_detected_update(struct ble_hrs *hrs, bool is_sensor_contact_detected)
{
	if (!hrs) {
		return -EFAULT;
	}

	hrs->is_sensor_contact_detected = is_sensor_contact_detected;
	return 0;
}

int ble_hrs_body_sensor_location_set(struct ble_hrs *hrs, uint8_t body_sensor_location)
{
	int err;
	ble_gatts_value_t gatts_value = {
		.len = sizeof(uint8_t),
		.offset = 0,
		.p_value = &body_sensor_location,
	};

	if (!hrs) {
		return -EFAULT;
	}

	err = sd_ble_gatts_value_set(hrs->conn_handle, hrs->bsl_handles.value_handle, &gatts_value);
	if (err) {
		LOG_ERR("Failed to update body sensor location, nrf_error %#x", err);
		return -EINVAL;
	}

	return 0;
}

void ble_hrs_conn_params_evt(struct ble_hrs *hrs, const struct ble_conn_params_evt *conn_params_evt)
{
	__ASSERT(hrs, "hrs is NULL");
	__ASSERT(conn_params_evt, "GATT event is NULL");

	if ((hrs->conn_handle == conn_params_evt->conn_handle)
	    && (conn_params_evt->id == BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED)) {
		hrs->max_hrm_len = MAX_HRM_LEN_CALC(conn_params_evt->att_mtu);
	}
}
