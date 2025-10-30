/*
 * Copyright (c) 2013 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup ble_db_discovery BLE Nordic database discovery library
 * @{
 * @brief Library for discovery of a service and its characteristics at the peer server.
 */

#include <stdint.h>
#include <zephyr/kernel.h>
#include <ble_gattc.h>
#include <bm/bluetooth/ble_gq.h>

#ifndef BLE_DB_DISCOVERY_H__
#define BLE_DB_DISCOVERY_H__

/**
 * @brief Macro for defining a ble_db_discovery instance.
 *
 * @param _name Name of the instance.
 */
#define BLE_DB_DISCOVERY_DEF(_name)                                                                \
	static struct ble_db_discovery _name = {.discovery_in_progress = 0,                        \
						.conn_handle = BLE_CONN_HANDLE_INVALID};           \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_db_discovery_on_ble_evt, &_name,                     \
			     BLE_DB_DISC_BLE_OBSERVER_PRIO)

/**
 * @brief BLE GATT database characteristic.
 */
struct ble_gatt_db_char {
	/**
	 * @brief Structure containing information about the characteristic.
	 */
	ble_gattc_char_t characteristic;
	/**
	 * @brief CCCD Handle value for this characteristic.
	 *
	 * This will be set to BLE_GATT_HANDLE_INVALID if a CCCD is not present at the server.
	 */
	uint16_t cccd_handle;
	/**
	 * @brief Extended Properties Handle value for this characteristic.
	 *
	 * This will be set to BLE_GATT_HANDLE_INVALID if an Extended
	 * Properties descriptor is not present at the server.
	 */
	uint16_t ext_prop_handle;
	/**
	 * @brief User Description Handle value for this characteristic.
	 *
	 * This will be set to BLE_GATT_HANDLE_INVALID if a User Description
	 * descriptor is not present at the server.
	 */
	uint16_t user_desc_handle;
	/**
	 * @brief Report Reference Handle value for this characteristic.
	 * This will be set to BLE_GATT_HANDLE_INVALID if a Report Reference
	 * descriptor is not present at the server.
	 */
	uint16_t report_ref_handle;
};

/**
 * @brief BLE GATT database service.
 */
struct ble_gatt_db_srv {
	/**
	 * @brief UUID of the service.
	 */
	ble_uuid_t srv_uuid;
	/**
	 * @brief Number of characteristics present in the service.
	 */
	uint8_t char_count;
	/**
	 * @brief Service Handle Range.
	 */
	ble_gattc_handle_range_t handle_range;
	/**
	 * @brief Array of information related to the characteristics present in the service.
	 *
	 * This list can extend further than one.
	 */
	struct ble_gatt_db_char charateristics[CONFIG_BLE_GATT_DB_MAX_CHARS];
};

/**
 * @brief BLE database discovery event type.
 */
enum ble_db_discovery_evt_type {
	/**
	 * @brief Event indicating that the discovery of one service is complete.
	 */
	BLE_DB_DISCOVERY_COMPLETE,
	/**
	 * @brief Event indicating that an internal error has occurred in the DB Discovery module.
	 *
	 * This could typically be because of the SoftDevice API returning an error code during the
	 * DB discover.
	 */
	BLE_DB_DISCOVERY_ERROR,
	/**
	 * @brief Event indicating that the service was not found at the peer.
	 */
	BLE_DB_DISCOVERY_SRV_NOT_FOUND,
	/**
	 * @brief Event indicating that the DB discovery instance is available.
	 */
	BLE_DB_DISCOVERY_AVAILABLE
};

/**
 * @brief BLE database discovery event.
 */
struct ble_db_discovery_evt {
	/**
	 * @brief Type of event.
	 */
	enum ble_db_discovery_evt_type evt_type;
	/**
	 * @brief Handle of the connection for which this event has occurred.
	 */
	uint16_t conn_handle;
	union {
		/**
		 * @brief Structure containing the information about the GATT Database at the
		 * server.
		 *
		 * This will be filled when the event type is @ref BLE_DB_DISCOVERY_COMPLETE. The
		 * UUID field of this will be filled when the event type is @ref
		 * BLE_DB_DISCOVERY_SRV_NOT_FOUND.
		 */
		struct ble_gatt_db_srv discovered_db;
		/**
		 * @brief Pointer to DB discovery instance @ref ble_db_discovery_t, indicating
		 * availability to the new discovery process.
		 *
		 * This will be filled when the event type is @ref BLE_DB_DISCOVERY_AVAILABLE.
		 */
		void const *db_instance;
		/**
		 * @brief nRF Error code indicating the type of error which occurred in the DB
		 * Discovery module.
		 *
		 * This will be filled when the event type is @ref BLE_DB_DISCOVERY_ERROR.
		 */
		struct {
			/**
			 * @brief Error reason.
			 */
			uint32_t reason;
		} error;
	} params;
};

typedef void (*ble_db_discovery_evt_handler)(struct ble_db_discovery_evt *evt);

/**
 * @brief BLE database discovery configuration.
 */
struct ble_db_discovery_config {
	/**
	 * @brief Event handler to be called by the DB Discovery module.
	 */
	ble_db_discovery_evt_handler evt_handler;
	/**
	 * @brief Pointer to BLE GATT Queue instance.
	 */
	struct ble_gq *gatt_queue;
	/**
	 * @brief Connection handle where the discovery is started.
	 */
	uint16_t conn_handle;
};

/**
 * @brief BLE database discovery user event.
 */
struct ble_db_discovery_user_evt {
	/**
	 * @brief Pending event.
	 */
	struct ble_db_discovery_evt evt;
	/**
	 * @brief Event handler which should be called to raise this event.
	 */
	ble_db_discovery_evt_handler evt_handler;
};

/**
 * @brief BLE database discovery.
 */
struct ble_db_discovery {
	/**
	 * @brief Information related to the current service being discovered.
	 *
	 * This is intended for internal use during service discovery.
	 */
	struct ble_gatt_db_srv services[CONFIG_BLE_DB_DISCOVERY_MAX_SRV];
	/**
	 * @brief Number of services at the peer's GATT database.
	 */
	uint8_t srv_count;
	/**
	 * @brief Index of the current characteristic being discovered.
	 *
	 * This is intended for internal use during service discovery.
	 */
	uint8_t curr_char_ind;
	/**
	 * @brief Index of the current service being discovered.
	 *
	 * This is intended for internal use during service discovery.
	 */
	uint8_t curr_srv_ind;
	/**
	 * @brief Number of service discoveries made, both successful and unsuccessful.
	 */
	uint8_t discoveries_count;
	/**
	 * @brief Variable to indicate whether there is a service discovery in progress.
	 */
	bool discovery_in_progress;
	/**
	 * @brief Connection handle on which the discovery is started.
	 */
	uint16_t conn_handle;
	/**
	 * @brief The index to the pending user event array, pointing to the last added pending user
	 * event.
	 */
	uint32_t pending_usr_evt_index;
	/**
	 * @brief Whenever a discovery related event is to be raised to a user module, it is stored
	 * in this array first.
	 *
	 * When all expected services have been discovered, all pending events are sent to the
	 * corresponding user modules.
	 */
	struct ble_db_discovery_user_evt pending_usr_evts[CONFIG_BLE_DB_DISCOVERY_MAX_SRV];
};

/**
 * @brief Function for initializing the DB Discovery module.
 *
 * @param[in] db_config   Pointer to DB discovery initialization structure.
 *
 * @retval NRF_SUCCESS    On successful initialization.
 * @retval NRF_ERROR_NULL If the initialization structure was NULL or
 *                        the structure content is empty.
 */
uint32_t ble_db_discovery_init(struct ble_db_discovery *dbd,
			       struct ble_db_discovery_config *db_config);

/**
 * @brief Function for registering with the DB Discovery module.
 *
 * @details The application can use this function to inform which service it is interested in
 *          discovering at the server.
 *
 * @param[in] uuid Pointer to the UUID of the service to be discovered at the server.
 *
 * @note The total number of services that can be discovered by this module is @ref
 *       CONFIG_BLE_DB_DISCOVERY_MAX_SRV. This effectively means that the maximum number of
 *       registrations possible is equal to the @ref CONFIG_BLE_DB_DISCOVERY_MAX_SRV.
 *
 * @retval NRF_SUCCESS             Operation success.
 * @retval NRF_ERROR_NULL          When a NULL pointer is passed as input.
 * @retval NRF_ERROR_INVALID_STATE If this function is called without calling the
 *                                 @ref ble_db_discovery_config.
 * @retval NRF_ERROR_NO_MEM        The maximum number of registrations allowed by this module
 *                                 has been reached.
 */
uint32_t ble_db_discovery_evt_register(const ble_uuid_t *const uuid);

/**
 * @brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]     ble_evt Pointer to the BLE event received.
 * @param[in,out] context Pointer to the DB Discovery structure.
 */
void ble_db_discovery_on_ble_evt(ble_evt_t const *ble_evt, void *context);

#endif /* BLE_DB_DISCOVERY_H__ */

/** @} */
