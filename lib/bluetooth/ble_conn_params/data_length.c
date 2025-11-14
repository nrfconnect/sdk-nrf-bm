/*
 * Copyright (c) 2012-2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <nrf_error.h>
#include <ble_gap.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ble_conn_params, CONFIG_BLE_CONN_PARAMS_LOG_LEVEL);

#define BLE_GAP_DATA_LENGTH_DEFAULT 27
#define BLE_GAP_DATA_LENGTH_MAX 251

extern void ble_conn_params_event_send(const struct ble_conn_params_evt *evt);

static struct {
	struct ble_conn_params_data_length data_length;
	struct ble_conn_params_data_length desired;
	uint8_t data_length_update_pending : 1;
} links[CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT] = {
	[0 ... CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT - 1] = {
		.data_length.tx = BLE_GAP_DATA_LENGTH_DEFAULT,
		.data_length.rx = BLE_GAP_DATA_LENGTH_DEFAULT,
		.desired.tx = CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_TX,
		.desired.rx = CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_RX,
	}
};

static void data_length_update(uint16_t conn_handle, int idx)
{
	uint32_t nrf_err;
	bool retry;
	ble_gap_data_length_params_t dlp = {
		.max_tx_octets = links[idx].desired.tx,
		.max_rx_octets = links[idx].desired.rx,
		.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
		.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
	};
	ble_gap_data_length_limitation_t dll = {0};

	do {
		retry = false;

		nrf_err = sd_ble_gap_data_length_update(conn_handle, &dlp, &dll);
		if (nrf_err == NRF_ERROR_BUSY) {
			/* Retry later. */
			LOG_DBG("Another procedure is ongoing, will retry");
			links[idx].data_length_update_pending = true;
			return;
		} else if (nrf_err == NRF_ERROR_RESOURCES) {
			if ((dll.tx_payload_limited_octets != 0) ||
			    (dll.rx_payload_limited_octets != 0)) {
				LOG_WRN("The requested TX and RX packet lengths are too long by "
					"%u, %u bytes.", dll.tx_payload_limited_octets,
					dll.rx_payload_limited_octets);

				/* Lower the desired max data length to the highest value
				 * the SoftDevice can support with current configuration,
				 * then retry.
				 */
				links[idx].desired.tx -= dll.tx_payload_limited_octets;
				links[idx].desired.rx -= dll.rx_payload_limited_octets;
				dlp.max_tx_octets = links[idx].desired.tx,
				dlp.max_rx_octets = links[idx].desired.rx,

				retry = true;
			}
			if (dll.tx_rx_time_limited_us != 0) {
				LOG_ERR("The requested combination of TX and RX packet lengths "
					"is too long by %u microseconds.",
					dll.tx_rx_time_limited_us);
			}
		} else if (nrf_err) {
			LOG_ERR("Failed to initiate or respond to Data Length Update procedure, "
				"nrf_error %#x", nrf_err);
		}
	} while (retry);
}

static void on_data_length_update_request_evt(uint16_t conn_handle, int idx,
					      const ble_gap_evt_data_length_update_request_t *evt)
{
	const struct ble_conn_params_data_length dl_requested = {
		.tx = evt->peer_params.max_tx_octets,
		.rx = evt->peer_params.max_rx_octets,
	};

	LOG_INF("Peer %#x requested data length of TX %u, RX %u bytes", conn_handle,
		dl_requested.tx, dl_requested.rx);

	/* The peer's RX/TX is our TX/RX. Flip the values here. */
	links[idx].desired.tx = MIN(dl_requested.rx, CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_TX);
	links[idx].desired.rx = MIN(dl_requested.tx, CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_RX);

	data_length_update(conn_handle, idx);
}

static void on_data_length_update_evt(uint16_t conn_handle, int idx,
				      const ble_gap_evt_data_length_update_t *evt)
{
	/* Update the connection data length. */
	links[idx].data_length.tx = evt->effective_params.max_tx_octets;
	links[idx].data_length.rx = evt->effective_params.max_rx_octets;
	links[idx].data_length_update_pending = false;

	LOG_INF("Data length updated to TX %u, RX %u for connection %#x",
		links[idx].data_length.tx, links[idx].data_length.rx, conn_handle);

	/* The Data length update has finished, send an event to the application. */
	const struct ble_conn_params_evt app_evt = {
		.id = BLE_CONN_PARAMS_EVT_DATA_LENGTH_UPDATED,
		.conn_handle = conn_handle,
		.data_length.tx = links[idx].data_length.tx,
		.data_length.rx = links[idx].data_length.rx,
	};

	ble_conn_params_event_send(&app_evt);
}

static void on_connected(uint16_t conn_handle, int idx, const ble_gap_evt_connected_t *evt)
{
	ARG_UNUSED(evt);

	if (IS_ENABLED(CONFIG_BLE_CONN_PARAMS_INITIATE_DATA_LENGTH_UPDATE)) {
		LOG_INF("Initiating Data Length Update procedure (TX %u -> %u, RX %u -> %u bytes) "
			"for peer %#x", links[idx].data_length.tx, links[idx].desired.tx,
			links[idx].data_length.rx, links[idx].desired.rx, conn_handle);

		data_length_update(conn_handle, idx);
	}
}

static void on_disconnected(uint16_t conn_handle, int idx)
{
	ARG_UNUSED(conn_handle);

	links[idx].data_length.tx = BLE_GAP_DATA_LENGTH_DEFAULT;
	links[idx].data_length.rx = BLE_GAP_DATA_LENGTH_DEFAULT;
	links[idx].desired.tx = CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_TX;
	links[idx].desired.rx = CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_RX;
	links[idx].data_length_update_pending = false;
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
		/* There is no pending Data Length update or the update was just attempted
		 * and retry is not needed immediately. Return here.
		 */
		return;
	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnected(conn_handle, idx);
		/* No neeed to retry Data Length update if disconnected. Return here. */
		return;

	case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
		on_data_length_update_evt(
			conn_handle, idx, &evt->evt.gap_evt.params.data_length_update);
		break;
	case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
		on_data_length_update_request_evt(
			conn_handle, idx, &evt->evt.gap_evt.params.data_length_update_request);
		break;

	default:
		/* Ignore */
		break;
	}

	/* Retry the Data Length Update procedure for the current connection handle
	 * if the SoftDevice was previously busy.
	 */
	if (links[idx].data_length_update_pending) {
		links[idx].data_length_update_pending = false;
		data_length_update(conn_handle, idx);
	}
}
NRF_SDH_BLE_OBSERVER(ble_observer, on_ble_evt, NULL, HIGH);

uint32_t ble_conn_params_data_length_set(uint16_t conn_handle,
					 struct ble_conn_params_data_length dl)
{
	const int idx = nrf_sdh_ble_idx_get(conn_handle);

	if (idx < 0) {
		return NRF_ERROR_INVALID_PARAM;
	}

	if (dl.tx < BLE_GAP_DATA_LENGTH_DEFAULT || dl.tx > CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_TX ||
	    dl.rx < BLE_GAP_DATA_LENGTH_DEFAULT || dl.rx > CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_RX) {
		return NRF_ERROR_INVALID_PARAM;
	}

	links[idx].desired.tx = dl.tx;
	links[idx].desired.rx = dl.rx;
	data_length_update(conn_handle, idx);

	return NRF_SUCCESS;
}

uint32_t ble_conn_params_data_length_get(uint16_t conn_handle,
					 struct ble_conn_params_data_length *dl)
{
	const int idx = nrf_sdh_ble_idx_get(conn_handle);

	if (idx < 0) {
		return NRF_ERROR_INVALID_PARAM;
	}

	if (!dl) {
		return NRF_ERROR_NULL;
	}

	dl->tx = links[idx].data_length.tx;
	dl->rx = links[idx].data_length.rx;

	return NRF_SUCCESS;
}
