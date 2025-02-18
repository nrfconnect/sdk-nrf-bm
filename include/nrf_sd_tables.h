/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup nrf_sd_tables SoftDevice Tables
 * @{
 * @brief    Tables for the SoftDevice.
 */

#ifndef NRF_SD_TABLES_H__
#define NRF_SD_TABLES_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert Softdevice event to string.
 *
 * @param evt Softdevice event.
 *
 * @returns Softdevice event string.
 */
const char *sd_evt_tostr(int evt);

/**
 * @brief Convert Softdevice error to string.
 *
 * @param err Softdevice error.
 *
 * @returns Softdevice error string.
 */
const char *sd_error_tostr(int err);

/**
 * @brief Convert Softdevice error to errno.
 *
 * @param sd_error Softdevice global error code.
 *
 * @returns Error converted to errno.
 * @retval -0xbad if error is unknown.
 */
const int sd_error_to_errno(int sd_error);

#ifdef __cplusplus
}
#endif

#endif /* NRF_SD_TABLES_H__ */

/** @} */
