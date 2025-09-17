/**
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BLE_SERVICES_COMMON_H__
#define BLE_SERVICES_COMMON_H__

#include <ble.h>
#include <ble_gap.h>
#include <zephyr/sys/byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint16_t is_notification_enabled(const uint8_t *gatts_write_data)
{
	const uint16_t cccd_val = sys_get_le16(gatts_write_data);

	return (cccd_val & BLE_GATT_HVX_NOTIFICATION);
}

static inline uint16_t is_indication_enabled(const uint8_t *gatts_write_data)
{
	const uint16_t cccd_val = sys_get_le16(gatts_write_data);

	return (cccd_val & BLE_GATT_HVX_INDICATION);
}

#define gap_conn_sec_mode_from_u8(x)                                                               \
	{                                                                                          \
		.sm = ((x) >> 4) & 0xf, .lv = (x) & 0xf,                                           \
	}

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

/**
 * @brief Set sec_mode to require signing or encryption, no MITM protection needed.
 */
#define BLE_GAP_CONN_SEC_MODE_SIGNED_NO_MITM                                                       \
	(ble_gap_conn_sec_mode_t)                                                                  \
	{                                                                                          \
		.sm = 2, .lv = 1                                                                   \
	}

/**
 * @brief Set sec_mode to require signing or encryption with MITM protection.
 */
#define BLE_GAP_CONN_SEC_MODE_SIGNED_WITH_MITM                                                     \
	(ble_gap_conn_sec_mode_t)                                                                  \
	{                                                                                          \
		.sm = 2, .lv = 2                                                                   \
	}

#ifdef __cplusplus
}
#endif

#endif /* BLE_SERVICES_COMMON_H__ */
