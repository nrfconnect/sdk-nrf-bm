/*
 * Copyright (c) 2012 - 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 *
 * @defgroup ble_nus_client Nordic UART Service Client
 * @{
 * @ingroup  ble_sdk_srv
 * @brief    Nordic UART Service Client module.
 *
 * @details  This module contains the APIs and types exposed by the Nordic UART Service Client
 *           module. The application can use these APIs and types to perform the discovery of
 *           the Nordic UART Service at the peer and to interact with it.
 *
 * @note    The application must register this module as the BLE event observer by using the
 *          NRF_SDH_BLE_OBSERVER macro. Example:
 *          @code
 *              ble_nus_client instance;
 *              NRF_SDH_BLE_OBSERVER(anything, BLE_NUS_C_BLE_OBSERVER_PRIO,
 *                                   ble_nus_client_on_ble_evt, &instance);
 *          @endcode
 *
 */

#ifndef BLE_NUS_CLIENT_H__
#define BLE_NUS_CLIENT_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_gatt.h"
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/ble_gq.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Macro for defining a ble_nus_client instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_NUS_CLIENT_DEF(_name)                                                                  \
	static struct ble_nus_client _name;                                                        \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_nus_client_on_ble_evt, &_name, HIGH)

/** Used vendor-specific UUID. */
#define NUS_BASE_UUID                                                                              \
	{{0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x00, 0x00,      \
	  0x40, 0x6E}}

/** Byte 12 and 13 of the Nordic UART Service UUID. */
#define BLE_UUID_NUS_SERVICE	       0x0001
/** Byte 12 and 13 of the NUS RX Characteristic UUID. */
#define BLE_UUID_NUS_RX_CHARACTERISTIC 0x0002
/** Byte 12 and 13 of the NUS TX Characteristic UUID. */
#define BLE_UUID_NUS_TX_CHARACTERISTIC 0x0003

#define OPCODE_LENGTH 1
#define HANDLE_LENGTH 2

/**
 * @brief   Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART
 * service module.
 */
#ifndef BLE_NUS_MAX_DATA_LEN
#define BLE_NUS_MAX_DATA_LEN (CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE - OPCODE_LENGTH - HANDLE_LENGTH)
#endif

/**
 * @brief NUS Client event type.
 */
enum ble_nus_client_evt_type {
	/** Event indicating that the NUS service and its characteristics were found. */
	BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE,
	/** Event indicating that the client received something from a peer. */
	BLE_NUS_CLIENT_EVT_NUS_TX_EVT,
	/** Event indicating that the NUS server disconnected. */
	BLE_NUS_CLIENT_EVT_DISCONNECTED,
	/** Error. */
	BLE_NUS_CLIENT_EVT_ERROR
};

/**
 * @brief Handles on the connected peer device needed to interact with it.
 */
struct ble_nus_client_handles {
	/** Handle of the NUS TX characteristic, as provided by a discovery. */
	uint16_t nus_tx_handle;
	/** Handle of the CCCD of the NUS TX characteristic, as provided by a discovery. */
	uint16_t nus_tx_cccd_handle;
	/** Handle of the NUS RX characteristic, as provided by a discovery. */
	uint16_t nus_rx_handle;
};

/**
 * @brief Structure containing the NUS event data received from the peer.
 */
struct ble_nus_client_evt {
	/** Type of the event. */
	enum ble_nus_client_evt_type evt_type;
	/** Connection handle on which the NUS client service was discovered on the peer device. */
	uint16_t conn_handle;
	union {
		struct {
			/** Handles on which the Nordic UART service characteristics were discovered
			 * on the peer device. This is filled if the evt_type is @ref
			 * BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE.
			 */
			struct ble_nus_client_handles handles;
		} discovery_complete;
		struct {
			/** Data received with length. This is filled if the evt_type is @ref
			 * BLE_NUS_CLIENT_EVT_NUS_TX_EVT.
			 */
			uint8_t *data;
			uint16_t data_len;
		} nus_tx_evt;
		struct {
			/** Disconnection reason. This is filled if the evt_type is @ref
			 * BLE_NUS_CLIENT_EVT_DISCONNECTED.
			 */
			uint32_t reason;
		} disconnected;
		struct {
			/** Error reason */
			uint32_t reason;
		} error;
	} params;
};

/* Forward declaration of the ble_nus_client type. */
struct ble_nus_client;

/**
 * @brief   Event handler type.
 *
 * @details This is the type of the event handler that is to be provided by the application
 *          of this module to receive events.
 */
typedef void (*ble_nus_client_evt_handler_t)(struct ble_nus_client *ble_nus_c,
					     struct ble_nus_client_evt const *evt);

/**
 * @brief NUS Client structure.
 */
struct ble_nus_client {
	/** UUID type. */
	uint8_t uuid_type;
	/** Handle of the current connection. Set with @ref ble_nus_client_handles_assign when
	 * connected.
	 */
	uint16_t conn_handle;
	/** Handles on the connected peer device needed to interact with it. */
	struct ble_nus_client_handles handles;
	/** Application event handler to be called when there is an event related to the NUS. */
	ble_nus_client_evt_handler_t evt_handler;
	/** Pointer to BLE GATT Queue instance. */
	const struct ble_gq *gatt_queue;
};

/**
 * @brief NUS Client configuration structure.
 */
struct ble_nus_client_config {
	/** Application event handler to be called when there is an event related to the NUS. */
	ble_nus_client_evt_handler_t evt_handler;
	/** Pointer to BLE GATT Queue instance. */
	const struct ble_gq *gatt_queue;
	/** Pointer to BLE DB discovery instance. */
	struct ble_db_discovery *db_discovery;
};

/**
 * @brief     Function for initializing the Nordic UART client module.
 *
 * @details   This function registers with the Database Discovery module
 *            for the NUS. The Database Discovery module looks for the presence
 *            of a NUS instance at the peer when a discovery is started.
 *
 * @param[in] ble_nus_client        Pointer to the NUS client structure.
 * @param[in] ble_nus_client_config Pointer to the NUS configuration structure that contains the
 *                             configuration information.
 *
 * @retval    NRF_SUCCESS If the module was initialized successfully.
 * @retval    err_code    Otherwise, this function propagates the error code
 *                        returned by the Database Discovery module API
 *                        @ref ble_db_discovery_service_register.
 */
uint32_t ble_nus_client_init(struct ble_nus_client *ble_nus_client,
			     struct ble_nus_client_config *ble_nus_client_config);

/**
 * @brief Function for handling events from the Database Discovery module.
 *
 * @details This function handles an event from the Database Discovery module, and determines
 *          whether it relates to the discovery of NUS at the peer. If it does, the function
 *          calls the application's event handler to indicate that NUS was
 *          discovered at the peer. The function also populates the event with service-related
 *          information before providing it to the application.
 *
 * @param[in] ble_nus_client Pointer to the NUS client structure.
 * @param[in] evt       Pointer to the event received from the Database Discovery module.
 */
void ble_nus_client_on_db_disc_evt(struct ble_nus_client *ble_nus_client,
				   struct ble_db_discovery_evt *evt);

/**
 * @brief     Function for handling BLE events from the SoftDevice.
 *
 * @details   This function handles the BLE events received from the SoftDevice. If a BLE
 *            event is relevant to the NUS module, the function uses the event's data to update
 *            internal variables and, if necessary, send events to the application.
 *
 * @param[in] ble_evt     Pointer to the BLE event.
 * @param[in] context     Pointer to the NUS client structure.
 */
void ble_nus_client_on_ble_evt(ble_evt_t const *ble_evt, void *context);

/**
 * @brief   Function for requesting the peer to start sending notification of TX characteristic.
 *
 * @details This function enables notifications of the NUS TX characteristic at the peer
 *          by writing to the CCCD of the NUS TX characteristic.
 *
 * @param   ble_nus_client Pointer to the NUS client structure.
 *
 * @retval  NRF_SUCCESS If the operation was successful.
 * @retval  err_code	Otherwise, this API propagates the error code returned by function @ref
 * nrf_ble_gq_item_add.
 */
uint32_t ble_nus_client_tx_notif_enable(struct ble_nus_client *ble_nus_client);

/**
 * @brief Function for sending a string to the server.
 *
 * @details This function writes the RX characteristic of the server.
 *
 * @param[in] ble_nus_client Pointer to the NUS client structure.
 * @param[in] string         String to be sent.
 * @param[in] length         Length of the string.
 *
 * @retval NRF_SUCCESS If the string was sent successfully.
 * @retval err_code    Otherwise, this API propagates the error code returned by function @ref
 * ble_gq_item_add.
 */
uint32_t ble_nus_client_string_send(struct ble_nus_client *ble_nus_client, uint8_t *string,
				    uint16_t length);

/**
 * @brief Function for assigning handles to this instance of nus_c.
 *
 * @details Call this function when a link has been established with a peer to
 *          associate the link to this instance of the module. This makes it
 *          possible to handle several links and associate each link to a particular
 *          instance of this module. The connection handle and attribute handles are
 *          provided from the discovery event @ref BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE.
 *
 * @param[in] ble_nus_client Pointer to the NUS client structure instance to associate with these
 *                           handles.
 * @param[in] conn_handle    Connection handle to associated with the given NUS Instance.
 * @param[in] peer_handles   Attribute handles on the NUS server that you want this NUS client to
 *                           interact with.
 *
 * @retval    NRF_SUCCESS    If the operation was successful.
 * @retval    NRF_ERROR_NULL If a nus was a NULL pointer.
 * @retval    err_code       Otherwise, this API propagates the error code returned
 *                           by function @ref nrf_ble_gq_item_add.
 */
uint32_t ble_nus_client_handles_assign(struct ble_nus_client *ble_nus_client, uint16_t conn_handle,
				       struct ble_nus_client_handles const *peer_handles);

#ifdef __cplusplus
}
#endif

#endif /** BLE_NUS_CLIENT_H__ */

/** @} */
