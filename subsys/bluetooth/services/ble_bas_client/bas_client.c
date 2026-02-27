/*
 * Copyright (c) 2012 - 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <ble_gattc.h>
#include <bm/bluetooth/services/ble_bas_client.h>
#include <bm/bluetooth/services/uuid.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bas_client, CONFIG_BLE_BAS_CLIENT_LOG_LEVEL);

static void bas_error_handler(uint32_t nrf_error, void *ctx, uint16_t conn_handle)
{
	struct ble_bas_client *bas_client = (struct ble_bas_client *)ctx;
	struct ble_bas_client_evt err_evt = {.evt_type = BLE_BAS_CLIENT_EVT_ERROR,
					     .conn_handle = conn_handle,
					     .error.reason = nrf_error};

	LOG_DBG("A GATT Client error has occurred on conn_handle: 0X%X", conn_handle);

	if (bas_client->evt_handler != NULL) {
		bas_client->evt_handler(bas_client, &err_evt);
	}
}

static void bas_gq_event_handler(const struct ble_gq_req *req, struct ble_gq_evt *evt)
{
	if (evt->evt_type != BLE_GQ_EVT_ERROR) {
		return;
	}

	bas_error_handler(evt->error.reason, req->ctx, evt->conn_handle);
}

static void on_read_rsp(struct ble_bas_client *bas_client, ble_evt_t const *ble_evt)
{
	const ble_gattc_evt_read_rsp_t *response;

	/* Check if the event is on the link for this instance. */
	if (bas_client->conn_handle != ble_evt->evt.gattc_evt.conn_handle) {
		return;
	}

	response = &ble_evt->evt.gattc_evt.params.read_rsp;

	if (response->handle == bas_client->peer_bas_db.bl_handle) {
		struct ble_bas_client_evt evt = {.conn_handle = ble_evt->evt.gattc_evt.conn_handle,
						 .evt_type = BLE_BAS_CLIENT_EVT_BATT_READ_RESP,
						 .battery_level = response->data[0]};

		bas_client->evt_handler(bas_client, &evt);
	}
}

static void on_hvx(struct ble_bas_client *ble_bas_client, ble_evt_t const *ble_evt)
{
	/* Check if the event is on the link for this instance. */
	if (ble_bas_client->conn_handle != ble_evt->evt.gattc_evt.conn_handle) {
		return;
	}
	/* Check if this notification is a battery level notification. */
	if (ble_evt->evt.gattc_evt.params.hvx.handle == ble_bas_client->peer_bas_db.bl_handle) {
		if (ble_evt->evt.gattc_evt.params.hvx.len == 1) {
			struct ble_bas_client_evt ble_bas_client_evt = {
				.conn_handle = ble_evt->evt.gattc_evt.conn_handle,
				.evt_type = BLE_BAS_CLIENT_EVT_BATT_NOTIFICATION,
				.battery_level = ble_evt->evt.gattc_evt.params.hvx.data[0]};

			ble_bas_client->evt_handler(ble_bas_client, &ble_bas_client_evt);
		}
	}
}

void ble_bas_on_db_disc_evt(struct ble_bas_client *ble_bas_client,
			    const struct ble_db_discovery_evt *db_evt)
{
	/* Check if the Battery Service was discovered. */
	if (db_evt->evt_type == BLE_DB_DISCOVERY_COMPLETE &&
	    db_evt->params.discovered_db.srv_uuid.uuid == BLE_UUID_BATTERY_SERVICE &&
	    db_evt->params.discovered_db.srv_uuid.type == BLE_UUID_TYPE_BLE) {
		/* Find the CCCD Handle of the Battery Level characteristic. */
		struct ble_bas_client_evt evt = {.evt_type = BLE_BAS_CLIENT_EVT_DISCOVERY_COMPLETE,
						 .conn_handle = db_evt->conn_handle};

		for (uint8_t i = 0; i < db_evt->params.discovered_db.char_count; i++) {
			if (db_evt->params.discovered_db.characteristics[i]
				    .characteristic.uuid.uuid == BLE_UUID_BATTERY_LEVEL_CHAR) {
				/* Found Battery Level characteristic. Store CCCD handle and break.
				 */
				evt.bas_db.bl_cccd_handle =
					db_evt->params.discovered_db.characteristics[i].cccd_handle;
				evt.bas_db.bl_handle =
					db_evt->params.discovered_db.characteristics[i]
						.characteristic.handle_value;
				break;
			}
		}

		LOG_DBG("Battery Service discovered at peer.");

		/** If the instance has been assigned prior to db_discovery, assign the db_handles.
		 */
		if ((ble_bas_client->peer_bas_db.bl_cccd_handle == BLE_GATT_HANDLE_INVALID) &&
		    (ble_bas_client->peer_bas_db.bl_handle == BLE_GATT_HANDLE_INVALID) &&
		    (ble_bas_client->conn_handle != BLE_CONN_HANDLE_INVALID)) {
			ble_bas_client->peer_bas_db = evt.bas_db;
		}
		ble_bas_client->evt_handler(ble_bas_client, &evt);
	} else if ((db_evt->evt_type == BLE_DB_DISCOVERY_SRV_NOT_FOUND) ||
		   (db_evt->evt_type == BLE_DB_DISCOVERY_ERROR)) {
		LOG_DBG("Battery Service discovery failure at peer. ");
	}
}

static uint32_t cccd_configure(struct ble_bas_client *ble_bas_client, bool notification_enable)
{
	LOG_DBG("Configuring CCCD. CCCD Handle = %d, Connection Handle = %d",
		ble_bas_client->peer_bas_db.bl_cccd_handle, ble_bas_client->conn_handle);

	uint16_t cccd_val = notification_enable ? BLE_GATT_HVX_NOTIFICATION : 0;
	uint8_t cccd[BLE_CCCD_VALUE_LEN] = {cccd_val & 0x00ff, (cccd_val >> 8) & 0x00ff};
	struct ble_gq_req bas_client_req = {.type = BLE_GQ_REQ_GATTC_WRITE,
					    .evt_handler = bas_gq_event_handler,
					    .ctx = ble_bas_client,
					    .gattc_write.handle =
						    ble_bas_client->peer_bas_db.bl_cccd_handle,
					    .gattc_write.offset = 0,
					    .gattc_write.write_op = BLE_GATT_OP_WRITE_REQ,
					    .gattc_write.p_value = cccd,
					    .gattc_write.len = BLE_CCCD_VALUE_LEN};

	return ble_gq_item_add(ble_bas_client->gatt_queue, &bas_client_req,
			       ble_bas_client->conn_handle);
}

uint32_t ble_bas_client_init(struct ble_bas_client *ble_bas_client,
			     struct ble_bas_client_config *ble_bas_client_config)
{
	if (!ble_bas_client || !ble_bas_client_config) {
		return NRF_ERROR_NULL;
	}

	ble_uuid_t bas_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = BLE_UUID_BATTERY_SERVICE};

	ble_bas_client->conn_handle = BLE_CONN_HANDLE_INVALID;
	ble_bas_client->peer_bas_db.bl_cccd_handle = BLE_GATT_HANDLE_INVALID;
	ble_bas_client->peer_bas_db.bl_handle = BLE_GATT_HANDLE_INVALID;
	ble_bas_client->evt_handler = ble_bas_client_config->evt_handler;
	ble_bas_client->gatt_queue = ble_bas_client_config->gatt_queue;

	return ble_db_discovery_service_register(ble_bas_client_config->db_discovery, &bas_uuid);
}

static void on_disconnected(struct ble_bas_client *ble_bas_client, const ble_evt_t *ble_evt)
{
	if (ble_bas_client->conn_handle == ble_evt->evt.gap_evt.conn_handle) {
		ble_bas_client->conn_handle = BLE_CONN_HANDLE_INVALID;
		ble_bas_client->peer_bas_db.bl_cccd_handle = BLE_GATT_HANDLE_INVALID;
		ble_bas_client->peer_bas_db.bl_handle = BLE_GATT_HANDLE_INVALID;
	}
}

void ble_bas_client_on_ble_evt(ble_evt_t const *ble_evt, void *context)
{
	if ((ble_evt == NULL) || (context == NULL)) {
		return;
	}

	struct ble_bas_client *ble_bas_client = (struct ble_bas_client *)context;

	switch (ble_evt->header.evt_id) {
	case BLE_GATTC_EVT_HVX:
		on_hvx(ble_bas_client, ble_evt);
		break;

	case BLE_GATTC_EVT_READ_RSP:
		on_read_rsp(ble_bas_client, ble_evt);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnected(ble_bas_client, ble_evt);
		break;

	default:
		break;
	}
}

uint32_t ble_bas_client_bl_notif_enable(struct ble_bas_client *ble_bas_client)
{
	if (!ble_bas_client) {
		return NRF_ERROR_NULL;
	}

	if (ble_bas_client->conn_handle == BLE_CONN_HANDLE_INVALID) {
		return NRF_ERROR_INVALID_STATE;
	}

	return cccd_configure(ble_bas_client, true);
}

uint32_t ble_bas_client_bl_read(struct ble_bas_client *ble_bas_client)
{
	if (!ble_bas_client) {
		return NRF_ERROR_NULL;
	}
	if (ble_bas_client->conn_handle == BLE_CONN_HANDLE_INVALID) {
		return NRF_ERROR_INVALID_STATE;
	}

	struct ble_gq_req bas_client_req = {.type = BLE_GQ_REQ_GATTC_READ,
					    .gattc_read.handle =
						    ble_bas_client->peer_bas_db.bl_handle,
					    .evt_handler = bas_gq_event_handler,
					    .ctx = ble_bas_client};

	return ble_gq_item_add(ble_bas_client->gatt_queue, &bas_client_req,
			       ble_bas_client->conn_handle);
}

uint32_t ble_bas_client_handles_assign(struct ble_bas_client *ble_bas_client, uint16_t conn_handle,
				       struct ble_bas_client_db *peer_handles)
{
	if (!ble_bas_client) {
		return NRF_ERROR_NULL;
	}

	ble_bas_client->conn_handle = conn_handle;
	if (peer_handles != NULL) {
		ble_bas_client->peer_bas_db = *peer_handles;
	}

	return ble_gq_conn_handle_register(ble_bas_client->gatt_queue, conn_handle);
}
