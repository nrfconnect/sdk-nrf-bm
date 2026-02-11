/*
 * Copyright (c) 2013 - 2026 Nordic Semiconductor ASA
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
#include <bm/bluetooth/ble_gatt_db.h>

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
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_db_discovery_on_ble_evt, &_name, HIGH)

/**
 * @brief BLE database discovery event type.
 */
enum ble_db_discovery_evt_type {
	/**
	 * @brief Event indicating that the discovery of one service is complete.
	 */
	BLE_DB_DISCOVERY_COMPLETE,
	/**
	 * @brief Event indicating that the service was not found at the peer.
	 */
	BLE_DB_DISCOVERY_SRV_NOT_FOUND,
	/**
	 * @brief Event indicating that the DB discovery instance is available.
	 */
	BLE_DB_DISCOVERY_AVAILABLE,
	/**
	 * @brief Event indicating that an internal error has occurred in the DB Discovery library.
	 *
	 * This could typically be because of the SoftDevice API returning an error code during the
	 * database discovery.
	 */
	BLE_DB_DISCOVERY_ERROR,
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
		 * @brief Parameters for
		 * - @ref BLE_DB_DISCOVERY_COMPLETE
		 * - @ref BLE_DB_DISCOVERY_SRV_NOT_FOUND
		 *
		 * Contains information about the GATT Database at the server.
		 *
		 * Only the UUID field will be filled when the event type is
		 * @ref BLE_DB_DISCOVERY_SRV_NOT_FOUND.
		 */
		struct ble_gatt_db_srv discovered_db;
		/**
		 * @brief Parameters for @ref BLE_DB_DISCOVERY_ERROR.
		 */
		struct {
			/**
			 * @brief Error reason.
			 */
			uint32_t reason;
		} error;
	} params;
};

/* Forward declaration. */
struct ble_db_discovery;

/**
 * @brief DB discovery event handler type.
 */
typedef void (*ble_db_discovery_evt_handler)(struct ble_db_discovery *db_discovery,
					     struct ble_db_discovery_evt *evt);

/**
 * @brief BLE database discovery configuration.
 */
struct ble_db_discovery_config {
	/**
	 * @brief Event handler to be called by the DB Discovery library.
	 */
	ble_db_discovery_evt_handler evt_handler;
	/**
	 * @brief Pointer to BLE GATT Queue instance.
	 */
	const struct ble_gq *gatt_queue;
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
	 * @brief UUID of registered handlers
	 */
	ble_uuid_t registered_uuids[CONFIG_BLE_DB_DISCOVERY_MAX_SRV];
	/**
	 * @brief Instance event handler.
	 */
	ble_db_discovery_evt_handler evt_handler;
	/**
	 * @brief BLE GATT Queue instance.
	 */
	const struct ble_gq *gatt_queue;
	/**
	 * @brief The number of UUIDs registered with the DB Discovery library.
	 */
	uint32_t num_registered_uuids;
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
	 * @brief The index to the pending user event array, pointing to the last added pending
	 *        user event.
	 */
	uint32_t pending_usr_evt_index;
	/**
	 * @brief Whenever a discovery related event is to be raised, it is stored in this array
	 *        first. When all registered services have been attempted discovered, all pending
	 *        events are sent to the user.
	 */
	struct ble_db_discovery_user_evt pending_usr_evts[CONFIG_BLE_DB_DISCOVERY_MAX_SRV];
};

/**
 * @brief Initialize the DB Discovery library.
 *
 * @param[out] db_discovery DB discovery instance.
 * @param[in] db_config DB discovery initialization structure.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @p db_config, @ref ble_db_discovery_config::evt_handler,
 *                        @ref ble_db_discovery_config::gatt_queue or @p db_discovery are @c NULL.
 */
uint32_t ble_db_discovery_init(struct ble_db_discovery *db_discovery,
			       const struct ble_db_discovery_config *db_config);

/**
 * @brief Start the discovery of the GATT database at the server.
 *
 * @param[in,out] db_discovery DB Discovery instance.
 * @param[in] conn_handle The handle of the connection for which the discovery should be started.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @p db_discovery is @c NULL.
 * @retval NRF_ERROR_INVALID_STATE If this function is called without calling the
 *                                 @ref ble_db_discovery_init, or without calling the
 *                                 @ref ble_db_discovery_service_register.
 * @retval NRF_ERROR_BUSY If a discovery is already in progress using @p db_discovery.
 *                        Use a different database discovery instance,
 *                        or wait for a DB Discovery event before retrying.
 * @return In addition, this function may return any error
 *         returned by the following functions:
 *         - @ref ble_gq_conn_handle_register
 *         - @ref ble_gq_item_add
 */
uint32_t ble_db_discovery_start(struct ble_db_discovery *db_discovery, uint16_t conn_handle);

/**
 * @brief Register service UUID with the DB Discovery instance.
 *
 * @details The application should use this function to inform which service it is interested in
 *          discovering at the server.
 *
 * @param[in,out] db_discovery DB discovery instance.
 * @param[in] uuid UUID of the service to be discovered at the server.
 *
 * @note The total number of services that can be discovered by this library is @ref
 *       CONFIG_BLE_DB_DISCOVERY_MAX_SRV. This effectively means that the maximum number of
 *       registrations possible is equal to @ref CONFIG_BLE_DB_DISCOVERY_MAX_SRV.
 *       Registering an already registered service UUID will not have any effect.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @p db_discovery is @c NULL.
 * @retval NRF_ERROR_INVALID_STATE If this function is called without calling the
 *                                 @ref ble_db_discovery_init function.
 * @retval NRF_ERROR_NO_MEM If the maximum number of allowed registrations has been reached.
 */
uint32_t ble_db_discovery_service_register(struct ble_db_discovery *db_discovery,
					   const ble_uuid_t *uuid);

/**
 * @brief Application's BLE Stack event handler.
 *
 * @param[in] ble_evt BLE event received.
 * @param[in,out] context BLE DB discovery instance.
 */
void ble_db_discovery_on_ble_evt(const ble_evt_t *ble_evt, void *context);

#endif /* BLE_DB_DISCOVERY_H__ */

/** @} */
