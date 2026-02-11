/*
 * Copyright (c) 2015 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <bm/bluetooth/ble_scan.h>
#include <bm/bluetooth/ble_adv_data.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_scan, CONFIG_BLE_SCAN_LOG_LEVEL);

uint32_t ble_scan_copy_addr_to_sd_gap_addr(ble_gap_addr_t *gap_addr,
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

static void ble_scan_connect_with_target(const struct ble_scan *scan,
					 const ble_gap_evt_adv_report_t *adv_report)
{
	uint32_t nrf_err;
	struct ble_scan_evt scan_evt = {
		.evt_type = BLE_SCAN_EVT_CONNECTING_ERROR,
	};

	/* Establish connection. */
	nrf_err = sd_ble_gap_connect(&adv_report->peer_addr, &scan->scan_params,
				     &scan->conn_params, scan->conn_cfg_tag);
	if (nrf_err) {
		LOG_ERR("Connection failed, nrf_error %#x", nrf_err);
		if (scan->evt_handler) {
			scan_evt.params.connecting_err.reason = nrf_err;
			scan->evt_handler(&scan_evt);
		}
	}
}

#if defined(CONFIG_BLE_SCAN_FILTER)

#if (CONFIG_BLE_SCAN_ADDRESS_COUNT > 0)
static bool adv_addr_compare(const ble_gap_evt_adv_report_t *adv_report,
			     const struct ble_scan *scan)
{
	const ble_gap_addr_t *addr = scan->scan_filters.addr_filter.target_addr;
	const ble_gap_addr_t *peer_addr;

	/* Search for address. */
	for (uint8_t i = 0; i < scan->scan_filters.addr_filter.addr_cnt; i++) {
		peer_addr = &adv_report->peer_addr;
		if (memcmp(addr[i].addr, peer_addr->addr, sizeof(peer_addr->addr)) == 0) {
			return true;
		}
	}

	return false;
}

static int addr_filter_add(struct ble_scan *scan, const struct ble_scan_filter_data *data)
{
	uint8_t *addr = data->addr_filter.addr;
	ble_gap_addr_t *addr_filter = scan->scan_filters.addr_filter.target_addr;
	uint8_t *counter = &scan->scan_filters.addr_filter.addr_cnt;

	/* Check for duplicated filter. */
	for (uint8_t i = 0; i < CONFIG_BLE_SCAN_ADDRESS_COUNT; i++) {
		if (!memcmp(addr_filter[i].addr, addr, BLE_GAP_ADDR_LEN)) {
			return NRF_SUCCESS;
		}
	}

	/* If no memory for filter. */
	if (*counter >= CONFIG_BLE_SCAN_ADDRESS_COUNT) {
		return NRF_ERROR_NO_MEM;
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
#endif /* CONFIG_BLE_SCAN_ADDRESS_COUNT */

#if (CONFIG_BLE_SCAN_NAME_COUNT > 0)
static uint16_t advdata_search(const uint8_t *encoded_data, uint16_t data_len, uint16_t *offset,
			       uint8_t ad_type)
{
	uint16_t i = 0;
	uint16_t new_offset;
	uint16_t len;

	__ASSERT_NO_MSG(offset != NULL);

	if (!encoded_data) {
		return 0;
	}

	while ((i + 1 < data_len) && ((i < *offset) || (encoded_data[i + 1] != ad_type))) {
		/* Jump to next data. */
		i += (encoded_data[i] + 1);
	}

	if (i >= data_len) {
		return 0;
	}

	new_offset = i + 2;
	len = encoded_data[i] ? (encoded_data[i] - 1) : 0;

	if (!len || ((new_offset + len) > data_len)) {
		/* Malformed. Zero length or extends beyond provided data. */
		return 0;
	}

	*offset = new_offset;

	return len;
}

static bool advdata_name_find(const uint8_t *encoded_data, uint16_t data_len,
			      const char *target_name)
{
	uint16_t parsed_name_len;
	const uint8_t *parsed_name;
	uint16_t data_offset = 0;

	__ASSERT_NO_MSG(target_name != NULL);

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

static bool adv_name_compare(const struct ble_scan *scan, uint8_t *data, uint16_t len)
{
	const struct ble_scan_name_filter *name_filter = &scan->scan_filters.name_filter;

	/* Compare the name found with the name filter. */
	for (uint8_t i = 0; i < scan->scan_filters.name_filter.name_cnt; i++) {
		if (advdata_name_find(data, len, name_filter->target_name[i])) {
			return true;
		}
	}

	return false;
}

static int name_filter_add(struct ble_scan *scan, const struct ble_scan_filter_data *data)
{
	char *name = data->name_filter.name;
	uint8_t *counter = &scan->scan_filters.name_filter.name_cnt;
	uint8_t name_len = strlen(name);

	/* Check the name length. */
	if ((name_len == 0) || (name_len > CONFIG_BLE_SCAN_NAME_MAX_LEN)) {
		return NRF_ERROR_DATA_SIZE;
	}

	/* Check for duplicated filter. */
	for (uint8_t i = 0; i < CONFIG_BLE_SCAN_NAME_COUNT; i++) {
		if (!strcmp(scan->scan_filters.name_filter.target_name[i], name)) {
			return NRF_SUCCESS;
		}
	}

	/* If no memory for filter. */
	if (*counter >= CONFIG_BLE_SCAN_NAME_COUNT) {
		return NRF_ERROR_NO_MEM;
	}

	/* Add name to filter. */
	memcpy(scan->scan_filters.name_filter.target_name[(*counter)++], name, strlen(name));

	LOG_DBG("Adding filter on %s name", name);

	return NRF_SUCCESS;
}
#endif /* CONFIG_BLE_SCAN_NAME_COUNT */

#if (CONFIG_BLE_SCAN_SHORT_NAME_COUNT > 0)
static bool adv_short_name_compare(const struct ble_scan *scan, uint8_t *data, uint16_t len)
{
	const struct ble_scan_short_name_filter *name_filter =
		&scan->scan_filters.short_name_filter;

	/* Compare the name found with the name filters. */
	for (uint8_t i = 0; i < scan->scan_filters.short_name_filter.name_cnt; i++) {
		if (ble_adv_data_short_name_find(
			    data, len,
			    name_filter->short_name[i].short_target_name,
			    name_filter->short_name[i].short_name_min_len)) {
			return true;
		}
	}

	return false;
}

static int short_name_filter_add(struct ble_scan *scan,
				 const struct ble_scan_filter_data *data)
{
	uint8_t *counter = &scan->scan_filters.short_name_filter.name_cnt;
	struct ble_scan_short_name_filter *short_name_filter =
		&scan->scan_filters.short_name_filter;
	uint8_t name_len = strlen(data->short_name_filter.short_name);

	/* Check the name length. */
	if ((name_len == 0) ||
	    (name_len > CONFIG_BLE_SCAN_SHORT_NAME_MAX_LEN) ||
	    (name_len < data->short_name_filter.short_name_min_len)) {
		return NRF_ERROR_DATA_SIZE;
	}

	/* Check for duplicated filter. */
	for (uint8_t i = 0; i < CONFIG_BLE_SCAN_SHORT_NAME_COUNT; i++) {
		if (!strcmp(short_name_filter->short_name[i].short_target_name,
			    data->short_name_filter.short_name)) {
			return NRF_SUCCESS;
		}
	}

	/* If no memory for filter. */
	if (*counter >= CONFIG_BLE_SCAN_SHORT_NAME_COUNT) {
		return NRF_ERROR_NO_MEM;
	}

	/* Add name to the filter. */
	short_name_filter->short_name[(*counter)].short_name_min_len =
		data->short_name_filter.short_name_min_len;
	memcpy(short_name_filter->short_name[(*counter)++].short_target_name,
	       data->short_name_filter.short_name, strlen(data->short_name_filter.short_name));

	LOG_DBG("Adding filter on %s name", data->short_name_filter.short_name);

	return NRF_SUCCESS;
}
#endif /* CONFIG_BLE_SCAN_SHORT_NAME_COUNT */

#if (CONFIG_BLE_SCAN_UUID_COUNT > 0)
static bool adv_uuid_compare(const struct ble_scan *scan, uint8_t *data, uint16_t len)
{
	const struct ble_scan_uuid_filter *uuid_filter = &scan->scan_filters.uuid_filter;
	const bool all_filters_mode = scan->scan_filters.all_filters_mode;
	uint8_t uuid_match_cnt = 0;

	for (uint8_t i = 0; i < scan->scan_filters.uuid_filter.uuid_cnt; i++) {

		if (ble_adv_data_uuid_find(data, len, &uuid_filter->uuid[i])) {
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
	if ((all_filters_mode && (uuid_match_cnt == scan->scan_filters.uuid_filter.uuid_cnt)) ||
	    ((!all_filters_mode) && (uuid_match_cnt > 0))) {
		return true;
	}

	return false;
}

static int uuid_filter_add(struct ble_scan *scan, const struct ble_scan_filter_data *data)
{
	const ble_uuid_t *uuid = &data->uuid_filter.uuid;
	ble_uuid_t *uuid_filter = scan->scan_filters.uuid_filter.uuid;
	uint8_t *counter = &scan->scan_filters.uuid_filter.uuid_cnt;

	/* Check for duplicated filter.*/
	for (uint8_t i = 0; i < CONFIG_BLE_SCAN_UUID_COUNT; i++) {
		if (uuid_filter[i].uuid == uuid->uuid) {
			return NRF_SUCCESS;
		}
	}

	/* If no memory. */
	if (*counter >= CONFIG_BLE_SCAN_UUID_COUNT) {
		return NRF_ERROR_NO_MEM;
	}

	/* Add UUID to the filter. */
	uuid_filter[(*counter)++] = *uuid;
	LOG_DBG("Added filter on UUID %#x", uuid->uuid);

	return NRF_SUCCESS;
}
#endif /* CONFIG_BLE_SCAN_UUID_COUNT */

#if (CONFIG_BLE_SCAN_APPEARANCE_COUNT > 0)
static bool adv_appearance_compare(const struct ble_scan *scan, uint8_t *data, uint16_t len)
{
	const struct ble_scan_appearance_filter *appearance_filter =
		&scan->scan_filters.appearance_filter;

	/* Verify if the advertised appearance matches the provided appearance. */
	for (uint8_t i = 0; i < scan->scan_filters.appearance_filter.appearance_cnt; i++) {
		if (ble_adv_data_appearance_find(data, len, &appearance_filter->appearance[i])) {
			return true;
		}
	}
	return false;
}

static int appearance_filter_add(struct ble_scan *scan, const struct ble_scan_filter_data *data)
{
	uint16_t appearance = data->appearance_filter.appearance;
	uint16_t *appearance_filter = scan->scan_filters.appearance_filter.appearance;
	uint8_t *counter = &scan->scan_filters.appearance_filter.appearance_cnt;

	/* Check for duplicated filter. */
	for (uint8_t i = 0; i < CONFIG_BLE_SCAN_APPEARANCE_COUNT; i++) {
		if (appearance_filter[i] == appearance) {
			return NRF_SUCCESS;
		}
	}

	/* If no memory. */
	if (*counter >= CONFIG_BLE_SCAN_APPEARANCE_COUNT) {
		return NRF_ERROR_NO_MEM;
	}

	/* Add appearance to the filter. */
	appearance_filter[(*counter)++] = appearance;
	LOG_DBG("Added filter on appearance %#x", appearance);
	return NRF_SUCCESS;
}
#endif /* CONFIG_BLE_SCAN_APPEARANCE_COUNT */

uint32_t ble_scan_filter_add(struct ble_scan *scan, uint8_t type,
			     const struct ble_scan_filter_data *data)
{
	if (!scan || !data) {
		return NRF_ERROR_NULL;
	}

	switch (type) {
#if (CONFIG_BLE_SCAN_NAME_COUNT > 0)
	case BLE_SCAN_NAME_FILTER: {
		return name_filter_add(scan, data);
	}
#endif

#if (CONFIG_BLE_SCAN_SHORT_NAME_COUNT > 0)
	case BLE_SCAN_SHORT_NAME_FILTER: {
		return short_name_filter_add(scan, data);
	}
#endif

#if (CONFIG_BLE_SCAN_ADDRESS_COUNT > 0)
	case BLE_SCAN_ADDR_FILTER: {
		return addr_filter_add(scan, data);
	}
#endif

#if (CONFIG_BLE_SCAN_UUID_COUNT > 0)
	case BLE_SCAN_UUID_FILTER: {
		return uuid_filter_add(scan, data);
	}
#endif

#if (CONFIG_BLE_SCAN_APPEARANCE_COUNT > 0)
	case BLE_SCAN_APPEARANCE_FILTER: {
		return appearance_filter_add(scan, data);
	}
#endif

	default:
		return NRF_ERROR_INVALID_PARAM;
	}
}

uint32_t ble_scan_all_filter_remove(struct ble_scan *scan)
{
#if (CONFIG_BLE_SCAN_NAME_COUNT > 0)
	struct ble_scan_name_filter *name_filter = &scan->scan_filters.name_filter;

	memset(name_filter->target_name, 0, sizeof(name_filter->target_name));
	name_filter->name_cnt = 0;
#endif

#if (CONFIG_BLE_SCAN_SHORT_NAME_COUNT > 0)
	struct ble_scan_short_name_filter *short_name_filter =
		&scan->scan_filters.short_name_filter;

	memset(short_name_filter->short_name, 0, sizeof(short_name_filter->short_name));
	short_name_filter->name_cnt = 0;
#endif

#if (CONFIG_BLE_SCAN_ADDRESS_COUNT > 0)
	struct ble_scan_addr_filter *addr_filter = &scan->scan_filters.addr_filter;

	memset(addr_filter->target_addr, 0, sizeof(addr_filter->target_addr));
	addr_filter->addr_cnt = 0;
#endif

#if (CONFIG_BLE_SCAN_UUID_COUNT > 0)
	struct ble_scan_uuid_filter *uuid_filter = &scan->scan_filters.uuid_filter;

	memset(uuid_filter->uuid, 0, sizeof(uuid_filter->uuid));
	uuid_filter->uuid_cnt = 0;
#endif

#if (CONFIG_BLE_SCAN_APPEARANCE_COUNT > 0)
	struct ble_scan_appearance_filter *appearance_filter =
		&scan->scan_filters.appearance_filter;

	memset(appearance_filter->appearance, 0, sizeof(appearance_filter->appearance));
	appearance_filter->appearance_cnt = 0;
#endif

	return NRF_SUCCESS;
}

uint32_t ble_scan_filters_enable(struct ble_scan *scan, uint8_t mode, bool match_all)
{
	int nrf_err;
	struct ble_scan_filters *filters;

	if (!scan) {
		return NRF_ERROR_NULL;
	}

	/* Check if the mode is correct. */
	if ((!(mode & BLE_SCAN_ADDR_FILTER)) &&
	    (!(mode & BLE_SCAN_NAME_FILTER)) &&
	    (!(mode & BLE_SCAN_UUID_FILTER)) &&
	    (!(mode & BLE_SCAN_SHORT_NAME_FILTER)) &&
	    (!(mode & BLE_SCAN_APPEARANCE_FILTER))) {
		return NRF_ERROR_INVALID_PARAM;
	}

	/* Disable filters.*/
	nrf_err = ble_scan_filters_disable(scan);
	__ASSERT_NO_MSG(nrf_err == NRF_SUCCESS);

	filters = &scan->scan_filters;

	/* Turn on the filters of your choice. */
#if (CONFIG_BLE_SCAN_ADDRESS_COUNT > 0)
	if (mode & BLE_SCAN_ADDR_FILTER) {
		filters->addr_filter.addr_filter_enabled = true;
	}
#endif

#if (CONFIG_BLE_SCAN_NAME_COUNT > 0)
	if (mode & BLE_SCAN_NAME_FILTER) {
		filters->name_filter.name_filter_enabled = true;
	}
#endif

#if (CONFIG_BLE_SCAN_SHORT_NAME_COUNT > 0)
	if (mode & BLE_SCAN_SHORT_NAME_FILTER) {
		filters->short_name_filter.short_name_filter_enabled = true;
	}
#endif

#if (CONFIG_BLE_SCAN_UUID_COUNT > 0)
	if (mode & BLE_SCAN_UUID_FILTER) {
		filters->uuid_filter.uuid_filter_enabled = true;
	}
#endif

#if (CONFIG_BLE_SCAN_APPEARANCE_COUNT > 0)
	if (mode & BLE_SCAN_APPEARANCE_FILTER) {
		filters->appearance_filter.appearance_filter_enabled = true;
	}
#endif

	/* Select the filter mode. */
	filters->all_filters_mode = match_all;

	return NRF_SUCCESS;
}

uint32_t ble_scan_filters_disable(struct ble_scan *scan)
{
	if (!scan) {
		return NRF_ERROR_NULL;
	}

	/* Disable all filters.*/
#if (CONFIG_BLE_SCAN_NAME_COUNT > 0)
	bool *name_filter_enabled = &scan->scan_filters.name_filter.name_filter_enabled;
	*name_filter_enabled = false;
#endif

#if (CONFIG_BLE_SCAN_ADDRESS_COUNT > 0)
	bool *addr_filter_enabled = &scan->scan_filters.addr_filter.addr_filter_enabled;
	*addr_filter_enabled = false;
#endif

#if (CONFIG_BLE_SCAN_UUID_COUNT > 0)
	bool *uuid_filter_enabled = &scan->scan_filters.uuid_filter.uuid_filter_enabled;
	*uuid_filter_enabled = false;
#endif

#if (CONFIG_BLE_SCAN_APPEARANCE_COUNT > 0)
	bool *appearance_filter_enabled =
		&scan->scan_filters.appearance_filter.appearance_filter_enabled;
	*appearance_filter_enabled = false;
#endif

	return NRF_SUCCESS;
}

uint32_t ble_scan_filter_get(const struct ble_scan *scan, struct ble_scan_filters *status)
{
	if (!scan || !status) {
		return NRF_ERROR_NULL;
	}

	*status = scan->scan_filters;

	return NRF_SUCCESS;
}

#endif /* CONFIG_BLE_SCAN_FILTER */

bool is_allow_list_used(const struct ble_scan *scan)
{
	if (scan->scan_params.filter_policy == BLE_GAP_SCAN_FP_WHITELIST ||
	    scan->scan_params.filter_policy ==
		    BLE_GAP_SCAN_FP_WHITELIST_NOT_RESOLVED_DIRECTED) {
		return true;
	}

	return false;
}

uint32_t ble_scan_init(struct ble_scan *scan, struct ble_scan_config *config)
{
	if (!scan || !config) {
		return NRF_ERROR_NULL;
	}

	scan->evt_handler = config->evt_handler;

#if defined(CONFIG_BLE_SCAN_FILTER)
	/* Disable all scanning filters. */
	memset(&scan->scan_filters, 0, sizeof(scan->scan_filters));
#endif

	scan->connect_if_match = config->connect_if_match;
	scan->conn_cfg_tag = config->conn_cfg_tag;
	scan->scan_params = config->scan_params;
	scan->conn_params = config->conn_params;

	/* Assign a buffer where the advertising reports are to be stored by the SoftDevice. */
	scan->scan_buffer.p_data = scan->scan_buffer_data[0];
	scan->scan_buffer.len = CONFIG_BLE_SCAN_BUFFER_SIZE;

	return NRF_SUCCESS;
}

uint32_t ble_scan_params_set(struct ble_scan *scan, const ble_gap_scan_params_t *scan_params)
{
	if (!scan || !scan_params) {
		return NRF_ERROR_NULL;
	}

	(void)sd_ble_gap_scan_stop();

	/* Assign new scanning parameters. */
	scan->scan_params = *scan_params;

	LOG_DBG("Scanning parameters have been changed successfully");

	return NRF_SUCCESS;
}

uint32_t ble_scan_start(const struct ble_scan *scan)
{
	uint32_t nrf_err;
	struct ble_scan_evt scan_evt = {
		.evt_type = BLE_SCAN_EVT_ALLOW_LIST_REQUEST,
	};

	if (!scan) {
		return NRF_ERROR_NULL;
	}

	(void)sd_ble_gap_scan_stop();

	/* If the allow list is used and the event handler is not NULL,
	 * send the allow list request.
	 */
	if (is_allow_list_used(scan)) {
		if (scan->evt_handler) {
			scan->evt_handler(&scan_evt);
		}
	}

	/* Start the scanning. */
	nrf_err = sd_ble_gap_scan_start(&scan->scan_params, &scan->scan_buffer);

	/* It is okay to ignore NRF_ERROR_INVALID_STATE, because the scan stopped earlier. */
	if (nrf_err && (nrf_err != NRF_ERROR_INVALID_STATE)) {
		LOG_ERR("sd_ble_gap_scan_start returned nrf_error %#x", nrf_err);
		return nrf_err;
	}
	LOG_DBG("Scanning");

	return NRF_SUCCESS;
}

void ble_scan_stop(const struct ble_scan *scan)
{
	ARG_UNUSED(scan);
	/* It is ok to ignore the function return value here, because this function can return
	 * NRF_SUCCESS or NRF_ERROR_INVALID_STATE, when app is not in the scanning state.
	 */
	(void)sd_ble_gap_scan_stop();
}

static void ble_scan_on_adv_report(struct ble_scan *scan,
				   const ble_gap_evt_adv_report_t *adv_report)
{
	static uint8_t *adv_data;
	static uint16_t adv_data_len;
	static int buffer_idx;

	struct ble_scan_evt scan_evt = {
		.scan_params = &scan->scan_params,
	};
	bool active_match_all = scan->scan_params.active && scan->scan_filters.all_filters_mode;

	/* If the allow list is used, do not check the filters and return. */
	if (is_allow_list_used(scan)) {
		scan_evt.evt_type = BLE_SCAN_EVT_ALLOW_LIST_ADV_REPORT;
		scan_evt.params.allow_list_adv_report.report = adv_report;
		scan->evt_handler(&scan_evt);

		if (scan->connect_if_match) {
			(void)sd_ble_gap_scan_stop();
			ble_scan_connect_with_target(scan, adv_report);
		} else {
			(void)sd_ble_gap_scan_start(NULL, &scan->scan_buffer);
		}

		return;
	}

	if (adv_report->type.status == BLE_GAP_ADV_DATA_STATUS_INCOMPLETE_MORE_DATA) {
		LOG_INF("Incomplete, expecting more data!");
	}

	/* If active scanning where we need to match all we need the scan response for the same
	 * address before processing the filters.
	 */
	if (active_match_all && !adv_report->type.scan_response) {
		/* Store what we have as advertising data and continue for the scan response. */
		adv_data = adv_report->data.p_data;
		adv_data_len = adv_report->data.len;
		/* Use other buffer index. */
		buffer_idx = buffer_idx ? 0 : 1;
		scan->scan_buffer.p_data = scan->scan_buffer_data[buffer_idx];
		(void)sd_ble_gap_scan_start(NULL, &scan->scan_buffer);
		return;
	}

#if defined(CONFIG_BLE_SCAN_FILTER)
	uint8_t filter_cnt = 0;
	uint8_t filter_match_cnt = 0;

#if (CONFIG_BLE_SCAN_ADDRESS_COUNT > 0)
	/* Check the address filter. */
	if (scan->scan_filters.addr_filter.addr_filter_enabled) {
		/* Increase number of active filters. */
		filter_cnt++;
		if (adv_addr_compare(adv_report, scan)) {
			/* Number of filters matched. */
			filter_match_cnt++;
			/* Information about the filters matched. */
			scan_evt.params.filter_match.filter_match.address_filter_match = true;
		}
	}
#endif /* CONFIG_BLE_SCAN_ADDRESS_COUNT */

#if (CONFIG_BLE_SCAN_NAME_COUNT > 0)
	/* Check the name filter. */
	if (scan->scan_filters.name_filter.name_filter_enabled) {
		filter_cnt++;
		if (adv_name_compare(scan, adv_report->data.p_data, adv_report->data.len) ||
		    (active_match_all && adv_name_compare(scan, adv_data, adv_data_len))) {
			filter_match_cnt++;

			scan_evt.params.filter_match.filter_match.name_filter_match = true;
		}
	}
#endif /* CONFIG_BLE_SCAN_NAME_COUNT */

#if (CONFIG_BLE_SCAN_SHORT_NAME_COUNT > 0)
	/* Check the short name filter. */
	if (scan->scan_filters.short_name_filter.short_name_filter_enabled) {
		filter_cnt++;
		if (adv_short_name_compare(scan, adv_report->data.p_data, adv_report->data.len) ||
		    (active_match_all && adv_short_name_compare(scan, adv_data, adv_data_len))) {
			filter_match_cnt++;

			scan_evt.params.filter_match.filter_match.short_name_filter_match = true;
		}
	}
#endif /* CONFIG_BLE_SCAN_SHORT_NAME_COUNT */

#if (CONFIG_BLE_SCAN_UUID_COUNT > 0)
	/* Check the UUID filter. */
	if (scan->scan_filters.uuid_filter.uuid_filter_enabled) {
		filter_cnt++;
		if (adv_uuid_compare(scan, adv_report->data.p_data, adv_report->data.len) ||
		    (active_match_all && adv_uuid_compare(scan, adv_data, adv_data_len))) {
			filter_match_cnt++;

			scan_evt.params.filter_match.filter_match.uuid_filter_match = true;
		}
	}
#endif /* CONFIG_BLE_SCAN_UUID_COUNT */

#if (CONFIG_BLE_SCAN_APPEARANCE_COUNT > 0)
	/* Check the appearance filter. */
	if (scan->scan_filters.appearance_filter.appearance_filter_enabled) {
		filter_cnt++;
		if (adv_appearance_compare(scan, adv_report->data.p_data, adv_report->data.len) ||
		    (active_match_all && adv_appearance_compare(scan, adv_data, adv_data_len))) {
			filter_match_cnt++;

			scan_evt.params.filter_match.filter_match.appearance_filter_match = true;
		}
	}
#endif /* CONFIG_BLE_SCAN_APPEARANCE_COUNT */

	/* In the multifilter mode, the number of the active filters must equal the number of the
	 * filters matched to generate the notification.
	 */
	if (scan->scan_filters.all_filters_mode && (filter_match_cnt == filter_cnt)) {
		scan_evt.evt_type = BLE_SCAN_EVT_FILTER_MATCH;
		scan_evt.params.filter_match.adv_report = adv_report;
		if (scan->connect_if_match) {
			(void)sd_ble_gap_scan_stop();
			ble_scan_connect_with_target(scan, adv_report);
		}
	} else if ((!scan->scan_filters.all_filters_mode) && (filter_match_cnt > 0)) {
		/* In the normal filter mode, only one filter match is needed to generate the
		 * notification to the main application.
		 */
		scan_evt.evt_type = BLE_SCAN_EVT_FILTER_MATCH;
		scan_evt.params.filter_match.adv_report = adv_report;
		if (scan->connect_if_match) {
			(void)sd_ble_gap_scan_stop();
			ble_scan_connect_with_target(scan, adv_report);
		}
	} else {
		scan_evt.evt_type = BLE_SCAN_EVT_NOT_FOUND;
		scan_evt.params.not_found.report = adv_report;
	}

	/* If the event handler is not NULL, notify the main application. */
	if (scan->evt_handler) {
		scan->evt_handler(&scan_evt);
	}

#endif /* CONFIG_BLE_SCAN_FILTER */

	/* Resume the scanning. */
	(void)sd_ble_gap_scan_start(NULL, &scan->scan_buffer);
}

static void ble_scan_on_timeout(const struct ble_scan *scan,
				const ble_gap_evt_t *gap)
{
	const ble_gap_evt_timeout_t *timeout = &gap->params.timeout;
	struct ble_scan_evt scan_evt = {
		.evt_type = BLE_SCAN_EVT_SCAN_TIMEOUT,
		.scan_params = &scan->scan_params,
		.params.timeout.src = timeout->src,
	};

	if (timeout->src == BLE_GAP_TIMEOUT_SRC_SCAN) {
		LOG_DBG("BLE_GAP_SCAN_TIMEOUT");
		if (scan->evt_handler) {
			scan->evt_handler(&scan_evt);
		}
	}
}

static void ble_scan_on_connected_evt(const struct ble_scan *scan,
				      const ble_gap_evt_t *gap_evt)
{
	struct ble_scan_evt scan_evt = {
		.evt_type = BLE_SCAN_EVT_CONNECTED,
		.params.connected.connected = &gap_evt->params.connected,
		.params.connected.conn_handle = gap_evt->conn_handle,
		.scan_params = &scan->scan_params,
	};

	if (scan->evt_handler) {
		scan->evt_handler(&scan_evt);
	}
}

void ble_scan_on_ble_evt(const ble_evt_t *ble_evt, void *context)
{
	struct ble_scan *scan = (struct ble_scan *)context;
	const ble_gap_evt_adv_report_t *adv_report = &ble_evt->evt.gap_evt.params.adv_report;
	const ble_gap_evt_t *gap_evt = &ble_evt->evt.gap_evt;

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_ADV_REPORT:
		ble_scan_on_adv_report(scan, adv_report);

		scan->scan_buffer.p_data = scan->scan_buffer_data[0];
		break;

	case BLE_GAP_EVT_TIMEOUT:
		ble_scan_on_timeout(scan, gap_evt);
		break;

	case BLE_GAP_EVT_CONNECTED:
		ble_scan_on_connected_evt(scan, gap_evt);
		break;

	default:
		break;
	}
}
