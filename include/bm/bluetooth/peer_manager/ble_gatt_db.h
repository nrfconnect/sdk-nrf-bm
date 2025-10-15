/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 *
 * @defgroup ble_sdk_lib_gatt_db GATT Database Service Structure
 * @{
 * @ingroup  ble_sdk_lib
 */

#ifndef BLE_GATT_DB_H__
#define BLE_GATT_DB_H__

#include <stdint.h>
#include <ble.h>
#include <ble_gattc.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief The maximum number of characteristics present in a service record. */
#define BLE_GATT_DB_MAX_CHARS 6

/**@brief Structure for holding the characteristic and the handle of its CCCD present on a server.
 */
typedef struct {
	/** @brief Structure containing information about the characteristic. */
	ble_gattc_char_t characteristic;
	/**
	 * @brief CCCD Handle value for this characteristic. This will be set to
	 *        BLE_GATT_HANDLE_INVALID if a CCCD is not present at the server.
	 */
	uint16_t cccd_handle;
	/**
	 * @brief Extended Properties Handle value for this characteristic.
	 *        This will be set to BLE_GATT_HANDLE_INVALID if an Extended
	 *        Properties descriptor is not present at the server.
	 */
	uint16_t ext_prop_handle;
	/**
	 * @brief User Description Handle value for this characteristic. This
	 *        will be set to BLE_GATT_HANDLE_INVALID if a User Description
	 *        descriptor is not present at the server.
	 */
	uint16_t user_desc_handle;
	/**
	 * @brief Report Reference Handle value for this characteristic. This
	 *        will be set to BLE_GATT_HANDLE_INVALID if a Report Reference
	 *        descriptor is not present at the server.
	 */
	uint16_t report_ref_handle;
} ble_gatt_db_char_t;

/**
 * @brief Structure for holding information about the service and the characteristics present on a
 *        server.
 */
typedef struct {
	/** @brief UUID of the service. */
	ble_uuid_t srv_uuid;
	/** @brief Number of characteristics present in the service. */
	uint8_t char_count;
	/** @brief Service Handle Range. */
	ble_gattc_handle_range_t handle_range;
	/**
	 * @brief Array of information related to the characteristics present in the service.
	 *        This list can extend further than one.
	 */
	ble_gatt_db_char_t charateristics[BLE_GATT_DB_MAX_CHARS];
} ble_gatt_db_srv_t;

#ifdef __cplusplus
}
#endif

#endif /* BLE_GATT_DB_H__ */

/** @} */
