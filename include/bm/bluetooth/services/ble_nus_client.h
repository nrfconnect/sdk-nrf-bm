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
 * @brief Nordic UART Service Client service.
 */

#ifndef BLE_NUS_CLIENT_H__
#define BLE_NUS_CLIENT_H__

#include <stdint.h>
#include <bm/bluetooth/ble_common.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/ble_gq.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macro for defining a ble_nus_client instance.
 *
 * @param _name Name of the instance.
 */
#define BLE_NUS_CLIENT_DEF(_name)                                                                  \
	static struct ble_nus_client _name;                                                        \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_nus_client_on_ble_evt, &_name, HIGH)

/** Vendor-specific UUID base for the Nordic UART Service. */
#define BLE_NUS_UUID_BASE { 0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,                        \
			    0x93, 0xF3, 0xA3, 0xB5, 0x00, 0x00, 0x40, 0x6E }

/** Byte 12 and 13 of the Nordic UART Service UUID. */
#define BLE_UUID_NUS_SERVICE 0x0001
/** Byte 12 and 13 of the NUS RX Characteristic UUID. */
#define BLE_UUID_NUS_RX_CHARACTERISTIC 0x0002
/** Byte 12 and 13 of the NUS TX Characteristic UUID. */
#define BLE_UUID_NUS_TX_CHARACTERISTIC 0x0003

/**
 * @brief Macro for calculating the maximum length of data (in bytes) that can be transmitted to
 *        the peer by the Nordic UART service, given the ATT MTU size.
 */
#define BLE_NUS_CLIENT_MAX_DATA_LEN_CALC(mtu_size) ((mtu_size) - ATT_OPCODE_LEN - ATT_HANDLE_LEN)

/**
 * @brief Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART
 * service.
 */
#ifndef BLE_NUS_CLIENT_MAX_DATA_LEN
#define BLE_NUS_CLIENT_MAX_DATA_LEN                                                                \
	(BLE_NUS_CLIENT_MAX_DATA_LEN_CALC(CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE))
#endif

/**
 * @brief NUS Client event type.
 */
enum ble_nus_client_evt_type {
	/** Event indicating that the NUS service and its characteristics were found. */
	BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE,
	/** Event indicating that the client received something from a peer. */
	BLE_NUS_CLIENT_EVT_TX_DATA,
	/** Event indicating that the NUS server disconnected. */
	BLE_NUS_CLIENT_EVT_DISCONNECTED,
	/** Error. */
	BLE_NUS_CLIENT_EVT_ERROR
};

/**
 * @brief Handles of the connected peer device, as provided by a discovery.
 */
struct ble_nus_client_handles {
	/** Handle of the NUS TX characteristic. */
	uint16_t nus_tx_handle;
	/** Handle of the CCCD of the NUS TX characteristic. */
	uint16_t nus_tx_cccd_handle;
	/** Handle of the NUS RX characteristic. */
	uint16_t nus_rx_handle;
};

/**
 * @brief Nordic UART Service Client event structure.
 *
 * @details This structure is passed to an event coming from service.
 */
struct ble_nus_client_evt {
	/** Event type */
	enum ble_nus_client_evt_type evt_type;
	/** Connection handle. */
	uint16_t conn_handle;
	union {
		/** @ref BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE event data. */
		struct {
			/** Nordic UART service characteristics handles. */
			struct ble_nus_client_handles handles;
		} discovery_complete;
		/** @ref BLE_NUS_CLIENT_EVT_TX_DATA event data. */
		struct {
			/** Data received. */
			uint8_t *data;
			/** Length of data received. */
			uint16_t length;
		} tx_data;
		/** @ref BLE_NUS_CLIENT_EVT_DISCONNECTED event data. */
		struct {
			/** Disconnection reason. */
			uint32_t reason;
		} disconnected;
		/** @ref BLE_BAS_EVT_ERROR event data. */
		struct {
			/** Error reason */
			uint32_t reason;
		} error;
	};
};

/* Forward declaration of the ble_nus_client type. */
struct ble_nus_client;

/** @brief Nordic UART Service Client event handler type. */
typedef void (*ble_nus_client_evt_handler_t)(struct ble_nus_client *ble_nus_c,
					     const struct ble_nus_client_evt *evt);

/*
 * @brief Nordic UART Service Client configuration structure.
 *
 * @details This structure contains the initialization information for the service. The application
 *          must fill this structure and pass it to the service using the @ref ble_nus_client_init
 *          function.
 */
struct ble_nus_client_config {
	/** Application event handler to be called on a NUS Client event. */
	ble_nus_client_evt_handler_t evt_handler;
	/** Bluetooth LE GATT Queue instance. */
	const struct ble_gq *gatt_queue;
	/** Bluetooth LE DB discovery instance. */
	struct ble_db_discovery *db_discovery;
};

/**
 * @brief NUS Client structure.
 */
struct ble_nus_client {
	/** UUID type, see @ref BLE_UUID_TYPES. */
	uint8_t uuid_type;
	/** Handle of the current connection. Set with @ref ble_nus_client_handles_assign when
	 * connected.
	 */
	uint16_t conn_handle;
	/** Handles on the connected peer device needed to interact with it. */
	struct ble_nus_client_handles handles;
	/** Application event handler to be called on a NUS Client event. */
	ble_nus_client_evt_handler_t evt_handler;
	/** Bluetooth LE GATT Queue instance. */
	const struct ble_gq *gatt_queue;
};

/**
 * @brief Initialize the Nordic UART client service.
 *
 * @details This function registers the NUS Client with the Database Discovery library.
 *
 * @param[in] nus_client NUS client structure.
 * @param[in] nus_client_config NUS configuration structure.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @c nus_client, @c nus_client_config or
 *                        @c nus_client_config.gatt_queue is NULL.
 * @return In addition, this function may return any error returned by the following functions:
 *         - @ref ble_db_discovery_service_register
 *         - @ref sd_ble_uuid_vs_add.
 */
uint32_t ble_nus_client_init(struct ble_nus_client *nus_client,
			     const struct ble_nus_client_config *nus_client_config);

/**
 * @brief Handle events from the Database Discovery library.
 *
 * @details This function must be called from the application on a Database Discovery event.
 *          It handles the event from the Database Discovery library, and determines
 *          whether it relates to the discovery of NUS at the peer. If it does, the function
 *          calls the application's event handler to indicate that NUS was
 *          discovered at the peer. The function also populates the event with service-related
 *          information before providing it to the application.
 *
 * @param[in] nus_client NUS client structure.
 * @param[in] evt Event received from the Database Discovery library.
 */
void ble_nus_client_on_db_disc_evt(struct ble_nus_client *nus_client,
				   const struct ble_db_discovery_evt *evt);

/**
 * @brief Bluetooth LE event handler for the Nordic UART Service Client.
 *
 * @details Handles all Bluetooth LE stack events that are of interest to the
 *          Nordic UART Service Client.
 *
 * @note This handler is registered automatically by @ref BLE_NUS_CLIENT_DEF and
 *       is called by the SoftDevice handler. The application does not need to
 *       call it directly.
 *
 * @param[in] ble_evt    Bluetooth LE stack event.
 * @param[in] nus_client Pointer to the @ref ble_nus_client instance.
 */
void ble_nus_client_on_ble_evt(const ble_evt_t *ble_evt, void *nus_client);

/**
 * @brief Request the peer to start sending notification of TX characteristic.
 *
 * @details This function enables notifications of the NUS TX characteristic at the peer
 *          by writing to the CCCD of the NUS TX characteristic.
 *
 * @param nus_client NUS client structure.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @c nus_client is NULL.
 * @return In addition, this function may return any error returned by the following functions:
 *        - @ref nrf_ble_gq_item_add()
 */
uint32_t ble_nus_client_tx_notif_enable(struct ble_nus_client *nus_client);

/**
 * @brief Request the peer to stop sending notification of TX characteristic.
 *
 * @details This function disables notification of the NUS TX characteristic at the peer
 *          by writing to the CCCD of the NUS TX characteristic.
 *
 * @param[in] nus_client NUS client structure.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @c nus_client is NULL.
 * @return In addition, this function may return any error returned by the following functions:
 *        - @ref nrf_ble_gq_item_add()
 */
uint32_t ble_nus_client_tx_notif_disable(struct ble_nus_client *nus_client);

/**
 * @brief Send a string to the server.
 *
 * @details This function writes the RX characteristic of the server.
 *
 * @param[in] nus_client NUS client structure.
 * @param[in] string String to be sent.
 * @param[in] length Length of the string.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @c nus_client is NULL.
 * @retval NRF_ERROR_INVALID_PARAM If length is larger than the client maximum data length.
 * @retval NRF_ERROR_INVALID_STATE If the NUS Client connection handle is invalid.
 * @return In addition, this function may return any error returned by the following functions:
 *        - @ref ble_gq_item_add()
 */
uint32_t ble_nus_client_string_send(struct ble_nus_client *nus_client, const uint8_t *string,
				    uint16_t length);

/**
 * @brief Assign handles to this instance of the service.
 *
 * @details This function must be called by the application when a link has been established with a
 *          peer to associate the link to this instance of the service. This makes it possible to
 *          handle several links and associate each link to a particular instance of this service.
 *          The connection handle and attribute handles are provided from the discovery event
 *          @ref BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE.
 *
 * @param[in] nus_client NUS client structure instance to associate with the handles.
 * @param[in] conn_handle Connection handle to associated with the given NUS Instance.
 * @param[in] peer_handles Attribute handles on the NUS server to interact with.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @c nus_client or @c peer_handles is NULL.
 * @return In addition, this function may return any error returned by the following functions:
 *         - @ref nrf_ble_gq_item_add()
 */
uint32_t ble_nus_client_handles_assign(struct ble_nus_client *nus_client, uint16_t conn_handle,
				      const struct ble_nus_client_handles *peer_handles);

#ifdef __cplusplus
}
#endif

#endif /** BLE_NUS_CLIENT_H__ */

/** @} */
