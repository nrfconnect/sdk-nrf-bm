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
#include <bm/softdevice_handler/nrf_sdh_ble.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Used vendor specific UUID */
#define BLE_MCUMGR_SERVICE_UUID { 0x84, 0xAA, 0x60, 0x74, 0x52, 0x8A, 0x8B, 0x86, \
				  0xD3, 0x4C, 0xB7, 0x1D, 0x1D, 0xDC, 0x53, 0x8D  }

#define BLE_MCUMGR_CHARACTERISTIC_UUID { 0x48, 0x7C, 0x99, 0x74, 0x11, 0x26, 0x9E, 0xAE, \
					 0x01, 0x4E, 0xCE, 0xFB, 0x28, 0x78, 0x2E, 0xDA  }

/** The UUID of the TX Characteristic */
#define BLE_MCUMGR_SERVICE_UUID_SUB 0xdc1d
#define BLE_MCUMGR_CHARACTERISTIC_UUID_SUB 0x7828

/**
 * @brief Function for initializing the MCUmgr Bluetooth service.
 *
 * @retval 0 On success.
 * @retval -EINVAL Invalid parameters.
 */
int ble_mcumgr_init(void);

/**
 * @brief Function for getting the MCUmgr Bluetooth service UUID type.
 *
 * @retval service_type
 */
uint8_t ble_mcumgr_service_uuid_type(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_MCUMGR_H__ */

/** @} */
