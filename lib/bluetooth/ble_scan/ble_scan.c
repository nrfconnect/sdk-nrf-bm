/*
 * Copyright (c) 2015 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

// #include "sdk_common.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <bm/bluetooth/ble_scan.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ble_scan, 3);

// <o> NRF_BLE_SCAN_BUFFER - Data length for an advertising set.
#ifndef NRF_BLE_SCAN_BUFFER
#define NRF_BLE_SCAN_BUFFER 31
#endif

// <o> NRF_BLE_SCAN_NAME_MAX_LEN - Maximum size for the name to search in the advertisement report.
#ifndef NRF_BLE_SCAN_NAME_MAX_LEN
#define NRF_BLE_SCAN_NAME_MAX_LEN 32
#endif

// <o> NRF_BLE_SCAN_SHORT_NAME_MAX_LEN - Maximum size of the short name to search for in the
// advertisement report.
#ifndef NRF_BLE_SCAN_SHORT_NAME_MAX_LEN
#define NRF_BLE_SCAN_SHORT_NAME_MAX_LEN 32
#endif

// <o> NRF_BLE_SCAN_SCAN_INTERVAL - Scanning interval. Determines the scan interval in units of
// 0.625 millisecond.
#ifndef NRF_BLE_SCAN_SCAN_INTERVAL
#define NRF_BLE_SCAN_SCAN_INTERVAL 160
#endif

// <o> NRF_BLE_SCAN_SCAN_DURATION - Duration of a scanning session in units of 10 ms. Range: 0x0001
// - 0xFFFF (10 ms to 10.9225 ms). If set to 0x0000, the scanning continues until it is explicitly
// disabled.
#ifndef NRF_BLE_SCAN_SCAN_DURATION
#define NRF_BLE_SCAN_SCAN_DURATION 0
#endif

// <o> NRF_BLE_SCAN_SCAN_WINDOW - Scanning window. Determines the scanning window in units of 0.625
// millisecond.
#ifndef NRF_BLE_SCAN_SCAN_WINDOW
#define NRF_BLE_SCAN_SCAN_WINDOW 80
#endif

// <o> NRF_BLE_SCAN_SLAVE_LATENCY - Determines the slave latency in counts of connection events.
#ifndef NRF_BLE_SCAN_SLAVE_LATENCY
#define NRF_BLE_SCAN_SLAVE_LATENCY 5
#endif

static void nrf_ble_scan_connect_with_target(struct nrf_ble_scan const *const p_scan_ctx,
					     ble_gap_evt_adv_report_t const *const p_adv_report)
{
	int err_code;
	struct scan_evt scan_evt;

	// For readability.
	ble_gap_addr_t const *p_addr = &p_adv_report->peer_addr;
	ble_gap_scan_params_t const *p_scan_params = &p_scan_ctx->scan_params;
	ble_gap_conn_params_t const *p_conn_params = &p_scan_ctx->conn_params;
	uint8_t con_cfg_tag = p_scan_ctx->conn_cfg_tag;

	// Return if the automatic connection is disabled.
	if (!p_scan_ctx->connect_if_match) {
		return;
	}

	// Stop scanning.
	nrf_ble_scan_stop();

	memset(&scan_evt, 0, sizeof(scan_evt));

	// Establish connection.
	err_code = sd_ble_gap_connect(p_addr, p_scan_params, p_conn_params, con_cfg_tag);
	if (err_code != NRF_SUCCESS) {
		// TODO: Trigger event handler with error
	}

	LOG_DBG("Connecting");

	scan_evt.scan_evt_id = NRF_BLE_SCAN_EVT_CONNECTING_ERROR;
	scan_evt.params.connecting_err.err_code = err_code;

	LOG_DBG("Connection status: %d", err_code);

	// If an error occurred, send an event to the event handler.
	if ((err_code != NRF_SUCCESS) && (p_scan_ctx->evt_handler != NULL)) {
		p_scan_ctx->evt_handler(&scan_evt);
	}
}

#if CONFIG_BT_SCAN_FILTER_ENABLE || 1
#if (NRF_BLE_SCAN_ADDRESS_CNT > 0)

static bool find_peer_addr(ble_gap_evt_adv_report_t const *const p_adv_report,
			   ble_gap_addr_t const *p_addr)
{
	// Compare addresses.
	if (memcmp(p_addr->addr, p_adv_report->peer_addr.addr,
		   sizeof(p_adv_report->peer_addr.addr)) == 0) {
		return true;
	}

	return false;
}

static bool adv_addr_compare(ble_gap_evt_adv_report_t const *const p_adv_report,
			     struct nrf_ble_scan const *const p_scan_ctx)
{
	ble_gap_addr_t const *p_addr = p_scan_ctx->scan_filters.addr_filter.target_addr;
	uint8_t counter = p_scan_ctx->scan_filters.addr_filter.addr_cnt;

	for (uint8_t index = 0; index < counter; index++) {
		/* Search for address. */
		if (find_peer_addr(p_adv_report, &p_addr[index])) {
			return true;
		}
	}

	return false;
}

static int nrf_ble_scan_addr_filter_add(struct nrf_ble_scan *const p_scan_ctx,
					uint8_t const *p_addr)
{
	ble_gap_addr_t *p_addr_filter = p_scan_ctx->scan_filters.addr_filter.target_addr;
	uint8_t *p_counter = &p_scan_ctx->scan_filters.addr_filter.addr_cnt;
	uint8_t index;

	/* If no memory for filter. */
	if (*p_counter >= NRF_BLE_SCAN_ADDRESS_CNT) {
		return NRF_ERROR_NO_MEM;
	}

	// Check for duplicated filter.
	for (index = 0; index < NRF_BLE_SCAN_ADDRESS_CNT; index++) {
		if (!memcmp(p_addr_filter[index].addr, p_addr, BLE_GAP_ADDR_LEN)) {
			return NRF_SUCCESS;
		}
	}

	for (uint8_t i = 0; i < BLE_GAP_ADDR_LEN; i++) {
		p_addr_filter[*p_counter].addr[i] = p_addr[i];
	}

	// Address type is not used so set it to 0.
	p_addr_filter[*p_counter].addr_type = 0;

	LOG_DBG("Filter set on address 0x");
	NRF_LOG_HEXDUMP_DEBUG(p_addr_filter[*p_counter].addr, BLE_GAP_ADDR_LEN);

	// Increase the address filter counter.
	*p_counter += 1;

	return NRF_SUCCESS;
}

#endif // NRF_BLE_SCAN_ADDRESS_CNT

#if (NRF_BLE_SCAN_NAME_CNT > 0)

static bool adv_name_compare(ble_gap_evt_adv_report_t const *p_adv_report,
			     struct nrf_ble_scan const *const p_scan_ctx)
{
	struct nrf_ble_scan_name_filter const *p_name_filter =
		&p_scan_ctx->scan_filters.name_filter;
	uint8_t counter = p_scan_ctx->scan_filters.name_filter.name_cnt;
	uint8_t index;
	uint16_t data_len;

	data_len = p_adv_report->data.len;

	// Compare the name found with the name filter.
	for (index = 0; index < counter; index++) {
		if (ble_advdata_name_find(p_adv_report->data.p_data, data_len,
					  p_name_filter->target_name[index])) {
			return true;
		}
	}

	return false;
}

static int nrf_ble_scan_name_filter_add(struct nrf_ble_scan *const p_scan_ctx, char const *p_name)
{
	uint8_t index;
	uint8_t *counter = &p_scan_ctx->scan_filters.name_filter.name_cnt;
	uint8_t name_len = strlen(p_name);

	// Check the name length.
	if ((name_len == 0) || (name_len > NRF_BLE_SCAN_NAME_MAX_LEN)) {
		return NRF_ERROR_DATA_SIZE;
	}

	// If no memory for filter.
	if (*counter >= NRF_BLE_SCAN_NAME_CNT) {
		return NRF_ERROR_NO_MEM;
	}

	// Check for duplicated filter.
	for (index = 0; index < NRF_BLE_SCAN_NAME_CNT; index++) {
		if (!strcmp(p_scan_ctx->scan_filters.name_filter.target_name[index], p_name)) {
			return NRF_SUCCESS;
		}
	}

	// Add name to filter.
	memcpy(p_scan_ctx->scan_filters.name_filter.target_name[(*counter)++], p_name,
	       strlen(p_name));

	LOG_DBG("Adding filter on %s name", p_name);

	return NRF_SUCCESS;
}

#endif // NRF_BLE_SCAN_NAME_CNT

#if (NRF_BLE_SCAN_SHORT_NAME_CNT > 0)
static bool adv_short_name_compare(ble_gap_evt_adv_report_t const *const p_adv_report,
				   struct nrf_ble_scan const *const p_scan_ctx)
{
	nrf_ble_scan_short_name_filter_t const *p_name_filter =
		&p_scan_ctx->scan_filters.short_name_filter;
	uint8_t counter = p_scan_ctx->scan_filters.short_name_filter.name_cnt;
	uint8_t index;
	uint16_t data_len;

	data_len = p_adv_report->data.len;

	// Compare the name found with the name filters.
	for (index = 0; index < counter; index++) {
		if (ble_advdata_short_name_find(
			    p_adv_report->data.p_data, data_len,
			    p_name_filter->short_name[index].short_target_name,
			    p_name_filter->short_name[index].short_name_min_len)) {
			return true;
		}
	}

	return false;
}

static int nrf_ble_scan_short_name_filter_add(struct nrf_ble_scan *const p_scan_ctx,
					      nrf_ble_scan_short_name_t const *p_short_name)
{
	uint8_t index;
	uint8_t *p_counter = &p_scan_ctx->scan_filters.short_name_filter.name_cnt;
	nrf_ble_scan_short_name_filter_t *p_short_name_filter =
		&p_scan_ctx->scan_filters.short_name_filter;
	uint8_t name_len = strlen(p_short_name->p_short_name);

	// Check the name length.
	if ((name_len == 0) || (name_len > NRF_BLE_SCAN_SHORT_NAME_MAX_LEN)) {
		return NRF_ERROR_DATA_SIZE;
	}

	// If no memory for filter.
	if (*p_counter >= NRF_BLE_SCAN_SHORT_NAME_CNT) {
		return NRF_ERROR_NO_MEM;
	}

	// Check for duplicated filter.
	for (index = 0; index < NRF_BLE_SCAN_SHORT_NAME_CNT; index++) {
		if (!strcmp(p_short_name_filter->short_name[index].short_target_name,
			    p_short_name->p_short_name)) {
			return NRF_SUCCESS;
		}
	}

	// Add name to the filter.
	p_short_name_filter->short_name[(*p_counter)].short_name_min_len =
		p_short_name->short_name_min_len;
	memcpy(p_short_name_filter->short_name[(*p_counter)++].short_target_name,
	       p_short_name->p_short_name, strlen(p_short_name->p_short_name));

	LOG_DBG("Adding filter on %s name", p_short_name->p_short_name);

	return NRF_SUCCESS;
}

#endif

#if (NRF_BLE_SCAN_UUID_CNT > 0)

static bool adv_uuid_compare(ble_gap_evt_adv_report_t const *const p_adv_report,
			     struct nrf_ble_scan const *const p_scan_ctx)
{
	nrf_ble_scan_uuid_filter_t const *p_uuid_filter = &p_scan_ctx->scan_filters.uuid_filter;
	bool const all_filters_mode = p_scan_ctx->scan_filters.all_filters_mode;
	uint8_t const counter = p_scan_ctx->scan_filters.uuid_filter.uuid_cnt;
	uint8_t index;
	uint16_t data_len;
	uint8_t uuid_match_cnt = 0;

	data_len = p_adv_report->data.len;

	for (index = 0; index < counter; index++) {

		if (ble_advdata_uuid_find(p_adv_report->data.p_data, data_len,
					  &p_uuid_filter->uuid[index])) {
			uuid_match_cnt++;

			// In the normal filter mode, only one UUID is needed to match.
			if (!all_filters_mode) {
				break;
			}
		} else if (all_filters_mode) {
			break;
		} else {
			// Do nothing.
		}
	}

	// In the multifilter mode, all UUIDs must be found in the advertisement packets.
	if ((all_filters_mode && (uuid_match_cnt == counter)) ||
	    ((!all_filters_mode) && (uuid_match_cnt > 0))) {
		return true;
	}

	return false;
}

static int nrf_ble_scan_uuid_filter_add(struct nrf_ble_scan *const p_scan_ctx,
					ble_uuid_t const *p_uuid)
{
	ble_uuid_t *p_uuid_filter = p_scan_ctx->scan_filters.uuid_filter.uuid;
	uint8_t *p_counter = &p_scan_ctx->scan_filters.uuid_filter.uuid_cnt;
	uint8_t index;

	// If no memory.
	if (*p_counter >= NRF_BLE_SCAN_UUID_CNT) {
		return NRF_ERROR_NO_MEM;
	}

	// Check for duplicated filter.
	for (index = 0; index < NRF_BLE_SCAN_UUID_CNT; index++) {
		if (p_uuid_filter[index].uuid == p_uuid->uuid) {
			return NRF_SUCCESS;
		}
	}

	// Add UUID to the filter.
	p_uuid_filter[(*p_counter)++] = *p_uuid;
	LOG_DBG("Added filter on UUID %x", p_uuid->uuid);

	return NRF_SUCCESS;
}

#endif // NRF_BLE_SCAN_UUID_CNT

#if (NRF_BLE_SCAN_APPEARANCE_CNT)

static bool adv_appearance_compare(ble_gap_evt_adv_report_t const *const p_adv_report,
				   struct nrf_ble_scan const *const p_scan_ctx)
{
	nrf_ble_scan_appearance_filter_t const *p_appearance_filter =
		&p_scan_ctx->scan_filters.appearance_filter;
	uint8_t const counter = p_scan_ctx->scan_filters.appearance_filter.appearance_cnt;
	uint8_t index;
	uint16_t data_len;

	data_len = p_adv_report->data.len;

	// Verify if the advertised appearance matches the provided appearance.
	for (index = 0; index < counter; index++) {
		if (ble_advdata_appearance_find(p_adv_report->data.p_data, data_len,
						&p_appearance_filter->appearance[index])) {
			return true;
		}
	}
	return false;
}

static int nrf_ble_scan_appearance_filter_add(struct nrf_ble_scan *const p_scan_ctx,
					      uint16_t appearance)
{
	uint16_t *p_appearance_filter = p_scan_ctx->scan_filters.appearance_filter.appearance;
	uint8_t *p_counter = &p_scan_ctx->scan_filters.appearance_filter.appearance_cnt;
	uint8_t index;

	// If no memory.
	if (*p_counter >= NRF_BLE_SCAN_APPEARANCE_CNT) {
		return NRF_ERROR_NO_MEM;
	}

	// Check for duplicated filter.
	for (index = 0; index < NRF_BLE_SCAN_APPEARANCE_CNT; index++) {
		if (p_appearance_filter[index] == appearance) {
			return NRF_SUCCESS;
		}
	}

	// Add appearance to the filter.
	p_appearance_filter[(*p_counter)++] = appearance;
	LOG_DBG("Added filter on appearance %x", appearance);
	return NRF_SUCCESS;
}

#endif // NRF_BLE_SCAN_APPEARANCE_CNT

int nrf_ble_scan_filter_set(struct nrf_ble_scan *const scan_ctx, enum nrf_ble_scan_filter_type type,
			    void const *data)
{
	if (scan_ctx == NULL) {
		return NRF_ERROR_NULL;
	}
	if (data == NULL) {
		return NRF_ERROR_NULL;
	}

	switch (type) {
#if (NRF_BLE_SCAN_NAME_CNT > 0)
	case SCAN_NAME_FILTER: {
		char *name = (char *)data;
		return nrf_ble_scan_name_filter_add(scan_ctx, name);
	}
#endif

#if (NRF_BLE_SCAN_SHORT_NAME_CNT > 0)
	case SCAN_SHORT_NAME_FILTER: {
		nrf_ble_scan_short_name_t *short_name = (nrf_ble_scan_short_name_t *)data;
		return nrf_ble_scan_short_name_filter_add(scan_ctx, short_name);
	}
#endif

#if (NRF_BLE_SCAN_ADDRESS_CNT > 0)
	case SCAN_ADDR_FILTER: {
		uint8_t *addr = (uint8_t *)data;
		return nrf_ble_scan_addr_filter_add(scan_ctx, addr);
	}
#endif

#if (NRF_BLE_SCAN_UUID_CNT > 0)
	case SCAN_UUID_FILTER: {
		ble_uuid_t *uuid = (ble_uuid_t *)data;
		return nrf_ble_scan_uuid_filter_add(scan_ctx, uuid);
	}
#endif

#if (NRF_BLE_SCAN_APPEARANCE_CNT > 0)
	case SCAN_APPEARANCE_FILTER: {
		uint16_t appearance = *((uint16_t *)data);
		return nrf_ble_scan_appearance_filter_add(scan_ctx, appearance);
	}
#endif

	default:
		return NRF_ERROR_INVALID_PARAM;
	}
}

int nrf_ble_scan_all_filter_remove(struct nrf_ble_scan *const p_scan_ctx)
{
#if (NRF_BLE_SCAN_NAME_CNT > 0)
	struct nrf_ble_scan_name_filter *p_name_filter = &p_scan_ctx->scan_filters.name_filter;
	memset(p_name_filter->target_name, 0, sizeof(p_name_filter->target_name));
	p_name_filter->name_cnt = 0;
#endif

#if (NRF_BLE_SCAN_SHORT_NAME_CNT > 0)
	nrf_ble_scan_short_name_filter_t *p_short_name_filter =
		&p_scan_ctx->scan_filters.short_name_filter;
	memset(p_short_name_filter->short_name, 0, sizeof(p_short_name_filter->short_name));
	p_short_name_filter->name_cnt = 0;
#endif

#if (NRF_BLE_SCAN_ADDRESS_CNT > 0)
	nrf_ble_scan_addr_filter_t *p_addr_filter = &p_scan_ctx->scan_filters.addr_filter;
	memset(p_addr_filter->target_addr, 0, sizeof(p_addr_filter->target_addr));
	p_addr_filter->addr_cnt = 0;
#endif

#if (NRF_BLE_SCAN_UUID_CNT > 0)
	nrf_ble_scan_uuid_filter_t *p_uuid_filter = &p_scan_ctx->scan_filters.uuid_filter;
	memset(p_uuid_filter->uuid, 0, sizeof(p_uuid_filter->uuid));
	p_uuid_filter->uuid_cnt = 0;
#endif

#if (NRF_BLE_SCAN_APPEARANCE_CNT > 0)
	nrf_ble_scan_appearance_filter_t *p_appearance_filter =
		&p_scan_ctx->scan_filters.appearance_filter;
	memset(p_appearance_filter->appearance, 0, sizeof(p_appearance_filter->appearance));
	p_appearance_filter->appearance_cnt = 0;
#endif

	return NRF_SUCCESS;
}

int nrf_ble_scan_filters_enable(struct nrf_ble_scan *const scan_ctx, uint8_t mode, bool match_all)
{
	// VERIFY_PARAM_NOT_NULL(p_scan_ctx);
	if (scan_ctx == NULL) {
		return NRF_ERROR_NULL;
	}

	// Check if the mode is correct.
	if ((!(mode & NRF_BLE_SCAN_ADDR_FILTER)) && (!(mode & NRF_BLE_SCAN_NAME_FILTER)) &&
	    (!(mode & NRF_BLE_SCAN_UUID_FILTER)) && (!(mode & NRF_BLE_SCAN_SHORT_NAME_FILTER)) &&
	    (!(mode & NRF_BLE_SCAN_APPEARANCE_FILTER))) {
		return NRF_ERROR_INVALID_PARAM;
	}

	int err_code;

	// Disable filters.
	err_code = nrf_ble_scan_filters_disable(scan_ctx);
	// ASSERT(err_code == NRF_SUCCESS);

	struct nrf_ble_scan_filters *filters = &scan_ctx->scan_filters;

	// Turn on the filters of your choice.
#if (NRF_BLE_SCAN_ADDRESS_CNT > 0)
	if (mode & NRF_BLE_SCAN_ADDR_FILTER) {
		p_filters->addr_filter.addr_filter_enabled = true;
	}
#endif

#if (NRF_BLE_SCAN_NAME_CNT > 0)
	if (mode & NRF_BLE_SCAN_NAME_FILTER) {
		filters->name_filter.name_filter_enabled = true;
	}
#endif

#if (NRF_BLE_SCAN_SHORT_NAME_CNT > 0)
	if (mode & NRF_BLE_SCAN_SHORT_NAME_FILTER) {
		p_filters->short_name_filter.short_name_filter_enabled = true;
	}
#endif

#if (NRF_BLE_SCAN_UUID_CNT > 0)
	if (mode & NRF_BLE_SCAN_UUID_FILTER) {
		p_filters->uuid_filter.uuid_filter_enabled = true;
	}
#endif

#if (NRF_BLE_SCAN_APPEARANCE_CNT > 0)
	if (mode & NRF_BLE_SCAN_APPEARANCE_FILTER) {
		p_filters->appearance_filter.appearance_filter_enabled = true;
	}
#endif

	// Select the filter mode.
	filters->all_filters_mode = match_all;

	return NRF_SUCCESS;
}

int nrf_ble_scan_filters_disable(struct nrf_ble_scan *const p_scan_ctx)
{
	if (!p_scan_ctx) {
		return NRF_ERROR_NULL;
	}

	// Disable all filters.
#if (NRF_BLE_SCAN_NAME_CNT > 0)
	bool *p_name_filter_enabled = &p_scan_ctx->scan_filters.name_filter.name_filter_enabled;
	*p_name_filter_enabled = false;
#endif

#if (NRF_BLE_SCAN_ADDRESS_CNT > 0)
	bool *p_addr_filter_enabled = &p_scan_ctx->scan_filters.addr_filter.addr_filter_enabled;
	*p_addr_filter_enabled = false;
#endif

#if (NRF_BLE_SCAN_UUID_CNT > 0)
	bool *p_uuid_filter_enabled = &p_scan_ctx->scan_filters.uuid_filter.uuid_filter_enabled;
	*p_uuid_filter_enabled = false;
#endif

#if (NRF_BLE_SCAN_APPEARANCE_CNT > 0)
	bool *p_appearance_filter_enabled =
		&p_scan_ctx->scan_filters.appearance_filter.appearance_filter_enabled;
	*p_appearance_filter_enabled = false;
#endif

	return NRF_SUCCESS;
}

int nrf_ble_scan_filter_get(struct nrf_ble_scan *const scan_ctx,
			    struct nrf_ble_scan_filters *status)
{
	if (scan_ctx == NULL) {
		return NRF_ERROR_NULL;
	}
	if (status == NULL) {
		return NRF_ERROR_NULL;
	}

	*status = scan_ctx->scan_filters;

	return NRF_SUCCESS;
}

#endif // CONFIG_BT_SCAN_FILTER_ENABLE

/**@brief Function for calling the BLE_GAP_EVT_ADV_REPORT event to check whether the received
 *        scanning data matches the scan configuration.
 *
 * @param[in] p_scan_ctx    Pointer to the Scanning Module instance.
 * @param[in] p_adv_report  Advertising report.
 */
static void nrf_ble_scan_on_adv_report(struct nrf_ble_scan const *const p_scan_ctx,
				       ble_gap_evt_adv_report_t const *const p_adv_report)
{
	struct scan_evt scan_evt;

#if CONFIG_BT_SCAN_FILTER_ENABLE || 1
	uint8_t filter_cnt = 0;
	uint8_t filter_match_cnt = 0;
#endif

	memset(&scan_evt, 0, sizeof(scan_evt));

	scan_evt.p_scan_params = &p_scan_ctx->scan_params;

	// If the whitelist is used, do not check the filters and return.
	if (is_whitelist_used(p_scan_ctx)) {
		scan_evt.scan_evt_id = NRF_BLE_SCAN_EVT_WHITELIST_ADV_REPORT;
		scan_evt.params.p_not_found = p_adv_report;
		p_scan_ctx->evt_handler(&scan_evt);

		sd_ble_gap_scan_start(NULL, &p_scan_ctx->scan_buffer);
		nrf_ble_scan_connect_with_target(p_scan_ctx, p_adv_report);

		return;
	}

#if BT_SCAN_FILTER_ENABLE
	bool const all_filter_mode = p_scan_ctx->scan_filters.all_filters_mode;
	bool is_filter_matched = false;

#if (NRF_BLE_SCAN_ADDRESS_CNT > 0)
	bool const addr_filter_enabled = p_scan_ctx->scan_filters.addr_filter.addr_filter_enabled;
#endif

#if (NRF_BLE_SCAN_NAME_CNT > 0)
	bool const name_filter_enabled = p_scan_ctx->scan_filters.name_filter.name_filter_enabled;
#endif

#if (NRF_BLE_SCAN_SHORT_NAME_CNT > 0)
	bool const short_name_filter_enabled =
		p_scan_ctx->scan_filters.short_name_filter.short_name_filter_enabled;
#endif

#if (NRF_BLE_SCAN_UUID_CNT > 0)
	bool const uuid_filter_enabled = p_scan_ctx->scan_filters.uuid_filter.uuid_filter_enabled;
#endif

#if (NRF_BLE_SCAN_APPEARANCE_CNT > 0)
	bool const appearance_filter_enabled =
		p_scan_ctx->scan_filters.appearance_filter.appearance_filter_enabled;
#endif

#if (NRF_BLE_SCAN_ADDRESS_CNT > 0)
	// Check the address filter.
	if (addr_filter_enabled) {
		// Number of active filters.
		filter_cnt++;
		if (adv_addr_compare(p_adv_report, p_scan_ctx)) {
			// Number of filters matched.
			filter_match_cnt++;
			// Information about the filters matched.
			scan_evt.params.filter_match.filter_match.address_filter_match = true;
			is_filter_matched = true;
		}
	}
#endif

#if (NRF_BLE_SCAN_NAME_CNT > 0)
	// Check the name filter.
	if (name_filter_enabled) {
		filter_cnt++;
		if (adv_name_compare(p_adv_report, p_scan_ctx)) {
			filter_match_cnt++;

			// Information about the filters matched.
			scan_evt.params.filter_match.filter_match.name_filter_match = true;
			is_filter_matched = true;
		}
	}
#endif

#if (NRF_BLE_SCAN_SHORT_NAME_CNT > 0)
	if (short_name_filter_enabled) {
		filter_cnt++;
		if (adv_short_name_compare(p_adv_report, p_scan_ctx)) {
			filter_match_cnt++;

			// Information about the filters matched.
			scan_evt.params.filter_match.filter_match.short_name_filter_match = true;
			is_filter_matched = true;
		}
	}
#endif

#if (NRF_BLE_SCAN_UUID_CNT > 0)
	// Check the UUID filter.
	if (uuid_filter_enabled) {
		filter_cnt++;
		if (adv_uuid_compare(p_adv_report, p_scan_ctx)) {
			filter_match_cnt++;
			// Information about the filters matched.
			scan_evt.params.filter_match.filter_match.uuid_filter_match = true;
			is_filter_matched = true;
		}
	}
#endif

#if (NRF_BLE_SCAN_APPEARANCE_CNT > 0)
	// Check the appearance filter.
	if (appearance_filter_enabled) {
		filter_cnt++;
		if (adv_appearance_compare(p_adv_report, p_scan_ctx)) {
			filter_match_cnt++;
			// Information about the filters matched.
			scan_evt.params.filter_match.filter_match.appearance_filter_match = true;
			is_filter_matched = true;
		}
	}

	scan_evt.scan_evt_id = NRF_BLE_SCAN_EVT_NOT_FOUND;
#endif

	scan_evt.params.filter_match.p_adv_report = p_adv_report;

	// In the multifilter mode, the number of the active filters must equal the number of the
	// filters matched to generate the notification.
	if (all_filter_mode && (filter_match_cnt == filter_cnt)) {
		scan_evt.scan_evt_id = NRF_BLE_SCAN_EVT_FILTER_MATCH;
		nrf_ble_scan_connect_with_target(p_scan_ctx, p_adv_report);
	}
	// In the normal filter mode, only one filter match is needed to generate the notification
	// to the main application.
	else if ((!all_filter_mode) && is_filter_matched) {
		scan_evt.scan_evt_id = NRF_BLE_SCAN_EVT_FILTER_MATCH;
		nrf_ble_scan_connect_with_target(p_scan_ctx, p_adv_report);
	} else {
		scan_evt.scan_evt_id = NRF_BLE_SCAN_EVT_NOT_FOUND;
		scan_evt.params.p_not_found = p_adv_report;
	}

	// If the event handler is not NULL, notify the main application.
	if (p_scan_ctx->evt_handler != NULL) {
		p_scan_ctx->evt_handler(&scan_evt);
	}

#endif // BT_SCAN_FILTER_ENABLE

	// Resume the scanning.
	(void)sd_ble_gap_scan_start(NULL, &p_scan_ctx->scan_buffer);
}

/**@brief Function for checking whether the whitelist is used.
 *
 * @param[in] p_scan_ctx   Scanning Module instance.
 */
bool is_whitelist_used(struct nrf_ble_scan const *const scan_ctx)
{
	if (scan_ctx->scan_params.filter_policy == BLE_GAP_SCAN_FP_WHITELIST ||
	    scan_ctx->scan_params.filter_policy ==
		    BLE_GAP_SCAN_FP_WHITELIST_NOT_RESOLVED_DIRECTED) {
		return true;
	}

	return false;
}

/**@brief Function for restoring the default scanning parameters.
 *
 * @param[out] p_scan_ctx    Pointer to the Scanning Module instance.
 */
static void nrf_ble_scan_default_param_set(struct nrf_ble_scan *const scan_ctx)
{
	// Set the default parameters.
	scan_ctx->scan_params.active = 1;
#if (NRF_SD_BLE_API_VERSION > 7)
	p_scan_ctx->scan_params.interval_us = NRF_BLE_SCAN_SCAN_INTERVAL * UNIT_0_625_MS;
	p_scan_ctx->scan_params.window_us = NRF_BLE_SCAN_SCAN_WINDOW * UNIT_0_625_MS;
#else
	scan_ctx->scan_params.interval = NRF_BLE_SCAN_SCAN_INTERVAL;
	scan_ctx->scan_params.window = NRF_BLE_SCAN_SCAN_WINDOW;
#endif // #if (NRF_SD_BLE_API_VERSION > 7)
	scan_ctx->scan_params.timeout = NRF_BLE_SCAN_SCAN_DURATION;
	scan_ctx->scan_params.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL;
	scan_ctx->scan_params.scan_phys = BLE_GAP_PHY_1MBPS;
}

/**@brief Function for setting the default connection parameters.
 *
 * @param[out] p_scan_ctx    Pointer to the Scanning Module instance.
 */
static void nrf_ble_scan_default_conn_param_set(struct nrf_ble_scan *const scan_ctx)
{
	scan_ctx->conn_params.conn_sup_timeout = BLE_GAP_CP_CONN_SUP_TIMEOUT_MIN;
	scan_ctx->conn_params.min_conn_interval = BLE_GAP_CONN_INTVL_MS_TO_UNITS(25);
	scan_ctx->conn_params.max_conn_interval = BLE_GAP_CONN_INTVL_MS_TO_UNITS(25);
	scan_ctx->conn_params.slave_latency = (uint16_t)NRF_BLE_SCAN_SLAVE_LATENCY;
}

/**@brief Function for calling the BLE_GAP_EVT_TIMEOUT event.
 *
 * @param[in] p_scan_ctx  Pointer to the Scanning Module instance.
 * @param[in] p_gap       GAP event structure.
 */
static void nrf_ble_scan_on_timeout(struct nrf_ble_scan const *const scan_ctx,
				    ble_gap_evt_t const *const gap)
{
	ble_gap_evt_timeout_t const *timeout = &gap->params.timeout;
	struct scan_evt scan_evt = {0};

	if (timeout->src == BLE_GAP_TIMEOUT_SRC_SCAN) {
		LOG_DBG("BLE_GAP_SCAN_TIMEOUT");
		if (scan_ctx->evt_handler != NULL) {
			scan_evt.scan_evt_id = NRF_BLE_SCAN_EVT_SCAN_TIMEOUT;
			scan_evt.p_scan_params = &scan_ctx->scan_params;
			scan_evt.params.timeout.src = timeout->src;

			scan_ctx->evt_handler(&scan_evt);
		}
	}
}

/**@brief Function for stopping the scanning.
 */
void nrf_ble_scan_stop(void)
{
	// It is ok to ignore the function return value here, because this function can return
	// NRF_SUCCESS or NRF_ERROR_INVALID_STATE, when app is not in the scanning state.
	(void)sd_ble_gap_scan_stop();
}

int nrf_ble_scan_init(struct nrf_ble_scan *const scan_ctx,
		      struct nrf_ble_scan_init const *const init,
		      nrf_ble_scan_evt_handler_t evt_handler)
{
	if (scan_ctx == NULL) {
		return NRF_ERROR_NULL;
	}

	scan_ctx->evt_handler = evt_handler;

#if BT_SCAN_FILTER_ENABLE
	// Disable all scanning filters.
	memset(&p_scan_ctx->scan_filters, 0, sizeof(p_scan_ctx->scan_filters));
#endif

	// If the pointer to the initialization structure exist, use it to scan the configuration.
	if (init != NULL) {
		scan_ctx->connect_if_match = init->connect_if_match;
		scan_ctx->conn_cfg_tag = init->conn_cfg_tag;

		if (init->p_scan_param != NULL) {
			scan_ctx->scan_params = *init->p_scan_param;
		} else {
			// Use the default static configuration.
			nrf_ble_scan_default_param_set(scan_ctx);
		}

		if (init->p_conn_param != NULL) {
			scan_ctx->conn_params = *init->p_conn_param;
		} else {
			// Use the default static configuration.
			nrf_ble_scan_default_conn_param_set(scan_ctx);
		}
	}
	// If pointer is NULL, use the static default configuration.
	else {
		nrf_ble_scan_default_param_set(scan_ctx);
		nrf_ble_scan_default_conn_param_set(scan_ctx);

		scan_ctx->connect_if_match = false;
	}

	// Assign a buffer where the advertising reports are to be stored by the SoftDevice.
	scan_ctx->scan_buffer.p_data = scan_ctx->scan_buffer_data;
	scan_ctx->scan_buffer.len = NRF_BLE_SCAN_BUFFER;

	return NRF_SUCCESS;
}

int nrf_ble_scan_start(struct nrf_ble_scan const *const p_scan_ctx)
{
	// VERIFY_PARAM_NOT_NULL(p_scan_ctx);

	if (p_scan_ctx == NULL) {
		LOG_ERR("Scan ctx is NULL");
	}

	int err_code;
	struct scan_evt scan_evt = {0};

	nrf_ble_scan_stop();

	// If the whitelist is used and the event handler is not NULL, send the whitelist request to
	// the main application.
	if (is_whitelist_used(p_scan_ctx)) {
		if (p_scan_ctx->evt_handler != NULL) {
			scan_evt.scan_evt_id = NRF_BLE_SCAN_EVT_WHITELIST_REQUEST;
			p_scan_ctx->evt_handler(&scan_evt);
		}
	}

	// Start the scanning.
	err_code = sd_ble_gap_scan_start(&p_scan_ctx->scan_params, &p_scan_ctx->scan_buffer);

	// It is okay to ignore this error, because the scan stopped earlier.
	if ((err_code != NRF_ERROR_INVALID_STATE) && (err_code != NRF_SUCCESS)) {
		LOG_ERR("sd_ble_gap_scan_start returned 0x%x", err_code);
		return (err_code);
	}
	LOG_DBG("Scanning");

	return NRF_SUCCESS;
}

int nrf_ble_scan_params_set(struct nrf_ble_scan *const p_scan_ctx,
			    ble_gap_scan_params_t const *const p_scan_param)
{
	// VERIFY_PARAM_NOT_NULL(p_scan_ctx);
	nrf_ble_scan_stop();

	if (p_scan_param != NULL) {
		// Assign new scanning parameters.
		p_scan_ctx->scan_params = *p_scan_param;
	} else {
		// If NULL, use the default static configuration.
		nrf_ble_scan_default_param_set(p_scan_ctx);
	}

	LOG_DBG("Scanning parameters have been changed successfully");

	return NRF_SUCCESS;
}

/**@brief Function for calling the BLE_GAP_EVT_CONNECTED event.
 *
 * @param[in] p_scan_ctx  Pointer to the Scanning Module instance.
 * @param[in] p_gap_evt   GAP event structure.
 */
static void nrf_ble_scan_on_connected_evt(struct nrf_ble_scan const *const p_scan_ctx,
					  ble_gap_evt_t const *const p_gap_evt)
{
	struct scan_evt scan_evt;

	memset(&scan_evt, 0, sizeof(scan_evt));
	scan_evt.scan_evt_id = NRF_BLE_SCAN_EVT_CONNECTED;
	scan_evt.params.connected.p_connected = &p_gap_evt->params.connected;
	scan_evt.params.connected.conn_handle = p_gap_evt->conn_handle;
	scan_evt.p_scan_params = &p_scan_ctx->scan_params;

	if (p_scan_ctx->evt_handler != NULL) {
		p_scan_ctx->evt_handler(&scan_evt);
	}
}

int nrf_ble_scan_copy_addr_to_sd_gap_addr(ble_gap_addr_t *p_gap_addr,
					  const uint8_t addr[BLE_GAP_ADDR_LEN])
{
	// VERIFY_PARAM_NOT_NULL(p_gap_addr);

	for (uint8_t i = 0; i < BLE_GAP_ADDR_LEN; ++i) {
		p_gap_addr->addr[i] = addr[BLE_GAP_ADDR_LEN - (i + 1)];
	}

	return NRF_SUCCESS;
}

void nrf_ble_scan_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_contex)
{
	struct nrf_ble_scan *p_scan_data = (struct nrf_ble_scan *)p_contex;
	ble_gap_evt_adv_report_t const *p_adv_report = &p_ble_evt->evt.gap_evt.params.adv_report;
	ble_gap_evt_t const *p_gap_evt = &p_ble_evt->evt.gap_evt;

	switch (p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_ADV_REPORT:
		nrf_ble_scan_on_adv_report(p_scan_data, p_adv_report);
		break;

	case BLE_GAP_EVT_TIMEOUT:
		nrf_ble_scan_on_timeout(p_scan_data, p_gap_evt);
		break;

	case BLE_GAP_EVT_CONNECTED:
		nrf_ble_scan_on_connected_evt(p_scan_data, p_gap_evt);
		break;

	default:
		break;
	}
}
