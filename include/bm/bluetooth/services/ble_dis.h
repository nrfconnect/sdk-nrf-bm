/*
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/** @file
 *
 * @defgroup ble_dis Device Information Service
 * @{
 * @brief Device Information Service module.
 *
 * @details This module implements the Device Information Service.
 *          During initialization it adds the Device Information Service to the Bluetooth LE stack
 *          database. It then encodes the supplied information, and adds the corresponding
 *          characteristics.
 */

#ifndef BLE_DIS_H__
#define BLE_DIS_H__

#include <stdint.h>
#include <zephyr/sys/util.h>
#include <bm/bluetooth/ble_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Default security configuration. */
#define BLE_DIS_CONFIG_SEC_MODE_DEFAULT                                                            \
	{                                                                                          \
		.device_info_char.read = BLE_GAP_CONN_SEC_MODE_OPEN,                               \
	}

/** @brief Default characteristic values, populated from the @c CONFIG_BLE_DIS_* Kconfig options. */
#define BLE_DIS_VALUES_DEFAULTS                                                                    \
	{                                                                                          \
		.manufacturer_name = CONFIG_BLE_DIS_MANUFACTURER_NAME,                             \
		.model_number      = CONFIG_BLE_DIS_MODEL_NUMBER,                                  \
		.serial_number     = CONFIG_BLE_DIS_SERIAL_NUMBER,                                 \
		.hw_revision       = CONFIG_BLE_DIS_HW_REVISION,                                   \
		.fw_revision       = CONFIG_BLE_DIS_FW_REVISION,                                   \
		.sw_revision       = CONFIG_BLE_DIS_SW_REVISION,                                   \
		IF_ENABLED(CONFIG_BLE_DIS_SYSTEM_ID, (                                             \
			.sys_id = &(const struct ble_dis_sys_id){                                  \
				.manufacturer_id            = CONFIG_BLE_DIS_SYSTEM_ID_MID,        \
				.organizationally_unique_id = CONFIG_BLE_DIS_SYSTEM_ID_OUI,        \
			},))                                                                       \
		IF_ENABLED(CONFIG_BLE_DIS_PNP_ID, (                                                \
			.pnp_id = &(const struct ble_dis_pnp_id){                                  \
				.vendor_id_source = CONFIG_BLE_DIS_PNP_VID_SRC,                    \
				.vendor_id        = CONFIG_BLE_DIS_PNP_VID,                        \
				.product_id       = CONFIG_BLE_DIS_PNP_PID,                        \
				.product_version  = CONFIG_BLE_DIS_PNP_VER,                        \
			},))                                                                       \
		IF_ENABLED(CONFIG_BLE_DIS_REGULATORY_CERT, (                                       \
			.reg_cert = &(const struct ble_dis_reg_cert){                              \
				.list = (const uint8_t[8])                                         \
					sys_uint64_to_array(CONFIG_BLE_DIS_REGULATORY_CERT_LIST),  \
				.len  = 8,                                                         \
			},))                                                                       \
	}

/**
 * @name PnP ID Vendor ID Source values
 * @{
 */
/** Vendor ID assigned by the Bluetooth SIG. */
#define BLE_DIS_VENDOR_ID_SRC_BLUETOOTH_SIG 1
/** Vendor ID assigned by the USB Implementer's Forum. */
#define BLE_DIS_VENDOR_ID_SRC_USB_IMPL_FORUM 2
/** @} */

/** @brief System ID characteristic value. */
struct ble_dis_sys_id {
	/** Manufacturer ID. Only the 5 least significant octets are used. */
	uint64_t manufacturer_id;
	/** Organizationally unique ID. Only the 3 least significant octets are used. */
	uint32_t organizationally_unique_id;
};

/** @brief PnP ID characteristic value. */
struct ble_dis_pnp_id {
	/** Vendor ID source. See @ref BLE_DIS_VENDOR_ID_SRC_BLUETOOTH_SIG. */
	uint8_t vendor_id_source;
	/** Vendor ID. */
	uint16_t vendor_id;
	/** Product ID. */
	uint16_t product_id;
	/** Product version. */
	uint16_t product_version;
};

/** @brief IEEE 11073-20601 Regulatory Certification Data List characteristic value. */
struct ble_dis_reg_cert {
	/** Encoded opaque structure based on the IEEE 11073-20601 specification. */
	const uint8_t *list;
	/** Length of @ref ble_dis_reg_cert.list. */
	uint16_t len;
};

/** @brief Device Information Service characteristic values. */
struct ble_dis_values {
	/** Manufacturer Name String. @c NULL or empty omits the characteristic. */
	const char *manufacturer_name;
	/** Model Number String. @c NULL or empty omits the characteristic. */
	const char *model_number;
	/** Serial Number String. @c NULL or empty omits the characteristic. */
	const char *serial_number;
	/** Hardware Revision String. @c NULL or empty omits the characteristic. */
	const char *hw_revision;
	/** Firmware Revision String. @c NULL or empty omits the characteristic. */
	const char *fw_revision;
	/** Software Revision String. @c NULL or empty omits the characteristic. */
	const char *sw_revision;

	/** System ID. @c NULL omits the characteristic. */
	const struct ble_dis_sys_id *sys_id;
	/** Plug and Play ID. @c NULL omits the characteristic. */
	const struct ble_dis_pnp_id *pnp_id;
	/** IEEE 11073-20601 Regulatory Certification Data List. @c NULL omits characteristic. */
	const struct ble_dis_reg_cert *reg_cert;
};

/** @brief Device Information Service configuration. */
struct ble_dis_config {
	/** Security configuration. */
	struct {
		/** Device information characteristic */
		struct {
			/** Security requirement for reading all characteristic values of the
			 * device information service.
			 */
			ble_gap_conn_sec_mode_t read;
		} device_info_char;
	} sec_mode;

	/** Characteristic values. @c NULL builds the service from the Kconfig defaults. */
	const struct ble_dis_values *values;
};

/**
 * @brief Initialize the Device Information Service.
 *
 * @details This call allows the application to initialize the device information service.
 *          It adds the DIS service and DIS characteristics to the database.
 *
 * @param[in] dis_config Device Information Service configuration.
 *                       If @p dis_config is @c NULL the function returns @ref NRF_ERROR_NULL.
 *                       If @ref ble_dis_config::values is @c NULL the values are set based on
 *                       the @c CONFIG_BLE_DIS_* Kconfig options.
 *                       If a member of @ref ble_dis_values is left @c NULL (or empty, for strings)
 *                       the characteristic is omitted.
 *
 * @retval NRF_SUCCESS On success.
 * @retval NRF_ERROR_NULL If @p dis_config is @c NULL.
 * @return In addition, this function may return any error
 *         returned by the following SoftDevice functions:
 *         - @ref sd_ble_gatts_service_add()
 *         - @ref sd_ble_gatts_characteristic_add()
 */
uint32_t ble_dis_init(const struct ble_dis_config *dis_config);

#ifdef __cplusplus
}
#endif

#endif /* BLE_DIS_H__ */

/** @} */
