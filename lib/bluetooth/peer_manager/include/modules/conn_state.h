/*
 * Copyright (c) 2015 - 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 *
 * @defgroup pm_conn_state Peer Manager connection state
 * @{
 * @brief Module for storing data on BLE connections.
 *
 * @details This module stores certain states for each connection, which can be queried by
 *          connection handle. The module uses BLE events to keep the states updated.
 *
 *          In addition to the preprogrammed states, this module can also keep track of a number of
 *          binary user states, or user flags. These are reset to 0 for new connections, but
 *          otherwise not touched by this module.
 *
 *          This module uses atomics to make the flag operations thread-safe.
 */

#ifndef PM_CONN_STATE_H__
#define PM_CONN_STATE_H__

#include <stdbool.h>
#include <stdint.h>
#include <ble.h>
#include <ble_gap.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Connection handle statuses. */
enum pm_conn_state_status {
	/** The connection handle is invalid. */
	PM_CONN_STATUS_INVALID,
	/** The connection handle refers to a connection that has been disconnected,
	 *  but not yet invalidated.
	 */
	PM_CONN_STATUS_DISCONNECTED,
	/** The connection handle refers to an active connection. */
	PM_CONN_STATUS_CONNECTED,
};

/** The maximum number of connections supported. */
#define PM_CONN_STATE_MAX_CONNECTIONS BLE_GAP_ROLE_COUNT_COMBINED_MAX

/** Invalid user flag. */
#define PM_CONN_STATE_USER_FLAG_INVALID CONFIG_PM_CONN_STATE_USER_FLAG_COUNT

/** @brief Type used to present a list of conn_handles. */
struct pm_conn_state_conn_handle_list {
	/** The length of the list. */
	uint32_t len;
	/** The list of handles. */
	uint16_t conn_handles[PM_CONN_STATE_MAX_CONNECTIONS];
};

/**
 * @brief Function to be called when a flag ID is set.
 *
 * See @ref pm_conn_state_for_each_set_user_flag.
 *
 * @param[in] conn_handle  The connection the flag is set for.
 * @param[in] ctx Arbitrary pointer provided by the caller of
 *                @ref pm_conn_state_for_each_set_user_flag.
 */
typedef void (*pm_conn_state_user_function_t)(uint16_t conn_handle, void *ctx);

/**
 * @defgroup pm_conn_state_functions BLE connection state functions
 * @{
 */

/**
 * @brief Initialize or reset the module.
 *
 * @details This function sets all states to their default,
 *          removing all records of connection handles.
 */
void pm_conn_state_init(void);

/**
 * @brief Check whether a connection handle represents a valid connection.
 *
 * @details A connection might be valid and have a PM_CONN_STATUS_DISCONNECTED status.
 *          Those connections are invalidated after a new connection occurs.
 *
 * @param[in] conn_handle Handle of the connection.
 *
 * @retval true If @c conn_handle represents a valid connection, thus a connection for which
		we have a record.
 * @retval false If @c conn_handle is @ref BLE_GAP_ROLE_INVALID, or if it has never been recorded.
 */
bool pm_conn_state_valid(uint16_t conn_handle);

/**
 * @brief Get the role of the local device in a connection.
 *
 * @param[in] conn_handle Handle of the connection to get the role for.
 *
 * @return The role of the local device in the connection (see @ref BLE_GAP_ROLES).
 *         If conn_handle is not valid, the function returns @ref BLE_GAP_ROLE_INVALID.
 */
uint8_t pm_conn_state_role(uint16_t conn_handle);

/**
 * @brief Get the status of a connection.
 *
 * @param[in] conn_handle Handle of the connection.
 *
 * @return The status of the connection.
 *         If conn_handle is not valid, the function returns @ref PM_CONN_STATE_INVALID.
 */
enum pm_conn_state_status pm_conn_state_status(uint16_t conn_handle);

/**
 * @brief Check whether a connection is encrypted.
 *
 * @param[in] conn_handle Handle of connection to get the encryption state for.
 *
 * @retval true If the connection is encrypted.
 * @retval false If the connection is not encrypted or conn_handle is invalid.
 */
bool pm_conn_state_encrypted(uint16_t conn_handle);

/**
 * @brief Check whether a connection encryption is protected from Man in the Middle
 *        (MITM) attacks.
 *
 * @param[in]  conn_handle  Handle of connection to get the MITM state for.
 *
 * @retval true If the connection is encrypted with MITM protection.
 * @retval false If the connection is not encrypted, or encryption is not MITM protected, or
 *               conn_handle is invalid.
 */
bool pm_conn_state_mitm_protected(uint16_t conn_handle);

/**
 * @brief Check whether a connection was bonded using LE Secure Connections (LESC).
 *
 * The connection must currently be encrypted.
 *
 * @note This function will report false if bonded, and the LESC bonding was unauthenticated
 *       ("Just Works") and happened in a previous connection. To detect such cases as well, check
 *       the stored bonding key, e.g. in Peer Manager, which has a LESC flag associated with it.
 *
 * @param[in] conn_handle Handle of connection to get the LESC state for.
 *
 * @retval true If the connection was bonded using LESC.
 * @retval false If the connection has not been bonded using LESC, or @c conn_handle is invalid.
 */
bool pm_conn_state_lesc(uint16_t conn_handle);

/**
 * @brief Get a list of all connection handles for which the module has a record.
 *
 * @details This function takes into account connections whose state is
 *          @ref PM_CONN_STATUS_DISCONNECTED.
 *
 * @return  A list of all valid connection handles for which the module has a record.
 */
struct pm_conn_state_conn_handle_list pm_conn_state_conn_handles(void);

/**
 * @brief Obtain exclusive access to one of the user flag collections.
 *
 * @details The acquired collection contains one flag for each connection. These flags can be set
 *          and read individually for each connection.
 *
 *          The state of user flags will not be modified by the connection state module, except to
 *          set it to 0 for a connection when that connection is invalidated.
 *
 * @return The index of the acquired flag,
 *         or @ref PM_CONN_STATE_USER_FLAG_INVALID if none are available.
 */
int pm_conn_state_user_flag_acquire(void);

/**
 * @brief Read the value of a user flag.
 *
 * @param[in] conn_handle Handle of connection to get the flag state for.
 * @param[in] flag_index Which flag to get the state for.
 *
 * @return  The state of the flag. If @c conn_handle is invalid, the function returns false.
 */
bool pm_conn_state_user_flag_get(uint16_t conn_handle, uint16_t flag_index);

/**
 * @brief Set the value of a user flag.
 *
 * @param[in] conn_handle Handle of connection to set the flag state for.
 * @param[in] flag_index Which flag to set the state for.
 * @param[in] value Value to set the flag state to.
 */
void pm_conn_state_user_flag_set(uint16_t conn_handle, uint16_t flag_index, bool value);

/**
 * @brief Run a function for each connection that has a user flag set.
 *
 * @param[in] flag_index Which flag to check.
 * @param[in] user_function The function to run when a flag is set.
 * @param[in] ctx Arbitrary context to be passed to @p user_function.
 *
 * @return The number of times @p user_function was run.
 */
uint32_t pm_conn_state_for_each_set_user_flag(uint16_t flag_index,
					      pm_conn_state_user_function_t user_function,
					      void *ctx);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* PM_CONN_STATE_H__ */

/** @} */
