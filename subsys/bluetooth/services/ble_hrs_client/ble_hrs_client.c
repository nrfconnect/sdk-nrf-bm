/*
 * Copyright (c) 2012 - 2026 Nordic Semiconductor ASA
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
	struct ble_hrs_client *hrs_client = (struct ble_hrs_client *)req->ctx;
	struct ble_hrs_client_evt evt = {
		.evt_type = BLE_HRS_CLIENT_EVT_ERROR,
		.conn_handle = gq_evt->conn_handle,
		.error.reason = gq_evt->error.reason,
	};

	switch (gq_evt->evt_type) {
	case BLE_GQ_EVT_ERROR:
		hrs_client->evt_handler(hrs_client, &evt);
		break;
	}
}

static void on_hvx(struct ble_hrs_client *hrs_client, const ble_evt_t *ble_evt)
{
	uint8_t flags;
	uint32_t min_len;
	uint32_t index;
	const ble_gattc_evt_hvx_t *hvx = &ble_evt->evt.gattc_evt.params.hvx;
	struct ble_hrs_client_evt hrs_client_evt = {
		.evt_type = BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION,
		.conn_handle = hrs_client->conn_handle,
		.hrm_notification.rr_intervals_cnt = 0,
	};

	/* Check if the event is on the link for this instance. */
	if (hrs_client->conn_handle != ble_evt->evt.gattc_evt.conn_handle) {
		return;
	}

	/* Check if this is a heart rate notification. */
	if (hvx->handle != hrs_client->handles.hrm_handle) {
		return;
	}

	LOG_DBG("HVX link: %#x hrm_handle: %#x",
		hvx->handle, hrs_client->handles.hrm_handle);

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
		hrs_client_evt.hrm_notification.hr_value = hvx->data[index++];
	} else {
		/* 16-bit heart rate value received. */
		hrs_client_evt.hrm_notification.hr_value = sys_get_le16(&hvx->data[index]);
		index += sizeof(uint16_t);
	}

	/* RR intervals are variable-length; consume as many complete pairs as available. */
	if ((flags & HRM_FLAG_MASK_HR_RR_INT)) {
		for (uint32_t i = 0; i < CONFIG_BLE_HRS_CLIENT_RR_INTERVALS_MAX_COUNT; i++) {
			if ((index + sizeof(uint16_t)) > hvx->len) {
				break;
			}
			hrs_client_evt.hrm_notification.rr_intervals[i] =
				sys_get_le16(&hvx->data[index]);
			index += sizeof(uint16_t);
			hrs_client_evt.hrm_notification.rr_intervals_cnt++;
		}
	}

	hrs_client->evt_handler(hrs_client, &hrs_client_evt);
}

static void on_disconnected(struct ble_hrs_client *hrs_client, const ble_evt_t *ble_evt)
{
	if (hrs_client->conn_handle == ble_evt->evt.gap_evt.conn_handle) {
		hrs_client->conn_handle = BLE_CONN_HANDLE_INVALID;
		hrs_client->handles.hrm_cccd_handle = BLE_GATT_HANDLE_INVALID;
		hrs_client->handles.hrm_handle = BLE_GATT_HANDLE_INVALID;
	}
}

void ble_hrs_on_db_disc_evt(struct ble_hrs_client *hrs_client,
			    const struct ble_db_discovery_evt *db_discovery_evt)
{
	__ASSERT(hrs_client, "HRS client instance is NULL");
	__ASSERT(db_discovery_evt, "Discovery event is NULL");

	const struct ble_gatt_db_char *db_char;
	struct ble_hrs_client_evt hrs_client_evt = {
		.evt_type = BLE_HRS_CLIENT_EVT_DISCOVERY_COMPLETE,
		.conn_handle = db_discovery_evt->conn_handle,
	};
	struct ble_hrs_handles *handles = &hrs_client->handles;

	/* Check if the Heart Rate Service was discovered. */
	if (db_discovery_evt->evt_type != BLE_DB_DISCOVERY_COMPLETE ||
	    db_discovery_evt->discovered_db->srv_uuid.uuid != BLE_UUID_HEART_RATE_SERVICE ||
	    db_discovery_evt->discovered_db->srv_uuid.type != BLE_UUID_TYPE_BLE) {
		return;
	}

	/* Find the CCCD Handle of the Heart Rate Measurement characteristic. */
	for (uint32_t i = 0; i < db_discovery_evt->discovered_db->char_count; i++) {
		db_char = &db_discovery_evt->discovered_db->characteristics[i];

		if (db_char->characteristic.uuid.uuid == BLE_UUID_HEART_RATE_MEASUREMENT_CHAR) {
			/* Found Heart Rate characteristic. Store CCCD handle and break. */
			hrs_client_evt.discovery_complete.handles.hrm_cccd_handle =
				db_char->cccd_handle;
			hrs_client_evt.discovery_complete.handles.hrm_handle =
				db_char->characteristic.handle_value;
			break;
		}
	}

	LOG_DBG("HRS discovered");

	/* If the instance has been assigned prior to db_discovery, assign the db_handles. */
	if (hrs_client->conn_handle != BLE_CONN_HANDLE_INVALID) {
		if ((handles->hrm_cccd_handle == BLE_GATT_HANDLE_INVALID) &&
		    (handles->hrm_handle == BLE_GATT_HANDLE_INVALID)) {
			hrs_client->handles = hrs_client_evt.discovery_complete.handles;
		}
	}

	hrs_client->evt_handler(hrs_client, &hrs_client_evt);
}

uint32_t ble_hrs_client_init(struct ble_hrs_client *hrs_client,
			     const struct ble_hrs_client_config *hrs_client_config)
{
	ble_uuid_t hrs_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_HEART_RATE_SERVICE,
	};

	if (!hrs_client || !hrs_client_config) {
		return NRF_ERROR_NULL;
	}

	if (!hrs_client_config->evt_handler || !hrs_client_config->gatt_queue) {
		return NRF_ERROR_NULL;
	}

	hrs_client->evt_handler = hrs_client_config->evt_handler;
	hrs_client->gatt_queue = hrs_client_config->gatt_queue;
	hrs_client->conn_handle = BLE_CONN_HANDLE_INVALID;
	hrs_client->handles.hrm_cccd_handle = BLE_GATT_HANDLE_INVALID;
	hrs_client->handles.hrm_handle = BLE_GATT_HANDLE_INVALID;

	return ble_db_discovery_service_register(hrs_client_config->db_discovery, &hrs_uuid);
}

void ble_hrs_client_on_ble_evt(const ble_evt_t *ble_evt, void *hrs_client)
{
	__ASSERT(ble_evt, "ble_evt is NULL");
	__ASSERT(hrs_client, "hrs_client is NULL");

	struct ble_hrs_client *ble_hrs_client = hrs_client;

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

static uint32_t cccd_configure(struct ble_hrs_client *hrs_client, bool enable)
{
	if (hrs_client->conn_handle == BLE_CONN_HANDLE_INVALID ||
	    hrs_client->handles.hrm_cccd_handle == BLE_GATT_HANDLE_INVALID) {
		return NRF_ERROR_INVALID_STATE;
	}

	LOG_DBG("CCCD cfg cccd_handle: %#x conn_handle; %#x",
		hrs_client->handles.hrm_cccd_handle, hrs_client->conn_handle);

	uint8_t cccd[BLE_CCCD_VALUE_LEN];

	sys_put_le16(enable ? BLE_GATT_HVX_NOTIFICATION : 0, cccd);
	struct ble_gq_req hrs_c_req = {
		.type = BLE_GQ_REQ_GATTC_WRITE,
		.evt_handler = ble_hrs_client_on_ble_gq_event,
		.ctx = hrs_client,
		.gattc_write.handle = hrs_client->handles.hrm_cccd_handle,
		.gattc_write.len = BLE_CCCD_VALUE_LEN,
		.gattc_write.p_value = cccd,
		.gattc_write.write_op = BLE_GATT_OP_WRITE_REQ,
	};

	return ble_gq_item_add(hrs_client->gatt_queue, &hrs_c_req,
			       hrs_client->conn_handle);
}

uint32_t ble_hrs_client_hrm_notif_enable(struct ble_hrs_client *hrs_client)
{
	if (!hrs_client) {
		return NRF_ERROR_NULL;
	}

	return cccd_configure(hrs_client, true);
}

uint32_t ble_hrs_client_hrm_notif_disable(struct ble_hrs_client *hrs_client)
{
	if (!hrs_client) {
		return NRF_ERROR_NULL;
	}

	return cccd_configure(hrs_client, false);
}

uint32_t ble_hrs_client_handles_assign(struct ble_hrs_client *hrs_client,
					uint16_t conn_handle,
					const struct ble_hrs_handles *handles)
{
	if (!hrs_client) {
		return NRF_ERROR_NULL;
	}

	hrs_client->conn_handle = conn_handle;

	if (handles) {
		hrs_client->handles = *handles;
	}

	return ble_gq_conn_handle_register(hrs_client->gatt_queue, conn_handle);
}
