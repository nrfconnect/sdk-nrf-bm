/**
 * Copyright (c) 2013 - 2021, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/**@file
 *
 * @defgroup ble_db_discovery Database Discovery
 * @{
 * @ingroup ble_sdk_lib
 *
 * @brief Database discovery module.
 *
 * @details This module contains the APIs and types exposed by the DB Discovery module. These APIs
 *          and types can be used by the application to perform discovery of a service and its
 *          characteristics at the peer server. This module can also be used to discover the
 *          desired services in multiple remote devices.
 *
 * @warning The maximum number of characteristics per service that can be discovered by this module
 *          is determined by the number of characteristics in the service structure defined in
 *          db_disc_config.h. If the peer has more than the supported number of characteristics,
 * then the first found will be discovered and any further characteristics will be ignored. Only the
 *          following descriptors will be searched for at the peer: Client Characteristic
 * Configuration, Characteristic Extended Properties, Characteristic User Description, and Report
 * Reference.
 *
 * @note Presently only one instance of a Primary Service can be discovered by this module. If
 *       there are multiple instances of the service at the peer, only the first instance
 *       of it at the peer is fetched and returned to the application.
 *
 * @note The application must propagate BLE stack events to this module by calling
 *       ble_db_discovery_on_ble_evt().
 *
 */

#include <stdint.h>
#include <zephyr/kernel.h>
#include <ble_gattc.h>
#include <bm/bluetooth/ble_gq.h>
#define BLE_GATT_DB_MAX_CHARS	 6
#define BLE_DB_DISCOVERY_MAX_SRV 6

#ifndef BLE_DB_DISCOVERY_H__
#define BLE_DB_DISCOVERY_H__

/**@brief Macro for defining a ble_db_discovery instance.
 *
 * @param _name Name of the instance.
 * @hideinitializer
 */
#define BLE_DB_DISCOVERY_DEF(_name)                                                                \
	static struct ble_db_discovery _name = {.discovery_in_progress = 0,                        \
						.conn_handle = BLE_CONN_HANDLE_INVALID};           \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_db_discovery_on_ble_evt, &_name,                     \
			     BLE_DB_DISC_BLE_OBSERVER_PRIO)

struct ble_gatt_db_char {
	ble_gattc_char_t
		characteristic; /**< Structure containing information about the characteristic. */
	uint16_t cccd_handle;	/**< CCCD Handle value for this characteristic. This will be set to
				   BLE_GATT_HANDLE_INVALID if a CCCD is not present at the server. */
	uint16_t ext_prop_handle;   /**< Extended Properties Handle value for this characteristic.
				       This will be set to BLE_GATT_HANDLE_INVALID if an Extended
				       Properties descriptor is not present at the server. */
	uint16_t user_desc_handle;  /**< User Description Handle value for this characteristic. This
				       will be set to BLE_GATT_HANDLE_INVALID if a User Description
				       descriptor is not present at the server. */
	uint16_t report_ref_handle; /**< Report Reference Handle value for this characteristic. This
				       will be set to BLE_GATT_HANDLE_INVALID if a Report Reference
				       descriptor is not present at the server. */
};

struct ble_gatt_db_srv {
	ble_uuid_t srv_uuid; /**< UUID of the service. */
	uint8_t char_count;  /**< Number of characteristics present in the service. */
	ble_gattc_handle_range_t handle_range; /**< Service Handle Range. */
	struct ble_gatt_db_char
		charateristics[BLE_GATT_DB_MAX_CHARS]; /**< Array of information related to the
							  characteristics present in the service.
							  This list can extend further than one. */
};

enum ble_db_discovery_evt_type {
	BLE_DB_DISCOVERY_COMPLETE, /**< Event indicating that the discovery of one service is
				      complete. */
	BLE_DB_DISCOVERY_ERROR, /**< Event indicating that an internal error has occurred in the DB
				   Discovery module. This could typically be because of the
				   SoftDevice API returning an error code during the DB discover.*/
	BLE_DB_DISCOVERY_SRV_NOT_FOUND, /**< Event indicating that the service was not found at the
					   peer.*/
	BLE_DB_DISCOVERY_AVAILABLE	/**< Event indicating that the DB discovery instance is
					   available.*/
};

struct ble_db_discovery_evt {
	enum ble_db_discovery_evt_type evt_type; /**< Type of event. */
	uint16_t conn_handle; /**< Handle of the connection for which this event has occurred. */
	union {
		struct ble_gatt_db_srv
			discovered_db;	   /**< Structure containing the information about the GATT
					      Database at the server. This will be filled when the event
					      type is @ref BLE_DB_DISCOVERY_COMPLETE. The UUID field of
					      this will be filled when the event type is @ref
					      BLE_DB_DISCOVERY_SRV_NOT_FOUND. */
		void const *p_db_instance; /**< Pointer to DB discovery instance @ref
					      ble_db_discovery_t, indicating availability to the new
					      discovery process. This will be filled when the event
					      type is @ref BLE_DB_DISCOVERY_AVAILABLE. */
		uint32_t err_code; /**< nRF Error code indicating the type of error which occurred
				      in the DB Discovery module. This will be filled when the event
				      type is @ref BLE_DB_DISCOVERY_ERROR. */
	} params;
};

typedef void (*ble_db_discovery_evt_handler)(struct ble_db_discovery_evt *p_evt);

struct ble_db_discovery_init {
	ble_db_discovery_evt_handler
		evt_handler;	     /**< Event handler to be called by the DB Discovery module. */
	struct ble_gq *p_gatt_queue; /**< Pointer to BLE GATT Queue instance. */
};

struct ble_db_discovery_user_evt {
	struct ble_db_discovery_evt evt; /**< Pending event. */
	ble_db_discovery_evt_handler
		evt_handler; /**< Event handler which should be called to raise this event. */
};

struct ble_db_discovery {
	struct ble_gatt_db_srv
		services[BLE_DB_DISCOVERY_MAX_SRV]; /**< Information related to the current service
						       being discovered. This is intended for
						       internal use during service discovery.*/
	uint8_t srv_count;     /**< Number of services at the peer's GATT database.*/
	uint8_t curr_char_ind; /**< Index of the current characteristic being discovered. This is
				  intended for internal use during service discovery.*/
	uint8_t curr_srv_ind;  /**< Index of the current service being discovered. This is intended
				  for internal use during service discovery.*/
	uint8_t discoveries_count;  /**< Number of service discoveries made, both successful and
				       unsuccessful. */
	bool discovery_in_progress; /**< Variable to indicate whether there is a service discovery
				       in progress. */
	uint16_t conn_handle;	    /**< Connection handle on which the discovery is started. */
	uint32_t pending_usr_evt_index; /**< The index to the pending user event array, pointing to
					   the last added pending user event. */
	struct ble_db_discovery_user_evt pending_usr_evts
		[BLE_DB_DISCOVERY_MAX_SRV]; /**< Whenever a discovery related event is to be raised
					       to a user module, it is stored in this array first.
					       When all expected services have been discovered, all
					       pending events are sent to the corresponding user
					       modules. */
};

/**@brief Function for initializing the DB Discovery module.
 *
 * @param[in] p_db_init   Pointer to DB discovery initialization structure.
 *
 * @retval NRF_SUCCESS    On successful initialization.
 * @retval NRF_ERROR_NULL If the initialization structure was NULL or
 *                        the structure content is empty.
 */
uint32_t ble_db_discovery_init(struct ble_db_discovery_init *p_db_init);

/**@brief Function for closing the DB Discovery module.
 *
 * @details This function will clear up any internal variables and states maintained by the
 *          module. To re-use the module after calling this function, the function @ref
 *          ble_db_discovery_init must be called again. When using more than one DB Discovery
 *          instance, this function should be called for each instance.
 *
 * @param[out] p_db_discovery Pointer to the DB discovery structure.
 *
 * @retval NRF_SUCCESS Operation success.
 */
uint32_t ble_db_discovery_close(struct ble_db_discovery *const p_db_discovery);

/**@brief Function for registering with the DB Discovery module.
 *
 * @details The application can use this function to inform which service it is interested in
 *          discovering at the server.
 *
 * @param[in] p_uuid Pointer to the UUID of the service to be discovered at the server.
 *
 * @note The total number of services that can be discovered by this module is @ref
 *       BLE_DB_DISCOVERY_MAX_SRV. This effectively means that the maximum number of
 *       registrations possible is equal to the @ref BLE_DB_DISCOVERY_MAX_SRV.
 *
 * @retval NRF_SUCCESS             Operation success.
 * @retval NRF_ERROR_NULL          When a NULL pointer is passed as input.
 * @retval NRF_ERROR_INVALID_STATE If this function is called without calling the
 *                                 @ref ble_db_discovery_init.
 * @retval NRF_ERROR_NO_MEM        The maximum number of registrations allowed by this module
 *                                 has been reached.
 */
uint32_t ble_db_discovery_evt_register(const ble_uuid_t *const p_uuid);

/**@brief Function for starting the discovery of the GATT database at the server.
 *
 * @param[out] p_db_discovery Pointer to the DB Discovery structure.
 * @param[in]  conn_handle    The handle of the connection for which the discovery should be
 *                            started.
 *
 * @retval NRF_SUCCESS             Operation success.
 * @retval NRF_ERROR_NULL          When a NULL pointer is passed as input.
 * @retval NRF_ERROR_INVALID_STATE If this function is called without calling the
 *                                 @ref ble_db_discovery_init, or without calling
 *                                 @ref ble_db_discovery_evt_register.
 * @retval NRF_ERROR_BUSY          If a discovery is already in progress using
 *                                 @p p_db_discovery. Use a different @ref ble_db_discovery_t
 *                                 structure, or wait for a DB Discovery event before retrying.
 * @return                         This API propagates the error code returned by functions:
 *                                 @ref nrf_ble_gq_conn_handle_register and @ref
 * nrf_ble_gq_item_add.
 */
uint32_t ble_db_discovery_start(struct ble_db_discovery *p_db_discovery, uint16_t conn_handle);

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]     p_ble_evt Pointer to the BLE event received.
 * @param[in,out] p_context Pointer to the DB Discovery structure.
 */
void ble_db_discovery_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context);

#endif // BLE_DB_DISCOVERY_H__

/** @} */
