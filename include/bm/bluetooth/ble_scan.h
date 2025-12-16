/*
 * Copyright (c) 2018 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 *
 * @defgroup ble_scan Scan Library
 * @{
 * @brief Library for handling the BLE scanning.
 *
 * @details The Scan Library handles the BLE scanning for your application.
 *          The library offers several criteria for filtering the devices available for connection,
 *          and it can also work in the simple mode without using the filtering.
 *          If an event handler is provided, your main application can react to a filter match or to
 *          the need of setting the allow list. The library can also be configured to automatically
 *          connect after it matches a filter or a device from the allow list.
 *
 * @note    The Scan library also supports applications with a multicentral link.
 */

#ifndef BLE_SCAN_H__
#define BLE_SCAN_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ble.h>
#include <ble_gap.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macro for defining a ble_scan instance.
 *
 * @param _name Name of the instance.
 * @hideinitializer
 */
#define BLE_SCAN_DEF(_name)                                                                        \
	static struct ble_scan _name;                                                              \
	extern void ble_scan_on_ble_evt(const ble_evt_t *ble_evt, void *ctx);                      \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_scan_on_ble_evt, &_name, HIGH)

/**
 * @brief Scan events.
 *
 * @details These events are propagated to the main application if a handler is provided during the
 * initialization of the Scan library.
 *
 * @note @ref BLE_SCAN_EVT_ALLOW_LIST_REQUEST cannot be ignored if allow list is used.
 */
enum ble_scan_evt_type {
	/**
	 * @brief A filter is matched or all filters are matched in the multifilter mode.
	 */
	BLE_SCAN_EVT_FILTER_MATCH,
	/**
	 * @brief Request the allow list from the main application.
	 *
	 * For allow list scanning to work, the allow list must be set when this event occurs.
	 */
	BLE_SCAN_EVT_ALLOW_LIST_REQUEST,
	/**
	 * @brief Send notification to the main application when a device from the allow list is
	 * found.
	 */
	BLE_SCAN_EVT_ALLOW_LIST_ADV_REPORT,
	/**
	 * @brief The filter was not matched for the scan data.
	 */
	BLE_SCAN_EVT_NOT_FOUND,
	/**
	 * @brief Scan timeout.
	 */
	BLE_SCAN_EVT_SCAN_TIMEOUT,
	/**
	 * @brief Error occurred when establishing the connection.
	 *
	 * In this event, an error is passed from the function call @ref sd_ble_gap_connect.
	 */
	BLE_SCAN_EVT_CONNECTING_ERROR,
	/**
	 * @brief Connected to device.
	 */
	BLE_SCAN_EVT_CONNECTED,
	/**
	 * @brief Error.
	 */
	BLE_SCAN_EVT_ERROR,
};

/**
 * @defgroup ble_scan_filter_type Filter types
 * @{
 */

/** Filters the device name. */
 #define BLE_SCAN_NAME_FILTER (0x01)
/** Filters the device address. */
#define BLE_SCAN_ADDR_FILTER (0x02)
/** Filters the UUID. */
#define BLE_SCAN_UUID_FILTER (0x04)
/** Filters the appearance. */
#define BLE_SCAN_APPEARANCE_FILTER (0x08)
/** Filters the device short name. */
#define BLE_SCAN_SHORT_NAME_FILTER (0x10)
/** @} */

/**
 * @brief Scan short name.
 */
struct ble_scan_short_name {
	/**
	 * @brief Pointer to the short name.
	 */
	const char *short_name;
	/**
	 * @brief Minimum length of the short name.
	 */
	uint8_t short_name_min_len;
};

/**
 * @brief Filter status.
 */
struct ble_scan_filter_match {
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
 * @brief Scan library event.
 *
 * @details This is used to send library event data to the main application when an event occurs.
 */
struct ble_scan_evt {
	/** Type of event. */
	enum ble_scan_evt_type evt_type;
	/** GAP scanning parameters. These parameter are needed to establish connection. */
	const ble_gap_scan_params_t *scan_params;
	union {
		/** Scan filter match. */
		struct {
			/** Event structure for @ref BLE_GAP_EVT_ADV_REPORT. This data
			 * allows the main application to establish connection.
			 */
			const ble_gap_evt_adv_report_t *adv_report;
			/** Matching filters. Information about matched filters. */
			struct ble_scan_filter_match filter_match;
		} filter_match;
		/** Timeout event parameters. */
		ble_gap_evt_timeout_t timeout;
		/** Advertising report event parameters for allow list. */
		struct {
			/** Advertising report */
			const ble_gap_evt_adv_report_t *report;
		} allow_list_adv_report;
		/** Advertising report event parameters	when filter is not found. */
		struct {
			/** Advertising report */
			const ble_gap_evt_adv_report_t *report;
		} not_found;
		/** Connected event parameters. */
		struct {
			/** Connected event parameters. */
			const ble_gap_evt_connected_t *connected;
			/** Connection handle of the device on which the event occurred. */
			uint16_t conn_handle;
		} connected;
		/** Error event when connecting. Propagates the error code returned by
		 *  @ref sd_ble_gap_scan_start.
		 */
		struct {
			/** Indicates success or failure of an API procedure. In case of
			 *  failure, a comprehensive error code indicating the cause or reason
			 *  for failure is provided.
			 */
			int reason;
		} connecting_err;
		/** Error event. */
		struct {
			/** Error reason */
			uint32_t reason;
		} error;
	} params;
};

/**
 * @brief BLE Scan event handler type.
 */
typedef void (*ble_scan_evt_handler_t)(const struct ble_scan_evt *scan_evt);

#if defined(CONFIG_BLE_SCAN_FILTER)

#if CONFIG_BLE_SCAN_NAME_COUNT > 0
/** Scan name filter */
struct ble_scan_name_filter {
	/** Names that the main application will scan for,
	 *  and that will be advertised by the peripherals.
	 */
	char target_name[CONFIG_BLE_SCAN_NAME_COUNT][CONFIG_BLE_SCAN_NAME_MAX_LEN];
	/** Number of target names. */
	uint8_t name_cnt;
	/** Flag to inform about enabling or disabling this filter. */
	bool name_filter_enabled;
};
#endif

#if CONFIG_BLE_SCAN_SHORT_NAME_COUNT > 0
/** Scan short name filter */
struct ble_scan_short_name_filter {
	struct {
		/** Short names that the main application will scan for, and that will be
		 *  advertised by the peripherals.
		 */
		char short_target_name[CONFIG_BLE_SCAN_SHORT_NAME_MAX_LEN];
		/** Minimum length of the short name. */
		uint8_t short_name_min_len;
	} short_name[CONFIG_BLE_SCAN_SHORT_NAME_COUNT];
	/** Number of short target names. */
	uint8_t name_cnt;
	/** Flag to inform about enabling or disabling this filter. */
	bool short_name_filter_enabled;
};
#endif

#if CONFIG_BLE_SCAN_ADDRESS_COUNT > 0
/** Scan address filter */
struct ble_scan_addr_filter {
	/** Addresses in the same format as the format used by the SoftDevice that the
	 *  main application will scan for, and that will be advertised by the peripherals.
	 */
	ble_gap_addr_t target_addr[CONFIG_BLE_SCAN_ADDRESS_COUNT];
	/** Number of target addresses. */
	uint8_t addr_cnt;
	/** Flag to inform about enabling or disabling this filter. */
	bool addr_filter_enabled;
};
#endif

#if CONFIG_BLE_SCAN_UUID_COUNT > 0
/** Scan UUID filter */
struct ble_scan_uuid_filter {
	/** UUIDs that the main application will scan for, and that will be advertised by
	 *  the peripherals.
	 */
	ble_uuid_t uuid[CONFIG_BLE_SCAN_UUID_COUNT];
	/** Number of target UUIDs in list. */
	uint8_t uuid_cnt;
	/** Flag to inform about enabling or disabling this filter. */
	bool uuid_filter_enabled;
};
#endif

#if CONFIG_BLE_SCAN_APPEARANCE_COUNT > 0
/** Scan appearance filter */
struct ble_scan_appearance_filter {
	/** Apperances that the main application will scan for, and that will be advertised by the
	 *  peripherals.
	 */
	uint16_t appearance[CONFIG_BLE_SCAN_APPEARANCE_COUNT];
	/** Number of appearances in list. */
	uint8_t appearance_cnt;
	/** Flag to inform about enabling or disabling this filter.*/
	bool appearance_filter_enabled;
};
#endif

/**
 * @brief Filter data.
 *
 * @details This contains all filter data and the information about enabling and disabling
 * any type of filters. Flag all_filter_mode informs about the filter mode. If this flag is set,
 * then all types of enabled filters must be matched for the library to send a notification to the
 * main application. Otherwise, it is enough to match one of the filters to send a notification.
 */
struct ble_scan_filters {
#if CONFIG_BLE_SCAN_NAME_COUNT > 0
	/** Name filter data. */
	struct ble_scan_name_filter name_filter;
#endif
#if CONFIG_BLE_SCAN_SHORT_NAME_COUNT > 0
	/** Short name filter data. */
	struct ble_scan_short_name_filter short_name_filter;
#endif
#if CONFIG_BLE_SCAN_ADDRESS_COUNT > 0
	/** Address filter data. */
	struct ble_scan_addr_filter addr_filter;
#endif
#if CONFIG_BLE_SCAN_UUID_COUNT > 0
	/** UUID filter data. */
	struct ble_scan_uuid_filter uuid_filter;
#endif
#if CONFIG_BLE_SCAN_APPEARANCE_COUNT > 0
	/** Appearance filter data. */
	struct ble_scan_appearance_filter appearance_filter;
#endif
	/** Filter mode. If true, all set filters must be matched to generate an event. */
	bool all_filters_mode;
};

#endif /* CONFIG_BLE_SCAN_FILTER */

#define BLE_SCAN_SCAN_PARAMS_DEFAULT                                                               \
{                                                                                                  \
	.active = 1,                                                                               \
	.interval = CONFIG_BLE_SCAN_INTERVAL,                                                      \
	.window = CONFIG_BLE_SCAN_WINDOW,                                                          \
	.timeout = CONFIG_BLE_SCAN_DURATION,                                                       \
	.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,                                               \
	.scan_phys = BLE_GAP_PHY_1MBPS,                                                            \
}

#define BLE_SCAN_CONN_PARAMS_DEFAULT                                                               \
{                                                                                                  \
	.conn_sup_timeout = BLE_GAP_CP_CONN_SUP_TIMEOUT_MIN,                                       \
	.min_conn_interval = CONFIG_BLE_SCAN_MIN_CONNECTION_INTERVAL,                              \
	.max_conn_interval = CONFIG_BLE_SCAN_MAX_CONNECTION_INTERVAL,                              \
	.slave_latency = (uint16_t)CONFIG_BLE_SCAN_PERIPHERAL_LATENCY,                             \
}

/**
 * @brief Scan instance configuration.
 */
struct ble_scan_config {
	/*  BLE GAP scan parameters required to initialize the module. */
	ble_gap_scan_params_t scan_params;
	/*  If set to true, the module automatically connects after a filter
	 *  match or successful identification of a device from the allow list.
	 */
	bool connect_if_match;
	/*  Connection parameters. */
	ble_gap_conn_params_t conn_params;
	/*  Variable to keep track of what connection settings will be used
	 *  if a filer match or an allow list match results in a connection.
	 */
	uint8_t conn_cfg_tag;
	/** Handler for the scanning events. */
	ble_scan_evt_handler_t evt_handler;
};

/**
 * @brief Scan library instance with options for the different scanning modes.
 *
 * @details This structure stores all library settings. It is used to enable or disable scanning
 * modes and to configure filters.
 */
struct ble_scan {
#if defined(CONFIG_BLE_SCAN_FILTER)
	/** Filter data. */
	struct ble_scan_filters scan_filters;
#endif
	/** If set to true, the library automatically connects after a filter
	 *  match or successful identification of a device from the allow list.
	 */
	bool connect_if_match;
	/** Connection parameters. */
	ble_gap_conn_params_t conn_params;
	/** Variable to keep track of what connection settings will be used
	 *  if a filer match or a allow list match results in a connection.
	 */
	uint8_t conn_cfg_tag;
	/** GAP scanning parameters. */
	ble_gap_scan_params_t scan_params;
	/** Handler for the scanning events. */
	ble_scan_evt_handler_t evt_handler;
	/** Buffer where advertising reports will be stored by the SoftDevice. */
	uint8_t scan_buffer_data[CONFIG_BLE_SCAN_BUFFER_SIZE];
	/** Structure-stored pointer to the buffer where advertising
	 *  reports will be stored by the SoftDevice.
	 */
	ble_data_t scan_buffer;
};

/**
 * @brief Check if the allow list is used.
 *
 * @param[in] scan_ctx Scan library instance.
 *
 * @return true if allow list is used.
 */
bool is_allow_list_used(const struct ble_scan *const scan_ctx);

/**
 * @brief Initialize the Scan library.
 *
 * @param[out] scan Scan library instance. This structure must be supplied by the application.
 *                  It is initialized by this function and is later used to identify this
 *                  particular library instance.
 *
 * @param[in] config Configuration parameters.
 *
 * @retval NRF_SUCCESS If initialization was successful.
 * @retval NRF_ERROR_NULL When a NULL pointer is passed as input.
 */
int ble_scan_init(struct ble_scan *scan, struct ble_scan_config *config);

/**
 * @brief Start scanning.
 *
 * @details This function starts the scanning according to the configuration set during the
 * initialization.
 *
 * @param[in] scan_ctx Scan library instance.
 *
 * @retval NRF_SUCCESS If scanning started. Otherwise, an error code is returned.
 * @retval NRF_ERROR_NULL If NULL pointer is passed as input.
 *
 * @return This API propagates the error code returned by the
 *         SoftDevice API @ref sd_ble_gap_scan_start.
 */
int ble_scan_start(const struct ble_scan *const scan_ctx);

/**
 * @brief Stop scanning.
 *
 * @param[in] scan_ctx Scan library instance.
 */
void ble_scan_stop(const struct ble_scan *const scan_ctx);

#if defined(CONFIG_BLE_SCAN_FILTER)

/**
 * @brief Enable filtering.
 *
 * @details The filters can be combined with each other. For example, you can enable one filter or
 *          several filters. For example, (BLE_SCAN_NAME_FILTER | BLE_SCAN_UUID_FILTER)
 *          enables UUID and name filters.
 *
 * @param[in] mode Filter mode: @c ble_scan_filter_type.
 * @param[in] match_all If this flag is set, all types of enabled filters must be
 *                      matched before generating @ref BLE_SCAN_EVT_FILTER_MATCH to the main
 *                      application. Otherwise, it is enough to match one filter to trigger the
 *                      filter match event.
 * @param[in] scan_ctx Scan library instance.
 *
 * @retval NRF_SUCCESS If the filters are enabled successfully.
 * @retval NRF_ERROR_INVALID_PARAM  If the filter mode is invalid.
 * @retval NRF_ERROR_NULL If a NULL pointer is passed as input.
 */
int ble_scan_filters_enable(struct ble_scan *const scan_ctx, uint8_t mode, bool match_all);

/**
 * @brief Disable filtering.
 *
 * @details Disable all filters.
 *          Even if the automatic connection establishing is enabled,
 *          the connection will not be established with the first device found after
 *          this function is called.
 *
 * @param[in] scan_ctx Scan library instance.
 *
 * @retval NRF_SUCCESS If filters are disabled successfully.
 * @retval NRF_ERROR_NULL If a NULL pointer is passed as input.
 */
int ble_scan_filters_disable(struct ble_scan *const scan_ctx);

/**
 * @brief Get filter status.
 *
 * @details This function returns the filter setting and whether it is enabled or disabled.

 * @param[out] status Filter status.
 * @param[in]  scan_ctx Scan library instance.
 *
 * @retval NRF_SUCCESS If filter status is returned.
 * @retval NRF_ERROR_NULL If a NULL pointer is passed as input.
 */
int ble_scan_filter_get(struct ble_scan *const scan_ctx, struct ble_scan_filters *status);

/**
 * @brief Add scan filter.
 *
 * @details This function adds a new filter by type @ref ble_scan_filter_type_t.
 *          The filter will be added if the number of filters of a given type does not exceed @ref
 *          CONFIG_BLE_SCAN_UUID_COUNT, @ref CONFIG_BLE_SCAN_NAME_COUNT, @ref
 *          CONFIG_BLE_SCAN_ADDRESS_COUNT, or @ref CONFIG_BLE_SCAN_APPEARANCE_COUNT, depending on
 *          the filter type, and if the same filter has not already been set.
 *
 * @param[in,out] scan_ctx Scan library instance.
 * @param[in] type Filter type.
 * @param[in] data The filter data to add.
 *
 * @retval NRF_SUCCESS If the filter is added successfully.
 * @retval NRF_ERROR_NULL If a NULL pointer is passed as input.
 * @retval NRF_ERROR_DATA_SIZE If the name filter length is too long. Maximum name filter
 *                             length corresponds to @ref NRF_BLE_SCAN_NAME_MAX_LEN.
 * @retval NRF_ERROR_NO_MEMORY If the number of available filters is exceeded.
 * @retval NRF_ERROR_INVALID_PARAM If the filter type is incorrect. Available filter types:
 *                                 @ref ble_scan_filter_type_t.
 * @retval BLE_ERROR_GAP_INVALID_BLE_ADDR If the BLE address type is invalid.
 */
int ble_scan_filter_add(struct ble_scan *const scan_ctx, uint8_t type,
			const void *data);

/**
 * @brief Remove all filters.
 *
 * @details The function removes all previously set filters.
 *
 * @note After using this function the filters are still enabled.
 *
 * @param[in,out] scan_ctx Scan library instance.
 *
 * @retval NRF_SUCCESS If all filters are removed successfully.
 */
int ble_scan_all_filter_remove(struct ble_scan *const scan_ctx);

#endif /* CONFIG_BLE_SCAN_FILTER */

/**
 * @brief Set the scanning parameters.
 *
 * @details Use this function to change scanning parameters. During the parameter change
 *          the scan is stopped. To resume scanning, use @ref ble_scan_start.
 *
 * @param[in,out] scan_ctx Scan library instance.
 * @param[in] scan_params GAP scanning parameters. Can be initialized as NULL.
 *
 * @retval NRF_SUCCESS If parameters are changed successfully.
 * @retval NRF_ERROR_NULL If a NULL pointer is passed as input.
 */
int ble_scan_params_set(struct ble_scan *const scan_ctx, const ble_gap_scan_params_t *scan_params);

/**
 * @brief Handler for BLE stack events.
 *
 * @param[in] ble_evt BLE event.
 * @param[in,out] scan Scan library instance.
 */
void ble_scan_on_ble_evt(const ble_evt_t *ble_evt, void *scan);

/**
 * @brief Convert the raw address to the SoftDevice GAP address.
 *
 * @details This function inverts the byte order in the address. If you enter the address as it is
 *          displayed (for example, on a phone screen from left to right), you must use
 *          this function to convert the address to the SoftDevice address type.
 *
 * @note This function does not decode an address type.
 *
 * @param[out] gap_addr The Bluetooth Low Energy address.
 * @param[in] addr Address to be converted to the SoftDevice address.
 *
 * @retval NRF_ERROR_NULL If a NULL pointer is passed as input.
 * @retval NRF_SUCCESS If the address is copied and converted successfully.
 */
int ble_scan_copy_addr_to_sd_gap_addr(ble_gap_addr_t *gap_addr,
				      const uint8_t addr[BLE_GAP_ADDR_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* BLE_SCAN_H__ */

/** @} */
