/*
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm/bluetooth/services/ble_hrs_central.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/services/uuid.h>
#include <bm/bluetooth/ble_gatt_db.h>
#include <ble_types.h>
#include <ble_gattc.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_hrs_central, CONFIG_BLE_HRS_CENTRAL_LOG_LEVEL);

/* Bit mask used to extract the type of heart rate value. This is used to
 * find if the received heart rate is a 16 bit value or an 8 bit value.
 */
#define HRM_FLAG_MASK_HR_16BIT (0x01 << 0)

/* Bit mask used to extract the presence of RR_INTERVALS. This is used to
 * find if the received measurement includes RR_INTERVALS.
 */
#define HRM_FLAG_MASK_HR_RR_INT (0x01 << 4)

static void gatt_error_handler(const struct ble_gq_req *req, struct ble_gq_evt *gq_evt)
{
	struct ble_hrs_central *ble_hrs_central = (struct ble_hrs_central *)req->ctx;
	struct ble_hrs_central_evt evt = {
		.evt_type = BLE_HRS_CENTRAL_EVT_ERROR,
		.conn_handle = gq_evt->conn_handle,
		.params.error.reason = gq_evt->error.reason
	};

	LOG_DBG("A GATT Client error has occurred on conn_handle 0X%X, nrf_error %#x",
		gq_evt->conn_handle, gq_evt->error.reason);

	if (ble_hrs_central->evt_handler) {
		ble_hrs_central->evt_handler(ble_hrs_central, &evt);
	}
}

static void on_hvx(struct ble_hrs_central *ble_hrs_central, const ble_evt_t *ble_evt)
{
	uint32_t index = 0;
	const ble_gattc_evt_hvx_t *hvx = &ble_evt->evt.gattc_evt.params.hvx;
	struct ble_hrs_central_evt ble_hrs_central_evt = {
		.evt_type = BLE_HRS_CENTRAL_EVT_HRM_NOTIFICATION,
		.conn_handle = ble_hrs_central->conn_handle,
		.params.hrm.rr_intervals_cnt = 0,
	};

	/* Check if the event is on the link for this instance. */
	if (ble_hrs_central->conn_handle != ble_evt->evt.gattc_evt.conn_handle) {
		LOG_DBG("Received HVX on link 0x%x, not associated to this instance. Ignore.",
			ble_evt->evt.gattc_evt.conn_handle);
		return;
	}

	/* Check if this is a heart rate notification. */
	if (hvx->handle != ble_hrs_central->peer_hrs_db.hrm_handle) {
		return;
	}

	LOG_DBG("Received HVX on link 0x%x, hrm_handle 0x%x",
		hvx->handle, ble_hrs_central->peer_hrs_db.hrm_handle);

	if (!(hvx->data[index++] & HRM_FLAG_MASK_HR_16BIT)) {
		/* 8-bit heart rate value received. */
		ble_hrs_central_evt.params.hrm.hr_value =
			hvx->data[index++];
	} else {
		/* 16-bit heart rate value received. */
		ble_hrs_central_evt.params.hrm.hr_value =
			(((uint16_t)((uint8_t *)(&(hvx->data[index])))[0])) |
			(((uint16_t)((uint8_t *)(&(hvx->data[index])))[1]) << 8);
		index += sizeof(uint16_t);
	}

	if ((hvx->data[0] & HRM_FLAG_MASK_HR_RR_INT)) {
		ble_hrs_central_evt.params.hrm.rr_intervals_cnt =
			CONFIG_BLE_HRS_CENTRAL_RR_INTERVALS_MAX_COUNT;

		for (uint32_t i = 0; i < CONFIG_BLE_HRS_CENTRAL_RR_INTERVALS_MAX_COUNT; i++) {
			if (index >= hvx->len) {
				ble_hrs_central_evt.params.hrm.rr_intervals_cnt = (uint8_t)i;
				break;
			}
			ble_hrs_central_evt.params.hrm.rr_intervals[i] =
				(((uint16_t)((uint8_t *)(&(hvx->data[index])))[0])) |
				(((uint16_t)((uint8_t *)(&(hvx->data[index])))[1]) << 8);
			index += sizeof(uint16_t);
		}
	}

	ble_hrs_central->evt_handler(ble_hrs_central, &ble_hrs_central_evt);
}

static void on_disconnected(struct ble_hrs_central *ble_hrs_central, const ble_evt_t *ble_evt)
{
	if (ble_hrs_central->conn_handle == ble_evt->evt.gap_evt.conn_handle) {
		ble_hrs_central->conn_handle = BLE_CONN_HANDLE_INVALID;
		ble_hrs_central->peer_hrs_db.hrm_cccd_handle = BLE_GATT_HANDLE_INVALID;
		ble_hrs_central->peer_hrs_db.hrm_handle = BLE_GATT_HANDLE_INVALID;
	}
}

void ble_hrs_on_db_disc_evt(struct ble_hrs_central *ble_hrs_central,
			    const struct ble_db_discovery_evt *evt)
{
	const struct ble_gatt_db_char *db_char;
	struct ble_hrs_central_evt hrs_c_evt = {
		.evt_type = BLE_HRS_CENTRAL_EVT_DISCOVERY_COMPLETE,
		.conn_handle = evt->conn_handle,
	};
	struct hrs_db *hrs_db = &ble_hrs_central->peer_hrs_db;

	/* Check if the Heart Rate Service was discovered. */
	if (evt->evt_type == BLE_DB_DISCOVERY_COMPLETE &&
	    evt->params.discovered_db.srv_uuid.uuid == BLE_UUID_HEART_RATE_SERVICE &&
	    evt->params.discovered_db.srv_uuid.type == BLE_UUID_TYPE_BLE) {
		/* Find the CCCD Handle of the Heart Rate Measurement characteristic. */
		for (uint32_t i = 0; i < evt->params.discovered_db.char_count; i++) {
			db_char = &evt->params.discovered_db.charateristics[i];

			if (db_char->characteristic.uuid.uuid ==
				BLE_UUID_HEART_RATE_MEASUREMENT_CHAR) {
				/* Found Heart Rate characteristic. Store CCCD handle and break. */
				hrs_c_evt.params.peer_db.hrm_cccd_handle =
					db_char->cccd_handle;
				hrs_c_evt.params.peer_db.hrm_handle =
					db_char->characteristic.handle_value;
				break;
			}
		}

		LOG_DBG("Heart Rate Service discovered at peer.");
		/* If the instance has been assigned prior to db_discovery,
		 * assign the db_handles.
		 */
		if (ble_hrs_central->conn_handle != BLE_CONN_HANDLE_INVALID) {
			if ((hrs_db->hrm_cccd_handle == BLE_GATT_HANDLE_INVALID) &&
			    (hrs_db->hrm_handle == BLE_GATT_HANDLE_INVALID)) {
				ble_hrs_central->peer_hrs_db = hrs_c_evt.params.peer_db;
			}
		}

		ble_hrs_central->evt_handler(ble_hrs_central, &hrs_c_evt);
	}
}

uint32_t ble_hrs_central_init(struct ble_hrs_central *ble_hrs_central,
			      struct ble_hrs_central_config *ble_hrs_central_init)
{
	ble_uuid_t hrs_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_HEART_RATE_SERVICE,
	};

	if (!ble_hrs_central || !ble_hrs_central_init) {
		return NRF_ERROR_NULL;
	}

	ble_hrs_central->evt_handler = ble_hrs_central_init->evt_handler;
	ble_hrs_central->gatt_queue = ble_hrs_central_init->gatt_queue;
	ble_hrs_central->conn_handle = BLE_CONN_HANDLE_INVALID;
	ble_hrs_central->peer_hrs_db.hrm_cccd_handle = BLE_GATT_HANDLE_INVALID;
	ble_hrs_central->peer_hrs_db.hrm_handle = BLE_GATT_HANDLE_INVALID;

	return ble_db_discovery_service_register(ble_hrs_central_init->db_discovery, &hrs_uuid);
}

void ble_hrs_central_on_ble_evt(ble_evt_t const *ble_evt, void *ctx)
{
	struct ble_hrs_central *ble_hrs_central = (struct ble_hrs_central *)ctx;

	if (!ble_hrs_central || !ble_evt) {
		return;
	}

	switch (ble_evt->header.evt_id) {
	case BLE_GATTC_EVT_HVX:
		on_hvx(ble_hrs_central, ble_evt);
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnected(ble_hrs_central, ble_evt);
		break;
	default:
		break;
	}
}

static uint32_t cccd_configure(struct ble_hrs_central *ble_hrs_central, bool enable)
{
	LOG_DBG("Configuring CCCD. CCCD Handle = %d, Connection Handle = %d",
		ble_hrs_central->peer_hrs_db.hrm_cccd_handle, ble_hrs_central->conn_handle);

	uint16_t cccd_val = enable ? BLE_GATT_HVX_NOTIFICATION : 0;
	uint8_t cccd[BLE_CCCD_VALUE_LEN] = {
		(cccd_val & 0x00FF),
		(cccd_val & 0xFF00) >> 8
	};
	struct ble_gq_req hrs_c_req = {
		.type = BLE_GQ_REQ_GATTC_WRITE,
		.evt_handler = gatt_error_handler,
		.ctx = ble_hrs_central,
		.gattc_write.handle = ble_hrs_central->peer_hrs_db.hrm_cccd_handle,
		.gattc_write.len = BLE_CCCD_VALUE_LEN,
		.gattc_write.p_value = cccd,
		.gattc_write.write_op = BLE_GATT_OP_WRITE_REQ,
	};

	return ble_gq_item_add(ble_hrs_central->gatt_queue, &hrs_c_req,
			       ble_hrs_central->conn_handle);
}

uint32_t ble_hrs_central_hrm_notif_enable(struct ble_hrs_central *ble_hrs_central)
{
	if (!ble_hrs_central) {
		return NRF_ERROR_NULL;
	}

	return cccd_configure(ble_hrs_central, true);
}

uint32_t ble_hrs_central_handles_assign(struct ble_hrs_central *ble_hrs_central,
					uint16_t conn_handle,
					const struct hrs_db *p_peer_hrs_handles)
{
	if (!ble_hrs_central) {
		return NRF_ERROR_NULL;
	}

	ble_hrs_central->conn_handle = conn_handle;

	if (p_peer_hrs_handles) {
		ble_hrs_central->peer_hrs_db = *p_peer_hrs_handles;
	}

	return ble_gq_conn_handle_register(ble_hrs_central->gatt_queue, conn_handle);
}
/** @} */
