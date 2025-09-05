/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @defgroup gatts_cache_manager GATT Server Cache Manager
 * @ingroup peer_manager
 * @{
 * @brief An internal module of @ref peer_manager. A module for managing persistent storing of GATT
 *        attributes pertaining to the GATT server role of the local device.
 */

#ifndef GATTS_CACHE_MANAGER_H__
#define GATTS_CACHE_MANAGER_H__

#include <stdint.h>
#include <ble.h>
#include <ble_gap.h>
#include <bluetooth/peer_manager/peer_manager_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function for initializing the GATT Server Cache Manager module.
 *
 * @retval NRF_SUCCESS         Initialization was successful.
 * @retval NRF_ERROR_INTERNAL  If an internal error occurred.
 */
uint32_t gscm_init(void);

/**
 * @brief Function for triggering local GATT database data to be stored persistently. Values are
 *        retrieved from the SoftDevice and written to persistent storage.
 *
 * @param[in]  conn_handle  Connection handle to perform update on.
 *
 * @retval NRF_SUCCESS                    Store operation started.
 * @retval BLE_ERROR_INVALID_CONN_HANDLE  conn_handle does not refer to an active connection with a
 *                                        bonded peer.
 * @retval NRF_ERROR_INVALID_DATA         Refused to update because the GATT database is already up
 *                                        to date.
 * @retval NRF_ERROR_BUSY                 Unable to perform operation at this time. Reattempt later.
 * @retval NRF_ERROR_DATA_SIZE            Write buffer not large enough. Call will never work with
 *                                        this GATT database.
 * @retval NRF_ERROR_RESOURCES         No room in persistent_storage. Free up space; the
 *                                        operation will be automatically reattempted after the
 *                                        next FDS garbage collection procedure.
 */
uint32_t gscm_local_db_cache_update(uint16_t conn_handle);

/**
 * @brief Function for applying stored local GATT database data to the SoftDevice. Values are
 *        retrieved from persistent storage and given to the SoftDevice.
 *
 * @param[in]  conn_handle  Connection handle to apply values to.
 *
 * @retval NRF_SUCCESS                    Store operation started.
 * @retval BLE_ERROR_INVALID_CONN_HANDLE  conn_handle does not refer to an active connection with a
 *                                        bonded peer.
 * @retval NRF_ERROR_INVALID_DATA         The stored data was rejected by the SoftDevice, which
 *                                        probably means that the local database has changed. The
 *                                        system part of the sys_attributes was attempted applied,
 *                                        so service changed indications can be sent to subscribers.
 * @retval NRF_ERROR_BUSY                 Unable to perform operation at this time. Reattempt later.
 * @retval NRF_ERROR_INTERNAL             An unexpected error happened.
 */
uint32_t gscm_local_db_cache_apply(uint16_t conn_handle);

/**
 * @brief Function for storing the fact that the local database has changed, for all currently
 *        bonded peers.
 *
 * @note This will cause a later call to @ref gscm_service_changed_ind_needed to return true for
 *       a connection with a currently bonded peer.
 */
void gscm_local_database_has_changed(void);

/**
 * @brief Function for checking if a service changed indication should be sent.
 *
 * @param[in]  conn_handle  The connection to check.
 *
 * @return true if a service changed indication should be sent, false if not.
 */
bool gscm_service_changed_ind_needed(uint16_t conn_handle);

/**
 * @brief Function for sending a service changed indication to a connected peer.
 *
 * @param[in]  conn_handle  The connection to send the indication on.
 *
 * @retval NRF_SUCCESS                       Indication sent or not needed.
 * @retval BLE_ERROR_INVALID_CONN_HANDLE     conn_handle does not refer to an active connection.
 * @retval NRF_ERROR_BUSY                    Unable to send indication at this time. Reattempt
 * later.
 * @retval BLE_ERROR_GATTS_SYS_ATTR_MISSING  Information missing. Apply local cache, then reattempt.
 * @retval NRF_ERROR_INVALID_PARAM           From @ref sd_ble_gatts_service_changed. Unexpected.
 * @retval NRF_ERROR_NOT_SUPPORTED           Service changed characteristic is not present.
 * @retval NRF_ERROR_INVALID_STATE           Service changed cannot be indicated to this peer
 *                                           because the peer has not subscribed to it.
 * @retval NRF_ERROR_INTERNAL                An unexpected error happened.
 */
uint32_t gscm_service_changed_ind_send(uint16_t conn_handle);

/**
 * @brief Function for specifying that a peer has been made aware of the latest local database
 *        change.
 *
 * @note After calling this, a later call to @ref gscm_service_changed_ind_needed will to return
 *       false for this peer unless @ref gscm_local_database_has_changed is called again.
 *
 * @param[in]  peer_id  The connection to send the indication on.
 */
void gscm_db_change_notification_done(pm_peer_id_t peer_id);

#ifdef __cplusplus
}
#endif

#endif /* GATTS_CACHE_MANAGER_H__ */

/** @} */
