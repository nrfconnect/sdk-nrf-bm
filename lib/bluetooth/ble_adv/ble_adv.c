/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <nrf_error.h>
#include "zephyr/toolchain.h"
#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/ble_adv_data.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_adv, CONFIG_BLE_ADV_LOG_LEVEL);

/* Total number of possible advertising modes  */
#define BLE_ADV_MODES (5)

static bool whitelist_has_entries(struct ble_adv *ble_adv)
{
	return ble_adv->whitelist_in_use;
}

static bool use_whitelist(struct ble_adv *ble_adv)
{
	return (IS_ENABLED(CONFIG_BLE_ADV_USE_WHITELIST) &&
		!ble_adv->whitelist_temporarily_disabled && whitelist_has_entries(ble_adv));
}

static bool peer_addr_is_valid(const ble_gap_addr_t *addr)
{
	for (uint32_t i = 0; i < BLE_GAP_ADDR_LEN; i++) {
		if (addr->addr[i] != 0) {
			return true;
		}
	}
	return false;
}

static enum ble_adv_mode adv_mode_next(enum ble_adv_mode adv_mode)
{
	return (enum ble_adv_mode)((adv_mode + 1) % BLE_ADV_MODES);
}

static bool adv_mode_is_directed(enum ble_adv_mode mode)
{
	switch (mode) {
	case BLE_ADV_MODE_DIRECTED_HIGH_DUTY:
	case BLE_ADV_MODE_DIRECTED:
		return true;
	default:
		return false;
	}
}

static bool adv_mode_has_whitelist(enum ble_adv_mode mode)
{
	switch (mode) {
	case BLE_ADV_MODE_FAST:
	case BLE_ADV_MODE_SLOW:
		return true;
	default:
		return false;
	}
}

static void on_connected(struct ble_adv *ble_adv, ble_evt_t const *ble_evt)
{
	if (ble_evt->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_PERIPH) {
		ble_adv->conn_handle = ble_evt->evt.gap_evt.conn_handle;
	}
}

static void on_disconnected(struct ble_adv *ble_adv, ble_evt_t const *ble_evt)
{
	uint32_t nrf_err;
	struct ble_adv_evt adv_evt;

	ble_adv->whitelist_temporarily_disabled = false;

	if (IS_ENABLED(CONFIG_BLE_ADV_RESTART_ON_DISCONNECT)) {
		if (ble_evt->evt.gap_evt.conn_handle == ble_adv->conn_handle) {
			nrf_err = ble_adv_start(ble_adv, BLE_ADV_MODE_DIRECTED_HIGH_DUTY);
			if (nrf_err) {
				adv_evt.evt_type = BLE_ADV_EVT_ERROR;
				adv_evt.error.reason = nrf_err;
				ble_adv->evt_handler(ble_adv, &adv_evt);
			}
		}
	}
}

static void on_terminated(struct ble_adv *ble_adv, ble_evt_t const *ble_evt)
{
	uint32_t nrf_err;
	const uint8_t reason = ble_evt->evt.gap_evt.params.adv_set_terminated.reason;
	struct ble_adv_evt adv_evt;

	/* Start advertising in the next mode */
	if (reason == BLE_GAP_EVT_ADV_SET_TERMINATED_REASON_TIMEOUT ||
	    reason == BLE_GAP_EVT_ADV_SET_TERMINATED_REASON_LIMIT_REACHED) {
		LOG_DBG("Advertising timeout");
		nrf_err = ble_adv_start(ble_adv, adv_mode_next(ble_adv->mode_current));
		if (nrf_err) {
			adv_evt.error.reason = nrf_err;
			adv_evt.evt_type = BLE_ADV_EVT_ERROR;
			ble_adv->evt_handler(ble_adv, &adv_evt);
		}
	}
}

static uint32_t flags_set(struct ble_adv *ble_adv, uint8_t flags)
{
	uint32_t nrf_err;
	uint8_t *parsed_flags;

	parsed_flags = ble_adv_data_parse(ble_adv->adv_data.adv_data.p_data,
					  ble_adv->adv_data.adv_data.len, BLE_GAP_AD_TYPE_FLAGS);

	if (!parsed_flags) {
		LOG_WRN("Unable to find flags in current advertising data");
		return NRF_ERROR_INVALID_PARAM;
	}

	*parsed_flags = flags;

	nrf_err = sd_ble_gap_adv_set_configure(&ble_adv->adv_handle, &ble_adv->adv_data,
					       &ble_adv->adv_params);
	if (nrf_err) {
		LOG_ERR("Failed to set advertising flags, nrf_error %#x", nrf_err);
		return NRF_ERROR_INVALID_PARAM;
	}

	return NRF_SUCCESS;
}

static uint32_t set_adv_mode_directed_high_duty(struct ble_adv *ble_adv,
					   ble_gap_adv_params_t *adv_params)
{
#if CONFIG_BLE_ADV_DIRECTED_ADVERTISING_HIGH_DUTY
	adv_params->properties.type =
		BLE_GAP_ADV_TYPE_CONNECTABLE_NONSCANNABLE_DIRECTED_HIGH_DUTY_CYCLE;

	adv_params->duration = BLE_GAP_ADV_TIMEOUT_HIGH_DUTY_MAX;
	adv_params->interval = 0;
#endif
	return NRF_SUCCESS;
}

static uint32_t set_adv_mode_directed(struct ble_adv *ble_adv, ble_gap_adv_params_t *adv_params)
{
#if CONFIG_BLE_ADV_DIRECTED_ADVERTISING
#if defined(BLE_GAP_ADV_TYPE_EXTENDED_CONNECTABLE_NONSCANNABLE_DIRECTED)
	if (IS_ENABLED(CONFIG_BLE_ADV_EXTENDED_ADVERTISING)) {
		adv_params->properties.type =
			BLE_GAP_ADV_TYPE_EXTENDED_CONNECTABLE_NONSCANNABLE_DIRECTED;
	} else {
		adv_params->properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_NONSCANNABLE_DIRECTED;
	}
#else
	adv_params->properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_NONSCANNABLE_DIRECTED;
#endif

	adv_params->duration = CONFIG_BLE_ADV_DIRECTED_ADVERTISING_TIMEOUT;
	adv_params->interval = CONFIG_BLE_ADV_DIRECTED_ADVERTISING_INTERVAL;
#endif
	return NRF_SUCCESS;
}

static uint32_t set_adv_mode_fast(struct ble_adv *ble_adv, ble_gap_adv_params_t *adv_params)
{
#if CONFIG_BLE_ADV_FAST_ADVERTISING
	uint32_t nrf_err;

#if defined(BLE_GAP_ADV_TYPE_EXTENDED_CONNECTABLE_NONSCANNABLE_UNDIRECTED)
	if (IS_ENABLED(CONFIG_BLE_ADV_EXTENDED_ADVERTISING)) {
		ble_adv->adv_params.properties.type =
			BLE_GAP_ADV_TYPE_EXTENDED_CONNECTABLE_NONSCANNABLE_UNDIRECTED;
	} else {
		ble_adv->adv_params.properties.type =
			BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
	}
#else
	ble_adv->adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
#endif

	adv_params->interval = CONFIG_BLE_ADV_FAST_ADVERTISING_INTERVAL;
	adv_params->duration = CONFIG_BLE_ADV_FAST_ADVERTISING_TIMEOUT;
	if (use_whitelist(ble_adv)) {
		/* Set filter policy and advertising flags */
		adv_params->filter_policy = BLE_GAP_ADV_FP_FILTER_CONNREQ;
		nrf_err = flags_set(ble_adv, BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED);
		if (nrf_err) {
			return nrf_err;
		}
	}
#endif
	return NRF_SUCCESS;
}

static uint32_t set_adv_mode_slow(struct ble_adv *ble_adv, ble_gap_adv_params_t *adv_params)
{
#if CONFIG_BLE_ADV_SLOW_ADVERTISING
	uint32_t nrf_err;

#if defined(BLE_GAP_ADV_TYPE_EXTENDED_CONNECTABLE_NONSCANNABLE_UNDIRECTED)
	if (IS_ENABLED(CONFIG_BLE_ADV_EXTENDED_ADVERTISING)) {
		ble_adv->adv_params.properties.type =
			BLE_GAP_ADV_TYPE_EXTENDED_CONNECTABLE_NONSCANNABLE_UNDIRECTED;
	} else {
		ble_adv->adv_params.properties.type =
			BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
	}
#else
	ble_adv->adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
#endif

	adv_params->interval = CONFIG_BLE_ADV_SLOW_ADVERTISING_INTERVAL;
	adv_params->duration = CONFIG_BLE_ADV_SLOW_ADVERTISING_TIMEOUT;

	if (use_whitelist(ble_adv)) {
		/* Set filter policy and advertising flags */
		adv_params->filter_policy = BLE_GAP_ADV_FP_FILTER_CONNREQ;
		nrf_err = flags_set(ble_adv, BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED);
		if (nrf_err) {
			return nrf_err;
		}
	}
#endif
	return NRF_SUCCESS;
}

static uint16_t adv_data_size_max_get(void)
{
	if (!IS_ENABLED(CONFIG_BLE_ADV_EXTENDED_ADVERTISING)) {
		return BLE_GAP_ADV_SET_DATA_SIZE_MAX;
	}

#ifdef BLE_GAP_ADV_SET_DATA_SIZE_EXTENDED_CONNECTABLE_MAX_SUPPORTED
	return BLE_GAP_ADV_SET_DATA_SIZE_EXTENDED_CONNECTABLE_MAX_SUPPORTED;
#else
	return BLE_GAP_ADV_SET_DATA_SIZE_MAX;
#endif
}

uint32_t ble_adv_conn_cfg_tag_set(struct ble_adv *ble_adv, uint8_t ble_cfg_tag)
{
	if (!ble_adv) {
		return NRF_ERROR_NULL;
	}

	ble_adv->conn_cfg_tag = ble_cfg_tag;

	return NRF_SUCCESS;
}

uint32_t ble_adv_init(struct ble_adv *ble_adv, struct ble_adv_config *ble_adv_config)
{
	uint32_t nrf_err;
	ble_gap_conn_sec_mode_t sec_mode = {0};

	if (!ble_adv || !ble_adv_config || !ble_adv_config->evt_handler) {
		return NRF_ERROR_NULL;
	}

	ble_adv->mode_current = BLE_ADV_MODE_IDLE;
	ble_adv->conn_cfg_tag = ble_adv_config->conn_cfg_tag;
	ble_adv->conn_handle = BLE_CONN_HANDLE_INVALID;
	ble_adv->adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
	ble_adv->evt_handler = ble_adv_config->evt_handler;

	memset(&ble_adv->peer_address, 0x00, sizeof(ble_adv->peer_address));

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
	nrf_err = sd_ble_gap_device_name_set(&sec_mode, CONFIG_BLE_ADV_NAME,
					     strlen(CONFIG_BLE_ADV_NAME));
	if (nrf_err) {
		LOG_ERR("Failed to set advertising name, nrf_error %#x", nrf_err);
		return NRF_ERROR_INVALID_PARAM;
	}

	ble_adv->adv_data.adv_data.p_data = ble_adv->enc_adv_data[0];
	ble_adv->adv_data.adv_data.len = adv_data_size_max_get();

	nrf_err = ble_adv_data_encode(&ble_adv_config->adv_data, ble_adv->enc_adv_data[0],
				      &ble_adv->adv_data.adv_data.len);
	if (nrf_err) {
		return nrf_err;
	}

	ble_adv->adv_data.scan_rsp_data.p_data = ble_adv->enc_scan_rsp_data[0];
	ble_adv->adv_data.scan_rsp_data.len = adv_data_size_max_get();

	nrf_err = ble_adv_data_encode(&ble_adv_config->sr_data,
				      ble_adv->adv_data.scan_rsp_data.p_data,
				      &ble_adv->adv_data.scan_rsp_data.len);
	if (nrf_err) {
		return nrf_err;
	}

	/* Configure a initial advertising configuration. The advertising data and parameters
	 * will be changed later when we call @ref ble_adv_start, but must be set
	 * to legal values here to define an advertising handle.
	 */
	ble_adv->adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
	ble_adv->adv_params.duration = BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED;
	ble_adv->adv_params.interval = BLE_GAP_ADV_INTERVAL_MAX;
	ble_adv->adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
	ble_adv->adv_params.primary_phy = BLE_GAP_PHY_AUTO;

	nrf_err = sd_ble_gap_adv_set_configure(&ble_adv->adv_handle, NULL, &ble_adv->adv_params);
	if (nrf_err) {
		LOG_ERR("Failed to set GAP advertising parameters, nrf_error %#x", nrf_err);
		return NRF_ERROR_INVALID_PARAM;
	}

	ble_adv->is_initialized = true;

	return NRF_SUCCESS;
}

uint32_t ble_adv_start(struct ble_adv *ble_adv, enum ble_adv_mode mode)
{
	uint32_t nrf_err;
	struct ble_adv_evt adv_evt;
	if (!ble_adv) {
		return NRF_ERROR_NULL;
	}
	if (!ble_adv->is_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	ble_adv->whitelist_in_use = false;
	ble_adv->whitelist_reply_expected = false;
	ble_adv->peer_addr_reply_expected = false;

	/* Initialize advertising parameters with default values */
	memset(&ble_adv->adv_params, 0, sizeof(ble_adv->adv_params));

	/* Reset peer address */
	memset(&ble_adv->peer_address, 0, sizeof(ble_adv->peer_address));

	/* If `mode` is initially directed advertising (and that's supported)
	 * ask the application for a peer address
	 */
	if (IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING)) {
		if (adv_mode_is_directed(mode)) {
			ble_adv->peer_addr_reply_expected = true;
			adv_evt.evt_type = BLE_ADV_EVT_PEER_ADDR_REQUEST;
			ble_adv->evt_handler(ble_adv, &adv_evt);
		}
	}

	/* Fetch the whitelist */
	if (IS_ENABLED(CONFIG_BLE_ADV_USE_WHITELIST)) {
		if (adv_mode_has_whitelist(mode) && !ble_adv->whitelist_temporarily_disabled) {
			ble_adv->whitelist_reply_expected = true;
			adv_evt.evt_type = BLE_ADV_EVT_WHITELIST_REQUEST;
			ble_adv->evt_handler(ble_adv, &adv_evt);
		}
	}

	ble_adv->adv_params.primary_phy = CONFIG_BLE_ADV_PRIMARY_PHY;
	ble_adv->adv_params.secondary_phy = CONFIG_BLE_ADV_SECONDARY_PHY;
	ble_adv->adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;

	/* Select the next advertising mode based on what's enabled */

	switch (mode) {
	case BLE_ADV_MODE_DIRECTED_HIGH_DUTY:
		if (IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING_HIGH_DUTY)) {
			LOG_INF("Directed advertising (high duty)");
			mode = BLE_ADV_MODE_DIRECTED_HIGH_DUTY;
			adv_evt.evt_type = BLE_ADV_EVT_DIRECTED_HIGH_DUTY;
			nrf_err = set_adv_mode_directed_high_duty(ble_adv, &ble_adv->adv_params);
			break;
		} __fallthrough;

	case BLE_ADV_MODE_DIRECTED:
		if (IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING)) {
			LOG_INF("Directed advertising");
			mode = BLE_ADV_MODE_DIRECTED;
			adv_evt.evt_type = BLE_ADV_EVT_DIRECTED;
			nrf_err = set_adv_mode_directed(ble_adv, &ble_adv->adv_params);
			break;
		} __fallthrough;

	case BLE_ADV_MODE_FAST:
		if (IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING)) {
			LOG_INF("Fast advertising");
			mode = BLE_ADV_MODE_FAST;
			adv_evt.evt_type = BLE_ADV_EVT_FAST;
			nrf_err = set_adv_mode_fast(ble_adv, &ble_adv->adv_params);
			if (nrf_err) {
				LOG_ERR("Failed to set fast advertising params, error: %d",
					nrf_err);
				return NRF_ERROR_INVALID_PARAM;
			}
			break;
		} __fallthrough;

	case BLE_ADV_MODE_SLOW:
		if (IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
			LOG_INF("Slow advertising");
			mode = BLE_ADV_MODE_SLOW;
			adv_evt.evt_type = BLE_ADV_EVT_SLOW;
			nrf_err = set_adv_mode_slow(ble_adv, &ble_adv->adv_params);
			if (nrf_err) {
				LOG_ERR("Failed to set slow advertising params, error: %d",
					nrf_err);
				return NRF_ERROR_INVALID_PARAM;
			}
			break;
		} __fallthrough;

	case BLE_ADV_MODE_IDLE:
	default:
		LOG_INF("Idle");
		mode = BLE_ADV_MODE_IDLE;
		adv_evt.evt_type = BLE_ADV_EVT_IDLE;
		break;
	}

	if (mode != BLE_ADV_MODE_IDLE) {
		nrf_err = sd_ble_gap_adv_set_configure(&ble_adv->adv_handle, &ble_adv->adv_data,
						       &ble_adv->adv_params);
		if (nrf_err) {
			LOG_ERR("Failed to set advertising data, nrf_error %#x", nrf_err);
			return NRF_ERROR_INVALID_PARAM;
		}

		nrf_err = sd_ble_gap_adv_start(ble_adv->adv_handle, ble_adv->conn_cfg_tag);
		if (nrf_err) {
			LOG_ERR("Failed to start advertising, nrf_error %#x", nrf_err);
			return NRF_ERROR_INVALID_PARAM;
		}
	}
	ble_adv->mode_current = mode;
	ble_adv->evt_handler(ble_adv, &adv_evt);

	return NRF_SUCCESS;
}

void ble_adv_on_ble_evt(const ble_evt_t *ble_evt, void *instance)
{
	struct ble_adv *ble_adv = (struct ble_adv *)instance;

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		on_connected(ble_adv, ble_evt);
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		/* Upon disconnection, activate whitelist and start directed advertising */
		on_disconnected(ble_adv, ble_evt);
		break;
	case BLE_GAP_EVT_ADV_SET_TERMINATED:
		/* Upon advertising time-out, move onto next advertising mode */
		on_terminated(ble_adv, ble_evt);
		break;
	default:
		/* Do nothing */
		break;
	}
}

uint32_t ble_adv_peer_addr_reply(struct ble_adv *ble_adv, const ble_gap_addr_t *peer_addr)
{
	if (!ble_adv || !peer_addr) {
		return NRF_ERROR_NULL;
	}
	if (!ble_adv->peer_addr_reply_expected) {
		return NRF_ERROR_INVALID_STATE;
	}
	if (!peer_addr_is_valid(peer_addr)) {
		return NRF_ERROR_INVALID_PARAM;
	}

	ble_adv->peer_addr_reply_expected = false;
	memcpy(&ble_adv->peer_address, peer_addr, sizeof(ble_adv->peer_address));

	return NRF_SUCCESS;
}

uint32_t ble_adv_whitelist_reply(struct ble_adv *ble_adv,
				 const ble_gap_addr_t *addrs, uint32_t addr_cnt,
				 const ble_gap_irk_t *irks, uint32_t irk_cnt)
{
	if (!ble_adv) {
		return NRF_ERROR_NULL;
	}
	if (!ble_adv->whitelist_reply_expected) {
		return NRF_ERROR_INVALID_STATE;
	}

	ble_adv->whitelist_reply_expected = false;
	ble_adv->whitelist_in_use = (addr_cnt > 0 || irk_cnt > 0);

	return NRF_SUCCESS;
}

uint32_t ble_adv_restart_without_whitelist(struct ble_adv *ble_adv)
{
	uint32_t nrf_err;

	(void)sd_ble_gap_adv_stop(ble_adv->adv_handle);

	ble_adv->whitelist_temporarily_disabled = true;
	ble_adv->adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;

	/* Set correct flags */
	nrf_err = flags_set(ble_adv, BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
	if (nrf_err) {
		return nrf_err;
	}

	nrf_err = ble_adv_start(ble_adv, ble_adv->mode_current);
	if (nrf_err) {
		return nrf_err;
	}

	return NRF_SUCCESS;
}

uint32_t ble_adv_data_update(struct ble_adv *ble_adv, const struct ble_adv_data *adv_data,
			const struct ble_adv_data *sr_data)
{
	uint32_t nrf_err;
	ble_gap_adv_data_t new_adv_data = {0};

	if (!ble_adv || (adv_data == NULL && sr_data == NULL)) {
		return NRF_ERROR_NULL;
	}
	if (!ble_adv->is_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (adv_data) {
		new_adv_data.adv_data.p_data =
			(ble_adv->adv_data.adv_data.p_data != ble_adv->enc_adv_data[0])
				? ble_adv->enc_adv_data[0]
				: ble_adv->enc_adv_data[1];

		new_adv_data.adv_data.len = adv_data_size_max_get();

		nrf_err = ble_adv_data_encode(adv_data, new_adv_data.adv_data.p_data,
					      &new_adv_data.adv_data.len);
		if (nrf_err) {
			return nrf_err;
		}
	}

	if (sr_data) {
		new_adv_data.scan_rsp_data.p_data =
			(ble_adv->adv_data.scan_rsp_data.p_data != ble_adv->enc_scan_rsp_data[0])
				? ble_adv->enc_scan_rsp_data[0]
				: ble_adv->enc_scan_rsp_data[1];

		new_adv_data.scan_rsp_data.len = adv_data_size_max_get();

		nrf_err = ble_adv_data_encode(sr_data, new_adv_data.scan_rsp_data.p_data,
					      &new_adv_data.scan_rsp_data.len);
		if (nrf_err) {
			return nrf_err;
		}
	}

	memcpy(&ble_adv->adv_data, &new_adv_data, sizeof(ble_adv->adv_data));

	nrf_err = sd_ble_gap_adv_set_configure(&ble_adv->adv_handle, &ble_adv->adv_data, NULL);
	if (nrf_err) {
		LOG_ERR("Failed to set GAP advertising data, nrf_error %#x", nrf_err);
		return NRF_ERROR_INVALID_PARAM;
	}

	return NRF_SUCCESS;
}
