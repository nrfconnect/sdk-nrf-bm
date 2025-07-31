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

LOG_MODULE_DECLARE(ble_conn_params, CONFIG_BLE_CONN_PARAMS_LOG_LEVEL);

extern void ble_conn_params_event_send(const struct ble_conn_params_evt *evt);

static struct {
	uint16_t att_mtu;
	uint16_t att_mtu_desired;
	uint8_t att_mtu_exchange_pending : 1;
} links[CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT] = {
	[0 ... CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT - 1] = {
		.att_mtu = BLE_GATT_ATT_MTU_DEFAULT,
		.att_mtu_desired = CONFIG_BLE_CONN_PARAMS_ATT_MTU,
	}
};

static void mtu_exchange_request(uint16_t conn_handle, int idx)
{
	int err;

	err = sd_ble_gattc_exchange_mtu_request(conn_handle, links[idx].att_mtu_desired);
	if (!err) {
		return;
	}

	if (err == NRF_ERROR_BUSY) {
		/* Retry */
		LOG_DBG("Another procedure is ongoing, will retry");
		links[idx].att_mtu_exchange_pending = true;
	} else if (err) {
		LOG_ERR("Failed to initiate ATT MTU exchange, nrf_error %#x", err);
	}
}

static void on_exchange_mtu_req_evt(uint16_t conn_handle, int idx,
				    const ble_gatts_evt_exchange_mtu_request_t *evt)
{
	int err;

	/* Determine the lowest ATT MTU between our own desired ATT MTU and the peer's,
	 * and at the same time ensure that we don't go lower than the actual MTU size.
	 */
	links[idx].att_mtu =
		MAX(links[idx].att_mtu, MIN(evt->client_rx_mtu, links[idx].att_mtu_desired));
	links[idx].att_mtu_exchange_pending = false;

	LOG_INF("Peer %#x requested ATT MTU of %u bytes", conn_handle, evt->client_rx_mtu);

	err = sd_ble_gatts_exchange_mtu_reply(conn_handle, links[idx].att_mtu);
	if (err) {
		LOG_ERR("Failed to reply to MTU exchange request, nrf_error %#x", err);
		return;
	}

	LOG_INF("ATT MTU set to %u bytes for peer %#x", links[idx].att_mtu, conn_handle);

	/* The ATT MTU exchange has finished, send an event to the application */
	const struct ble_conn_params_evt app_evt = {
		.id = BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED,
		.conn_handle = conn_handle,
		.att_mtu = links[idx].att_mtu,
	};

	ble_conn_params_event_send(&app_evt);
}

/* The effective ATT MTU is set to the lowest between what we requested and the peer's response.
 * This event concludes the ATT MTU exchange.
 */
static void on_exchange_mtu_rsp_evt(uint16_t conn_handle, int idx,
				    const ble_gattc_evt_exchange_mtu_rsp_t *evt)
{
	/* Determine the lowest ATT MTU between our own desired ATT MTU and the peer's. */
	links[idx].att_mtu = MIN(evt->server_rx_mtu, links[idx].att_mtu_desired);
	links[idx].att_mtu_exchange_pending = false;

	LOG_INF("ATT MTU set to %u bytes for peer %#x", links[idx].att_mtu, conn_handle);

	/* The ATT MTU exchange has finished, send an event to the application. */
	const struct ble_conn_params_evt app_evt = {
		.id = BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED,
		.conn_handle = conn_handle,
		.att_mtu = links[idx].att_mtu,
	};

	ble_conn_params_event_send(&app_evt);
}

static void on_connected(uint16_t conn_handle, int idx, const ble_gap_evt_connected_t *evt)
{
	ARG_UNUSED(evt);

	if (IS_ENABLED(CONFIG_BLE_CONN_PARAMS_INITIATE_ATT_MTU_EXCHANGE)) {
		LOG_INF("Initiating ATT MTU exchange procedure (%u -> %u bytes) for peer %#x",
			links[idx].att_mtu, links[idx].att_mtu_desired, conn_handle);

		mtu_exchange_request(conn_handle, idx);
	}
}

static void on_disconnected(uint16_t conn_handle, int idx)
{
	ARG_UNUSED(conn_handle);

	links[idx].att_mtu = BLE_GATT_ATT_MTU_DEFAULT;
	links[idx].att_mtu_desired = CONFIG_BLE_CONN_PARAMS_ATT_MTU;
	links[idx].att_mtu_exchange_pending = false;
}

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	const uint16_t conn_handle = evt->evt.common_evt.conn_handle;
	const int idx = nrf_sdh_ble_idx_get(conn_handle);

	__ASSERT(idx >= 0, "Invalid idx %d for conn_handle %#x, evt_id %#x",
		 idx, conn_handle, evt->header.evt_id);

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		on_connected(conn_handle, idx, &evt->evt.gap_evt.params.connected);
		/* There is no pending ATT MTU exchange or the exchange was just attempted
		 * and retry is not needed immediately. Return here.
		 */
		return;
	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnected(conn_handle, idx);
		/* No neeed to retry ATT MTU exchange if disconnected. Return here. */
		return;

	case BLE_GATTC_EVT_EXCHANGE_MTU_RSP:
		on_exchange_mtu_rsp_evt(
			conn_handle, idx, &evt->evt.gattc_evt.params.exchange_mtu_rsp);
		break;
	case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
		on_exchange_mtu_req_evt(
			conn_handle, idx, &evt->evt.gatts_evt.params.exchange_mtu_request);
		break;
	default:
		/* Ignore */
		break;
	}

	/* Retry the ATT MTU exchange procedure for the current connection handle
	 * if the SoftDevice was previously busy.
	 */
	if (links[idx].att_mtu_exchange_pending) {
		links[idx].att_mtu_exchange_pending = false;
		mtu_exchange_request(conn_handle, idx);
	}
}
NRF_SDH_BLE_OBSERVER(ble_observer, on_ble_evt, NULL, 0);

int ble_conn_params_att_mtu_set(uint16_t conn_handle, uint16_t att_mtu)
{
	const int idx = nrf_sdh_ble_idx_get(conn_handle);

	if (idx < 0) {
		return -EINVAL;
	}

	if (att_mtu < BLE_GATT_ATT_MTU_DEFAULT || CONFIG_BLE_CONN_PARAMS_ATT_MTU < att_mtu) {
		return -EINVAL;
	}

	links[idx].att_mtu_desired = att_mtu;
	mtu_exchange_request(conn_handle, idx);

	return 0;
}

int ble_conn_params_att_mtu_get(uint16_t conn_handle, uint16_t *att_mtu)
{
	const int idx = nrf_sdh_ble_idx_get(conn_handle);

	if (idx < 0) {
		return -EINVAL;
	}

	if (!att_mtu) {
		return -EFAULT;
	}

	*att_mtu = links[idx].att_mtu;

	return 0;
}
