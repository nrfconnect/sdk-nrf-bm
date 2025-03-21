/*
 * Copyright (c) 2012 - 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup ble_sdk_lib_advdata Advertising and Scan Response Data Encoder
 * @{
 */

#ifndef BLE_ADV_DATA_H__
#define BLE_ADV_DATA_H__

#include <stdint.h>
#include <stdbool.h>
#include <ble.h>
#include <ble_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advertising data LE Role types.
 */
enum ble_adv_data_le_role {
	/**
	 * @brief  LE Role AD structure not present.
	 */
	BLE_ADV_DATA_ROLE_NOT_PRESENT,
	/**
	 * @brief Only Peripheral Role supported.
	 */
	BLE_ADV_DATA_ROLE_ONLY_PERIPH,
	/**
	 * @brief Only Central Role supported
	 */
	BLE_ADV_DATA_ROLE_ONLY_CENTRAL,
	/**
	 * @brief Peripheral and Central Role supported, peripheral is preferred.
	 */
	BLE_ADV_DATA_ROLE_BOTH_PERIPH_PREFERRED,
	/**
	 * @brief Peripheral and Central Role supported, central is preferred.
	 */
	BLE_ADV_DATA_ROLE_BOTH_CENTRAL_PREFERRED
};

/**
 * @brief Advertising data name type.
 */
enum ble_adv_data_name_type {
	/**
	 * @brief Include no device name in advertising data.
	 */
	BLE_ADV_DATA_NO_NAME,
	/**
	 * @brief Include short device name in advertising data.
	 */
	BLE_ADV_DATA_SHORT_NAME,
	/**
	 * @brief Include full device name in advertising data.
	 */
	BLE_ADV_DATA_FULL_NAME
};

/**
 * @brief Advertising data UUID list.
 */
struct ble_adv_data_uuid_list {
	/**
	 * @brief UUID
	 */
	ble_uuid_t *uuid;
	/**
	 * @brief Length of the UUID list.
	 */
	uint8_t len;
};

/**
 * @brief Connection interval.
 */
struct ble_adv_data_conn_int {
	/**
	 * @brief Minimum connection interval
	 *
	 * In units of 1.25 ms, range 6 to 3200 (7.5 ms to 4 s).
	 */
	uint16_t min_conn_interval;
	/**
	 * @brief  Maximum connection interval.
	 *
	 * In units of 1.25 ms, range 6 to 3200 (7.5 ms to 4 s).
	 * The value 0xFFFF indicates no specific maximum.
	 */
	uint16_t max_conn_interval;
};

/**
 * @brief Manufacturer specific data.
 */
struct ble_adv_data_manufacturer {
	/**
	 * @brief Company identifier code.
	 */
	uint16_t company_identifier;
	/**
	 * @brief Manufacturer data length.
	 */
	uint16_t len;
	/**
	 * @brief Manufacturer data.
	 */
	uint8_t *data;
};

/**
 * @brief Service data.
 */
struct ble_adv_data_service {
	/**
	 * @brief Service UUID.
	 */
	uint16_t service_uuid;
	/**
	 * @brief Service data length.
	 */
	uint16_t len;
	/**
	 * @brief Service data.
	 */
	uint8_t *data;
};

/**
 * @brief Advertising data options.
 *
 * Settings for encoding of advertising data.
 */
struct ble_adv_data {
	/** Type of device name (short, long) */
	enum ble_adv_data_name_type name_type;
	/** Length of short device name (if short type is specified) */
	uint8_t short_name_len;
	/** Include Appearance */
	bool include_appearance;
	/** Include LE Bluetooth Device Address */
	bool include_ble_device_addr;
	/** Advertising data Flags */
	uint8_t flags;
	struct {
		/** List of UUIDs in the 'More Available' list */
		struct ble_adv_data_uuid_list more_available;
		/** List of UUIDs in the 'Complete' list */
		struct ble_adv_data_uuid_list complete;
		/** List of solicited UUIDs */
		struct ble_adv_data_uuid_list solicited;
	} uuid_lists;
	struct {
		/* Services. */
		struct ble_adv_data_service *service;
		/** Length of service */
		uint8_t len;
	} srv_list;
	/** TX Power Level */
	int8_t *tx_power_level;
	/** Slave Connection Interval Range */
	struct ble_adv_data_conn_int *slave_conn_int;
	/** Manufacturer specific data */
	struct ble_adv_data_manufacturer *manufacturer_data;
};

/**
 * @brief Encode data in the Advertising and Scan Response data format.
 *
 * This function encodes data into the Advertising and Scan Response data format
 * based on the fields in the supplied structures. This function can be used to create a payload
 * of Advertising packet or Scan Response packet, or a payload of NFC message intended for
 * initiating the Out-of-Band pairing.
 *
 * @param[in] ble_adv_data BLE advertising data context.
 * @param[out] buf  Output buffer.
 * @param[in,out] len Size of @p buf on input, length of encoded data on output.
 *
 * @retval 0 If the operation was successful.
 * @retval -EINVAL Invalid parameter.
 * @retval -E2BIG  Buffer is too small to encode all data.
 *
 * @warning This API may override the application's request to use the long name and use a short
 * name instead. This truncation will occur in case the long name does not fit the provided buffer
 * size. The application can specify a preferred short name length if truncation is required. For
 * example, if the complete device name is ABCD_HRMonitor, the application can specify the short
 * name length to be 8, so that the short device name appears as ABCD_HRM instead of ABCD_HRMo or
 * ABCD_HRMoni if the available size for the short name is 9 or 12 respectively, to have a more
 * appropriate short name. However, it should be noted that this is just a preference that the
 * application can specify, and if the preference is too large to fit in the provided buffer, the
 * name can be truncated further.
 */
int ble_adv_data_encode(const struct ble_adv_data *ble_adv_data, uint8_t *buf, uint16_t *len);

/**
 * @brief Search Advertising or Scan Response data for specific data types.
 *
 * This function searches through encoded data e.g. the data produced by @ref ble_adv_data_encode,
 * or the data found in Advertising reports (@ref BLE_GAP_EVT_ADV_REPORT), and returns the offset
 * of the data within the data buffer.
 *
 * The data with type @p ad_type can be found at buf[*offset] after calling
 * the function. This function can iterate through multiple instances of data of one
 * type by calling it again with the offset provided by the previous call.
 *
 * Example code for finding multiple instances of one type of data:
 *   offset = 0;
 *   ble_advdata_search(&data, len, &offset, AD_TYPE);
 *   first_instance_of_data = data[offset];
 *   ble_advdata_search(&data, len, &offset, AD_TYPE);
 *   second_instance_of_data = data[offset];
 *
 * @param[in] buf Encoded advertising data.
 * @param[in] len Buffer length.
 * @param[inout] offset The offset to start searching from, on input.
 *			The offset the data type can be found at, on output.
 * @param[in] ad_type The type of data to search for.
 *
 * @returns The length of the data if found, or 0 if no data was found with the type @p ad_type,
 *          or if @p buf or @p offset were @c NULL.
 */
uint16_t ble_adv_data_search(const uint8_t *buf, uint16_t len, uint16_t *offset, uint8_t ad_type);

/**
 * @brief Parse encoded Advertising or Scan Response data.
 *
 * This function searches through encoded data or the data found in Advertising reports
 * returns a pointer directly to the data within the data buffer.
 *
 * Example code:
 *  ad_type_data = ble_adv_data_parse(&data, len, AD_TYPE);
 *
 * @param[in] buf Encoded advertising data.
 * @param[in] len Buffer length.
 * @param[in] ad_type Type of data to search for.
 *
 * @returns A pointer to the data if found, or @c NULL if no data was found with the type
 *          @p ad_type, or if @p buf was @c NULL.
 */
uint8_t *ble_adv_data_parse(const uint8_t *buf, uint16_t len, uint8_t ad_type);

/**
 * @brief Search encoded Advertising data for a complete local name.
 *
 * @param[in] buf Encoded advertising data.
 * @param[in] len Buffer length.
 * @param[in] name Name to search for.
 *
 * @retval true   If @p name was found among @p buf, as a complete local name.
 * @retval false  If @p name was not found among @p buf, or if @p buf
 *                or @p name was @c NULL.
 */
bool ble_adv_data_name_find(const uint8_t *buf, uint16_t len, const char *name);

/**
 * @brief Search encoded Advertising data for a device shortened name.
 *
 * @param[in] buf Encoded advertising data.
 * @param[in] len Buffer length.
 * @param[in] name Name to search for.
 * @param[in] short_name_min_len Minimum length of the shortened name.
 *
 * If the shortened name in the Advertising data has the same length as the target name,
 * this function will return false, since this means that the complete name is actually
 * longer, thus different than the target name.
 *
 * @retval true   If @p name was found among @p buf, as short local name.
 * @retval false  If @p name was not found among @p buf, or if @p buf
 *                or @p name was @c NULL.
 */
bool ble_adv_data_short_name_find(const uint8_t *buf, uint16_t len, const char *name,
				  const uint8_t short_name_min_len);

/**
 * @brief Search encoded Advertising data for a UUID (16-bit or 128-bit).
 *
 * @param[in] buf Encoded Advertising data.
 * @param[in] len Buffer length.
 * @param[in] uuid UUID to search for.
 *
 * @retval true   If @p uuid was found among @p buf.
 * @retval false  If @p uuid was not found among @p buf, or if @p buf
 *                or @p uuid was @c NULL.
 */
bool ble_adv_data_uuid_find(const uint8_t *buf, uint16_t len, const ble_uuid_t *uuid);

/**
 * @brief Search encoded Advertising data for an appearance.
 *
 * @param[in] buf Encoded Advertising data.
 * @param[in] len Buffer length.
 * @param[in] appearance Appearance to search for.
 *
 * @retval true   If @p appearance was found among @p buf.
 * @retval false  If @p appearance was not found among @p buf, or if @p buf
 *                or @p appearance was @c NULL.
 */
bool ble_adv_data_appearance_find(const uint8_t *buf, uint16_t len, const uint16_t *appearance);

#ifdef __cplusplus
}
#endif

#endif /* BLE_ADV_DATA_H__ */

/** @} */
