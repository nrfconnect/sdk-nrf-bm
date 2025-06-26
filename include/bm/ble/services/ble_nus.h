/**
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup ble_nus BLE Nordic UART Service library
 * @{
 * @brief Library for handling UART over BLE.
 */

#ifndef BLE_NUS_H__
#define BLE_NUS_H__

#include <stdint.h>
#include <stdbool.h>
#include <softdevice/ble.h>
#include <bm/sdh/nrf_sdh_ble.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Vendor specific UUID base for the Nordic UART Service. */
#define BLE_NUS_UUID_BASE { 0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,                        \
			    0x93, 0xF3, 0xA3, 0xB5, 0x00, 0x00, 0x40, 0x6E }

/** Byte 12 and 13 of the Nordic UART Service UUID. */
#define BLE_UUID_NUS_SERVICE 0x0001
/** Byte 12 and 13 of the NUS RX Characteristic UUID. */
#define BLE_UUID_NUS_RX_CHARACTERISTIC 0x0002
/** Byte 12 and 13 of the NUS TX Characteristic UUID. */
#define BLE_UUID_NUS_TX_CHARACTERISTIC 0x0003

/* Forward declaration */
void ble_nus_on_ble_evt(ble_evt_t const *ble_evt, void *context);

/**
 * @brief Macro for defining a ble_nus instance.
 *
 * @param _name Name of the instance.
 * @hideinitializer
 */
#define BLE_NUS_DEF(_name)                                                                         \
	static struct ble_nus _name;                                                               \
	NRF_SDH_BLE_OBSERVER(_name ## _obs,                                                        \
			     ble_nus_on_ble_evt,                                                   \
			     &_name,                                                               \
			     0)

#define OPCODE_LENGTH 1
#define HANDLE_LENGTH 2

/**
 * @brief Macro for calculating maximum length of data (in bytes) that can be transmitted to
 *        the peer by the Nordic UART service module, given the ATT MTU size.
 */
#define BLE_NUS_MAX_DATA_LEN_CALC(mtu_size) ((mtu_size) - OPCODE_LENGTH - HANDLE_LENGTH)

/**
 * @brief Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART
 *        service module.
 */
#define BLE_NUS_MAX_DATA_LEN  BLE_NUS_MAX_DATA_LEN_CALC(CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE)

/** @brief Nordic UART Service event types. */
enum ble_nus_evt_type {
	/** Data received. */
	BLE_NUS_EVT_RX_DATA,
	/** Service is ready to accept new data to be transmitted. */
	BLE_NUS_EVT_TX_RDY,
	/** Notification has been enabled. */
	BLE_NUS_EVT_COMM_STARTED,
	/** Notification has been disabled. */
	BLE_NUS_EVT_COMM_STOPPED,
};

/**
 * @brief Nordic UART Service @ref BLE_NUS_EVT_RX_DATA event data.
 *
 * @details This structure is passed to an event when @ref BLE_NUS_EVT_RX_DATA occurs.
 */
struct ble_nus_evt_rx_data {
	/** Pointer to the buffer with received data. */
	uint8_t const *data;
	/** Length of received data. */
	uint16_t length;
};

/**
 * @brief Nordic UART Service client context structure.
 *
 * @details This structure contains state context related to hosts.
 */
struct ble_nus_client_context {
	/** Indicate if the peer has enabled notification of the RX characteristic. */
	bool is_notification_enabled;
};

/**
 * @brief Nordic UART Service event structure.
 *
 * @details This structure is passed to an event coming from service.
 */
struct ble_nus_evt {
	/** Event type. */
	enum ble_nus_evt_type type;
	/** Pointer to the instance. */
	struct ble_nus *nus;
	/** Connection handle. */
	uint16_t conn_handle;
	/** Pointer to the link context. */
	struct ble_nus_client_context *link_ctx;
	union {
		/** @ref BLE_NUS_EVT_RX_DATA event data. */
		struct ble_nus_evt_rx_data rx_data;
	} params;
};

/** @brief Nordic UART Service event handler type. */
typedef void (*ble_nus_evt_handler_t) (const struct ble_nus_evt *evt);

/*
 * @brief Nordic UART Service initialization structure.
 *
 * @details This structure contains the initialization information for the service. The application
 *          must fill this structure and pass it to the service using the @ref ble_nus_init
 *          function.
 */
struct ble_nus_config {
	/** Event handler to be called for handling received data. */
	ble_nus_evt_handler_t evt_handler;
};

/**
 * @brief Nordic UART Service structure.
 *
 * @details This structure contains status information related to the service.
 */
struct ble_nus {
	/** UUID type for Nordic UART Service Base UUID. */
	uint8_t uuid_type;
	/** Handle of Nordic UART Service (as provided by the SoftDevice). */
	uint16_t service_handle;
	/** Handles related to the TX characteristic (as provided by the SoftDevice). */
	ble_gatts_char_handles_t tx_handles;
	/** Handles related to the RX characteristic (as provided by the SoftDevice). */
	ble_gatts_char_handles_t rx_handles;
	/** Link context with handles of all current connections and its context. */
	struct ble_nus_ctx *const ctx;
	/** Event handler to be called for handling received data. */
	ble_nus_evt_handler_t evt_handler;
};

/**
 * @brief Function for initializing the Nordic UART Service.
 *
 * @param[out] nus Nordic UART Service structure. This structure must be supplied
 *                 by the application. It is initialized by this function and will
 *                 later be used to identify this particular service instance.
 * @param[in] nus_init Information needed to initialize the service.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p nus or @p nus_config is @c NULL.
 * @retval -EINVAL Invalid parameters.
 */
int ble_nus_init(struct ble_nus *nus, struct ble_nus_config const *nus_config);

/**
 * @brief Function for handling the Nordic UART Service's BLE events.
 *
 * @details The Nordic UART Service expects the application to call this function each time an
 *          event is received from the SoftDevice. This function processes the event if it
 *          is relevant and calls the Nordic UART Service event handler of the
 *          application if necessary.
 *
 * @param[in] ble_evt Event received from the SoftDevice.
 * @param[in] context Nordic UART Service structure.
 */
void ble_nus_on_ble_evt(ble_evt_t const *ble_evt, void *context);

/**
 * @brief Function for sending data to the peer.
 *
 * @details This function sends the input string as an RX characteristic notification to the
 *          peer.
 *
 * @param[in] nus Pointer to the Nordic UART Service structure.
 * @param[in] data String to be sent.
 * @param[in,out] length In: Length of the @p data string. Out: Number of bytes sent.
 * @param[in] conn_handle Connection handle of the destination client.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p nus, @p data or @p length is @c NULL.
 * @retval -EINVAL Invalid parameters.
 * @retval -ENOENT Invalid @p conn_handle.
 * @retval -EIO Failed to send notification.
 * @retval -ENOTCONN Connection handle unknown to the softdevice.
 * @retval -EPIPE Notifications not enabled in the CCCD.
 * @retval -EBADF Not found.
 * @retval -EAGAIN Not enough resources for operation.
 */
int ble_nus_data_send(struct ble_nus *nus, uint8_t *data, uint16_t *length, uint16_t conn_handle);

#ifdef __cplusplus
}
#endif

#endif /* BLE_NUS_H__ */

/** @} */
