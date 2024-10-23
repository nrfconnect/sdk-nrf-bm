/*
 * Copyright (c) 2012-2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <ble_gap.h>
#include <ble_gatts.h>
#include <ble_gattc.h>
#include <ble_conn_params.h>
#include <nrf_sdh_ble.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ble_conn_params);

extern void ble_conn_params_event_send(const struct ble_conn_params_evt *evt);

static struct {
	uint16_t att_mtu;
	uint8_t att_mtu_exchange_pending : 1;
	uint8_t connected : 1;
} links[CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT] = {
	[0 ... CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT - 1] = {
		.att_mtu = CONFIG_BLE_CONN_PARAMS_ATT_MTU,
	}
};

static void mtu_exchange_request(uint16_t conn_handle)
{
	int err;

	err = sd_ble_gattc_exchange_mtu_request(conn_handle, links[conn_handle].att_mtu);
	if (!err) {
		return;
	}

	if (err == NRF_ERROR_BUSY) {
		/* Retry */
		LOG_DBG("Another procedure is ongoing, will retry");
		links[conn_handle].att_mtu_exchange_pending = true;
	} else if (err) {
		LOG_ERR("Failed to initiate ATT MTU exchange, nrf_error %#x", err);
	}
}

static void on_exchange_mtu_req_evt(uint16_t conn_handle,
				    const ble_gatts_evt_exchange_mtu_request_t *evt)
{
	int err;
	uint16_t client_mtu = evt->client_rx_mtu;

	links[conn_handle].att_mtu = MIN(client_mtu, links[conn_handle].att_mtu);
	links[conn_handle].att_mtu_exchange_pending = false;

	LOG_INF("Peer %#x requested ATT MTU of %u bytes", conn_handle, client_mtu);

	err = sd_ble_gatts_exchange_mtu_reply(conn_handle, links[conn_handle].att_mtu);
	if (err) {
		LOG_ERR("Failed to reply to MTU exchange request, nrf_error %#x", err);
		return;
	}

	LOG_INF("ATT MTU set to %u bytes for peer %#x", links[conn_handle].att_mtu, conn_handle);

	/* The ATT MTU exchange has finished, send an event to the application */
	const struct ble_conn_params_evt app_evt = {
		.id = BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED,
		.conn_handle = conn_handle,
		.att_mtu = links[conn_handle].att_mtu,
	};

	ble_conn_params_event_send(&app_evt);
}

/* The effective ATT MTU is set to the lowest between what we requested and the peer's response.
 * This events concludes the ATT MTU exchange.
 */
static void on_exchange_mtu_rsp_evt(uint16_t conn_handle,
				    const ble_gattc_evt_exchange_mtu_rsp_t *evt)
{
	uint16_t server_rx_mtu = evt->server_rx_mtu;

	/* Determine the lowest MTU between our own desired MTU and the peer's */
	links[conn_handle].att_mtu_exchange_pending = false;
	links[conn_handle].att_mtu = MIN(server_rx_mtu, links[conn_handle].att_mtu);

	LOG_INF("ATT MTU set to %u bytes for peer %#x", links[conn_handle].att_mtu, conn_handle);

	/* Send an event to the application */
	const struct ble_conn_params_evt app_evt = {
		.id = BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED,
		.conn_handle = conn_handle,
		.att_mtu = links[conn_handle].att_mtu,
	};

	ble_conn_params_event_send(&app_evt);
}

static void on_connected(uint16_t conn_handle, const ble_gap_evt_connected_t *evt)
{
	if (IS_ENABLED(CONFIG_BLE_CONN_PARAMS_INITIATE_ATT_MTU_EXCHANGE)) {
		LOG_INF("Initiating ATT MTU exchange procedure (%u -> %u bytes) for peer %#x",
			BLE_GATT_ATT_MTU_DEFAULT, links[conn_handle].att_mtu, conn_handle);

		mtu_exchange_request(conn_handle);
	}

	links[conn_handle].connected = true;
}

static void on_disconnected(uint16_t conn_handle)
{
	links[conn_handle].connected = false;
}

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	const uint16_t conn_handle = evt->evt.common_evt.conn_handle;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		on_connected(conn_handle, &evt->evt.gap_evt.params.connected);
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnected(conn_handle);
		break;

	case BLE_GATTC_EVT_EXCHANGE_MTU_RSP:
		on_exchange_mtu_rsp_evt(
			conn_handle, &evt->evt.gattc_evt.params.exchange_mtu_rsp);
		break;
	case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
		on_exchange_mtu_req_evt(
			conn_handle, &evt->evt.gatts_evt.params.exchange_mtu_request);
		break;
	default:
		/* Ignore */
		break;
	}

	if (conn_handle > CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT) {
		return;
	}

	/* Retry any procedures that were busy */
	if (links[conn_handle].connected && links[conn_handle].att_mtu_exchange_pending) {
		links[conn_handle].att_mtu_exchange_pending = false;
		mtu_exchange_request(conn_handle);
	}
}
NRF_SDH_BLE_OBSERVER(ble_observer, on_ble_evt, NULL, 0);

int ble_conn_params_att_mtu_set(uint16_t conn_handle, uint16_t att_mtu)
{
	if (conn_handle > ARRAY_SIZE(links)) {
		return -EINVAL;
	}

	if (att_mtu < BLE_GATT_ATT_MTU_DEFAULT ||
	    att_mtu > CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE) {
		return -EINVAL;
	}

	links[conn_handle].att_mtu = att_mtu;
	mtu_exchange_request(conn_handle);

	return 0;
}

int ble_conn_params_att_mtu_get(uint16_t conn_handle, uint16_t *att_mtu)
{
	if (conn_handle > ARRAY_SIZE(links)) {
		return -EINVAL;
	}
	if (!att_mtu) {
		return -EFAULT;
	}

	*att_mtu = links[conn_handle].att_mtu;

	return 0;
}
