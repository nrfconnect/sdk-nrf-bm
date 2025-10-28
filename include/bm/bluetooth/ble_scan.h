/*
 * Copyright (c) 2018 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 *
 * @defgroup nrf_ble_scan Scanning Module
 * @{
 * @ingroup ble_sdk_lib
 * @brief Module for handling the BLE scanning.
 *
 * @details The Scanning Module handles the BLE scanning for your application.
 *          The module offers several criteria for filtering the devices available for connection,
 *          and it can also work in the simple mode without using the filtering.
 *          If an event handler is provided, your main application can react to a filter match or to
 * the need of setting the whitelist. The module can also be configured to automatically connect
 * after it matches a filter or a device from the whitelist.
 *
 * @note    The Scanning Module also supports applications with a multicentral link.
 */

#ifndef NRF_BLE_SCAN_H__
#define NRF_BLE_SCAN_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ble.h"
#include "ble_gap.h"

#ifdef __cplusplus
extern "C" {
#endif

// <e> NRF_BLE_SCAN_ENABLED - nrf_ble_scan - Scanning Module
//==========================================================
#ifndef NRF_BLE_SCAN_ENABLED
#define NRF_BLE_SCAN_ENABLED 1
#endif
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

// <o> NRF_BLE_SCAN_MIN_CONNECTION_INTERVAL - Determines minimum connection interval in
// milliseconds.
#ifndef NRF_BLE_SCAN_MIN_CONNECTION_INTERVAL
#define NRF_BLE_SCAN_MIN_CONNECTION_INTERVAL 7.5
#endif

// <o> NRF_BLE_SCAN_MAX_CONNECTION_INTERVAL - Determines maximum connection interval in
// milliseconds.
#ifndef NRF_BLE_SCAN_MAX_CONNECTION_INTERVAL
#define NRF_BLE_SCAN_MAX_CONNECTION_INTERVAL 30
#endif

// <o> NRF_BLE_SCAN_SLAVE_LATENCY - Determines the slave latency in counts of connection events.
#ifndef NRF_BLE_SCAN_SLAVE_LATENCY
#define NRF_BLE_SCAN_SLAVE_LATENCY 0
#endif

// <o> NRF_BLE_SCAN_SUPERVISION_TIMEOUT - Determines the supervision time-out in units of 10
// millisecond.
#ifndef NRF_BLE_SCAN_SUPERVISION_TIMEOUT
#define NRF_BLE_SCAN_SUPERVISION_TIMEOUT 4000
#endif

#ifndef NRF_BLE_SCAN_SCAN_PHY
#define NRF_BLE_SCAN_SCAN_PHY 0
#endif

// <e> NRF_BLE_SCAN_FILTER_ENABLE - Enabling filters for the Scanning Module.
//==========================================================
#ifndef NRF_BLE_SCAN_FILTER_ENABLE
#define NRF_BLE_SCAN_FILTER_ENABLE 1
#endif
// <o> NRF_BLE_SCAN_UUID_CNT - Number of filters for UUIDs.
#ifndef NRF_BLE_SCAN_UUID_CNT
#define NRF_BLE_SCAN_UUID_CNT 0
#endif

// <o> NRF_BLE_SCAN_NAME_CNT - Number of name filters.
#ifndef NRF_BLE_SCAN_NAME_CNT
#define NRF_BLE_SCAN_NAME_CNT 1
#endif

// <o> NRF_BLE_SCAN_SHORT_NAME_CNT - Number of short name filters.
#ifndef NRF_BLE_SCAN_SHORT_NAME_CNT
#define NRF_BLE_SCAN_SHORT_NAME_CNT 0
#endif

// <o> NRF_BLE_SCAN_ADDRESS_CNT - Number of address filters.
#ifndef NRF_BLE_SCAN_ADDRESS_CNT
#define NRF_BLE_SCAN_ADDRESS_CNT 0
#endif

// <o> NRF_BLE_SCAN_APPEARANCE_CNT - Number of appearance filters.
#ifndef NRF_BLE_SCAN_APPEARANCE_CNT
#define NRF_BLE_SCAN_APPEARANCE_CNT 0
#endif

/**
 * @defgroup NRF_BLE_SCAN_FILTER_MODE Filter modes
 * @{ */
#define NRF_BLE_SCAN_NAME_FILTER       (0x01) /** Filters the device name. */
#define NRF_BLE_SCAN_ADDR_FILTER       (0x02) /** Filters the device address. */
#define NRF_BLE_SCAN_UUID_FILTER       (0x04) /** Filters the UUID. */
#define NRF_BLE_SCAN_APPEARANCE_FILTER (0x08) /** Filters the appearance. */
#define NRF_BLE_SCAN_SHORT_NAME_FILTER (0x10) /** Filters the device short name. */
#define NRF_BLE_SCAN_ALL_FILTER	       (0x1F) /** Uses the combination of all filters. */
/* @} */

/**
 * @brief Macro for defining a nrf_ble_scan instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define NRF_BLE_SCAN_DEF(_name)                                                                    \
	static struct nrf_ble_scan _name;                                                          \
	NRF_SDH_BLE_OBSERVER(_name##_ble_obs, nrf_ble_scan_on_ble_evt, &_name,                     \
			     NRF_BLE_SCAN_OBSERVER_PRIO);

/**
 * @brief Enumeration for scanning events.
 *
 * @details These events are propagated to the main application if a handler is provided during
 *          the initialization of the Scanning Module. @ref NRF_BLE_SCAN_EVT_WHITELIST_REQUEST
 * 			cannot be ignored if whitelist is used.
 */
enum nrf_ble_scan_evt {
	/** A filter is matched or all filters are matched in the multifilter mode. */
	NRF_BLE_SCAN_EVT_FILTER_MATCH,
	/** Request the whitelist from the main application.
	 *  For whitelist scanning to work, the whitelist must be
	 *  set when this event occurs.
	 */
	NRF_BLE_SCAN_EVT_WHITELIST_REQUEST,
	/** Send notification to the main application when a
	 *device from the whitelist is found.
	 */
	NRF_BLE_SCAN_EVT_WHITELIST_ADV_REPORT,
	/** The filter was not matched for the scan data. */
	NRF_BLE_SCAN_EVT_NOT_FOUND,
	/** Scan timeout. */
	NRF_BLE_SCAN_EVT_SCAN_TIMEOUT,
	/** Error occurred when establishing the connection. In
	 *this event, an error is passed from the function call
	 *@ref sd_ble_gap_connect.
	 */
	NRF_BLE_SCAN_EVT_CONNECTING_ERROR,
	/** Connected to device. */
	NRF_BLE_SCAN_EVT_CONNECTED,
	NRF_BLE_SCAN_EVT_ERROR,
};

/**
 *@brief Types of filters.
 */
enum nrf_ble_scan_filter_type {
	/** Filter for names. */
	SCAN_NAME_FILTER,
	/** Filter for short names. */
	SCAN_SHORT_NAME_FILTER,
	/** Filter for addresses. */
	SCAN_ADDR_FILTER,
	/** Filter for UUIDs. */
	SCAN_UUID_FILTER,
	/** Filter for appearances. */
	SCAN_APPEARANCE_FILTER,
};

struct nrf_ble_scan_short_name {
	/** Pointer to the short name. */
	char const *p_short_name;
	/** Minimum length of the short name. */
	uint8_t short_name_min_len;
};

/**
 *  @brief Structure for Scanning Module initialization.
 */
struct nrf_ble_scan_init {
	/** BLE GAP scan parameters required to initialize the module. Can
	 *  be initialized as NULL. If NULL, the parameters required to
	 *  initialize the module are loaded from the static configuration.
	 */
	ble_gap_scan_params_t const *p_scan_param;
	/** If set to true, the module automatically connects after a filter
	 *  match or successful identification of a device from the whitelist.
	 */
	bool connect_if_match;
	/** Connection parameters. Can be initialized as NULL. If NULL, the
	 *  default static configuration is used.
	 */
	ble_gap_conn_params_t const *p_conn_param;
	/** Variable to keep track of what connection settings will be used
	 *  if a filer match or a whitelist match results in a connection.
	 */
	uint8_t conn_cfg_tag;
};

/**
 * @brief Structure for setting the filter status.
 *
 * @details This structure is used for sending filter status to the main application.
 */
struct nrf_ble_scan_filter_match {
	/** Set to 1 if name filter is matched. */
	uint8_t name_filter_match: 1;
	/** Set to 1 if address filter is matched. */
	uint8_t address_filter_match: 1;
	/** Set to 1 if uuid filter is matched. */
	uint8_t uuid_filter_match: 1;
	/** Set to 1 if appearance filter is matched. */
	uint8_t appearance_filter_match: 1;
	/** Set to 1 if short name filter is matched. */
	uint8_t short_name_filter_match: 1;
};

/**
 * @brief Event structure for @ref NRF_BLE_SCAN_EVT_FILTER_MATCH.
 */
struct nrf_ble_scan_evt_filter_match {
	/** Event structure for @ref BLE_GAP_EVT_ADV_REPORT. This data
	 *  allows the main application to establish connection.
	 */
	ble_gap_evt_adv_report_t const *p_adv_report;
	/** Matching filters. Information about matched filters. */
	struct nrf_ble_scan_filter_match filter_match;
};

/**
 * @brief Event structure for @ref NRF_BLE_SCAN_EVT_CONNECTING_ERROR.
 */
struct nrf_ble_scan_evt_connecting_err {
	/** Indicates success or failure of an API procedure. In case of
	 *  failure, a comprehensive error code indicating the cause or reason
	 *  for failure is provided.
	 */
	int err_code;
};

/**
 * @brief Event structure for @ref NRF_BLE_SCAN_EVT_CONNECTED.
 */
struct nrf_ble_scan_evt_connected {
	/** Connected event parameters. */
	ble_gap_evt_connected_t const *p_connected;
	/** Connection handle of the device on which the event occurred. */
	uint16_t conn_handle;
};

/**
 * @brief Structure for Scanning Module event data.
 *
 * @details This structure is used to send module event data to the main application when an event
 * occurs.
 */
struct scan_evt {
	/** Type of event propagated to the main application. */
	enum nrf_ble_scan_evt scan_evt_id;
	union {
		/** Scan filter match. */
		struct nrf_ble_scan_evt_filter_match filter_match;
		/** Timeout event parameters. */
		ble_gap_evt_timeout_t timeout;
		/** Advertising report event parameters for whitelist. */
		ble_gap_evt_adv_report_t const *p_whitelist_adv_report;
		/** Advertising report event parameters	when filter is not found. */
		ble_gap_evt_adv_report_t const *p_not_found;
		/** Connected event parameters. */
		struct nrf_ble_scan_evt_connected connected;
		/** Error event when connecting. Propagates the error code returned by the
		   SoftDevice API @ref sd_ble_gap_scan_start. */
		struct nrf_ble_scan_evt_connecting_err connecting_err;
		struct {
			uint32_t reason;
		} error;
	} params;
	/** GAP scanning parameters. These parameters
	 *  are needed to establish connection.
	 */
	ble_gap_scan_params_t const *p_scan_params;
};

/**
 * @brief BLE scanning event handler type.
 */
typedef void (*nrf_ble_scan_evt_handler_t)(struct scan_evt const *p_scan_evt);

#if CONFIG_BT_SCAN_FILTER_ENABLE || 1

#if (NRF_BLE_SCAN_NAME_CNT > 0)
struct nrf_ble_scan_name_filter {
	/** Names that the main application will scan
	 *  for, and that will be advertised by the
	 *  peripherals.
	 */
	char target_name[NRF_BLE_SCAN_NAME_CNT][NRF_BLE_SCAN_NAME_MAX_LEN];
	/** Name filter counter. */
	uint8_t name_cnt;
	/** Flag to inform about enabling or disabling this filter. */
	bool name_filter_enabled;
};
#endif

#if (NRF_BLE_SCAN_SHORT_NAME_CNT > 0)
typedef struct {
	struct {
		/** Short names that the
		 *  main application will
		 *  scan for, and that will
		 *  be advertised by the
		 *  peripherals.
		 */
		char short_target_name[NRF_BLE_SCAN_SHORT_NAME_MAX_LEN];
		/** Minimum length of the short name. */
		uint8_t short_name_min_len;
	} short_name[NRF_BLE_SCAN_SHORT_NAME_CNT];
	/** Short name filter counter. */
	uint8_t name_cnt;
	/** Flag to inform about enabling or disabling this filter. */
	bool short_name_filter_enabled;
} nrf_ble_scan_short_name_filter_t;
#endif

#if (NRF_BLE_SCAN_ADDRESS_CNT > 0)
struct nrf_ble_scan_addr_filter {
	struct ble_gap_addr
		/** Addresses in the same format as the
		format used by the SoftDevice that the
		main application will scan for, and that
		will be advertised by the peripherals. */
		target_addr[NRF_BLE_SCAN_ADDRESS_CNT];
	/** Address filter counter. */
	uint8_t addr_cnt;
	/** Flag to inform about enabling or disabling this filter. */
	bool addr_filter_enabled;
};
#endif

#if (NRF_BLE_SCAN_UUID_CNT > 0)
struct nrf_ble_scan_uuid_filter {
	struct ble_uuid
		/** UUIDs that the main application will scan for, and
		that will be advertised by the peripherals. */
		uuid[NRF_BLE_SCAN_UUID_CNT];
	/** UUID filter counter. */
	uint8_t uuid_cnt;
	/** Flag to inform about enabling or disabling this filter. */
	bool uuid_filter_enabled;
};
#endif

#if (NRF_BLE_SCAN_APPEARANCE_CNT > 0)
typedef struct {
	/** Apperances that the main application
	will scan for, and that will be
	advertised by the peripherals. */
	uint16_t appearance[NRF_BLE_SCAN_APPEARANCE_CNT];
	/** Appearance filter counter. */
	uint8_t appearance_cnt;
	/** Flag to inform about enabling or disabling this filter.*/
	bool appearance_filter_enabled;
} nrf_ble_scan_appearance_filter_t;
#endif

/**
 * @brief Filters data.
 *
 * @details This structure contains all filter data and the information about enabling and disabling
 * any type of filters. Flag all_filter_mode informs about the filter mode. If this flag is set,
 * then all types of enabled filters must be matched for the module to send a notification to the
 * main application. Otherwise, it is enough to match one of filters to send notification.
 */
struct nrf_ble_scan_filters {
#if (NRF_BLE_SCAN_NAME_CNT > 0)
	/** Name filter data. */
	struct nrf_ble_scan_name_filter name_filter;
#endif
#if (NRF_BLE_SCAN_SHORT_NAME_CNT > 0)
	/** Short name filter data. */
	struct nrf_ble_scan_short_name_filter short_name_filter;
#endif
#if (NRF_BLE_SCAN_ADDRESS_CNT > 0)
	/** Address filter data. */
	struct nrf_ble_scan_addr_filter addr_filter;
#endif
#if (NRF_BLE_SCAN_UUID_CNT > 0)
	/** UUID filter data. */
	struct nrf_ble_scan_uuid_filter uuid_filter;
#endif
#if (NRF_BLE_SCAN_APPEARANCE_CNT > 0)
	/** Appearance filter data. */
	nrf_ble_scan_appearance_filter_t appearance_filter;
#endif
	/** Filter mode. If true, all set filters must be matched to
	generate an event.*/
	bool all_filters_mode;
};

#endif // NRF_BLE_SCAN_FILTER_ENABLE

#ifndef NRF_BLE_SCAN_BUFFER
#define NRF_BLE_SCAN_BUFFER 31
#endif

/**
 * @brief Scan module instance. Options for the different scanning modes.
 *
 * @details This structure stores all module settings. It is used to enable or disable scanning
 * modes and to configure filters.
 */
struct nrf_ble_scan {
#if CONFIG_BT_SCAN_FILTER_ENABLE || 1
	/** Filter data. */
	struct nrf_ble_scan_filters scan_filters;
#endif
	/** If set to true, the module automatically connects after a filter
	match or successful identification of a device from the whitelist.*/
	bool connect_if_match;
	/** Connection parameters. */
	ble_gap_conn_params_t conn_params;
	/** Variable to keep track of what connection settings will be used
	if a filer match or a whitelist match results in a connection. */
	uint8_t conn_cfg_tag;
	/** GAP scanning parameters. */
	ble_gap_scan_params_t scan_params;
	/** Handler for the scanning events. Can be initialized as NULL if no
	handling is implemented in the main application. */
	nrf_ble_scan_evt_handler_t evt_handler;
	/** Buffer where advertising reports will be
	stored by the SoftDevice. */
	uint8_t scan_buffer_data[NRF_BLE_SCAN_BUFFER];
	/** Structure-stored pointer to the buffer where advertising
	reports will be stored by the SoftDevice. */
	ble_data_t scan_buffer;
};

/**
 * @brief Function for indicating that the Scanning Module is using the whitelist.
 *
 * @param[in] p_scan_ctx Pointer to the Scanning Module instance.
 *
 * @return Whether the whitelist is used.
 */
bool is_whitelist_used(struct nrf_ble_scan const *const p_scan_ctx);

/**
 * @brief Function for initializing the Scanning Module.
 *
 * @param[out] p_scan_ctx   Pointer to the Scanning Module instance. This structure must be supplied
 * by the application. It is initialized by this function and is later used to identify this
 * particular module instance.
 *
 * @param[in]  p_init       Can be initialized as NULL. If NULL, the parameters required to
 * initialize the module are loaded from static configuration. If module is to establish the
 * connection automatically, this must be initialized with the relevant data.
 * @param[in]  evt_handler  Handler for the scanning events.
 *                          Can be initialized as NULL if no handling is implemented in the main
 * application.
 *
 * @retval NRF_SUCCESS      If initialization was successful.
 * @retval NRF_ERROR_NULL   When the NULL pointer is passed as input.
 */
int nrf_ble_scan_init(struct nrf_ble_scan *const p_scan_ctx,
		      struct nrf_ble_scan_init const *const p_init,
		      nrf_ble_scan_evt_handler_t evt_handler);

/**
 * @brief Function for starting scanning.
 *
 * @details This function starts the scanning according to the configuration set during the
 * initialization.
 *
 * @param[in] p_scan_ctx       Pointer to the Scanning Module instance.
 *
 * @retval    NRF_SUCCESS      If scanning started. Otherwise, an error code is returned.
 * @retval    NRF_ERROR_NULL   If NULL pointer is passed as input.
 *
 * @return                     This API propagates the error code returned by the
 *                             SoftDevice API @ref sd_ble_gap_scan_start.
 */
int nrf_ble_scan_start(struct nrf_ble_scan const *const p_scan_ctx);

/**
 * @brief Function for stopping scanning.
 */
void nrf_ble_scan_stop(void);

#if CONFIG_BT_SCAN_FILTER_ENABLE || 1

/**
 * @brief Function for enabling filtering.
 *
 * @details The filters can be combined with each other. For example, you can enable one filter or
 * several filters. For example, (NRF_BLE_SCAN_NAME_FILTER | NRF_BLE_SCAN_UUID_FILTER) enables UUID
 * and name filters.
 *
 * @param[in] mode                  Filter mode: @ref NRF_BLE_SCAN_FILTER_MODE.
 * @param[in] match_all             If this flag is set, all types of enabled filters must be
 * matched before generating @ref NRF_BLE_SCAN_EVT_FILTER_MATCH to the main application. Otherwise,
 * it is enough to match one filter to trigger the filter match event.
 * @param[in] p_scan_ctx            Pointer to the Scanning Module instance.
 *
 * @retval NRF_SUCCESS              If the filters are enabled successfully.
 * @retval NRF_ERROR_INVALID_PARAM  If the filter mode is incorrect. Available filter modes: @ref
 * NRF_BLE_SCAN_FILTER_MODE.
 * @retval NRF_ERROR_NULL           If a NULL pointer is passed as input.
 */
int nrf_ble_scan_filters_enable(struct nrf_ble_scan *const p_scan_ctx, uint8_t mode,
				bool match_all);

/**
 * @brief Function for disabling filtering.
 *
 * @details This function disables all filters.
 *          Even if the automatic connection establishing is enabled,
 *          the connection will not be established with the first
	    device found after this function is called.
 *
 * @param[in] p_scan_ctx            Pointer to the Scanning Module instance.
 *
 * @retval NRF_SUCCESS              If filters are disabled successfully.
 * @retval NRF_ERROR_NULL           If a NULL pointer is passed as input.
 */
int nrf_ble_scan_filters_disable(struct nrf_ble_scan *const p_scan_ctx);

/**
 * @brief Function for getting filter status.
 *
 * @details This function returns the filter setting and whether it is enabled or disabled.

 * @param[out] p_status              Filter status.
 * @param[in]  p_scan_ctx            Pointer to the Scanning Module instance.
 *
 * @retval NRF_SUCCESS               If filter status is returned.
 * @retval NRF_ERROR_NULL            If a NULL pointer is passed as input.
 */
int nrf_ble_scan_filter_get(struct nrf_ble_scan *const p_scan_ctx,
			    struct nrf_ble_scan_filters *p_status);

/**
 * @brief Function for adding any type of filter to the scanning.
 *
 * @details This function adds a new filter by type @ref nrf_ble_scan_filter_type_t.
 *          The filter will be added if the number of filters of a given type does not exceed @ref
 * NRF_BLE_SCAN_UUID_CNT,
 *          @ref NRF_BLE_SCAN_NAME_CNT, @ref NRF_BLE_SCAN_ADDRESS_CNT, or @ref
 * NRF_BLE_SCAN_APPEARANCE_CNT, depending on the filter type, and if the same filter has not already
 * been set.
 *
 * @param[in,out] p_scan_ctx        Pointer to the Scanning Module instance.
 * @param[in]     type              Filter type.
 * @param[in]     p_data            The filter data to add.
 *
 * @retval NRF_SUCCESS                    If the filter is added successfully.
 * @retval NRF_ERROR_NULL                 If a NULL pointer is passed as input.
 * @retval NRF_ERROR_DATA_SIZE            If the name filter length is too long. Maximum name filter
 * length corresponds to @ref NRF_BLE_SCAN_NAME_MAX_LEN.
 * @retval NRF_ERROR_NO_MEMORY            If the number of available filters is exceeded.
 * @retval NRF_ERROR_INVALID_PARAM        If the filter type is incorrect. Available filter types:
 * @ref nrf_ble_scan_filter_type_t.
 * @retval BLE_ERROR_GAP_INVALID_BLE_ADDR If the BLE address type is invalid.
 */
int nrf_ble_scan_filter_set(struct nrf_ble_scan *const p_scan_ctx,
			    enum nrf_ble_scan_filter_type type, void const *p_data);

/**
 * @brief Function for removing all set filters.
 *
 * @details The function removes all previously set filters.
 *
 * @note After using this function the filters are still enabled.
 *
 * @param[in,out] p_scan_ctx Pointer to the Scanning Module instance.
 *
 * @retval NRF_SUCCESS       If all filters are removed successfully.
 */
int nrf_ble_scan_all_filter_remove(struct nrf_ble_scan *const p_scan_ctx);

#endif // CONFIG_BT_SCAN_FILTER_ENABLE

/**
 * @brief Function for changing the scanning parameters.
 *
 **@details Use this function to change scanning parameters. During the parameter change
 *         the scan is stopped. To resume scanning, use @ref nrf_ble_scan_start.
 *         Scanning parameters can be set to NULL. If so, the default static configuration
 *         is used. For example, use this function when the @ref
 *NRF_BLE_SCAN_EVT_WHITELIST_ADV_REPORT event is generated. The generation of this event means that
 *there is a risk that the whitelist is empty. In such case, this function can change the scanning
 *parameters, so that the whitelist is not used, and you avoid the error caused by scanning with the
 *whitelist when there are no devices on the whitelist.
 *
 * @param[in,out] p_scan_ctx     Pointer to the Scanning Module instance.
 * @param[in]     p_scan_param   GAP scanning parameters. Can be initialized as NULL.
 *
 * @retval NRF_SUCCESS          If parameters are changed successfully.
 * @retval NRF_ERROR_NULL       If a NULL pointer is passed as input.
 */
int nrf_ble_scan_params_set(struct nrf_ble_scan *const p_scan_ctx,
			    ble_gap_scan_params_t const *p_scan_param);

/**
 * @brief Function for handling the BLE stack events of the application.
 *
 * @param[in]     p_ble_evt     Pointer to the BLE event received.
 * @param[in,out] p_scan        Pointer to the Scanning Module instance.
 */
void nrf_ble_scan_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_scan);

/**
 * @brief Function for converting the raw address to the SoftDevice GAP address.
 *
 * @details This function inverts the byte order in the address. If you enter the address as it is
 * displayed (for example, on a phone screen from left to right), you must use this function to
 *          convert the address to the SoftDevice address type.
 *
 * @note This function does not decode an address type.
 *
 * @param[out] p_gap_addr The Bluetooth Low Energy address.
 * @param[in]  addr       Address to be converted to the SoftDevice address.
 *
 * @retval NRF_ERROR_NULL                 If a NULL pointer is passed as input.
 * @retval NRF_SUCCESS                    If the address is copied and converted successfully.
 */
int nrf_ble_scan_copy_addr_to_sd_gap_addr(ble_gap_addr_t *p_gap_addr,
					  uint8_t const addr[BLE_GAP_ADDR_LEN]);

#ifdef __cplusplus
}
#endif

#endif // NRF_BLE_SCAN_H__

/** @} */