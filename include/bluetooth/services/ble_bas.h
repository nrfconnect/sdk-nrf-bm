/*
 * Copyright (c) 2012 - 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/** @file
 *
 * @defgroup ble_bas Battery Service
 * @{
 * @brief Battery Service.
 */
#ifndef BLE_BAS_H__
#define BLE_BAS_H__

#include <stdint.h>
#include <stdbool.h>
#include <ble.h>
#include <nrf_sdh_ble.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Define a Battery service instance.
 *
 * Define a battery service instance and register it as a Bluetooth event observer.
 */
#define BLE_BAS_DEF(_name)                                                                         \
	static struct ble_bas _name;                                                               \
	extern void ble_bas_on_ble_evt(const ble_evt_t *ble_evt, void *ctx);                       \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_bas_on_ble_evt, &_name, 0)

/**
 * @brief Battery service event types.
 */
enum ble_bas_evt_type {
	/**
	 * @brief Battery level notification enabled.
	 */
	BLE_BAS_EVT_NOTIFICATION_ENABLED,
	/**
	 * @brief Battery level notification disabled.
	 */
	BLE_BAS_EVT_NOTIFICATION_DISABLED
};

/**
 * @brief Battery service event.
 */
struct  ble_bas_evt {
	/**
	 * @brief Event type.
	 */
	enum ble_bas_evt_type evt_type;
	/**
	 * @brief Connection handle for which the event applies.
	 */
	uint16_t conn_handle;
};

/* Forward declaration */
struct ble_bas;

/**
 * @brief Battery service event handler type.
 */
typedef void (*ble_bas_evt_handler_t)(struct ble_bas *bas, const struct ble_bas_evt *evt);

/**
 * @brief Battery service configuration.
 */
struct ble_bas_config {
	/**
	 * @brief Battery service event handler.
	 */
	ble_bas_evt_handler_t evt_handler;
	/**
	 * @brief Report Reference Descriptor.
	 *
	 * If provided, a Report Reference descriptor with the specified
	 * value will be added to the Battery Level characteristic.
	 */
	struct {
		/**
		 * @brief Report ID.
		 *
		 * A non-zero value indicates that there is more than one instance
		 * of the same Report Type.
		 */
		uint8_t report_id;
		/**
		 * @brief Report type.
		 */
		uint8_t report_type;
	} *report_ref;
	/**
	 * @brief Allow notifications.
	 */
	bool can_notify;
	/**
	 * @brief Initial battery level.
	 */
	uint8_t battery_level;
	/**
	 * @brief Security requirement for reading the battery level characteristic value.
	 */
	ble_gap_conn_sec_mode_t batt_rd_sec;
	/**
	 * @brief Security requirement for writing the battery level characteristic CCCD.
	 */
	ble_gap_conn_sec_mode_t cccd_wr_sec;
	/**
	 * @brief Security requirement for reading the Report Reference characteristic descriptor.
	 */
	ble_gap_conn_sec_mode_t report_ref_rd_sec;
};

/**
 * @brief Battery Service structure.
 */
struct ble_bas {
	/**
	 * @brief Battery Service event handler.
	 */
	ble_bas_evt_handler_t evt_handler;
	/**
	 * @brief Battery service handle.
	 */
	uint16_t service_handle;
	/**
	 * @brief Report reference descriptor handler.
	 */
	uint16_t report_ref_handle;
	/**
	 * @brief Battery level characteristic handles.
	 */
	ble_gatts_char_handles_t battery_level_handles;
	/**
	 * @brief Battery level.
	 */
	uint8_t battery_level;
	/**
	 * @brief Whether notifications of battery level changes are supported.
	 */
	bool can_notify;
};

/**
 * @brief Initialize the battery service.
 *
 * @param bas Battery service.
 * @param bas_config Battery service configuration.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p bas or @p bas_config are @c NULL.
 * @retval -EINVAL Invalid parameters.
 */
int ble_bas_init(struct ble_bas *bas, const struct ble_bas_config *bas_config);

/**
 * @brief Update battery level.
 *
 * If this instance has notifications enabled, this function will notify the updated value
 * of the battery level to the peer with given @p conn_handle.
 *
 * @param bas Battery service.
 * @param conn_handle Connection handle.
 * @param battery_level Battery level (in percent of full capacity).
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p bas is @c NULL.
 * @retval -EINVAL Invalid parameters.
 * @retval -ENOTCONN Invalid connection handle.
 * @retval -EPIPE Notifications not enabled in the CCCD.
 */
int ble_bas_battery_level_update(struct ble_bas *bas, uint16_t conn_handle, uint8_t battery_level);

/**
 * @brief Notify battery level.
 *
 * @note For the requirements in the BAS specification to be fulfilled,
 *       this function must be called upon reconnection if the battery level has changed
 *       while the service has been disconnected from a bonded client.
 *
 * @param bas Battery service.
 * @param conn_handle Connection handle.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p bas is @c NULL.
 * @retval -EINVAL Invalid parameters.
 * @retval -ENOTCONN Invalid connection handle.
 * @retval -EPIPE Notifications not enabled in the CCCD.
 */
int ble_bas_battery_level_notify(struct ble_bas *bas, uint16_t conn_handle);

#ifdef __cplusplus
}
#endif

#endif /* BLE_BAS_H__ */

/** @} */
