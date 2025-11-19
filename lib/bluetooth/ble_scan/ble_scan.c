/*
 * Copyright (c) 2015 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <bm/bluetooth/ble_scan.h>
#include <zephyr/logging/log.h>
#include <bm/bluetooth/ble_adv_data.h>

LOG_MODULE_REGISTER(ble_scan, CONFIG_BLE_SCAN_LOG_LEVEL);

static void ble_scan_connect_with_target(struct ble_scan const *const scan_ctx,
					 ble_gap_evt_adv_report_t const *const adv_report)
{
	int err_code;
	struct scan_evt scan_evt;

	/* For readability. */
	ble_gap_addr_t const *addr = &adv_report->peer_addr;
	ble_gap_scan_params_t const *scan_params = &scan_ctx->scan_params;
	ble_gap_conn_params_t const *conn_params = &scan_ctx->conn_params;
	uint8_t con_cfg_tag = scan_ctx->conn_cfg_tag;

	/* Return if the automatic connection is disabled. */
	if (!scan_ctx->connect_if_match) {
		return;
	}

	/* Stop scanning. */
	ble_scan_stop();

	memset(&scan_evt, 0, sizeof(scan_evt));

	/* Establish connection. */
	err_code = sd_ble_gap_connect(addr, scan_params, conn_params, con_cfg_tag);
	if (err_code != NRF_SUCCESS) {
		/* TODO: Trigger event handler with error */
	}

	LOG_DBG("Connecting");

	scan_evt.scan_evt_id = BLE_SCAN_EVT_CONNECTING_ERROR;
	scan_evt.params.connecting_err.err_code = err_code;

	LOG_DBG("Connection status: %d", err_code);

	/* If an error occurred, send an event to the event handler. */
	if ((err_code != NRF_SUCCESS) && (scan_ctx->evt_handler != NULL)) {
		scan_ctx->evt_handler(&scan_evt);
	}
}

#if CONFIG_BLE_SCAN_FILTER_ENABLE
#if (CONFIG_BLE_SCAN_ADDRESS_CNT > 0)

static bool find_peer_addr(ble_gap_evt_adv_report_t const *const adv_report,
			   ble_gap_addr_t const *addr)
{
	/* Compare addresses.*/
	if (memcmp(addr->addr, adv_report->peer_addr.addr, sizeof(adv_report->peer_addr.addr)) ==
	    0) {
		return true;
	}

	return false;
}

static bool adv_addr_compare(ble_gap_evt_adv_report_t const *const adv_report,
			     struct ble_scan const *const scan_ctx)
{
	ble_gap_addr_t const *addr = scan_ctx->scan_filters.addr_filter.target_addr;
	uint8_t counter = scan_ctx->scan_filters.addr_filter.addr_cnt;

	for (uint8_t index = 0; index < counter; index++) {
		/* Search for address. */
		if (find_peer_addr(adv_report, &addr[index])) {
			return true;
		}
	}

	return false;
}

static int ble_scan_addr_filter_add(struct ble_scan *const scan_ctx, uint8_t const *addr)
{
	ble_gap_addr_t *addr_filter = scan_ctx->scan_filters.addr_filter.target_addr;
	uint8_t *counter = &scan_ctx->scan_filters.addr_filter.addr_cnt;
	uint8_t index;

	/* If no memory for filter. */
	if (*counter >= CONFIG_BLE_SCAN_ADDRESS_CNT) {
		return NRF_ERROR_NO_MEM;
	}

	/* Check for duplicated filter.*/
	for (index = 0; index < CONFIG_BLE_SCAN_ADDRESS_CNT; index++) {
		if (!memcmp(addr_filter[index].addr, addr, BLE_GAP_ADDR_LEN)) {
			return NRF_SUCCESS;
		}
	}

	for (uint8_t i = 0; i < BLE_GAP_ADDR_LEN; i++) {
		addr_filter[*counter].addr[i] = addr[i];
	}

	/* Address type is not used so set it to 0. */
	addr_filter[*counter].addr_type = 0;

	LOG_HEXDUMP_DBG(addr_filter[*counter].addr, BLE_GAP_ADDR_LEN, "Filter set on address");

	/* Increase the address filter counter. */
	*counter += 1;

	return NRF_SUCCESS;
}

#endif /* CONFIG_BLE_SCAN_ADDRESS_CNT */

#if (CONFIG_BLE_SCAN_NAME_CNT > 0)
static uint16_t advdata_search(uint8_t const *encoded_data, uint16_t data_len, uint16_t *offset,
			       uint8_t ad_type)
{
	if ((encoded_data == NULL) || (offset == NULL)) {
		return 0;
	}

	uint16_t i = 0;

	while ((i + 1 < data_len) && ((i < *offset) || (encoded_data[i + 1] != ad_type))) {
		/* Jump to next data. */
		i += (encoded_data[i] + 1);
	}

	if (i >= data_len) {
		return 0;
	}
	uint16_t new_offset = i + 2;

	uint16_t len = encoded_data[i] ? (encoded_data[i] - 1) : 0;

	if (!len || ((new_offset + len) > data_len)) {
		/* Malformed. Zero length or extends beyond provided data. */
		return 0;
	}
	*offset = new_offset;
	return len;
}

static bool advdata_name_find(uint8_t const *encoded_data, uint16_t data_len,
			      char const *target_name)
{
	uint16_t parsed_name_len;
	uint8_t const *parsed_name;
	uint16_t data_offset = 0;

	if (target_name == NULL) {
		return false;
	}

	parsed_name_len = advdata_search(encoded_data, data_len, &data_offset,
					 BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME);

	parsed_name = &encoded_data[data_offset];

	if ((data_offset != 0) && (parsed_name_len != 0) &&
	    (strlen(target_name) == parsed_name_len) &&
	    (memcmp(target_name, parsed_name, parsed_name_len) == 0)) {
		return true;
	}

	return false;
}

static bool adv_name_compare(ble_gap_evt_adv_report_t const *adv_report,
			     struct ble_scan const *const scan_ctx)
{
	struct ble_scan_name_filter const *name_filter = &scan_ctx->scan_filters.name_filter;
	uint8_t counter = scan_ctx->scan_filters.name_filter.name_cnt;
	uint8_t index;
	uint16_t data_len;

	data_len = adv_report->data.len;

	/* Compare the name found with the name filter. */
	for (index = 0; index < counter; index++) {
		if (advdata_name_find(adv_report->data.p_data, data_len,
				      name_filter->target_name[index])) {
			return true;
		}
	}

	return false;
}

static int ble_scan_name_filter_add(struct ble_scan *const scan_ctx, char const *name)
{
	uint8_t index;
	uint8_t *counter = &scan_ctx->scan_filters.name_filter.name_cnt;
	uint8_t name_len = strlen(name);

	/* Check the name length. */
	if ((name_len == 0) || (name_len > CONFIG_BLE_SCAN_NAME_MAX_LEN)) {
		return NRF_ERROR_DATA_SIZE;
	}

	/* If no memory for filter. */
	if (*counter >= CONFIG_BLE_SCAN_NAME_CNT) {
		return NRF_ERROR_NO_MEM;
	}

	/* Check for duplicated filter. */
	for (index = 0; index < CONFIG_BLE_SCAN_NAME_CNT; index++) {
		if (!strcmp(scan_ctx->scan_filters.name_filter.target_name[index], name)) {
			return NRF_SUCCESS;
		}
	}

	/* Add name to filter. */
	memcpy(scan_ctx->scan_filters.name_filter.target_name[(*counter)++], name, strlen(name));

	LOG_DBG("Adding filter on %s name", name);

	return NRF_SUCCESS;
}

#endif /* CONFIG_BLE_SCAN_NAME_CNT */

#if (CONFIG_BLE_SCAN_SHORT_NAME_CNT > 0)
static bool adv_short_name_compare(ble_gap_evt_adv_report_t const *const adv_report,
				   struct ble_scan const *const scan_ctx)
{
	struct ble_scan_short_name_filter const *name_filter =
		&scan_ctx->scan_filters.short_name_filter;
	uint8_t counter = scan_ctx->scan_filters.short_name_filter.name_cnt;
	uint8_t index;
	uint16_t data_len;

	data_len = adv_report->data.len;

	/* Compare the name found with the name filters. */
	for (index = 0; index < counter; index++) {
		if (ble_adv_data_short_name_find(
			    adv_report->data.p_data, data_len,
			    name_filter->short_name[index].short_target_name,
			    name_filter->short_name[index].short_name_min_len)) {
			return true;
		}
	}

	return false;
}

static int ble_scan_short_name_filter_add(struct ble_scan *const scan_ctx,
					  struct ble_scan_short_name const *short_name)
{
	uint8_t index;
	uint8_t *counter = &scan_ctx->scan_filters.short_name_filter.name_cnt;
	struct ble_scan_short_name_filter *short_name_filter =
		&scan_ctx->scan_filters.short_name_filter;
	uint8_t name_len = strlen(short_name->short_name);

	/* Check the name length. */
	if ((name_len == 0) || (name_len > CONFIG_BLE_SCAN_SHORT_NAME_MAX_LEN)) {
		return NRF_ERROR_DATA_SIZE;
	}

	/* If no memory for filter. */
	if (*counter >= CONFIG_BLE_SCAN_SHORT_NAME_CNT) {
		return NRF_ERROR_NO_MEM;
	}

	/* Check for duplicated filter. */
	for (index = 0; index < CONFIG_BLE_SCAN_SHORT_NAME_CNT; index++) {
		if (!strcmp(short_name_filter->short_name[index].short_target_name,
			    short_name->short_name)) {
			return NRF_SUCCESS;
		}
	}

	/* Add name to the filter. */
	short_name_filter->short_name[(*counter)].short_name_min_len =
		short_name->short_name_min_len;
	memcpy(short_name_filter->short_name[(*counter)++].short_target_name,
	       short_name->short_name, strlen(short_name->short_name));

	LOG_DBG("Adding filter on %s name", short_name->short_name);

	return NRF_SUCCESS;
}

#endif

#if (CONFIG_BLE_SCAN_UUID_CNT > 0)

static bool adv_uuid_compare(ble_gap_evt_adv_report_t const *const adv_report,
			     struct ble_scan const *const scan_ctx)
{
	struct ble_scan_uuid_filter const *uuid_filter = &scan_ctx->scan_filters.uuid_filter;
	bool const all_filters_mode = scan_ctx->scan_filters.all_filters_mode;
	uint8_t const counter = scan_ctx->scan_filters.uuid_filter.uuid_cnt;
	uint8_t index;
	uint16_t data_len;
	uint8_t uuid_match_cnt = 0;

	data_len = adv_report->data.len;

	for (index = 0; index < counter; index++) {

		if (ble_adv_data_uuid_find(adv_report->data.p_data, data_len,
					   &uuid_filter->uuid[index])) {
			uuid_match_cnt++;

			/* In the normal filter mode, only one UUID is needed to match. */
			if (!all_filters_mode) {
				break;
			}
		} else if (all_filters_mode) {
			break;
		}
	}

	/* In the multifilter mode, all UUIDs must be found in the advertisement packets. */
	if ((all_filters_mode && (uuid_match_cnt == counter)) ||
	    ((!all_filters_mode) && (uuid_match_cnt > 0))) {
		return true;
	}

	return false;
}

static int ble_scan_uuid_filter_add(struct ble_scan *const scan_ctx, ble_uuid_t const *uuid)
{
	ble_uuid_t *uuid_filter = scan_ctx->scan_filters.uuid_filter.uuid;
	uint8_t *counter = &scan_ctx->scan_filters.uuid_filter.uuid_cnt;
	uint8_t index;

	/* If no memory. */
	if (*counter >= CONFIG_BLE_SCAN_UUID_CNT) {
		return NRF_ERROR_NO_MEM;
	}

	/* Check for duplicated filter.*/
	for (index = 0; index < CONFIG_BLE_SCAN_UUID_CNT; index++) {
		if (uuid_filter[index].uuid == uuid->uuid) {
			return NRF_SUCCESS;
		}
	}

	/* Add UUID to the filter. */
	uuid_filter[(*counter)++] = *uuid;
	LOG_DBG("Added filter on UUID %x", uuid->uuid);

	return NRF_SUCCESS;
}

#endif /* CONFIG_BLE_SCAN_UUID_CNT */

#if (CONFIG_BLE_SCAN_APPEARANCE_CNT)

static bool adv_appearance_compare(ble_gap_evt_adv_report_t const *const adv_report,
				   struct ble_scan const *const scan_ctx)
{
	struct ble_scan_appearance_filter const *appearance_filter =
		&scan_ctx->scan_filters.appearance_filter;
	uint8_t const counter = scan_ctx->scan_filters.appearance_filter.appearance_cnt;
	uint8_t index;
	uint16_t data_len;

	data_len = adv_report->data.len;

	/* Verify if the advertised appearance matches the provided appearance. */
	for (index = 0; index < counter; index++) {
		if (ble_adv_data_appearance_find(adv_report->data.p_data, data_len,
						 &appearance_filter->appearance[index])) {
			return true;
		}
	}
	return false;
}

static int ble_scan_appearance_filter_add(struct ble_scan *const scan_ctx, uint16_t appearance)
{
	uint16_t *appearance_filter = scan_ctx->scan_filters.appearance_filter.appearance;
	uint8_t *counter = &scan_ctx->scan_filters.appearance_filter.appearance_cnt;
	uint8_t index;

	/* If no memory. */
	if (*counter >= CONFIG_BLE_SCAN_APPEARANCE_CNT) {
		return NRF_ERROR_NO_MEM;
	}

	/* Check for duplicated filter. */
	for (index = 0; index < CONFIG_BLE_SCAN_APPEARANCE_CNT; index++) {
		if (appearance_filter[index] == appearance) {
			return NRF_SUCCESS;
		}
	}

	/* Add appearance to the filter. */
	appearance_filter[(*counter)++] = appearance;
	LOG_DBG("Added filter on appearance %x", appearance);
	return NRF_SUCCESS;
}

#endif /* CONFIG_BLE_SCAN_APPEARANCE_CNT */

int ble_scan_filter_set(struct ble_scan *const scan_ctx, enum ble_scan_filter_type type,
			void const *data)
{
	if (scan_ctx == NULL) {
		return NRF_ERROR_NULL;
	}
	if (data == NULL) {
		return NRF_ERROR_NULL;
	}

	switch (type) {
#if (CONFIG_BLE_SCAN_NAME_CNT > 0)
	case SCAN_NAME_FILTER: {
		char *name = (char *)data;

		return ble_scan_name_filter_add(scan_ctx, name);
	}
#endif

#if (CONFIG_BLE_SCAN_SHORT_NAME_CNT > 0)
	case SCAN_SHORT_NAME_FILTER: {
		struct ble_scan_short_name *short_name = (struct ble_scan_short_name *)data;

		return ble_scan_short_name_filter_add(scan_ctx, short_name);
	}
#endif

#if (CONFIG_BLE_SCAN_ADDRESS_CNT > 0)
	case SCAN_ADDR_FILTER: {
		uint8_t *addr = (uint8_t *)data;

		return ble_scan_addr_filter_add(scan_ctx, addr);
	}
#endif

#if (CONFIG_BLE_SCAN_UUID_CNT > 0)
	case SCAN_UUID_FILTER: {
		ble_uuid_t *uuid = (ble_uuid_t *)data;

		return ble_scan_uuid_filter_add(scan_ctx, uuid);
	}
#endif

#if (CONFIG_BLE_SCAN_APPEARANCE_CNT > 0)
	case SCAN_APPEARANCE_FILTER: {
		uint16_t appearance = *((uint16_t *)data);

		return ble_scan_appearance_filter_add(scan_ctx, appearance);
	}
#endif

	default:
		return NRF_ERROR_INVALID_PARAM;
	}
}

int ble_scan_all_filter_remove(struct ble_scan *const scan_ctx)
{
#if (CONFIG_BLE_SCAN_NAME_CNT > 0)
	struct ble_scan_name_filter *name_filter = &scan_ctx->scan_filters.name_filter;

	memset(name_filter->target_name, 0, sizeof(name_filter->target_name));
	name_filter->name_cnt = 0;
#endif

#if (CONFIG_BLE_SCAN_SHORT_NAME_CNT > 0)
	struct ble_scan_short_name_filter *short_name_filter =
		&scan_ctx->scan_filters.short_name_filter;

	memset(short_name_filter->short_name, 0, sizeof(short_name_filter->short_name));
	short_name_filter->name_cnt = 0;
#endif

#if (CONFIG_BLE_SCAN_ADDRESS_CNT > 0)
	struct ble_scan_addr_filter *addr_filter = &scan_ctx->scan_filters.addr_filter;

	memset(addr_filter->target_addr, 0, sizeof(addr_filter->target_addr));
	addr_filter->addr_cnt = 0;
#endif

#if (CONFIG_BLE_SCAN_UUID_CNT > 0)
	struct ble_scan_uuid_filter *uuid_filter = &scan_ctx->scan_filters.uuid_filter;

	memset(uuid_filter->uuid, 0, sizeof(uuid_filter->uuid));
	uuid_filter->uuid_cnt = 0;
#endif

#if (CONFIG_BLE_SCAN_APPEARANCE_CNT > 0)
	struct ble_scan_appearance_filter *appearance_filter =
		&scan_ctx->scan_filters.appearance_filter;

	memset(appearance_filter->appearance, 0, sizeof(appearance_filter->appearance));
	appearance_filter->appearance_cnt = 0;
#endif

	return NRF_SUCCESS;
}

int ble_scan_filters_enable(struct ble_scan *const scan_ctx, uint8_t mode, bool match_all)
{
	if (!scan_ctx) {
		return NRF_ERROR_NULL;
	}

	/* Check if the mode is correct. */
	if ((!(mode & BLE_SCAN_ADDR_FILTER)) && (!(mode & BLE_SCAN_NAME_FILTER)) &&
	    (!(mode & BLE_SCAN_UUID_FILTER)) && (!(mode & BLE_SCAN_SHORT_NAME_FILTER)) &&
	    (!(mode & BLE_SCAN_APPEARANCE_FILTER))) {
		return NRF_ERROR_INVALID_PARAM;
	}

	int err_code;

	/* Disable filters.*/
	err_code = ble_scan_filters_disable(scan_ctx);
	if (err_code != NRF_SUCCESS) {
		return err_code;
	}

	struct ble_scan_filters *filters = &scan_ctx->scan_filters;

	/* Turn on the filters of your choice. */
#if (CONFIG_BLE_SCAN_ADDRESS_CNT > 0)
	if (mode & BLE_SCAN_ADDR_FILTER) {
		filters->addr_filter.addr_filter_enabled = true;
	}
#endif

#if (CONFIG_BLE_SCAN_NAME_CNT > 0)
	if (mode & BLE_SCAN_NAME_FILTER) {
		filters->name_filter.name_filter_enabled = true;
	}
#endif

#if (CONFIG_BLE_SCAN_SHORT_NAME_CNT > 0)
	if (mode & BLE_SCAN_SHORT_NAME_FILTER) {
		filters->short_name_filter.short_name_filter_enabled = true;
	}
#endif

#if (CONFIG_BLE_SCAN_UUID_CNT > 0)
	if (mode & BLE_SCAN_UUID_FILTER) {
		filters->uuid_filter.uuid_filter_enabled = true;
	}
#endif

#if (CONFIG_BLE_SCAN_APPEARANCE_CNT > 0)
	if (mode & BLE_SCAN_APPEARANCE_FILTER) {
		filters->appearance_filter.appearance_filter_enabled = true;
	}
#endif

	/* Select the filter mode. */
	filters->all_filters_mode = match_all;

	return NRF_SUCCESS;
}

int ble_scan_filters_disable(struct ble_scan *const scan_ctx)
{
	if (!scan_ctx) {
		return NRF_ERROR_NULL;
	}

	/* Disable all filters.*/
#if (CONFIG_BLE_SCAN_NAME_CNT > 0)
	bool *name_filter_enabled = &scan_ctx->scan_filters.name_filter.name_filter_enabled;
	*name_filter_enabled = false;
#endif

#if (CONFIG_BLE_SCAN_ADDRESS_CNT > 0)
	bool *addr_filter_enabled = &scan_ctx->scan_filters.addr_filter.addr_filter_enabled;
	*addr_filter_enabled = false;
#endif

#if (CONFIG_BLE_SCAN_UUID_CNT > 0)
	bool *uuid_filter_enabled = &scan_ctx->scan_filters.uuid_filter.uuid_filter_enabled;
	*uuid_filter_enabled = false;
#endif

#if (CONFIG_BLE_SCAN_APPEARANCE_CNT > 0)
	bool *appearance_filter_enabled =
		&scan_ctx->scan_filters.appearance_filter.appearance_filter_enabled;
	*appearance_filter_enabled = false;
#endif

	return NRF_SUCCESS;
}

int ble_scan_filter_get(struct ble_scan *const scan_ctx, struct ble_scan_filters *status)
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

#endif /* CONFIG_BLE_SCAN_FILTER_ENABLE */

static void ble_scan_on_adv_report(struct ble_scan const *const scan_ctx,
				   ble_gap_evt_adv_report_t const *const adv_report)
{
	struct scan_evt scan_evt = {0};

#if CONFIG_BLE_SCAN_FILTER_ENABLE
	uint8_t filter_cnt = 0;
	uint8_t filter_match_cnt = 0;
#endif

	scan_evt.scan_params = &scan_ctx->scan_params;

	/* If the whitelist is used, do not check the filters and return. */
	if (is_whitelist_used(scan_ctx)) {
		scan_evt.scan_evt_id = BLE_SCAN_EVT_WHITELIST_ADV_REPORT;
		scan_evt.params.not_found = adv_report;
		scan_ctx->evt_handler(&scan_evt);

		sd_ble_gap_scan_start(NULL, &scan_ctx->scan_buffer);
		ble_scan_connect_with_target(scan_ctx, adv_report);

		return;
	}

#if CONFIG_BLE_SCAN_FILTER_ENABLE
	bool const all_filter_mode = scan_ctx->scan_filters.all_filters_mode;
	bool is_filter_matched = false;

#if (CONFIG_BLE_SCAN_ADDRESS_CNT > 0)
	bool const addr_filter_enabled = scan_ctx->scan_filters.addr_filter.addr_filter_enabled;
#endif

#if (CONFIG_BLE_SCAN_NAME_CNT > 0)
	bool const name_filter_enabled = scan_ctx->scan_filters.name_filter.name_filter_enabled;
#endif

#if (CONFIG_BLE_SCAN_SHORT_NAME_CNT > 0)
	bool const short_name_filter_enabled =
		scan_ctx->scan_filters.short_name_filter.short_name_filter_enabled;
#endif

#if (CONFIG_BLE_SCAN_UUID_CNT > 0)
	bool const uuid_filter_enabled = scan_ctx->scan_filters.uuid_filter.uuid_filter_enabled;
#endif

#if (CONFIG_BLE_SCAN_APPEARANCE_CNT > 0)
	bool const appearance_filter_enabled =
		scan_ctx->scan_filters.appearance_filter.appearance_filter_enabled;
#endif

#if (CONFIG_BLE_SCAN_ADDRESS_CNT > 0)
	/* Check the address filter. */
	if (addr_filter_enabled) {
		/* Number of active filters. */
		filter_cnt++;
		if (adv_addr_compare(adv_report, scan_ctx)) {
			/* Number of filters matched. */
			filter_match_cnt++;
			/* Information about the filters matched. */
			scan_evt.params.filter_match.filter_match.address_filter_match = true;
			is_filter_matched = true;
		}
	}
#endif

#if (CONFIG_BLE_SCAN_NAME_CNT > 0)
	/* Check the name filter. */
	if (name_filter_enabled) {
		filter_cnt++;
		if (adv_name_compare(adv_report, scan_ctx)) {
			filter_match_cnt++;

			/* Information about the filters matched. */
			scan_evt.params.filter_match.filter_match.name_filter_match = true;
			is_filter_matched = true;
		}
	}
#endif

#if (CONFIG_BLE_SCAN_SHORT_NAME_CNT > 0)
	if (short_name_filter_enabled) {
		filter_cnt++;
		if (adv_short_name_compare(adv_report, scan_ctx)) {
			filter_match_cnt++;

			/* Information about the filters matched. */
			scan_evt.params.filter_match.filter_match.short_name_filter_match = true;
			is_filter_matched = true;
		}
	}
#endif

#if (CONFIG_BLE_SCAN_UUID_CNT > 0)
	/* Check the UUID filter. */
	if (uuid_filter_enabled) {
		filter_cnt++;
		if (adv_uuid_compare(adv_report, scan_ctx)) {
			filter_match_cnt++;
			/* Information about the filters matched. */
			scan_evt.params.filter_match.filter_match.uuid_filter_match = true;
			is_filter_matched = true;
		}
	}
#endif

#if (CONFIG_BLE_SCAN_APPEARANCE_CNT > 0)
	/* Check the appearance filter. */
	if (appearance_filter_enabled) {
		filter_cnt++;
		if (adv_appearance_compare(adv_report, scan_ctx)) {
			filter_match_cnt++;
			/* Information about the filters matched. */
			scan_evt.params.filter_match.filter_match.appearance_filter_match = true;
			is_filter_matched = true;
		}
	}

	scan_evt.scan_evt_id = BLE_SCAN_EVT_NOT_FOUND;
#endif

	scan_evt.params.filter_match.adv_report = adv_report;

	/** In the multifilter mode, the number of the active filters must equal the number of the
	 *  filters matched to generate the notification.
	 */
	if (all_filter_mode && (filter_match_cnt == filter_cnt)) {
		scan_evt.scan_evt_id = BLE_SCAN_EVT_FILTER_MATCH;
		ble_scan_connect_with_target(scan_ctx, adv_report);
	}
	/** In the normal filter mode, only one filter match is needed to generate the notification
	 *  to the main application.
	 */
	else if ((!all_filter_mode) && is_filter_matched) {
		scan_evt.scan_evt_id = BLE_SCAN_EVT_FILTER_MATCH;
		ble_scan_connect_with_target(scan_ctx, adv_report);
	} else {
		scan_evt.scan_evt_id = BLE_SCAN_EVT_NOT_FOUND;
		scan_evt.params.not_found = adv_report;
	}

	/* If the event handler is not NULL, notify the main application. */
	if (scan_ctx->evt_handler != NULL) {
		scan_ctx->evt_handler(&scan_evt);
	}

#endif /* CONFIG_BLE_SCAN_FILTER_ENABLE*/

	/* Resume the scanning. */
	(void)sd_ble_gap_scan_start(NULL, &scan_ctx->scan_buffer);
}

bool is_whitelist_used(struct ble_scan const *const scan_ctx)
{
	if (scan_ctx->scan_params.filter_policy == BLE_GAP_SCAN_FP_WHITELIST ||
	    scan_ctx->scan_params.filter_policy ==
		    BLE_GAP_SCAN_FP_WHITELIST_NOT_RESOLVED_DIRECTED) {
		return true;
	}

	return false;
}

static void ble_scan_default_param_set(struct ble_scan *const scan_ctx)
{
	/* Set the default parameters. */
	scan_ctx->scan_params.active = 1;
#if (NRF_SD_BLE_API_VERSION > 7)
	scan_ctx->scan_params.interval_us = CONFG_BLE_SCAN_SCAN_INTERVAL * UNIT_0_625_MS;
	scan_ctx->scan_params.window_us = CONFIG_BLE_SCAN_SCAN_WINDOW * UNIT_0_625_MS;
#else
	scan_ctx->scan_params.interval = CONFIG_BLE_SCAN_SCAN_INTERVAL;
	scan_ctx->scan_params.window = CONFIG_BLE_SCAN_SCAN_WINDOW;
#endif /* #if (NRF_SD_BLE_API_VERSION > 7) */
	scan_ctx->scan_params.timeout = CONFIG_BLE_SCAN_SCAN_DURATION;
	scan_ctx->scan_params.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL;
	scan_ctx->scan_params.scan_phys = BLE_GAP_PHY_1MBPS;
}

static void ble_scan_default_conn_param_set(struct ble_scan *const scan_ctx)
{
	scan_ctx->conn_params.conn_sup_timeout = BLE_GAP_CP_CONN_SUP_TIMEOUT_MIN;
	scan_ctx->conn_params.min_conn_interval = CONFIG_BLE_SCAN_MIN_CONNECTION_INTERVAL;
	scan_ctx->conn_params.max_conn_interval = CONFIG_BLE_SCAN_MAX_CONNECTION_INTERVAL;
	scan_ctx->conn_params.slave_latency = (uint16_t)CONFIG_BLE_SCAN_SLAVE_LATENCY;
}

static void ble_scan_on_timeout(struct ble_scan const *const scan_ctx,
				ble_gap_evt_t const *const gap)
{
	ble_gap_evt_timeout_t const *timeout = &gap->params.timeout;
	struct scan_evt scan_evt = {0};

	if (timeout->src == BLE_GAP_TIMEOUT_SRC_SCAN) {
		LOG_DBG("BLE_GAP_SCAN_TIMEOUT");
		if (scan_ctx->evt_handler != NULL) {
			scan_evt.scan_evt_id = BLE_SCAN_EVT_SCAN_TIMEOUT;
			scan_evt.scan_params = &scan_ctx->scan_params;
			scan_evt.params.timeout.src = timeout->src;

			scan_ctx->evt_handler(&scan_evt);
		}
	}
}

void ble_scan_stop(void)
{
	/** It is ok to ignore the function return value here, because this function can return
	 *  NRF_SUCCESS or NRF_ERROR_INVALID_STATE, when app is not in the scanning state.
	 */
	(void)sd_ble_gap_scan_stop();
}

int ble_scan_init(struct ble_scan *const scan_ctx, struct ble_scan_init const *const init,
		  ble_scan_evt_handler_t evt_handler)
{
	if (scan_ctx == NULL) {
		return NRF_ERROR_NULL;
	}

	scan_ctx->evt_handler = evt_handler;

#if CONFIG_BLE_SCAN_FILTER_ENABLE
	/* Disable all scanning filters. */
	memset(&scan_ctx->scan_filters, 0, sizeof(scan_ctx->scan_filters));
#endif

	/** If the pointer to the initialization structure exist, use it to scan the configuration.
	 */
	if (init != NULL) {
		scan_ctx->connect_if_match = init->connect_if_match;
		scan_ctx->conn_cfg_tag = init->conn_cfg_tag;

		if (init->scan_param != NULL) {
			scan_ctx->scan_params = *init->scan_param;
		} else {
			/* Use the default static configuration. */
			ble_scan_default_param_set(scan_ctx);
		}

		if (init->conn_param != NULL) {
			scan_ctx->conn_params = *init->conn_param;
		} else {
			/* Use the default static configuration. */
			ble_scan_default_conn_param_set(scan_ctx);
		}
	}
	/* If pointer is NULL, use the static default configuration. */
	else {
		ble_scan_default_param_set(scan_ctx);
		ble_scan_default_conn_param_set(scan_ctx);

		scan_ctx->connect_if_match = false;
	}

	/* Assign a buffer where the advertising reports are to be stored by the SoftDevice. */
	scan_ctx->scan_buffer.p_data = scan_ctx->scan_buffer_data;
	scan_ctx->scan_buffer.len = CONFIG_BLE_SCAN_BUFFER;

	return NRF_SUCCESS;
}

int ble_scan_start(struct ble_scan const *const scan_ctx)
{

	if (!scan_ctx) {
		return NRF_ERROR_NULL;
	}

	int err_code;
	struct scan_evt scan_evt = {0};

	ble_scan_stop();

	/** If the whitelist is used and the event handler is not NULL, send the whitelist request
	 *  to the main application.
	 */
	if (is_whitelist_used(scan_ctx)) {
		if (scan_ctx->evt_handler != NULL) {
			scan_evt.scan_evt_id = BLE_SCAN_EVT_WHITELIST_REQUEST;
			scan_ctx->evt_handler(&scan_evt);
		}
	}

	/* Start the scanning. */
	err_code = sd_ble_gap_scan_start(&scan_ctx->scan_params, &scan_ctx->scan_buffer);

	/* It is okay to ignore this error, because the scan stopped earlier. */
	if ((err_code != NRF_ERROR_INVALID_STATE) && (err_code != NRF_SUCCESS)) {
		LOG_ERR("sd_ble_gap_scan_start returned 0x%x", err_code);
		return err_code;
	}
	LOG_DBG("Scanning");

	return NRF_SUCCESS;
}

int ble_scan_params_set(struct ble_scan *const scan_ctx,
			ble_gap_scan_params_t const *const scan_param)
{
	if (!scan_ctx) {
		return NRF_ERROR_NULL;
	}
	ble_scan_stop();

	if (scan_param != NULL) {
		/* Assign new scanning parameters. */
		scan_ctx->scan_params = *scan_param;
	} else {
		/* If NULL, use the default static configuration. */
		ble_scan_default_param_set(scan_ctx);
	}

	LOG_DBG("Scanning parameters have been changed successfully");

	return NRF_SUCCESS;
}

static void ble_scan_on_connected_evt(struct ble_scan const *const scan_ctx,
				      ble_gap_evt_t const *const gap_evt)
{
	struct scan_evt scan_evt = {0};

	scan_evt.scan_evt_id = BLE_SCAN_EVT_CONNECTED;
	scan_evt.params.connected.connected = &gap_evt->params.connected;
	scan_evt.params.connected.conn_handle = gap_evt->conn_handle;
	scan_evt.scan_params = &scan_ctx->scan_params;

	if (scan_ctx->evt_handler != NULL) {
		scan_ctx->evt_handler(&scan_evt);
	}
}

int ble_scan_copy_addr_to_sd_gap_addr(ble_gap_addr_t *gap_addr,
				      const uint8_t addr[BLE_GAP_ADDR_LEN])
{
	if (!gap_addr) {
		return NRF_ERROR_NULL;
	}

	for (uint8_t i = 0; i < BLE_GAP_ADDR_LEN; ++i) {
		gap_addr->addr[i] = addr[BLE_GAP_ADDR_LEN - (i + 1)];
	}

	return NRF_SUCCESS;
}

void ble_scan_on_ble_evt(ble_evt_t const *ble_evt, void *contex)
{
	struct ble_scan *scan_data = (struct ble_scan *)contex;
	ble_gap_evt_adv_report_t const *adv_report = &ble_evt->evt.gap_evt.params.adv_report;
	ble_gap_evt_t const *gap_evt = &ble_evt->evt.gap_evt;

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_ADV_REPORT:
		ble_scan_on_adv_report(scan_data, adv_report);
		break;

	case BLE_GAP_EVT_TIMEOUT:
		ble_scan_on_timeout(scan_data, gap_evt);
		break;

	case BLE_GAP_EVT_CONNECTED:
		ble_scan_on_connected_evt(scan_data, gap_evt);
		break;

	default:
		break;
	}
}
