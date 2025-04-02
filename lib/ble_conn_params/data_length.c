/*
 * Copyright (c) 2012-2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <ble_gap.h>
#include <ble_conn_params.h>
#include <nrf_sdh_ble.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ble_conn_params, CONFIG_BLE_CONN_PARAMS_LOG_LEVEL);

#define BLE_GAP_DATA_LENGTH_DEFAULT 27
#define BLE_GAP_DATA_LENGTH_MAX 251

extern void ble_conn_params_event_send(const struct ble_conn_params_evt *evt);

static struct {
	uint8_t data_length;
	uint8_t data_length_update_pending : 1;
} links[CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT] = {
	[0 ... CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT - 1] = {
		.data_length = CONFIG_BLE_CONN_PARAMS_DATA_LENGTH,
	}
};

static void data_length_update(uint16_t conn_handle, int idx)
{
	int err;
	const ble_gap_data_length_params_t dlp = {
		.max_rx_octets = links[idx].data_length,
		.max_tx_octets = links[idx].data_length,
		.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
		.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
	};
	ble_gap_data_length_limitation_t dll = {0};

	err = sd_ble_gap_data_length_update(conn_handle, &dlp, &dll);
	if (!err) {
		return;
	}

	if (err == NRF_ERROR_BUSY) {
		/* Retry */
		LOG_DBG("Another procedure is ongoing, will retry");
		links[idx].data_length_update_pending = true;
	} else if (err) {
		LOG_ERR("Failed to initiate Data Length Update procedure, nrf_error %#x", err);
		if ((dll.tx_payload_limited_octets != 0) || (dll.rx_payload_limited_octets != 0)) {
			LOG_ERR("The requested TX/RX packet length is too long by %u/%u octets.",
				dll.tx_payload_limited_octets, dll.rx_payload_limited_octets);
		}
		if (dll.tx_rx_time_limited_us != 0) {
			LOG_ERR("The requested combination of TX and RX packet lengths "
				"is too long by %u microseconds.",
				dll.tx_rx_time_limited_us);
		}
	}
}

static void on_data_length_update_request_evt(uint16_t conn_handle, int idx,
					      const ble_gap_evt_data_length_update_request_t *evt)
{
	/* SoftDevice only supports symmetric RX/TX data length settings */
	const uint8_t data_length_requested = evt->peer_params.max_tx_octets;

	LOG_INF("Peer %#x requested a data length of %u bytes", conn_handle,
		data_length_requested);

	links[idx].data_length = MIN(data_length_requested, CONFIG_BLE_CONN_PARAMS_DATA_LENGTH);

	data_length_update(conn_handle, idx);
}

static void on_data_length_update_evt(uint16_t conn_handle, int idx,
				      const ble_gap_evt_data_length_update_t *evt)
{
	/* Update the connection data length.
	 * SoftDevice only supports symmetric RX/TX data length settings.
	 */
	links[idx].data_length = evt->effective_params.max_tx_octets;
	links[idx].data_length_update_pending = false;

	LOG_INF("Data length updated to %u for peer %#x", links[idx].data_length, conn_handle);

	/* The Data length update has finished, send an event to the application. */
	const struct ble_conn_params_evt app_evt = {
		.id = BLE_CONN_PARAMS_EVT_DATA_LENGTH_UPDATED,
		.conn_handle = conn_handle,
		.data_length = links[idx].data_length,
	};

	ble_conn_params_event_send(&app_evt);
}

static void on_connected(uint16_t conn_handle, int idx, const ble_gap_evt_connected_t *evt)
{
	ARG_UNUSED(evt);

	if (IS_ENABLED(CONFIG_BLE_CONN_PARAMS_INITIATE_DATA_LENGTH_UPDATE)) {
		LOG_INF("Initiating Data Length Update procedure (%u -> %u bytes) for peer %#x",
			BLE_GAP_DATA_LENGTH_DEFAULT, links[idx].data_length, conn_handle);

		data_length_update(conn_handle, idx);
	}
}

static void on_disconnected(uint16_t conn_handle, int idx)
{
	ARG_UNUSED(conn_handle);

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
NRF_SDH_BLE_OBSERVER(ble_observer, on_ble_evt, NULL, 0);

int ble_conn_params_data_length_set(uint16_t conn_handle, uint8_t data_length)
{
	const int idx = nrf_sdh_ble_idx_get(conn_handle);

	if (idx < 0) {
		return -EINVAL;
	}

	if (data_length < BLE_GAP_DATA_LENGTH_DEFAULT ||
	    data_length > BLE_GAP_DATA_LENGTH_MAX) {
		return -EINVAL;
	}

	links[idx].data_length = data_length;
	data_length_update(conn_handle, idx);

	return 0;
}

int ble_conn_params_data_length_get(uint16_t conn_handle, uint8_t *data_length)
{
	const int idx = nrf_sdh_ble_idx_get(conn_handle);

	if (idx < 0) {
		return -EINVAL;
	}

	if (!data_length) {
		return -EFAULT;
	}

	*data_length = links[idx].data_length;

	return 0;
}
