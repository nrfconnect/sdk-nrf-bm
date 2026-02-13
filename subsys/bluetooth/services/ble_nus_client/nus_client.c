/*
 * Copyright (c) 2012 - 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>

#include "ble.h"
#include <bm/bluetooth/services/ble_nus_client.h>
#include "ble_gattc.h"
#include <zephyr/logging/log.h>
#include <bm/bluetooth/services/uuid.h>

LOG_MODULE_REGISTER(ble_nus_client, CONFIG_BLE_NUS_CLIENT_LOG_LEVEL);

static void gatt_error_handler(const struct ble_gq_req *req, struct ble_gq_evt *evt)
{
	struct ble_nus_client *ble_nus_client = (struct ble_nus_client *)req->ctx;

	LOG_DBG("A GATT Client error has occurred on conn_handle: %#X", evt->conn_handle);

	if (ble_nus_client->evt_handler != NULL && evt->evt_type == BLE_GQ_EVT_ERROR) {
		ble_nus_client->evt_handler(
			ble_nus_client,
			&(struct ble_nus_client_evt){.conn_handle = evt->conn_handle,
						     .evt_type = BLE_NUS_CLIENT_EVT_ERROR,
						     .params.error.reason = evt->error.reason});
	}
}

void ble_nus_client_on_db_disc_evt(struct ble_nus_client *ble_nus_c,
				   struct ble_db_discovery_evt *evt)
{
	struct ble_nus_client_evt nus_c_evt = {0};
	struct ble_gatt_db_char *chars = evt->params.discovered_db.charateristics;

	/* Check if the NUS was discovered. */
	if ((evt->evt_type == BLE_DB_DISCOVERY_COMPLETE) &&
	    (evt->params.discovered_db.srv_uuid.uuid == BLE_UUID_NUS_SERVICE) &&
	    (evt->params.discovered_db.srv_uuid.type == ble_nus_c->uuid_type)) {
		for (uint32_t i = 0; i < evt->params.discovered_db.char_count; i++) {
			switch (chars[i].characteristic.uuid.uuid) {
			case BLE_UUID_NUS_RX_CHARACTERISTIC:
				nus_c_evt.params.discovery_complete.handles.nus_rx_handle =
					chars[i].characteristic.handle_value;
				break;

			case BLE_UUID_NUS_TX_CHARACTERISTIC:
				nus_c_evt.params.discovery_complete.handles.nus_tx_handle =
					chars[i].characteristic.handle_value;
				nus_c_evt.params.discovery_complete.handles.nus_tx_cccd_handle =
					chars[i].cccd_handle;
				break;

			default:
				break;
			}
		}
		if (ble_nus_c->evt_handler != NULL) {
			nus_c_evt.conn_handle = evt->conn_handle;
			nus_c_evt.evt_type = BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE;
			ble_nus_c->evt_handler(ble_nus_c, &nus_c_evt);
		}
	}
}

static void on_hvx(struct ble_nus_client *ble_nus_c, ble_evt_t const *ble_evt)
{
	/** HVX can only occur from client sending. */
	if ((ble_nus_c->handles.nus_tx_handle != BLE_GATT_HANDLE_INVALID) &&
	    (ble_evt->evt.gattc_evt.params.hvx.handle == ble_nus_c->handles.nus_tx_handle) &&
	    (ble_nus_c->evt_handler != NULL)) {
		struct ble_nus_client_evt ble_nus_c_evt;

		ble_nus_c_evt.evt_type = BLE_NUS_CLIENT_EVT_NUS_TX_EVT;
		ble_nus_c_evt.params.nus_tx_evt.data =
			(uint8_t *)ble_evt->evt.gattc_evt.params.hvx.data;
		ble_nus_c_evt.params.nus_tx_evt.data_len = ble_evt->evt.gattc_evt.params.hvx.len;

		ble_nus_c->evt_handler(ble_nus_c, &ble_nus_c_evt);
		LOG_DBG("Client sending data.");
	}
}

uint32_t ble_nus_client_init(struct ble_nus_client *ble_nus_c,
			     struct ble_nus_client_config *ble_nus_c_init)
{
	uint32_t nrf_err;
	ble_uuid_t uart_uuid;
	ble_uuid128_t nus_base_uuid = NUS_BASE_UUID;

	if (!ble_nus_c || !ble_nus_c_init || !(ble_nus_c_init->gatt_queue)) {
		return NRF_ERROR_NULL;
	}

	nrf_err = sd_ble_uuid_vs_add(&nus_base_uuid, &ble_nus_c->uuid_type);
	if (nrf_err) {
		LOG_ERR("sd_ble_uuid_vs_add failed, nrf_error %#x", nrf_err);
		return NRF_ERROR_INVALID_PARAM;
	}

	uart_uuid.type = ble_nus_c->uuid_type;
	uart_uuid.uuid = BLE_UUID_NUS_SERVICE;

	ble_nus_c->conn_handle = BLE_CONN_HANDLE_INVALID;
	ble_nus_c->evt_handler = ble_nus_c_init->evt_handler;
	ble_nus_c->handles.nus_tx_handle = BLE_GATT_HANDLE_INVALID;
	ble_nus_c->handles.nus_rx_handle = BLE_GATT_HANDLE_INVALID;
	ble_nus_c->gatt_queue = ble_nus_c_init->gatt_queue;

	return ble_db_discovery_service_register(ble_nus_c_init->db_discovery, &uart_uuid);
}

void ble_nus_client_on_ble_evt(ble_evt_t const *ble_evt, void *context)
{
	struct ble_nus_client *ble_nus_c = (struct ble_nus_client *)context;

	if (!ble_nus_c || !ble_evt) {
		return;
	}

	if (ble_nus_c->conn_handle == BLE_CONN_HANDLE_INVALID) {
		return;
	}

	switch (ble_evt->header.evt_id) {
	case BLE_GATTC_EVT_HVX:
		on_hvx(ble_nus_c, ble_evt);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		if (ble_nus_c->conn_handle != ble_evt->evt.gap_evt.conn_handle) {
			return;
		}
		if (ble_evt->evt.gap_evt.conn_handle == ble_nus_c->conn_handle &&
		    ble_nus_c->evt_handler != NULL) {
			struct ble_nus_client_evt nus_c_evt;

			nus_c_evt.evt_type = BLE_NUS_CLIENT_EVT_DISCONNECTED;
			nus_c_evt.params.disconnected.reason =
				ble_evt->evt.gap_evt.params.disconnected.reason;

			ble_nus_c->conn_handle = BLE_CONN_HANDLE_INVALID;
			ble_nus_c->evt_handler(ble_nus_c, &nus_c_evt);
		}
		break;

	default:
		/** No implementation needed. */
		break;
	}
}

static uint32_t cccd_configure(struct ble_nus_client *ble_nus_c, bool notification_enable)
{
	struct ble_gq_req cccd_req = {0};
	uint8_t cccd[BLE_CCCD_VALUE_LEN];
	uint16_t cccd_val = notification_enable ? BLE_GATT_HVX_NOTIFICATION : 0;

	cccd[0] = cccd_val & 0x00ff;
	cccd[1] = (cccd_val >> 8) & 0x00ff;

	cccd_req.type = BLE_GQ_REQ_GATTC_WRITE;
	cccd_req.evt_handler = gatt_error_handler;
	cccd_req.ctx = ble_nus_c;
	cccd_req.gattc_write.handle = ble_nus_c->handles.nus_tx_cccd_handle;
	cccd_req.gattc_write.len = BLE_CCCD_VALUE_LEN;
	cccd_req.gattc_write.offset = 0;
	cccd_req.gattc_write.p_value = cccd;
	cccd_req.gattc_write.write_op = BLE_GATT_OP_WRITE_REQ;
	cccd_req.gattc_write.flags = BLE_GATT_EXEC_WRITE_FLAG_PREPARED_WRITE;

	return ble_gq_item_add(ble_nus_c->gatt_queue, &cccd_req, ble_nus_c->conn_handle);
}

uint32_t ble_nus_client_tx_notif_enable(struct ble_nus_client *ble_nus_c)
{
	if (!ble_nus_c) {
		return NRF_ERROR_NULL;
	}

	if ((ble_nus_c->conn_handle == BLE_CONN_HANDLE_INVALID) ||
	    (ble_nus_c->handles.nus_tx_cccd_handle == BLE_GATT_HANDLE_INVALID)) {
		return NRF_ERROR_INVALID_STATE;
	}
	return cccd_configure(ble_nus_c, true);
}

uint32_t ble_nus_client_string_send(struct ble_nus_client *ble_nus_c, uint8_t *string,
				    uint16_t length)
{
	if (!ble_nus_c) {
		return NRF_ERROR_NULL;
	}

	struct ble_gq_req write_req = {0};

	if (length > BLE_NUS_MAX_DATA_LEN) {
		LOG_WRN("Content too long.");
		return NRF_ERROR_INVALID_PARAM;
	}
	if (ble_nus_c->conn_handle == BLE_CONN_HANDLE_INVALID) {
		LOG_WRN("Connection handle invalid.");
		return NRF_ERROR_INVALID_STATE;
	}

	write_req.type = BLE_GQ_REQ_GATTC_WRITE;
	write_req.evt_handler = gatt_error_handler;
	write_req.ctx = ble_nus_c;
	write_req.gattc_write.handle = ble_nus_c->handles.nus_rx_handle;
	write_req.gattc_write.len = length;
	write_req.gattc_write.offset = 0;
	write_req.gattc_write.p_value = string;
	write_req.gattc_write.write_op = BLE_GATT_OP_WRITE_CMD;
	write_req.gattc_write.flags = BLE_GATT_EXEC_WRITE_FLAG_PREPARED_WRITE;

	return ble_gq_item_add(ble_nus_c->gatt_queue, &write_req, ble_nus_c->conn_handle);
}

uint32_t ble_nus_client_handles_assign(struct ble_nus_client *ble_nus, uint16_t conn_handle,
				       struct ble_nus_client_handles const *peer_handles)
{
	if (!ble_nus) {
		return NRF_ERROR_NULL;
	}

	ble_nus->conn_handle = conn_handle;
	if (peer_handles != NULL) {
		ble_nus->handles.nus_tx_cccd_handle = peer_handles->nus_tx_cccd_handle;
		ble_nus->handles.nus_tx_handle = peer_handles->nus_tx_handle;
		ble_nus->handles.nus_rx_handle = peer_handles->nus_rx_handle;
	}
	return ble_gq_conn_handle_register(ble_nus->gatt_queue, conn_handle);
}
