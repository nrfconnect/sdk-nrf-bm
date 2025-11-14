/*
 * Copyright (c) 2012-2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <nrf_error.h>
#include <ble_gap.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ble_conn_params, CONFIG_BLE_CONN_PARAMS_LOG_LEVEL);

extern void ble_conn_params_event_send(const struct ble_conn_params_evt *evt);

/* Preferred connection parameters */
static const ble_gap_conn_params_t ppcp = {
	.min_conn_interval = CONFIG_BLE_CONN_PARAMS_MIN_CONN_INTERVAL,
	.max_conn_interval = CONFIG_BLE_CONN_PARAMS_MAX_CONN_INTERVAL,
	.slave_latency = CONFIG_BLE_CONN_PARAMS_PERIPHERAL_LATENCY,
	.conn_sup_timeout = CONFIG_BLE_CONN_PARAMS_SUP_TIMEOUT,
};

static struct {
	ble_gap_conn_params_t ppcp;
	uint8_t retries;
} links[CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT] = {
	[0 ... CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT - 1] = {
		.retries = CONFIG_BLE_CONN_PARAMS_NEGOTIATION_RETRIES,
	}
};

static void conn_params_negotiate(uint16_t conn_handle, int idx)
{
	uint32_t nrf_err;

	LOG_DBG("Negotiating desired parameters with peer %#x", conn_handle);

	nrf_err = sd_ble_gap_conn_param_update(conn_handle, &links[idx].ppcp);
	if (nrf_err) {
		LOG_ERR("Failed to request GAP connection parameters update, nrf_error %#x",
			nrf_err);
	}
}

static bool conn_params_can_agree(const ble_gap_conn_params_t *conn_params)
{
	uint16_t peripheral_latency_min;
	uint16_t peripheral_latency_max;
	uint16_t conn_sup_timeout_min;
	uint16_t conn_sup_timeout_max;

	/* The max_conn_interval field in the event contains the client connection interval */
	if ((conn_params->max_conn_interval < ppcp.min_conn_interval) ||
	    (conn_params->max_conn_interval > ppcp.max_conn_interval)) {
		LOG_DBG("Could not agree on connection interval %#x",
			conn_params->max_conn_interval);
		return false;
	}

	peripheral_latency_min =
		CLAMP(ppcp.slave_latency - CONFIG_BLE_CONN_PARAMS_MAX_PERIPHERAL_LATENCY_DEVIATION,
		      0, UINT16_MAX);
	peripheral_latency_max =
		CLAMP(ppcp.slave_latency + CONFIG_BLE_CONN_PARAMS_MAX_PERIPHERAL_LATENCY_DEVIATION,
		      0, UINT16_MAX);

	if (conn_params->slave_latency < peripheral_latency_min ||
	    conn_params->slave_latency > peripheral_latency_max) {
		LOG_DBG("Could not agree on peripheral latency %#x", conn_params->slave_latency);
		return false;
	}

	conn_sup_timeout_min =
		CLAMP(ppcp.conn_sup_timeout - CONFIG_BLE_CONN_PARAMS_MAX_SUP_TIMEOUT_DEVIATION, 0,
		      UINT16_MAX);
	conn_sup_timeout_max =
		CLAMP(ppcp.conn_sup_timeout + CONFIG_BLE_CONN_PARAMS_MAX_SUP_TIMEOUT_DEVIATION, 0,
		      UINT16_MAX);

	if (conn_params->conn_sup_timeout < conn_sup_timeout_min ||
	    conn_params->conn_sup_timeout > conn_sup_timeout_max) {
		LOG_DBG("Could not agree on supervision timeout %#x",
			conn_params->conn_sup_timeout);
		return false;
	}

	return true;
}

static void on_connected(uint16_t conn_handle, int idx, const ble_gap_evt_connected_t *evt)
{
	links[idx].retries = CONFIG_BLE_CONN_PARAMS_NEGOTIATION_RETRIES;

	/* Copy default ppcp */
	memcpy(&links[idx].ppcp, &ppcp, sizeof(ble_gap_conn_params_t));

	if (evt->role == BLE_GAP_ROLE_PERIPH) {
		if (!conn_params_can_agree(&evt->conn_params)) {
			conn_params_negotiate(conn_handle, idx);
		}
	}
}

static void on_conn_params_update(uint16_t conn_handle, int idx,
				  const ble_gap_evt_conn_param_update_t *evt)
{
	LOG_DBG("GAP connection params updated, conn. interval min %u max %u,"
		" peripheral latency %u, sup. timeout %u",
		evt->conn_params.min_conn_interval,
		evt->conn_params.max_conn_interval,
		evt->conn_params.slave_latency,
		evt->conn_params.conn_sup_timeout);

	if (conn_params_can_agree(&evt->conn_params)) {
		const struct ble_conn_params_evt app_evt = {
			.id = BLE_CONN_PARAMS_EVT_UPDATED,
			.conn_handle = conn_handle,
			.conn_params = evt->conn_params,
		};

		ble_conn_params_event_send(&app_evt);
		return;
	}

	if (links[idx].retries) {
		links[idx].retries--;
		conn_params_negotiate(conn_handle, idx);
		return;
	}

	LOG_WRN("Could not agree on peer %#x connection params", conn_handle);
	const struct ble_conn_params_evt app_evt = {
		.id = BLE_CONN_PARAMS_EVT_REJECTED,
		.conn_handle = conn_handle,
	};

	ble_conn_params_event_send(&app_evt);

	if (IS_ENABLED(CONFIG_BLE_CONN_PARAMS_DISCONNECT_ON_FAILURE)) {
		LOG_INF("Disconnecting from peer %#x", conn_handle);
		(void)sd_ble_gap_disconnect(conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
	}
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
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		break;

	case BLE_GAP_EVT_CONN_PARAM_UPDATE:
		on_conn_params_update(conn_handle, idx, &evt->evt.gap_evt.params.conn_param_update);
		break;

	default:
		/* Ignore */
		break;
	}
}
NRF_SDH_BLE_OBSERVER(ble_observer, on_ble_evt, NULL, HIGH);

static int on_state_evt(enum nrf_sdh_state_evt evt, void *ctx)
{
	uint32_t nrf_err;

	if (evt != NRF_SDH_STATE_EVT_BLE_ENABLED) {
		return 0;
	}

	nrf_err = sd_ble_gap_ppcp_set(&ppcp);
	if (nrf_err) {
		LOG_ERR("Failed to set preferred conn params, nrf_error %#x", nrf_err);
		return 0;
	}

	LOG_DBG("conn. interval min %u max %u, peripheral latency %u, sup. timeout %u",
		ppcp.min_conn_interval, ppcp.max_conn_interval,
		ppcp.slave_latency,
		ppcp.conn_sup_timeout);

	return 0;
}
NRF_SDH_STATE_EVT_OBSERVER(ble_conn_params_sdh_state_observer, on_state_evt, NULL, HIGH);

uint32_t ble_conn_params_override(uint16_t conn_handle, const ble_gap_conn_params_t *conn_params)
{
	uint32_t nrf_err;
	const int idx = nrf_sdh_ble_idx_get(conn_handle);

	if (idx < 0) {
		return BLE_ERROR_INVALID_CONN_HANDLE;
	}
	if (!conn_params) {
		return NRF_ERROR_NULL;
	}

	links[idx].ppcp = *conn_params;
	nrf_err = sd_ble_gap_conn_param_update(conn_handle, conn_params);
	if (nrf_err) {
		return nrf_err;
	}

	return NRF_SUCCESS;
}
