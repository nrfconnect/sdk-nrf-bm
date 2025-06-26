/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stddef.h>
#include <stdint.h>
#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <ble.h>
#include <zephyr/logging/log.h>

#define APP_RAM_START DT_REG_ADDR(DT_CHOSEN(zephyr_sram))

LOG_MODULE_DECLARE(nrf_sdh, CONFIG_NRF_SDH_LOG_LEVEL);

const char *gap_evt_tostr(int evt)
{
	switch (evt) {
	case BLE_GAP_EVT_CONNECTED:
		return "BLE_GAP_EVT_CONNECTED";
	case BLE_GAP_EVT_DISCONNECTED:
		return "BLE_GAP_EVT_DISCONNECTED";
	case BLE_GAP_EVT_CONN_PARAM_UPDATE:
		return "BLE_GAP_EVT_CONN_PARAM_UPDATE";
	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		return "BLE_GAP_EVT_SEC_PARAMS_REQUEST";
	case BLE_GAP_EVT_SEC_INFO_REQUEST:
		return "BLE_GAP_EVT_SEC_INFO_REQUEST";
	case BLE_GAP_EVT_PASSKEY_DISPLAY:
		return "BLE_GAP_EVT_PASSKEY_DISPLAY";
	case BLE_GAP_EVT_KEY_PRESSED:
		return "BLE_GAP_EVT_KEY_PRESSED";
	case BLE_GAP_EVT_AUTH_KEY_REQUEST:
		return "BLE_GAP_EVT_AUTH_KEY_REQUEST";
	case BLE_GAP_EVT_LESC_DHKEY_REQUEST:
		return "BLE_GAP_EVT_LESC_DHKEY_REQUEST";
	case BLE_GAP_EVT_AUTH_STATUS:
		return "BLE_GAP_EVT_AUTH_STATUS";
	case BLE_GAP_EVT_CONN_SEC_UPDATE:
		return "BLE_GAP_EVT_CONN_SEC_UPDATE";
	case BLE_GAP_EVT_TIMEOUT:
		return "BLE_GAP_EVT_TIMEOUT";
	case BLE_GAP_EVT_RSSI_CHANGED:
		return "BLE_GAP_EVT_RSSI_CHANGED";
#if defined(CONFIG_SOFTDEVICE_CENTRAL)
	case BLE_GAP_EVT_ADV_REPORT:
		return "BLE_GAP_EVT_ADV_REPORT";
	case BLE_GAP_EVT_SEC_REQUEST:
		return "BLE_GAP_EVT_SEC_REQUEST";
	case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
		return "BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST";
#endif
	case BLE_GAP_EVT_SCAN_REQ_REPORT:
		return "BLE_GAP_EVT_SCAN_REQ_REPORT";
	case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
		return "BLE_GAP_EVT_PHY_UPDATE_REQUEST";
	case BLE_GAP_EVT_PHY_UPDATE:
		return "BLE_GAP_EVT_PHY_UPDATE";
#if defined(CONFIG_SOFTDEVICE_DATA_LENGTH_UPDATE)
	case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
		return "BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST";
	case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
		return "BLE_GAP_EVT_DATA_LENGTH_UPDATE";
#endif
#if defined(CONFIG_SOFTDEVICE_QOS_CHANNEL_SURVEY_REPORT)
	case BLE_GAP_EVT_QOS_CHANNEL_SURVEY_REPORT:
		return "BLE_GAP_EVT_QOS_CHANNEL_SURVEY_REPORT";
#endif
	case BLE_GAP_EVT_ADV_SET_TERMINATED:
		return "BLE_GAP_EVT_ADV_SET_TERMINATED";
	default:
		return "unknown";
	}
}

int nrf_sdh_ble_app_ram_start_get(uint32_t *app_ram_start)
{
	if (!app_ram_start) {
		return -EFAULT;
	}

	*app_ram_start = APP_RAM_START;

	return 0;
}

static int default_cfg_set(void)
{
	int err;
	ble_cfg_t ble_cfg;

	const uint32_t app_ram_start = APP_RAM_START;
	const uint8_t  conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG;

	/* Save a configuration fo the BLE stack in a given connection tag.
	 * If any of the calls to sd_ble_cfg_set() fail, log the error but carry on so that
	 * wrong RAM settings can be caught by nrf_sdh_ble_enable() and a meaningful error
	 * message will be printed to the user suggesting the correct value.
	 */

	/* Configure the connection count. */
	memset(&ble_cfg, 0x00, sizeof(ble_cfg));
	ble_cfg.conn_cfg.conn_cfg_tag = conn_cfg_tag;
	ble_cfg.conn_cfg.params.gap_conn_cfg.conn_count = CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT;
	ble_cfg.conn_cfg.params.gap_conn_cfg.event_length = CONFIG_NRF_SDH_BLE_GAP_EVENT_LENGTH;

	err = sd_ble_cfg_set(BLE_CONN_CFG_GAP, &ble_cfg, app_ram_start);
	if (err) {
		LOG_WRN("Failed to set BLE_CONN_CFG_GAP, nrf_error %#x", err);
	}

	/* Configure the connection roles. */
	memset(&ble_cfg, 0, sizeof(ble_cfg));
#if CONFIG_SOFTDEVICE_PERIPHERAL
		ble_cfg.gap_cfg.role_count_cfg.periph_role_count =
			CONFIG_NRF_SDH_BLE_PERIPHERAL_LINK_COUNT;
		ble_cfg.gap_cfg.role_count_cfg.adv_set_count = BLE_GAP_ADV_SET_COUNT_DEFAULT;
#endif

#if CONFIG_SOFTDEVICE_CENTRAL
		ble_cfg.gap_cfg.role_count_cfg.central_role_count =
			CONFIG_NRF_SDH_BLE_CENTRAL_LINK_COUNT;
		ble_cfg.gap_cfg.role_count_cfg.central_sec_count =
			MIN(CONFIG_NRF_SDH_BLE_CENTRAL_LINK_COUNT,
			    BLE_GAP_ROLE_COUNT_CENTRAL_SEC_DEFAULT);
#endif

	err = sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &ble_cfg, app_ram_start);
	if (err) {
		LOG_WRN("Failed to set BLE_GAP_CFG_ROLE_COUNT, nrf_error %#x", err);
	}

	/* Configure the maximum ATT MTU. */
#if (CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE != 23)
	memset(&ble_cfg, 0x00, sizeof(ble_cfg));
	ble_cfg.conn_cfg.conn_cfg_tag = conn_cfg_tag;
	ble_cfg.conn_cfg.params.gatt_conn_cfg.att_mtu = CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE;

	err = sd_ble_cfg_set(BLE_CONN_CFG_GATT, &ble_cfg, app_ram_start);
	if (err) {
		LOG_WRN("Failed to set BLE_CONN_CFG_GATT, nrf_error %#x", err);
	}
#endif /* NRF_SDH_BLE_GATT_MAX_MTU_SIZE != 23 */

	/* Configure number of custom UUIDS. */
	memset(&ble_cfg, 0, sizeof(ble_cfg));
	ble_cfg.common_cfg.vs_uuid_cfg.vs_uuid_count = CONFIG_NRF_SDH_BLE_VS_UUID_COUNT;

	err = sd_ble_cfg_set(BLE_COMMON_CFG_VS_UUID, &ble_cfg, app_ram_start);
	if (err) {
		LOG_WRN("Failed to set BLE_COMMON_CFG_VS_UUID, nrf_error %#x", err);
	}

	/* Configure the GATTS attribute table. */
	memset(&ble_cfg, 0x00, sizeof(ble_cfg));
	ble_cfg.gatts_cfg.attr_tab_size.attr_tab_size = CONFIG_NRF_SDH_BLE_GATTS_ATTR_TAB_SIZE;

	err = sd_ble_cfg_set(BLE_GATTS_CFG_ATTR_TAB_SIZE, &ble_cfg, app_ram_start);
	if (err) {
		LOG_WRN("Failed to set BLE_GATTS_CFG_ATTR_TAB_SIZE, nrf_error %#x", err);
	}

	/* Configure Service Changed characteristic. */
	memset(&ble_cfg, 0x00, sizeof(ble_cfg));
	ble_cfg.gatts_cfg.service_changed.service_changed =
		IS_ENABLED(CONFIG_NRF_SDH_BLE_SERVICE_CHANGED);

	err = sd_ble_cfg_set(BLE_GATTS_CFG_SERVICE_CHANGED, &ble_cfg, app_ram_start);
	if (err) {
		LOG_WRN("Failed to set BLE_GATTS_CFG_SERVICE_CHANGED, nrf_error %#x", err);
	}

	LOG_DBG("SoftDevice configuration applied");

	return 0;
}

int nrf_sdh_ble_enable(uint8_t conn_cfg_tag)
{
	int err;
	uint32_t app_ram_minimum = APP_RAM_START;
	uint32_t const app_ram_start_link = APP_RAM_START;

	default_cfg_set();

	LOG_DBG("Application RAM starts at 0x%x", app_ram_start_link);

	err = sd_ble_enable(&app_ram_minimum);
	if (app_ram_minimum > app_ram_start_link) {
		LOG_ERR("Insufficient RAM allocated for the SoftDevice (have %#x, need %#x)",
			app_ram_start_link, app_ram_minimum);
	} else if (app_ram_minimum != app_ram_start_link) {
		LOG_DBG("Application RAM start location can be adjusted to %#x", app_ram_minimum);
	}

	if (err) {
		LOG_ERR("Failed to enable BLE, nrf_error %#x", err);
		return err;
	}

	LOG_DBG("SoftDevice BLE enabled");

	TYPE_SECTION_FOREACH(struct nrf_sdh_state_evt_observer, nrf_sdh_state_evt_observers, obs) {
		obs->handler(NRF_SDH_STATE_EVT_BLE_ENABLED, obs->context);
	}

	return 0;
}

static uint16_t conn_handles[CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT] = {
	[0 ... CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT - 1] = BLE_CONN_HANDLE_INVALID,
};

int _nrf_sdh_ble_idx_get(uint16_t conn_handle)
{
	for (int idx = 0; idx < ARRAY_SIZE(conn_handles); idx++) {
		if (conn_handles[idx] == conn_handle) {
			return idx;
		}
	}

	return -1;
}

static void idx_assign(uint16_t conn_handle)
{
	__ASSERT(conn_handle != BLE_CONN_HANDLE_INVALID, "Got invalid conn_handle from SoftDevice");

	for (int idx = 0; idx < ARRAY_SIZE(conn_handles); idx++) {
		__ASSERT(conn_handles[idx] != conn_handle,
			 "conn_handle %#x is already assigned to idx %d", conn_handle, idx);
	}

	for (int idx = 0; idx < ARRAY_SIZE(conn_handles); idx++) {
		if (conn_handles[idx] == BLE_CONN_HANDLE_INVALID) {
			conn_handles[idx] = conn_handle;
			LOG_DBG("Assigned idx %d to conn_handle %#x", idx, conn_handle);
			return;
		}
	}

	__ASSERT(false, "Failed to assign idx to conn_handle %#x", conn_handle);

	LOG_ERR("Failed to assign idx to conn_handle %#x", conn_handle);
}

static void idx_unassign(uint16_t conn_handle)
{
	for (int idx = 0; idx < ARRAY_SIZE(conn_handles); idx++) {
		if (conn_handles[idx] == conn_handle) {
			conn_handles[idx] = BLE_CONN_HANDLE_INVALID;
			LOG_DBG("Unassigned idx %d from conn_handle %#x", idx, conn_handle);
			return;
		}
	}

	__ASSERT(false, "Could not find any idx assigned to conn_handle %#x", conn_handle);
}

static void ble_evt_poll(void *context)
{
	int err;

	__aligned(4) static uint8_t evt_buffer[NRF_SDH_BLE_EVT_BUF_SIZE];
	ble_evt_t * const ble_evt = (ble_evt_t *)evt_buffer;

	while (true) {
		uint16_t evt_len = (uint16_t)sizeof(evt_buffer);

		err = sd_ble_evt_get(evt_buffer, &evt_len);
		if (err) {
			break;
		}

		if (IS_ENABLED(CONFIG_NRF_SDH_STR_TABLES)) {
			LOG_DBG("BLE event: %s", gap_evt_tostr(ble_evt->header.evt_id));
		} else {
			LOG_DBG("BLE event: %#x", ble_evt->header.evt_id);
		}

		if ((CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT > 1) &&
		    (ble_evt->header.evt_id == BLE_GAP_EVT_CONNECTED)) {
			idx_assign(ble_evt->evt.gap_evt.conn_handle);
		}

		/* Forward the event to BLE observers. */
		TYPE_SECTION_FOREACH(
			struct nrf_sdh_ble_evt_observer, nrf_sdh_ble_evt_observers, obs) {
			obs->handler(ble_evt, obs->context);
		}

		if ((CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT > 1) &&
		    (ble_evt->header.evt_id == BLE_GAP_EVT_DISCONNECTED)) {
			idx_unassign(ble_evt->evt.gap_evt.conn_handle);
		}
	}

	__ASSERT(err == NRF_ERROR_NOT_FOUND,
		"Failed to receive SoftDevice event, nrf_error %#x", err);
}

/* Listen to SoftDevice events */
NRF_SDH_STACK_EVT_OBSERVER(ble_evt_obs, ble_evt_poll, NULL, 0);
