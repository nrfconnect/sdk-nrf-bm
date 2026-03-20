/**
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup bm_ble_common Bluetooth LE Common
 * @{
 * @brief Definitions of common macros and helper functions.
 */

#ifndef BLE_COMMON_H__
#define BLE_COMMON_H__

#include <stdbool.h>
#include <ble_gap.h>
#include <ble_gatt.h>
#include <zephyr/sys/byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calculate how many 32-bit words are needed to hold n_bytes.
 *
 * @param[in] n_bytes Number of bytes.
 *
 * @return Number of 32-bit words.
 */
#define BYTES_TO_WORDS(n_bytes) (((n_bytes) + 3) >> 2)

/**
 * @brief Read CCCD value from data and check if notifications are enabled.
 *
 * @param[in] gatts_value Pointer to CCCD value.
 *
 * @return true if notifications are enabled, false if not.
 */
static inline bool is_notification_enabled(const uint8_t *gatts_value)
{
	const uint16_t cccd_val = sys_get_le16(gatts_value);

	return (cccd_val & BLE_GATT_HVX_NOTIFICATION);
}

/**
 * @brief Read CCCD value from data and check if indications are enabled.
 *
 * @param[in] gatts_value Pointer to CCCD value.
 *
 * @return true if indications are enabled, false if not.
 */
static inline bool is_indication_enabled(const uint8_t *gatts_value)
{
	const uint16_t cccd_val = sys_get_le16(gatts_value);

	return (cccd_val & BLE_GATT_HVX_INDICATION);
}

/**
 * @brief Compare GAP connection security mode.
 *
 * @param[in] a First conn_sec_mode value.
 * @param[in] b Second conn_sec_mode value.
 *
 * @return true if the GAP connection security modes are equal, false if not.
 */
static inline bool ble_gap_conn_sec_mode_equal(const ble_gap_conn_sec_mode_t *a,
					       const ble_gap_conn_sec_mode_t *b)
{
	return (a->sm == b->sm) && (a->lv == b->lv);
}

/**
 * @brief Set sec_mode to have no access rights.
 */
#define BLE_GAP_CONN_SEC_MODE_NO_ACCESS                                                            \
	(ble_gap_conn_sec_mode_t)                                                                  \
	{                                                                                          \
		.sm = 0, .lv = 0                                                                   \
	}

/**
 * @brief Set sec_mode to require no protection, open link.
 */
#define BLE_GAP_CONN_SEC_MODE_OPEN                                                                 \
	(ble_gap_conn_sec_mode_t)                                                                  \
	{                                                                                          \
		.sm = 1, .lv = 1                                                                   \
	}

/**
 * @brief Set sec_mode to require encryption, but no MITM protection.
 */
#define BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM                                                          \
	(ble_gap_conn_sec_mode_t)                                                                  \
	{                                                                                          \
		.sm = 1, .lv = 2                                                                   \
	}

/**
 * @brief Set sec_mode to require encryption and MITM protection.
 */
#define BLE_GAP_CONN_SEC_MODE_ENC_WITH_MITM                                                        \
	(ble_gap_conn_sec_mode_t)                                                                  \
	{                                                                                          \
		.sm = 1, .lv = 3                                                                   \
	}

/**
 * @brief Set sec_mode to require LESC encryption and MITM protection.
 */
#define BLE_GAP_CONN_SEC_MODE_LESC_ENC_WITH_MITM                                                   \
	(ble_gap_conn_sec_mode_t)                                                                  \
	{                                                                                          \
		.sm = 1, .lv = 4                                                                   \
	}

#ifdef __cplusplus
}
#endif

#endif /* BLE_COMMON_H__ */

/** @} */
