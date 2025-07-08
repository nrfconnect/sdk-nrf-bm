/**
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup ble_mcumgr BLE MCUmgr Service library
 * @{
 * @brief Library for handling MCUmgr over BLE.
 */

#ifndef BLE_MCUMGR_H__
#define BLE_MCUMGR_H__

#include <stdint.h>
#include <stdbool.h>
#include <ble.h>
#include <nrf_sdh_ble.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function for initializing the MCUmgr Bluetooth service.
 *
 * @retval 0 On success.
 * @retval -EINVAL Invalid parameters.
 */
int ble_mcumgr_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_MCUMGR_H__ */

/** @} */
