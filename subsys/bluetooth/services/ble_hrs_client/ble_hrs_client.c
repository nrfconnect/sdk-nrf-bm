/*
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm/bluetooth/services/ble_hrs_client.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/services/uuid.h>
#include <bm/bluetooth/ble_gatt_db.h>
#include <ble_types.h>
#include <ble_gattc.h>

#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_hrs_client, CONFIG_BLE_HRS_CLIENT_LOG_LEVEL);

/* Bit mask used to extract the type of heart rate value. This is used to
 * find if the received heart rate is a 16 bit value or an 8 bit value.
 */
#define HRM_FLAG_MASK_HR_16BIT (0x01 << 0)

/* Bit mask used to extract the presence of RR_INTERVALS. This is used to
 * find if the received measurement includes RR_INTERVALS.
 */
#define HRM_FLAG_MASK_HR_RR_INT (0x01 << 4)

#ifndef CONFIG_UNITY
static
#endif
void ble_hrs_client_on_ble_gq_event(const struct ble_gq_req *req, struct ble_gq_evt *gq_evt)
{
	struct ble_hrs_client *ble_hrs_client = (struct ble_hrs_client *)req->ctx;
	struct ble_hrs_client_evt evt = {0};

	switch (gq_evt->evt_type) {
	case BLE_GQ_EVT_ERROR:
		evt.evt_type = BLE_HRS_CLIENT_EVT_ERROR;
		evt.conn_handle = gq_evt->conn_handle;
		evt.params.error.reason = gq_evt->error.reason;

		ble_hrs_client->evt_handler(ble_hrs_client, &evt);
		break;
	}
}

static void on_hvx(struct ble_hrs_client *ble_hrs_client, const ble_evt_t *ble_evt)
{
	uint8_t flags;
	uint32_t min_len;
	uint32_t index;
	const ble_gattc_evt_hvx_t *hvx = &ble_evt->evt.gattc_evt.params.hvx;
	struct ble_hrs_client_evt ble_hrs_client_evt = {
		.evt_type = BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION,
		.conn_handle = ble_hrs_client->conn_handle,
		.params.hrm.rr_intervals_cnt = 0,
	};

	/* Check if the event is on the link for this instance. */
	if (ble_hrs_client->conn_handle != ble_evt->evt.gattc_evt.conn_handle) {
		return;
	}

	/* Check if this is a heart rate notification. */
	if (hvx->handle != ble_hrs_client->peer_hrs_db.hrm_handle) {
		return;
	}

	LOG_DBG("Received HVX on link 0x%x, hrm_handle 0x%x",
		hvx->handle, ble_hrs_client->peer_hrs_db.hrm_handle);

	/* Need at least 1 byte to read the flags. */
	if (hvx->len < 1) {
		return;
	}

	index = 0;
	flags = hvx->data[index++];

	/* Validate minimum payload length from flags:
	 *   1 (flags) + HR value size (1 or 2)
	 */
	min_len = 1 + ((flags & HRM_FLAG_MASK_HR_16BIT) ? sizeof(uint16_t) : sizeof(uint8_t));
	if (hvx->len < min_len) {
		LOG_WRN("HRM too short: len=%u need=%u", hvx->len, min_len);
		return;
	}

	/* All mandatory bytes are guaranteed present from here. */
	if (!(flags & HRM_FLAG_MASK_HR_16BIT)) {
		/* 8-bit heart rate value received. */
		ble_hrs_client_evt.params.hrm.hr_value = hvx->data[index++];
	} else {
		/* 16-bit heart rate value received. */
		ble_hrs_client_evt.params.hrm.hr_value = sys_get_le16(&hvx->data[index]);
		index += sizeof(uint16_t);
	}

	/* RR intervals are variable-length; consume as many complete pairs as available. */
	if ((flags & HRM_FLAG_MASK_HR_RR_INT)) {
		for (uint32_t i = 0; i < CONFIG_BLE_HRS_CLIENT_RR_INTERVALS_MAX_COUNT; i++) {
			if ((index + sizeof(uint16_t)) > hvx->len) {
				break;
			}
			ble_hrs_client_evt.params.hrm.rr_intervals[i] =
				sys_get_le16(&hvx->data[index]);
			index += sizeof(uint16_t);
			ble_hrs_client_evt.params.hrm.rr_intervals_cnt++;
		}
	}

	ble_hrs_client->evt_handler(ble_hrs_client, &ble_hrs_client_evt);
}

static void on_disconnected(struct ble_hrs_client *ble_hrs_client, const ble_evt_t *ble_evt)
{
	if (ble_hrs_client->conn_handle == ble_evt->evt.gap_evt.conn_handle) {
		ble_hrs_client->conn_handle = BLE_CONN_HANDLE_INVALID;
		ble_hrs_client->peer_hrs_db.hrm_cccd_handle = BLE_GATT_HANDLE_INVALID;
		ble_hrs_client->peer_hrs_db.hrm_handle = BLE_GATT_HANDLE_INVALID;
	}
}

void ble_hrs_on_db_disc_evt(struct ble_hrs_client *ble_hrs_client,
			    const struct ble_db_discovery_evt *evt)
{
	__ASSERT(ble_hrs_client, "HRS client instance is NULL");
	__ASSERT(evt, "Discovery event is NULL");

	const struct ble_gatt_db_char *db_char;
	struct ble_hrs_client_evt hrs_c_evt = {
		.evt_type = BLE_HRS_CLIENT_EVT_DISCOVERY_COMPLETE,
		.conn_handle = evt->conn_handle,
	};
	struct hrs_db *hrs_db = &ble_hrs_client->peer_hrs_db;

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
		if (ble_hrs_client->conn_handle != BLE_CONN_HANDLE_INVALID) {
			if ((hrs_db->hrm_cccd_handle == BLE_GATT_HANDLE_INVALID) &&
			    (hrs_db->hrm_handle == BLE_GATT_HANDLE_INVALID)) {
				ble_hrs_client->peer_hrs_db = hrs_c_evt.params.peer_db;
			}
		}

		ble_hrs_client->evt_handler(ble_hrs_client, &hrs_c_evt);
	}
}

uint32_t ble_hrs_client_init(struct ble_hrs_client *ble_hrs_client,
			      struct ble_hrs_client_config *ble_hrs_client_init)
{
	ble_uuid_t hrs_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_HEART_RATE_SERVICE,
	};

	if (!ble_hrs_client || !ble_hrs_client_init) {
		return NRF_ERROR_NULL;
	}

	if (!ble_hrs_client_init->evt_handler || !ble_hrs_client_init->gatt_queue) {
		return NRF_ERROR_NULL;
	}

	ble_hrs_client->evt_handler = ble_hrs_client_init->evt_handler;
	ble_hrs_client->gatt_queue = ble_hrs_client_init->gatt_queue;
	ble_hrs_client->conn_handle = BLE_CONN_HANDLE_INVALID;
	ble_hrs_client->peer_hrs_db.hrm_cccd_handle = BLE_GATT_HANDLE_INVALID;
	ble_hrs_client->peer_hrs_db.hrm_handle = BLE_GATT_HANDLE_INVALID;

	return ble_db_discovery_service_register(ble_hrs_client_init->db_discovery, &hrs_uuid);
}

void ble_hrs_client_on_ble_evt(const ble_evt_t *ble_evt, void *ble_hrs_client)
{
	__ASSERT(ble_evt, "BLE event is NULL");
	__ASSERT(ble_hrs_client, "HRS central instance is NULL");

	switch (ble_evt->header.evt_id) {
	case BLE_GATTC_EVT_HVX:
		on_hvx(ble_hrs_client, ble_evt);
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnected(ble_hrs_client, ble_evt);
		break;
	default:
		break;
	}
}

static uint32_t cccd_configure(struct ble_hrs_client *ble_hrs_client, bool enable)
{
	LOG_DBG("Configuring CCCD. CCCD Handle = %d, Connection Handle = %d",
		ble_hrs_client->peer_hrs_db.hrm_cccd_handle, ble_hrs_client->conn_handle);

	uint8_t cccd[BLE_CCCD_VALUE_LEN];

	sys_put_le16(enable ? BLE_GATT_HVX_NOTIFICATION : 0, cccd);
	struct ble_gq_req hrs_c_req = {
		.type = BLE_GQ_REQ_GATTC_WRITE,
		.evt_handler = ble_hrs_client_on_ble_gq_event,
		.ctx = ble_hrs_client,
		.gattc_write.handle = ble_hrs_client->peer_hrs_db.hrm_cccd_handle,
		.gattc_write.len = BLE_CCCD_VALUE_LEN,
		.gattc_write.p_value = cccd,
		.gattc_write.write_op = BLE_GATT_OP_WRITE_REQ,
	};

	return ble_gq_item_add(ble_hrs_client->gatt_queue, &hrs_c_req,
			       ble_hrs_client->conn_handle);
}

uint32_t ble_hrs_client_hrm_notif_enable(struct ble_hrs_client *ble_hrs_client)
{
	if (!ble_hrs_client) {
		return NRF_ERROR_NULL;
	}

	return cccd_configure(ble_hrs_client, true);
}

uint32_t ble_hrs_client_hrm_notif_disable(struct ble_hrs_client *ble_hrs_client)
{
	if (!ble_hrs_client) {
		return NRF_ERROR_NULL;
	}

	return cccd_configure(ble_hrs_client, false);
}

uint32_t ble_hrs_client_handles_assign(struct ble_hrs_client *ble_hrs_client,
					uint16_t conn_handle,
					const struct hrs_db *p_peer_hrs_handles)
{
	if (!ble_hrs_client) {
		return NRF_ERROR_NULL;
	}

	ble_hrs_client->conn_handle = conn_handle;

	if (p_peer_hrs_handles) {
		ble_hrs_client->peer_hrs_db = *p_peer_hrs_handles;
	}

	return ble_gq_conn_handle_register(ble_hrs_client->gatt_queue, conn_handle);
}
