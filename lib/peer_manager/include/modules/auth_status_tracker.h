/*
 * Copyright (c) 2018-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @defgroup auth_status_tracker Authorization Status Tracker
 * @ingroup peer_manager
 * @{
 * @brief An internal module of @ref peer_manager. A module for tracking peers with failed
 *        authorization attempts. It uses tracking policy, which is described in Bluetooth
 *        Core Specification v5.0, Vol 3, Part H, Section 2.3.6.
 */

#ifndef AUTH_STATUS_TRACKER_H__
#define AUTH_STATUS_TRACKER_H__

#include <stdint.h>
#include <ble_gap.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function for initializing the Authorization Status Tracker module.
 *
 * @retval NRF_SUCCESS Initialization was successful.
 * @retval Other       Other error codes might be returned by the @ref app_timer_create function.
 */
uint32_t ast_init(void);

/**
 * @brief Function for notifying about failed authorization attempts.
 *
 * @param[in]  conn_handle  Connection handle on which authorization attempt has failed.
 */
void ast_auth_error_notify(uint16_t conn_handle);

/**
 * @brief Function for checking if pairing request must be rejected.
 *
 * @param[in]  conn_handle  Connection handle on which this check must be performed.
 *
 * @retval  true   If the connected peer is blacklisted.
 * @retval  false  If the connected peer is not blacklisted.
 */
bool ast_peer_blacklisted(uint16_t conn_handle);

#ifdef __cplusplus
}
#endif

#endif /* AUTH_STATUS_TRACKER_H__ */

/** @} */
