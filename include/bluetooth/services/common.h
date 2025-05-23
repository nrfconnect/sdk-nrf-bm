/**
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <ble.h>
#include <zephyr/sys/byteorder.h>

/** TODO: rename this.. */
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
