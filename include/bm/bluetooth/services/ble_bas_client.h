/*
 * Copyright (c) 2012 - 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/**
 * @file
 *
 * @defgroup ble_bas_client Battery Service Client
 * @{
 * @brief Battery Service Client.
 */

#ifndef BLE_BAS_CLIENT_H__
#define BLE_BAS_CLIENT_H__

#include <stdint.h>
#include <ble.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/ble_gq.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macro for defining a ble_bas_client instance.
 *
 * @param _name Name of the instance.
 * @hideinitializer
 */
#define BLE_BAS_CLIENT_DEF(_name)                                                                  \
	static struct ble_bas_client _name;                                                        \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_bas_client_on_ble_evt, &_name, HIGH)

/**
 * @brief Battery Service Client event type.
 */
enum ble_bas_client_evt_type {
	/**
	 * @brief Event indicating that the Battery Service was discovered at the peer.
	 */
	BLE_BAS_CLIENT_EVT_DISCOVERY_COMPLETE,
	/**
	 * @brief Event indicating that a notification of the Battery Level characteristic was
	 * received from the peer.
	 */
	BLE_BAS_CLIENT_EVT_BATT_NOTIFICATION,
	/**
	 * @brief Event indicating that a read response on Battery Level characteristic was received
	 * from the peer.
	 */
	BLE_BAS_CLIENT_EVT_BATT_READ_RESP,
	/**
	 * @brief Error event.
	 */
	BLE_BAS_CLIENT_EVT_ERROR,
};

/**
 * @brief Structure containing the handles related to the Battery Service found on the peer.
 */
struct ble_bas_client_db {
	/** Handle of the CCCD of the Battery Level characteristic. */
	uint16_t bl_cccd_handle;
	/** Handle of the Battery Level characteristic, as provided by the SoftDevice. */
	uint16_t bl_handle;
};

/**
 * @brief Battery Service Client Event structure.
 */
struct ble_bas_client_evt {
	/** Event Type. */
	enum ble_bas_client_evt_type evt_type;
	/** Connection handle relevant to this event. */
	uint16_t conn_handle;
	union {
		/** Handles related to the Battery Service, found on the peer device. This is filled
		 * if the evt_type is @ref BLE_BAS_CLIENT_EVT_DISCOVERY_COMPLETE.
		 */
		struct ble_bas_client_db bas_db;
		/** Information about the battery level, received from peer. This field is used for
		 * the events @ref BLE_BAS_CLIENT_EVT_BATT_NOTIFICATION and @ref
		 * BLE_BAS_CLIENT_EVT_BATT_READ_RESP.
		 */
		uint8_t battery_level;
		/** @ref BLE_BAS_CLIENT_EVT_ERROR event data. */
		struct {
			/** Error reason. */
			uint32_t reason;
		} error;
	};
};

/** Forward declaration of the ble_bas_client type. */
struct ble_bas_client;

/**
 * @brief   Event handler type.
 *
 * @details This is the type of the event handler that is to be provided by the application
 *          of the BAS module to receive events.
 */
typedef void (*ble_bas_client_evt_handler_t)(struct ble_bas_client *bas_client,
					     struct ble_bas_client_evt *evt);

/**
 * @brief   Battery Service Client structure.
 */
struct ble_bas_client {
	/** Connection handle, as provided by the SoftDevice. */
	uint16_t conn_handle;
	/** Handles related to BAS on the peer.*/
	struct ble_bas_client_db peer_bas_db;
	/** Application event handler to be called when there is an event related to the Battery
	 *  Service.
	 */
	ble_bas_client_evt_handler_t evt_handler;
	/** Pointer to the BLE GATT Queue instance. */
	const struct ble_gq *gatt_queue;
};

/**
 * @brief   Battery Service Client initialization structure.
 */
struct ble_bas_client_config {
	/** Event handler to be called by the Battery Service Client module when there is an event
	 *  related to the Battery Service.
	 */
	ble_bas_client_evt_handler_t evt_handler;
	/** Pointer to the BLE GATT Queue instance. */
	const struct ble_gq *gatt_queue;
	/** Pointer to BLE DB discovery instance. */
	struct ble_db_discovery *db_discovery;
};

/**
 * @brief      Initialize the Battery Service Client module.
 *
 * @details    This function initializes the module and sets up database discovery to discover
 *             the Battery Service. After calling this function, call @ref ble_db_discovery_start
 *             to start discovery once a link with a peer has been established.
 *
 * @param[out] ble_bas_client Pointer to the Battery Service Client structure.
 * @param[in]  ble_bas_config Pointer to the Battery Service initialization structure that
 *                            contains the initialization information.
 *
 * @retval     NRF_SUCCESS Operation success.
 * @retval     NRF_ERROR_NULL A parameter is NULL.
 * @retval	   err_code Otherwise, an error code returned by @ref
 *                      ble_db_discovery_service_register.
 */
uint32_t ble_bas_client_init(struct ble_bas_client *ble_bas_client,
			     struct ble_bas_client_config *ble_bas_config);

/**
 * @brief     Handle BLE events from the SoftDevice.
 *
 * @details   This function handles the BLE events received from the SoftDevice. If a BLE
 *            event is relevant to the Battery Service Client module, the function uses the event's
 *            data to update interval variables and, if necessary, send events to the application.
 *
 * @note This function must be called by the application.
 *
 * @param[in] ble_evt Pointer to the BLE event.
 * @param[in] context Pointer to the Battery Service client structure.
 */
void ble_bas_client_on_ble_evt(ble_evt_t const *ble_evt, void *context);

/**
 * @brief   Enable notifications on the Battery Level characteristic.
 *
 * @details This function enables the notification of the Battery Level characteristic at the
 *          peer by writing to the CCCD of the Battery Level Characteristic.
 *
 * @param   ble_bas_client Pointer to the Battery Service client structure.
 *
 * @retval  NRF_SUCCESS If the SoftDevice has been requested to write to the CCCD of the peer.
 * @retval  NRF_ERROR_NULL Parameter is NULL.
 * @retval  NRF_ERROR_INVALID_STATE If the connection handle is invalid.
 * @retval  err_code Otherwise, an error code returned by @ref ble_gq_item_add
 */
uint32_t ble_bas_client_bl_notif_enable(struct ble_bas_client *ble_bas_client);

/**
 * @brief   Read the Battery Level characteristic.
 *
 * @param   ble_bas_client Pointer to the Battery Service client structure.
 *
 * @retval  NRF_SUCCESS If the read request was successfully queued to be sent to peer.
 * @retval  NRF_ERROR_NULL Parameter is NULL.
 * @retval  NRF_ERROR_INVALID_STATE If the connection handle is invalid.
 *
 */
uint32_t ble_bas_client_bl_read(struct ble_bas_client *ble_bas_client);

/**
 * @brief     Handle events from the Database Discovery module.
 *
 * @details   Call this function when you get a callback event from the Database Discovery module.
 *            This function handles an event from the Database Discovery module, and determines
 *            whether it relates to the discovery of Battery Service at the peer. If it does, this
 *            function calls the application's event handler to indicate that the Battery Service
 *            was discovered at the peer. The function also populates the event with service-related
 *            information before providing it to the application.
 *
 * @param     ble_bas_client Pointer to the Battery Service client structure.
 * @param[in] evt Pointer to the event received from the Database Discovery module.
 *
 */
void ble_bas_on_db_disc_evt(struct ble_bas_client *ble_bas_client,
			    struct ble_db_discovery_evt const *evt);

/**
 * @brief     Assign handles to this instance of bas_c.
 *
 * @details   Call this function when a link has been established with a peer to
 *            associate the link to this instance of the module. This makes it
 *            possible to handle several links and associate each link to a particular
 *            instance of this module. The connection handle and attribute handles are
 *            provided from the discovery event @ref BLE_BAS_CLIENT_EVT_DISCOVERY_COMPLETE.
 *
 * @param[in] ble_bas_client  Pointer to the Battery client structure instance for associating
 *                            the link.
 * @param[in] conn_handle Connection handle associated with the given Battery Client Instance.
 * @param[in] peer_handles Attribute handles on the BAS server you want this BAS client to
 *                         interact with.
 *
 * @retval NRF_ERROR_NULL Parameter is NULL
 * @retval err_code Otherwise, an error code returned by @ref ble_gq_conn_handle_register
 *
 */
uint32_t ble_bas_client_handles_assign(struct ble_bas_client *ble_bas_client, uint16_t conn_handle,
				       struct ble_bas_client_db *peer_handles);

#ifdef __cplusplus
}
#endif

#endif /* BLE_BAS_CLIENT_H__ */

/** @} */
