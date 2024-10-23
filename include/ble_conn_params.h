/*
 * Copyright (c) 2012-2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <ble_gap.h>

/**
 * @brief BLE connection parameter event.
 */
enum ble_conn_params_evt_id {
	/**
	 * @brief Connection parameters updated.
	 */
	BLE_CONN_PARAMS_EVT_UPDATED,
	/**
	 * @brief Could not agree on desired connection parameters.
	 */
	BLE_CONN_PARAMS_EVT_REJECTED,
	/**
	 * @brief ATT MTU exchange procedure completed.
	 */
	BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED,
	/**
	 * @brief GAP data length update procedure completed.
	 */
	BLE_CONN_PARAMS_EVT_DATA_LENGTH_UPDATED,
};

/**
 * @brief BLE connection parameter event.
 */
struct ble_conn_params_evt {
	/**
	 * @brief Event ID.
	 */
	enum ble_conn_params_evt_id id;
	/**
	 * @brief Connection handle.
	 */
	uint16_t conn_handle;
	union {
		/**
		 * @brief Negotiated GATT ATT MTU.
		 *
		 * From @ref BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED.
		 */
		uint16_t att_mtu;
		/**
		 * @brief Negotiated GAP Data length.
		 *
		 * From @ref BLE_CONN_PARAMS_EVT_DATA_LENGTH_UPDATED.
		 */
		uint8_t data_length;
	};
};

/**
 * @brief BLE connection parameters event handler prototype.
 */
typedef void (*ble_conn_params_evt_handler_t)(const struct ble_conn_params_evt *evt);

/**
 * @brief Set a handler function to receive events.
 *
 * The handler is optional.
 * If no handler is set, no events are sent to the application.
 *
 * @param handler Handler.
 *
 * @returns 0 On success.
 * @returns -EFAULT If @p handler is @c NULL.
 */
int ble_conn_params_event_handler_set(ble_conn_params_evt_handler_t handler);

/**
 * @brief Override GAP connection parameters for given peer.
 *
 * Request to update connection parameters with given parameters.
 * The result of the connection parameter update is signaled to the application
 * via the library's event handler.
 *
 * @param conn_handle Connection handle.
 * @param conn_params Connection parameters.
 *
 * @returns 0 Connection parameter update initiated successfully.
 * @returns -EINVAL If @p conn_handle is invalid.
 * @returns -EFAULT If @p conn_params is @c NULL.
 */
int ble_conn_params_override(uint16_t conn_handle, const ble_gap_conn_params_t *conn_params);

/**
 * @brief Initiate an ATT MTU exchange procedure for a given connection.
 *
 * The minimum supported ATT MTU value is 23 (connection's default).
 * The maximum supported ATT MTU value is 247.
 *
 * The SoftDevice needs to be configured to support non-default ATT MTU values
 * by setting @c CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE to the maximum ATT MTU value
 * the device shall support.
 *
 * The ATT MTU exchange procedure is executed asynchronously, and its completion
 * is signalled by the @ref BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED event.
 *
 * @param conn_handle Handle to the connection.
 * @param att_mtu Desired ATT MTU.
 *
 * @return 0 On success.
 * @return -EINVAL Invalid ATT MTU or connection handle.
 * @return -ENOTCONN Peer is not connected.
 */
int ble_conn_params_att_mtu_set(uint16_t conn_handle, uint16_t att_mtu);

/**
 * @brief Retrieve the current ATT MTU for a given connection.
 *
 * @param conn_handle Handle to the connection.
 * @param[out] att_mtu The ATT MTU value.
 *
 * @return 0 On success.
 * @return -EINVAL Invalid connection handle.
 * @return -EFAULT @p att_mtu is @c NULL.
 * @return -ENOTCONN Peer is not connected.
 */
int ble_conn_params_att_mtu_get(uint16_t conn_handle, uint16_t *att_mtu);

/**
 * @brief Initiate a GAP data length update procedure for a given connection.
 *
 * The data length update procedure is executed asynchronously, and its completion
 * is signalled by the @ref BLE_CONN_PARAMS_EVT_DATA_LENGTH_UPDATED event.
 *
 * @param conn_handle Handle to the connection.
 * @param data_length Desired GAP data length.
 *
 * @return 0 On success.
 * @return -EINVAL Invalid data length or connection handle.
 * @return -ENOTCONN Peer is not connected.
 */
int ble_conn_params_data_length_set(uint16_t conn_handle, uint8_t data_length);

/**
 * @brief Retrieve the current GAP data length for a given connection.
 *
 * @param conn_handle Handle to the connection.
 * @param[out] data_length The data length value.
 *
 * @return 0 On success.
 * @return -EINVAL Invalid connection handle.
 * @return -EFAULT @p data_length is @c NULL.
 * @return -ENOTCONN Peer is not connected.
 */
int ble_conn_params_data_length_get(uint16_t conn_handle, uint8_t *data_length);
