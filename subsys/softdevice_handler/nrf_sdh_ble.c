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

const char *sd_evt_tostr(int evt);

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
		LOG_DBG("RAM start location can be adjusted to %#x", app_ram_minimum);
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

		if (IS_ENABLED(CONFIG_SOFTDEVICE_STRING_TABLES)) {
			LOG_DBG("BLE event: %s", sd_evt_tostr(ble_evt->header.evt_id));
		} else {
			LOG_DBG("BLE event: %#x", ble_evt->header.evt_id);
		}

		/* Forward the event to BLE observers. */
		TYPE_SECTION_FOREACH(
			struct nrf_sdh_ble_evt_observer, nrf_sdh_ble_evt_observers, obs) {
			obs->handler(ble_evt, obs->context);
		}
	}

	__ASSERT(err == NRF_ERROR_NOT_FOUND,
		"Failed to receive SoftDevice event, nrf_error %#x", err);
}

/* Listen to SoftDevice events */
NRF_SDH_STACK_EVT_OBSERVER(ble_evt_obs, ble_evt_poll, NULL, 0);
