/**
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup ble_nus Bluetooth LE Nordic UART Service library
 * @{
 * @brief Library for handling UART over Bluetooth LE.
 */

#ifndef BLE_NUS_H__
#define BLE_NUS_H__

#include <stdint.h>
#include <stdbool.h>
#include <ble.h>
#include <bm/bluetooth/ble_common.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>

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
void ble_nus_on_ble_evt(const ble_evt_t *ble_evt, void *context);

/**
 * @brief Macro for defining a ble_nus instance.
 *
 * @param _name Name of the instance.
 * @hideinitializer
 */
#define BLE_NUS_DEF(_name)                                                                         \
	static struct ble_nus _name;                                                               \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_nus_on_ble_evt, &_name, HIGH)

/** @brief Default security configuration. */
#define BLE_NUS_CONFIG_SEC_MODE_DEFAULT                                                            \
	{                                                                                          \
		.nus_rx_char = {                                                                   \
			.read = BLE_GAP_CONN_SEC_MODE_OPEN,                                        \
			.write = BLE_GAP_CONN_SEC_MODE_OPEN,                                       \
		},                                                                                 \
		.nus_tx_char = {                                                                   \
			.read = BLE_GAP_CONN_SEC_MODE_OPEN,                                        \
			.cccd_write = BLE_GAP_CONN_SEC_MODE_OPEN,                                  \
		},                                                                                 \
	}

/**
 * @brief Macro for calculating maximum length of data (in bytes) that can be transmitted to
 *        the peer by the Nordic UART service module, given the ATT MTU size.
 */
#define BLE_NUS_MAX_DATA_LEN_CALC(mtu_size) ((mtu_size) - ATT_OPCODE_LEN - ATT_HANDLE_LEN)

/**
 * @brief Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART
 *        service module.
 */
#define BLE_NUS_MAX_DATA_LEN  BLE_NUS_MAX_DATA_LEN_CALC(CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE)

/** @brief Nordic UART Service event types. */
enum ble_nus_evt_type {
	/**
	 * @brief Data received.
	 */
	BLE_NUS_EVT_RX_DATA,
	/**
	 * @brief Service is ready to accept new data to be transmitted.
	 */
	BLE_NUS_EVT_TX_RDY,
	/**
	 * @brief Notification has been enabled.
	 */
	BLE_NUS_EVT_COMM_STARTED,
	/**
	 * @brief Notification has been disabled.
	 */
	BLE_NUS_EVT_COMM_STOPPED,
	/**
	 * @brief Error event.
	 */
	BLE_NUS_EVT_ERROR,
};

/**
 * @brief Nordic UART Service event structure.
 *
 * @details This structure is passed to an event coming from service.
 */
struct ble_nus_evt {
	/** Event type. */
	enum ble_nus_evt_type evt_type;
	/** Connection handle. */
	uint16_t conn_handle;
	union {
		/** @ref BLE_NUS_EVT_RX_DATA event data. */
		struct {
			/** Pointer to the buffer with received data. */
			const uint8_t *data;
			/** Length of received data. */
			uint16_t length;
		} rx_data;
		/** @ref BLE_BAS_EVT_ERROR event data. */
		struct {
			/** Error reason. */
			uint32_t reason;
		} error;
	};
};

struct ble_nus;

/** @brief Nordic UART Service event handler type. */
typedef void (*ble_nus_evt_handler_t)(struct ble_nus *nus, const struct ble_nus_evt *evt);

/**
 * @brief Nordic UART Service initialization structure.
 *
 * @details This structure contains the initialization information for the service. The application
 *          must fill this structure and pass it to the service using the @ref ble_nus_init
 *          function.
 */
struct ble_nus_config {
	/** Event handler to be called for handling received data. */
	ble_nus_evt_handler_t evt_handler;
	/** Security configuration. */
	struct {
		/** NUS Service RX characteristic */
		struct {
			/** Security requirement for reading NUS RX characteristic value. */
			ble_gap_conn_sec_mode_t read;
			/** Security requirement for writing NUS RX characteristic value. */
			ble_gap_conn_sec_mode_t write;
		} nus_rx_char;
		/** NUS Service TX characteristic */
		struct {
			/** Security requirement for reading NUS TX characteristic value. */
			ble_gap_conn_sec_mode_t read;
			/** Security requirement for writing NUS TX characteristic CCCD. */
			ble_gap_conn_sec_mode_t cccd_write;
		} nus_tx_char;
	} sec_mode;
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
	/** Event handler to be called for handling received data. */
	ble_nus_evt_handler_t evt_handler;
};

/**
 * @brief Function for initializing the Nordic UART Service.
 *
 * @param[out] nus Nordic UART Service structure. This structure must be supplied
 *                 by the application. It is initialized by this function and will
 *                 later be used to identify this particular service instance.
 * @param[in] nus_config Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @p nus or @p nus_config is @c NULL.
 * @return In addition, this function may return any error
 *         returned by the following SoftDevice functions:
 *         - @ref sd_ble_gatts_service_add()
 *         - @ref sd_ble_gatts_characteristic_add()
 *         - @ref sd_ble_uuid_vs_add()
 */
uint32_t ble_nus_init(struct ble_nus *nus, const struct ble_nus_config *nus_config);

/**
 * @brief Function for handling the Nordic UART Service's Bluetooth LE events.
 *
 * @note This function is registered automatically by @ref BLE_NUS_DEF
 *       and should not be called directly by the application.
 *
 * @param[in] ble_evt Event received from the SoftDevice.
 * @param[in] context Nordic UART Service structure.
 */
void ble_nus_on_ble_evt(const ble_evt_t *ble_evt, void *context);

/**
 * @brief Send data on the NUS TX characteristic as a notification.
 *
 * @details Sends data if the notification bit in the CCCD is set for @p conn_handle.
 *
 * @param[in] nus Pointer to the Nordic UART Service structure.
 * @param[in] data Data to be sent.
 * @param[in,out] length In: Length of the @p data. Out: Number of bytes sent.
 * @param[in] conn_handle Connection handle of the destination client.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @p nus, @p data, or @p length are @c NULL.
 * @retval NRF_ERROR_INVALID_STATE If the peer has not enabled TX notifications
 *         (CCCD not set for notifications).
 * @return In addition, this function may return any error
 *         returned by the following SoftDevice functions:
 *         - @ref sd_ble_gatts_value_get()
 *         - @ref sd_ble_gatts_hvx()
 */
uint32_t ble_nus_data_send(struct ble_nus *nus, uint8_t *data, uint16_t *length,
			   uint16_t conn_handle);

#ifdef __cplusplus
}
#endif

#endif /* BLE_NUS_H__ */

/** @} */
