/**
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @defgroup gatt_cache_manager GATT Cache Manager
 * @ingroup peer_manager
 * @{
 * @brief An internal module of @ref peer_manager. A module for managing persistent storing of GATT
 *        attributes.
 */

#ifndef GATT_CACHE_MANAGER_H__
#define GATT_CACHE_MANAGER_H__

#include <stdint.h>
#include <ble.h>
#include <ble_gap.h>
#include <bluetooth/peer_manager/peer_manager_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function for initializing the GATT Cache Manager module.
 *
 * @retval NRF_SUCCESS         Initialization was successful.
 * @retval NRF_ERROR_INTERNAL  If an internal error occurred.
 */
uint32_t gcm_init(void);

/**
 * @brief Function for dispatching SoftDevice events to the GATT Cache Manager module.
 *
 * @param[in]  p_ble_evt  The SoftDevice event.
 */
void gcm_ble_evt_handler(ble_evt_t const *p_ble_evt);

/**
 * @brief Function for triggering local GATT database data to be stored persistently.
 *
 * @details Values are retrieved from SoftDevice and written to persistent storage.
 *
 * @note This operation happens asynchronously, so any errors are reported as events.
 *
 * @note This function is only needed when you want to override the regular functionality of the
 *       module, e.g. to immediately store to flash instead of waiting for the native logic to
 *       perform the update.
 *
 * @param[in]  conn_handle  Connection handle to perform update on.
 *
 * @retval NRF_SUCCESS                    Store operation started.
 */
uint32_t gcm_local_db_cache_update(uint16_t conn_handle);

/**
 * @brief Function for manually informing that the local database has changed.
 *
 * @details This causes a service changed notification to be sent to all bonded peers that
 *          subscribe to it.
 */
void gcm_local_database_has_changed(void);

#ifdef __cplusplus
}
#endif

#endif /* GATT_CACHE_MANAGER_H__ */

/** @} */
