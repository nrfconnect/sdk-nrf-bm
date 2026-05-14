/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup ble_mds Bluetooth LE Memfault Diagnostic Service library
 * @{
 * @brief Library for exposing Memfault Diagnostic Service over Bluetooth LE.
 */

#ifndef BLE_MDS_H__
#define BLE_MDS_H__

#include <stdbool.h>
#include <stdint.h>

#include <ble.h>
#include <bm/bluetooth/ble_common.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Vendor specific UUID base for Memfault Diagnostic Service. */
#define BLE_MDS_UUID_BASE { 0x36, 0x84, 0xBD, 0x4E, 0x2F, 0x72, 0x71, 0xA3,                        \
			    0x07, 0x40, 0xA5, 0xF6, 0x00, 0x00, 0x22, 0x54 }

/** Memfault Diagnostic Service UUID. */
#define BLE_UUID_MDS_SERVICE 0x0000
/** Supported Features characteristic UUID. */
#define BLE_UUID_MDS_SUPPORTED_FEATURES_CHAR 0x0001
/** Device Identifier characteristic UUID. */
#define BLE_UUID_MDS_DEVICE_IDENTIFIER_CHAR 0x0002
/** Data URI characteristic UUID. */
#define BLE_UUID_MDS_DATA_URI_CHAR 0x0003
/** Authorization characteristic UUID. */
#define BLE_UUID_MDS_AUTHORIZATION_CHAR 0x0004
/** Data Export characteristic UUID. */
#define BLE_UUID_MDS_DATA_EXPORT_CHAR 0x0005

/**
 * @brief Bluetooth LE event handler for the Memfault Diagnostic Service.
 *
 * @note This handler is registered automatically by @ref BLE_MDS_DEF and is
 *       called by the SoftDevice handler. The application does not need to call
 *       it directly.
 *
 * @param[in] ble_evt Bluetooth LE stack event.
 * @param[in] context Pointer to the @ref ble_mds instance.
 */
void ble_mds_on_ble_evt(const ble_evt_t *ble_evt, void *context);

/**
 * @brief Macro for defining a ble_mds instance.
 *
 * @param _name Name of the instance.
 * @hideinitializer
 */
#define BLE_MDS_DEF(_name)                                                                         \
	static struct ble_mds _name;                                                               \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_mds_on_ble_evt, &_name, HIGH)

/** @brief Default security configuration. */
#define BLE_MDS_CONFIG_SEC_MODE_DEFAULT                                                            \
	{                                                                                          \
		.feature_char = {                                                                  \
			.read = BLE_GAP_CONN_SEC_MODE_OPEN,                                        \
		},                                                                                 \
		.device_id_char = {                                                                \
			.read = BLE_GAP_CONN_SEC_MODE_OPEN,                                        \
		},                                                                                 \
		.data_uri_char = {                                                                 \
			.read = BLE_GAP_CONN_SEC_MODE_OPEN,                                        \
		},                                                                                 \
		.auth_char = {                                                                     \
			.read = BLE_GAP_CONN_SEC_MODE_OPEN,                                        \
		},                                                                                 \
		.data_export_char = {                                                              \
			.write = BLE_GAP_CONN_SEC_MODE_OPEN,                                      \
			.cccd_write = BLE_GAP_CONN_SEC_MODE_OPEN,                                  \
		},                                                                                 \
	}

/**
 * @brief Memfault Diagnostic Service initialization configuration.
 */
struct ble_mds_config {
	/** Characteristic security configuration. */
	struct {
		/** Supported Features characteristic security. */
		struct {
			/**
			 * Security requirement for reading the Supported Features characteristic.
			 */
			ble_gap_conn_sec_mode_t read;
		} feature_char;
		/** Device Identifier characteristic security. */
		struct {
			/**
			 * Security requirement for reading the Device Identifier characteristic.
			 */
			ble_gap_conn_sec_mode_t read;
		} device_id_char;
		/** Data URI characteristic security. */
		struct {
			/** Security requirement for reading the Data URI characteristic. */
			ble_gap_conn_sec_mode_t read;
		} data_uri_char;
		/** Authorization characteristic security. */
		struct {
			/** Security requirement for reading the Authorization characteristic. */
			ble_gap_conn_sec_mode_t read;
		} auth_char;
		/** Data Export characteristic security. */
		struct {
			/** Security requirement for writing the Data Export characteristic. */
			ble_gap_conn_sec_mode_t write;
			/** Security requirement for writing the Data Export characteristic CCCD. */
			ble_gap_conn_sec_mode_t cccd_write;
		} data_export_char;
	} sec_mode;
};

/**
 * @brief Memfault Diagnostic Service instance.
 */
struct ble_mds {
	/** Vendor-specific UUID type assigned to @ref BLE_MDS_UUID_BASE. */
	uint8_t uuid_type;
	/** Service handle assigned by the Bluetooth LE stack. */
	uint16_t service_handle;
	/** Handles for the Supported Features characteristic. */
	ble_gatts_char_handles_t supported_features_handles;
	/** Handles for the Device Identifier characteristic. */
	ble_gatts_char_handles_t device_id_handles;
	/** Handles for the Data URI characteristic. */
	ble_gatts_char_handles_t data_uri_handles;
	/** Handles for the Authorization characteristic. */
	ble_gatts_char_handles_t auth_handles;
	/** Handles for the Data Export characteristic. */
	ble_gatts_char_handles_t data_export_handles;
	/** Connection handle for the active MDS subscriber. */
	uint16_t conn_handle;
	/** Whether the service instance has been initialized. */
	bool initialized;
	/** Whether a peer has enabled Data Export notifications. */
	bool subscriber_active;
	/** Whether the peer has enabled Memfault chunk streaming. */
	bool streaming_enabled;
	/** Whether notification transmission is blocked until a later stack event. */
	bool tx_blocked;
	/** Whether an HVN transaction is pending completion. */
	bool hvx_pending;
	/** Current MDS payload sequence number. */
	uint8_t seq_num;
	/** Next uptime, in milliseconds, when an empty packetizer should be polled. */
	int64_t next_empty_poll_ms;
	/** Next uptime, in milliseconds, when logs should be collected. */
	int64_t next_log_collection_ms;
	/** Pending Data Export notification payload. */
	uint8_t pending_payload[CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE - ATT_OPCODE_LEN -
				ATT_HANDLE_LEN];
	/** Length of the pending Data Export notification payload. */
	uint16_t pending_len;
};

/**
 * @brief Initialize the Memfault Diagnostic Service.
 *
 * @param[out] mds Memfault Diagnostic Service instance.
 * @param[in] cfg Memfault Diagnostic Service configuration.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @p mds or @p cfg is NULL.
 * @retval NRF_ERROR_NOT_FOUND If the Memfault device serial is not configured.
 * @retval NRF_ERROR_DATA_SIZE If a Memfault value does not fit in the local buffers.
 * @return In addition, this function may return any error returned by the following
 *         SoftDevice functions:
 *         - @ref sd_ble_uuid_vs_add()
 *         - @ref sd_ble_gatts_service_add()
 *         - @ref sd_ble_gatts_characteristic_add()
 */
uint32_t ble_mds_init(struct ble_mds *mds, const struct ble_mds_config *cfg);

/**
 * @brief Pump pending Memfault chunks to an active MDS subscriber.
 *
 * Call from the application main loop.
 *
 * @param[in,out] mds Memfault Diagnostic Service instance.
 */
void ble_mds_process(struct ble_mds *mds);

/**
 * @brief Get the SoftDevice UUID type assigned to the MDS UUID base.
 *
 * @param[in] mds Memfault Diagnostic Service instance.
 *
 * @return UUID type assigned to @ref BLE_MDS_UUID_BASE, or @ref BLE_UUID_TYPE_UNKNOWN
 *         if @p mds is NULL.
 */
uint8_t ble_mds_service_uuid_type(const struct ble_mds *mds);

#ifdef __cplusplus
}
#endif

#endif /* BLE_MDS_H__ */

/** @} */
