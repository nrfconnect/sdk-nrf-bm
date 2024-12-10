/*
 * Copyright (c) 2012-2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <ble_gap.h>
#include <ble_conn_params.h>
#include <errno.h>
#include <nrf_sdh_ble.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ble_conn_params, CONFIG_BLE_CONN_PARAMS_LOG_LEVEL);

extern void ble_conn_params_event_send(const struct ble_conn_params_evt *evt);

static struct {
	ble_gap_phys_t phy_mode;
	uint8_t phy_mode_update_pending : 1;
	uint8_t connected : 1;
} links[CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT] = {
	[0 ... CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT - 1] = {
		.phy_mode.tx_phys = CONFIG_BLE_CONN_PARAMS_PHY,
		.phy_mode.rx_phys = CONFIG_BLE_CONN_PARAMS_PHY,
	},
};

BUILD_ASSERT(CONFIG_BLE_CONN_PARAMS_PHY == BLE_GAP_PHY_AUTO ||
	     !!(CONFIG_BLE_CONN_PARAMS_PHY & BLE_GAP_PHYS_SUPPORTED), "Invalid PHY config");

static void radio_phy_mode_update(uint16_t conn_handle)
{
	int err;
	const ble_gap_phys_t phys = links[conn_handle].phy_mode;

	err = sd_ble_gap_phy_update(conn_handle, &phys);
	if (!err) {
		return;
	} else if (err == NRF_ERROR_BUSY) {
		/* Retry */
		links[conn_handle].phy_mode_update_pending = true;
		LOG_DBG("Failed PHY update procedure, another procedure is ongoing, "
			"Will retry");
	} else if (err == NRF_ERROR_RESOURCES) {
		/* PHY update failed. Use current PHY. */
		LOG_WRN("Failed PHY update procedure. Continue using current PHY mode");
		LOG_DBG("GAP event length (%d) may be too small",
			CONFIG_NRF_SDH_BLE_GAP_EVENT_LENGTH);
		links[conn_handle].phy_mode.tx_phys = CONFIG_BLE_CONN_PARAMS_PHY;
		links[conn_handle].phy_mode.rx_phys = CONFIG_BLE_CONN_PARAMS_PHY;
		radio_phy_mode_update(conn_handle);
	} else {
		LOG_ERR("Failed PHY update procedure, nrf_error %#x", err);
	}
}

static void on_radio_phy_mode_update_evt(uint16_t conn_handle, const ble_gap_evt_phy_update_t *evt)
{
	if (evt->status == BLE_HCI_STATUS_CODE_SUCCESS) {
		links[conn_handle].phy_mode_update_pending = false;
		links[conn_handle].phy_mode.tx_phys = evt->tx_phy;
		links[conn_handle].phy_mode.rx_phys = evt->rx_phy;
		LOG_INF("PHY updated for peer %#x, tx %u, rx %u", conn_handle,
			links[conn_handle].phy_mode.tx_phys, links[conn_handle].phy_mode.rx_phys);
	} else if (evt->status == BLE_HCI_DIFFERENT_TRANSACTION_COLLISION) {
		/* Retry */
		links[conn_handle].phy_mode_update_pending = true;
		LOG_DBG("Failed to initiate PHY update procedure, another procedure is ongoing, "
			"Will retry");
	} else {
		links[conn_handle].phy_mode_update_pending = false;
		LOG_ERR("PHY update failed with status %u for peer %#x", evt->status,
			conn_handle);
	}

	const struct ble_conn_params_evt app_evt = {
		.id = BLE_CONN_PARAMS_EVT_RADIO_PHY_MODE_UPDATED,
		.conn_handle = conn_handle,
		.phy_update_evt = *evt,
	};

	ble_conn_params_event_send(&app_evt);
}

static void on_radio_phy_mode_update_request_evt(uint16_t conn_handle,
						 const ble_gap_evt_phy_update_request_t *evt)
{
	LOG_INF("Peer %#x requested PHY update to tx %u, rx %u", conn_handle,
		evt->peer_preferred_phys.tx_phys, evt->peer_preferred_phys.rx_phys);

	links[conn_handle].phy_mode.tx_phys = evt->peer_preferred_phys.tx_phys;
	links[conn_handle].phy_mode.rx_phys = evt->peer_preferred_phys.rx_phys;

	radio_phy_mode_update(conn_handle);
}

static void on_connected(uint16_t conn_handle)
{
	if (IS_ENABLED(CONFIG_BLE_CONN_PARAMS_INITIATE_PHY_UPDATE)) {
		LOG_INF("Initiating PHY update procedure for peer %#x", conn_handle);
		radio_phy_mode_update(conn_handle);
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

	if (conn_handle > ARRAY_SIZE(links)) {
		return;
	}

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		on_connected(conn_handle);
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnected(conn_handle);
		break;

	case BLE_GAP_EVT_PHY_UPDATE:
		on_radio_phy_mode_update_evt(conn_handle, &evt->evt.gap_evt.params.phy_update);
		break;
	case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
		on_radio_phy_mode_update_request_evt(
			conn_handle, &evt->evt.gap_evt.params.phy_update_request);
		break;

	default:
		/* Ignore */
		break;
	}

	/* Retry any procedures that were busy */
	if (links[conn_handle].connected && links[conn_handle].phy_mode_update_pending) {
		links[conn_handle].phy_mode_update_pending = false;
		radio_phy_mode_update(conn_handle);
	}
}
NRF_SDH_BLE_OBSERVER(ble_observer, on_ble_evt, NULL, 0);

int ble_conn_params_phy_radio_mode_set(uint16_t conn_handle, ble_gap_phys_t phy_pref)
{
	if (conn_handle > ARRAY_SIZE(links)) {
		return -EINVAL;
	}
	if (!links[conn_handle].connected) {
		return -ENOTCONN;
	}

	links[conn_handle].phy_mode = phy_pref;
	radio_phy_mode_update(conn_handle);

	return 0;
}

int ble_conn_params_phy_radio_mode_get(uint16_t conn_handle, ble_gap_phys_t *phy_pref)
{
	if (conn_handle > ARRAY_SIZE(links)) {
		return -EINVAL;
	}
	if (!phy_pref) {
		return -EFAULT;
	}
	if (!links[conn_handle].connected) {
		return -ENOTCONN;
	}

	*phy_pref = links[conn_handle].phy_mode;

	return 0;
}
