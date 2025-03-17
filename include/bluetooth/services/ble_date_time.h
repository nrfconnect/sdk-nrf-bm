/*
 * Copyright (c) 2011 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief Contains definition of ble_date_time structure.
 */

/** @file
 *
 * @defgroup ble_sdk_srv_date_time BLE Date Time characteristic type
 * @{
 * @ingroup ble_sdk_lib
 * @brief Definition of struct ble_date_time type.
 */

#ifndef BLE_DATE_TIME_H__
#define BLE_DATE_TIME_H__

#include <stdint.h>
#include <zephyr/sys/byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Date and Time structure. */
struct ble_date_time {
	uint16_t year;
	uint8_t  month;
	uint8_t  day;
	uint8_t  hours;
	uint8_t  minutes;
	uint8_t  seconds;
};

static inline uint8_t ble_date_time_encode(const struct ble_date_time *date_time,
					   uint8_t *encoded_data)
{
	uint8_t len = 0;

	sys_put_le16(date_time->year, encoded_data);
	len += sizeof(uint16_t);

	encoded_data[len++] = date_time->month;
	encoded_data[len++] = date_time->day;
	encoded_data[len++] = date_time->hours;
	encoded_data[len++] = date_time->minutes;
	encoded_data[len++] = date_time->seconds;

	return len;
}

static inline uint8_t ble_date_time_decode(struct ble_date_time *date_time,
					   const uint8_t *encoded_data)
{
	uint8_t len = sizeof(uint16_t);

	date_time->year    = sys_get_le16(encoded_data);
	date_time->month   = encoded_data[len++];
	date_time->day     = encoded_data[len++];
	date_time->hours   = encoded_data[len++];
	date_time->minutes = encoded_data[len++];
	date_time->seconds = encoded_data[len++];

	return len;
}

#ifdef __cplusplus
}
#endif

#endif /* BLE_DATE_TIME_H__ */

/** @} */
