/*
 * Copyright (c) 2012-2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BLE_CONN_PARAMS_H__
#define BLE_CONN_PARAMS_H__

#include <stdint.h>
#include <ble_gap.h>

#ifdef __cplusplus
extern "C" {
#endif

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
	/**
	 * @brief GAP radio phy mode update procedure completed.
	 */
	BLE_CONN_PARAMS_EVT_RADIO_PHY_MODE_UPDATED,
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
		 * @brief Negotiated connection parameters.
		 *
		 * From @ref BLE_CONN_PARAMS_EVT_UPDATED.
		 */
		ble_gap_conn_params_t conn_params;
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
		/**
		 * @brief Negotiated radio PHY mode and procedure status.
		 *
		 * From @ref BLE_CONN_PARAMS_EVT_RADIO_PHY_MODE_UPDATED.
		 */
		ble_gap_evt_phy_update_t phy_update_evt;
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
int ble_conn_params_evt_handler_set(ble_conn_params_evt_handler_t handler);

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
 */
int ble_conn_params_data_length_get(uint16_t conn_handle, uint8_t *data_length);

/**
 * @brief Initiate a GAP radio PHY mode update procedure for a given connection.
 *
 * The radio PHY mode update procedure is executed asynchronously, and its completion
 * is signalled by the @ref BLE_CONN_PARAMS_EVT_RADIO_PHY_MODE_UPDATED event.
 *
 * @param conn_handle Handle to the connection.
 * @param phy_pref Desired GAP radio PHY mode.
 *
 * @return 0 On success.
 * @return -EINVAL Invalid data length or connection handle.
 */
int ble_conn_params_phy_radio_mode_set(uint16_t conn_handle, ble_gap_phys_t phy_pref);

/**
 * @brief Retrieve the current GAP radio PHY mode for a given connection.
 *
 * @param conn_handle Handle to the connection.
 * @param[out] phy_pref The radio PHY mode.
 *
 * @return 0 On success.
 * @return -EINVAL Invalid connection handle.
 * @return -EFAULT @p phy_pref is @c NULL.
 */
int ble_conn_params_phy_radio_mode_get(uint16_t conn_handle, ble_gap_phys_t *phy_pref);

#ifdef __cplusplus
}
#endif

#endif /* BLE_CONN_PARAMS_H__ */
