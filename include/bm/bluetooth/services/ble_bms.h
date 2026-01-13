/*
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup ble_bms Bond Management Service
 * @{
 *
 * @brief Bond Management Service (BMS) module.
 *
 * @details This module implements the Bond Management Service (BMS).
 *          By writing to the Bond Management Control Point, the connected peer can request the
 *          deletion of bond information from the device.
 *          If authorization is configured, the application must supply an event handler for
 *          receiving Bond Management Service events. Using this handler, the service requests
 *          authorization when a procedure is requested by writing to the
 *          Bond Management Control Point.
 */

#ifndef BLE_BMS_H__
#define BLE_BMS_H__

#include <stdint.h>
#include <stdbool.h>
#include <ble.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/bluetooth/ble_qwr.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macro for defining a ble_bms instance.
 *
 * @param _name Name of the instance.
 * @hideinitializer
 */
#define BLE_BMS_DEF(_name)                                                                         \
	static struct ble_bms _name;                                                               \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_bms_on_ble_evt, &_name, HIGH)

/** Maximum length of the Bond Management Control Point Characteristic (in bytes). */
#define BLE_BMS_CTRLPT_MAX_LEN 128
/** Maximum length of the Bond Management Control Point Authorization Code (in bytes). */
#define BLE_BMS_AUTH_CODE_MAX_LEN BLE_BMS_CTRLPT_MAX_LEN - 1

/** @defgroup BLE_BMS_FEATURES BMS feature bits
 * @{
 */
/** Delete bond of the requesting device (BR/EDR and LE). */
#define BLE_BMS_REQUESTING_DEVICE_BR_LE (0x01 << 0)
/** Delete bond of the requesting device (BR/EDR and LE) with an authorization code. */
#define BLE_BMS_REQUESTING_DEVICE_BR_LE_AUTH_CODE (0x01 << 1)
/** Delete bond of the requesting device (BR/EDR transport only). */
#define BLE_BMS_REQUESTING_DEVICE_BR (0x01 << 2)
/** Delete bond of the requesting device (BR/EDR transport only) with an authorization code. */
#define BLE_BMS_REQUESTING_DEVICE_BR_AUTH_CODE (0x01 << 3)
/** Delete bond of the requesting device (LE transport only). */
#define BLE_BMS_REQUESTING_DEVICE_LE (0x01 << 4)
/** Delete bond of the requesting device (LE transport only) with an authorization code. */
#define BLE_BMS_REQUESTING_DEVICE_LE_AUTH_CODE (0x01 << 5)
/** Delete all bonds on the device (BR/EDR and LE). */
#define BLE_BMS_ALL_BONDS_BR_LE (0x01 << 6)
/** Delete all bonds on the device (BR/EDR and LE) with an authorization code. */
#define BLE_BMS_ALL_BONDS_BR_LE_AUTH_CODE (0x01 << 7)
/** Delete all bonds on the device (BR/EDR transport only). */
#define BLE_BMS_ALL_BONDS_BR (0x01 << 8)
/** Delete all bonds on the device (BR/EDR transport only) with an authorization code. */
#define BLE_BMS_ALL_BONDS_BR_AUTH_CODE (0x01 << 9)
/** Delete all bonds on the device (LE transport only). */
#define BLE_BMS_ALL_BONDS_LE (0x01 << 10)
/** Delete all bonds on the device (LE transport only) with an authorization code. */
#define BLE_BMS_ALL_BONDS_LE_AUTH_CODE (0x01 << 11)
/** Delete all bonds on the device except for the bond of the requesting device
 * (BR/EDR and LE).
 */
#define BLE_BMS_ALL_EXCEPT_REQUESTING_DEVICE_BR_LE (0x01 << 12)
/** Delete all bonds on the device except for the bond of the requesting device
 *  (BR/EDR and LE) with an authorization code.
 */
#define BLE_BMS_ALL_EXCEPT_REQUESTING_DEVICE_BR_LE_AUTH_CODE (0x01 << 13)
/** Delete all bonds on the device except for the bond of the requesting device
 *  (BR/EDR transport only).
 */
#define BLE_BMS_ALL_EXCEPT_REQUESTING_DEVICE_BR (0x01 << 14)
/** Delete all bonds on the device except for the bond of the requesting device
 *  (BR/EDR transport only) with an authorization code.
 */
#define BLE_BMS_ALL_EXCEPT_REQUESTING_DEVICE_BR_AUTH_CODE (0x01 << 15)
/** Delete all bonds on the device except for the bond of the requesting device
 *  (LE transport only).
 */
#define BLE_BMS_ALL_EXCEPT_REQUESTING_DEVICE_LE (0x01 << 16)
/** Delete all bonds on the device except for the bond of the requesting device
 *  (LE transport only) with an authorization code.
 */
#define BLE_BMS_ALL_EXCEPT_REQUESTING_DEVICE_LE_AUTH_CODE (0x01 << 17)
/** @} */

/** Error sent back when receiving a control point write with an unsupported opcode. */
#define BLE_BMS_OPCODE_NOT_SUPPORTED 0x80
/** Error sent back when a control point operation fails. */
#define BLE_BMS_OPERATION_FAILED 0x81

/** @brief Supported features. */
struct ble_bms_features {
	/** Indicates whether the application wants to support the operation to delete all bonds. */
	bool delete_all : 1;
	/** Indicates whether the application wants to support the operation to delete all bonds
	 *  with authorization code.
	 */
	bool delete_all_auth : 1;
	/** Indicates whether the application wants to support the operation to delete the bonds of
	 *  the requesting device.
	 */
	bool delete_requesting : 1;
	/** Indicates whether the application wants to support the operation to delete the bonds of
	 *  therequesting device with authorization code.
	 */
	bool delete_requesting_auth : 1;
	/** Indicates whether the application wants to support the operation to delete all bonds
	 *  except for the bond of the requesting device.
	 */
	bool delete_all_but_requesting : 1;
	/** Indicates whether the application wants to support the operation to delete all bonds
	 *  except for the bond of the requesting device with authorization code.
	 */
	bool delete_all_but_requesting_auth : 1;
};

/** @brief BMS Control Point opcodes. */
enum ble_bms_op {
	/** Initiates the procedure to delete the bond of the requesting device on
	 *  BR/EDR and LE transports.
	 */
	BLE_BMS_OP_DEL_BOND_REQ_DEVICE_BR_LE = 0x01,
	/** Initiates the procedure to delete the bond of the requesting device on BR/EDR transport.
	 */
	BLE_BMS_OP_DEL_BOND_REQ_DEVICE_BR_ONLY = 0x02,
	/** Initiates the procedure to delete the bond of the requesting device on LE transport. */
	BLE_BMS_OP_DEL_BOND_REQ_DEVICE_LE_ONLY = 0x03,
	/** Initiates the procedure to delete all bonds on the device on BR/EDR and LE transports.
	 */
	BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_BR_LE = 0x04,
	/** Initiates the procedure to delete all bonds on the device on BR/EDR transport. */
	BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_BR_ONLY = 0x05,
	/** Initiates the procedure to delete all bonds on the device on LE transport. */
	BLE_BMS_OP_DEL_ALL_BONDS_ON_SERVER_LE_ONLY = 0x06,
	/** Initiates the procedure to delete all bonds except for the one of the requesting device
	 *  on BR/EDR and LE transports.
	 */
	BLE_BMS_OP_DEL_ALL_BUT_ACTIVE_BOND_BR_LE = 0x07,
	/** Initiates the procedure to delete all bonds except for the one of the requesting device
	 *  on BR/EDR transport.
	 */
	BLE_BMS_OP_DEL_ALL_BUT_ACTIVE_BOND_BR_ONLY = 0x08,
	/** Initiates the procedure to delete all bonds except for the one of the requesting device
	 *  on LE transport.
	 */
	BLE_BMS_OP_DEL_ALL_BUT_ACTIVE_BOND_LE_ONLY = 0x09,
	/** Indicates an invalid opcode or no pending opcode. */
	BLE_BMS_OP_NONE = 0xFF
};

/** @brief Authorization status values. */
enum ble_bms_auth_status {
	/** Authorization is granted. */
	BLE_BMS_AUTH_STATUS_ALLOWED,
	/** Authorization is denied. */
	BLE_BMS_AUTH_STATUS_DENIED,
	/** Authorization is pending. */
	BLE_BMS_AUTH_STATUS_PENDING
};

/** @brief Received authorization codes. */
struct ble_bms_auth_code {
	/** Authorization code. */
	uint8_t code[BLE_BMS_AUTH_CODE_MAX_LEN];
	/** Length of the authorization code. */
	uint16_t len;
};

/** @brief BMS event types. */
enum ble_bms_evt_type {
	/** Error event. */
	BLE_BMS_EVT_ERROR,
	/** Event that indicates that the application shall verify the supplied
	 *  authentication code.
	 */
	BLE_BMS_EVT_AUTH,
	/** Request to delete the bond of the requesting device. */
	BLE_BMS_EVT_BOND_DELETE_REQUESTING,
	/** Request to delete all bonds. */
	BLE_BMS_EVT_BOND_DELETE_ALL,
	/** Request to delete all bonds except for the requesting device. */
	BLE_BMS_EVT_BOND_DELETE_ALL_EXCEPT_REQUESTING,
};

/** @brief BMS events. */
struct ble_bms_evt {
	/** Type of event. */
	enum ble_bms_evt_type evt_type;
	union {
		/** @ref BLE_BMS_EVT_ERROR event data. */
		struct {
			/* Error reason */
			uint32_t reason;
		} error;
		/** @ref BLE_BMS_EVT_AUTH event data. */
		struct {
			/** Received authorization code. */
			struct ble_bms_auth_code auth_code;
		} auth;
	};
};

/** @brief BMS control points. */
struct ble_bms_ctrlpt {
	/** Control Point Op Code. */
	enum ble_bms_op op_code;
	/** Control Point Authorization Code. */
	struct ble_bms_auth_code auth_code;
};

/* Forward declaration of the struct ble_bms. */
struct ble_bms;

/** @brief BMS event handler type.
 *
 * The event handler returns a @ref BLE_GATT_STATUS_CODES "BLE GATT status code".
 */
typedef void (*ble_bms_evt_handler_t)(struct ble_bms *bms, struct ble_bms_evt *evt);

/** @brief BMS initialization structure with all information needed to initialize the service. */
struct ble_bms_config {
	/** Event handler to be called for handling events in the Bond Management Service. */
	ble_bms_evt_handler_t evt_handler;
	/** Initial value for features of the service. */
	struct ble_bms_features feature;
	/** Initial security level for the Feature characteristic. */
	ble_gap_conn_sec_mode_t feature_sec;
	/** Initial security level for the Control Point characteristic. */
	ble_gap_conn_sec_mode_t ctrlpt_sec;
	/** Pointer to the initialized Queued Write contexts. */
	struct ble_qwr *qwr;
	/** Initialized Queue Write contexts count. */
	uint8_t qwr_count;
};

/**@brief Status information for the service. */
struct ble_bms {
	/** Handle of the Bond Management Service (as provided by the BLE stack). */
	uint16_t service_handle;
	/** Handle of the current connection (as provided by the BLE stack).
	 * @ref BLE_CONN_HANDLE_INVALID if not in a connection.
	 */
	uint16_t conn_handle;
	/** Event handler to be called for handling events in the Bond Management Service. */
	ble_bms_evt_handler_t evt_handler;
	/** Value for features of the service (see @ref BLE_BMS_FEATURES). */
	struct ble_bms_features feature;
	/** Handles related to the Bond Management Feature characteristic. */
	ble_gatts_char_handles_t feature_handles;
	/** Handles related to the Bond Management Control Point characteristic. */
	ble_gatts_char_handles_t ctrlpt_handles;
	/** Authorization status. */
	enum ble_bms_auth_status auth_status;
};

/**
 * @brief Respond to an authorization request.
 *
 * @details Call this function when receiving the @ref BLE_BMS_EVT_AUTH event to
 *          respond to the service with an authorization result.
 *
 * @param[in] bms BMS structure.
 * @param[in] authorize Authorization response. True if the authorization is considered successful.
 *
 * @retval NRF_ERROR_NULL If @p bms was NULL.
 * @retval NRF_ERROR_INVALID_STATE If no authorization request was pending.
 * @retval NRF_SUCCESS If the response was received successfully.
 */
uint32_t ble_bms_auth_response(struct ble_bms *bms, bool authorize);

/**
 * @brief Initialize the Bond Management Service.
 *
 * @param[out] bms BMS structure.
 * @param[in] bms_config Information needed to initialize the service.
 *
 * @retval NRF_ERROR_NULL If @p bms or @p bms_config was NULL.
 * @retval NRF_SUCCESS If the service was initialized successfully.
 *                     Otherwise, an error code is returned.
 */
uint32_t ble_bms_init(struct ble_bms *bms, struct ble_bms_config *bms_config);

/**
 * @brief   Handle Bond Management BLE stack events.
 *
 * @details This function handles all events from the BLE stack that are of interest to the
 *          Bond Management Service.
 *
 * @param[in] ble_evt Event received from the BLE stack.
 * @param[in] context BMS structure.
 */
void ble_bms_on_ble_evt(const ble_evt_t *ble_evt, void *context);

/**
 * @brief Handle events from the @ref nrf_ble_qwr.
 *
 * @param[in] bms BMS structure.
 * @param[in] qwr Queued Write structure.
 * @param[in] evt Event received from the Queued Writes module.
 *
 * @retval BLE_GATT_STATUS_SUCCESS If the received event is accepted.
 * @retval NRF_BLE_QWR_REJ_REQUEST_ERR_CODE If the received event is not relevant for any of this
 *         module's attributes.
 * @retval BLE_BMS_OPCODE_NOT_SUPPORTED If the received opcode is not supported.
 * @retval BLE_GATT_STATUS_ATTERR_INSUF_AUTHORIZATION If the application handler returns that the
 *         authorization code is not valid.
 */
uint16_t ble_bms_on_qwr_evt(struct ble_bms *bms, struct ble_qwr *qwr,
			    const struct ble_qwr_evt *evt);

#ifdef __cplusplus
}
#endif

#endif /* BLE_BMS_H__ */

/** @} */
