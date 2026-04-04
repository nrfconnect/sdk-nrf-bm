/*
 * Copyright (c) 2012 - 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm/bluetooth/services/ble_nus_client.h>
#include <bm/bluetooth/services/uuid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_nus_client, CONFIG_BLE_NUS_CLIENT_LOG_LEVEL);

static void gatt_error_handler(const struct ble_gq_req *req, struct ble_gq_evt *gq_evt)
{
	struct ble_nus_client *ble_nus_client = (struct ble_nus_client *)req->ctx;
	struct ble_nus_client_evt nus_evt = {
		.conn_handle = gq_evt->conn_handle,
		.evt_type = BLE_NUS_CLIENT_EVT_ERROR,
		.error.reason = gq_evt->error.reason
	};

	LOG_DBG("A GATT Client error has occurred on conn_handle: %#X", gq_evt->conn_handle);

	if (ble_nus_client->evt_handler != NULL && gq_evt->evt_type == BLE_GQ_EVT_ERROR) {
		ble_nus_client->evt_handler(ble_nus_client, &nus_evt);
	}
}

void ble_nus_client_on_db_disc_evt(struct ble_nus_client *nus_client,
				   const struct ble_db_discovery_evt *db_evt)
{
	struct ble_nus_client_evt nus_c_evt = {0};
	const struct ble_gatt_db_char *chars = db_evt->discovered_db->characteristics;

	/* Check if the NUS was discovered. */
	if ((db_evt->evt_type == BLE_DB_DISCOVERY_COMPLETE) &&
	    (db_evt->discovered_db->srv_uuid.uuid == BLE_UUID_NUS_SERVICE) &&
	    (db_evt->discovered_db->srv_uuid.type == nus_client->uuid_type)) {
		for (uint32_t i = 0; i < db_evt->discovered_db->char_count; i++) {
			switch (chars[i].characteristic.uuid.uuid) {
			case BLE_UUID_NUS_RX_CHARACTERISTIC:
				nus_c_evt.discovery_complete.handles.nus_rx_handle =
					chars[i].characteristic.handle_value;
				break;

			case BLE_UUID_NUS_TX_CHARACTERISTIC:
				nus_c_evt.discovery_complete.handles.nus_tx_handle =
					chars[i].characteristic.handle_value;
				nus_c_evt.discovery_complete.handles.nus_tx_cccd_handle =
					chars[i].cccd_handle;
				break;

			default:
				break;
			}
		}
		if (nus_client->evt_handler != NULL) {
			nus_c_evt.conn_handle = db_evt->conn_handle;
			nus_c_evt.evt_type = BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE;
			nus_client->evt_handler(nus_client, &nus_c_evt);
		}
	}
}

static void on_hvx(struct ble_nus_client *nus_client, ble_evt_t const *ble_evt)
{
	struct ble_nus_client_evt ble_nus_c_evt = {
		.evt_type = BLE_NUS_CLIENT_EVT_TX_DATA,
		.tx_data = {
			.data = (uint8_t *)ble_evt->evt.gattc_evt.params.hvx.data,
			.length = ble_evt->evt.gattc_evt.params.hvx.len,
		},
	};

	/** HVX can only occur from client sending. */
	if ((nus_client->handles.nus_tx_handle != BLE_GATT_HANDLE_INVALID) &&
	    (ble_evt->evt.gattc_evt.params.hvx.handle == nus_client->handles.nus_tx_handle) &&
	    (nus_client->evt_handler != NULL)) {

		nus_client->evt_handler(nus_client, &ble_nus_c_evt);
		LOG_DBG("Client sending data.");
	}
}

uint32_t ble_nus_client_init(struct ble_nus_client *nus_client,
			     const struct ble_nus_client_config *nus_client_config)
{
	uint32_t nrf_err;
	ble_uuid_t uart_uuid;
	ble_uuid128_t nus_base_uuid = { .uuid128 = BLE_NUS_UUID_BASE };

	if (!nus_client || !nus_client_config || !(nus_client_config->gatt_queue)) {
		return NRF_ERROR_NULL;
	}

	nrf_err = sd_ble_uuid_vs_add(&nus_base_uuid, &nus_client->uuid_type);
	if (nrf_err) {
		LOG_ERR("sd_ble_uuid_vs_add failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	uart_uuid.type = nus_client->uuid_type;
	uart_uuid.uuid = BLE_UUID_NUS_SERVICE;

	nus_client->conn_handle = BLE_CONN_HANDLE_INVALID;
	nus_client->evt_handler = nus_client_config->evt_handler;
	nus_client->handles.nus_tx_handle = BLE_GATT_HANDLE_INVALID;
	nus_client->handles.nus_rx_handle = BLE_GATT_HANDLE_INVALID;
	nus_client->gatt_queue = nus_client_config->gatt_queue;

	return ble_db_discovery_service_register(nus_client_config->db_discovery, &uart_uuid);
}

void ble_nus_client_on_ble_evt(const ble_evt_t *ble_evt, void *context)
{
	struct ble_nus_client *nus_client = (struct ble_nus_client *)context;

	if (!nus_client || !ble_evt) {
		return;
	}

	if (nus_client->conn_handle == BLE_CONN_HANDLE_INVALID) {
		return;
	}

	switch (ble_evt->header.evt_id) {
	case BLE_GATTC_EVT_HVX:
		on_hvx(nus_client, ble_evt);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		if (nus_client->conn_handle != ble_evt->evt.gap_evt.conn_handle) {
			return;
		}
		if (ble_evt->evt.gap_evt.conn_handle == nus_client->conn_handle &&
		    nus_client->evt_handler != NULL) {
			struct ble_nus_client_evt nus_c_evt = {
				.evt_type = BLE_NUS_CLIENT_EVT_DISCONNECTED,
				.disconnected.reason =
					ble_evt->evt.gap_evt.params.disconnected.reason,
			};

			nus_client->conn_handle = BLE_CONN_HANDLE_INVALID;
			nus_client->evt_handler(nus_client, &nus_c_evt);
		}
		break;

	default:
		/** No implementation needed. */
		break;
	}
}

static uint32_t cccd_configure(struct ble_nus_client *nus_client, bool notification_enable)
{
	uint16_t cccd_val = notification_enable ? BLE_GATT_HVX_NOTIFICATION : 0;
	uint8_t cccd[BLE_CCCD_VALUE_LEN] = {cccd_val & 0x00ff, (cccd_val >> 8) & 0x00ff};
	struct ble_gq_req cccd_req = {
		.type = BLE_GQ_REQ_GATTC_WRITE,
		.evt_handler = gatt_error_handler,
		.ctx = nus_client,
		.gattc_write = {
			.handle = nus_client->handles.nus_tx_cccd_handle,
			.len = BLE_CCCD_VALUE_LEN,
			.offset = 0,
			.p_value = cccd,
			.write_op = BLE_GATT_OP_WRITE_REQ,
			.flags = BLE_GATT_EXEC_WRITE_FLAG_PREPARED_WRITE,
		},
	};

	return ble_gq_item_add(nus_client->gatt_queue, &cccd_req, nus_client->conn_handle);
}

uint32_t ble_nus_client_tx_notif_enable(struct ble_nus_client *nus_client)
{
	if (!nus_client) {
		return NRF_ERROR_NULL;
	}

	return cccd_configure(nus_client, true);
}

uint32_t ble_nus_client_tx_notif_disable(struct ble_nus_client *nus_client)
{
	if (!nus_client) {
		return NRF_ERROR_NULL;
	}

	return cccd_configure(nus_client, false);
}

uint32_t ble_nus_client_string_send(struct ble_nus_client *nus_client, const uint8_t *string,
				    uint16_t length)
{
	if (!nus_client) {
		return NRF_ERROR_NULL;
	}

	struct ble_gq_req write_req = {
		.type = BLE_GQ_REQ_GATTC_WRITE,
		.evt_handler = gatt_error_handler,
		.ctx = nus_client,
		.gattc_write = {
			.handle = nus_client->handles.nus_rx_handle,
			.len = length,
			.offset = 0,
			.p_value = string,
			.write_op = BLE_GATT_OP_WRITE_CMD,
			.flags = BLE_GATT_EXEC_WRITE_FLAG_PREPARED_WRITE,
		},
	};

	if (length > BLE_NUS_CLIENT_MAX_DATA_LEN) {
		LOG_WRN("Content too long.");
		return NRF_ERROR_INVALID_PARAM;
	}
	if (nus_client->conn_handle == BLE_CONN_HANDLE_INVALID) {
		LOG_WRN("Connection handle invalid.");
		return NRF_ERROR_INVALID_STATE;
	}

	return ble_gq_item_add(nus_client->gatt_queue, &write_req, nus_client->conn_handle);
}

uint32_t ble_nus_client_handles_assign(struct ble_nus_client *ble_nus, uint16_t conn_handle,
				       const struct ble_nus_client_handles *peer_handles)
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
