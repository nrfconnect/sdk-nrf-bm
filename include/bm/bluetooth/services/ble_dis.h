/*
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/** @file
 *
 * @defgroup ble_dis Device Information Service
 *
 * @brief Device Information Service module.
 *
 * @details This module implements the Device Information Service.
 *          During initialization it adds the Device Information Service to the BLE stack database.
 *          It then encodes the supplied information, and adds the corresponding characteristics.
 */

#ifndef BLE_DIS_H__
#define BLE_DIS_H__

#include <stdint.h>
#include <bm/bluetooth/services/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Default security configuration. */
#define BLE_DIS_CONFIG_SEC_MODE_DEFAULT                                                            \
	{                                                                                          \
		.device_info_char.read = BLE_GAP_CONN_SEC_MODE_OPEN,                               \
	}

/**
 * @brief Device Information service configuration.
 */
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
};

/**
 * @brief Initialize the Device Information Service.
 *
 * @details This call allows the application to initialize the device information service.
 *          It adds the DIS service and DIS characteristics to the database, using the
 *          values supplied through the Kconfig options.
 *
 * @param dis_config Device Information Service configuration.
 *
 * @return NRF_SUCCESS on successful initialization of service.
 * @retval NRF_ERROR_NULL If @p dis_config is @c NULL.
 */
uint32_t ble_dis_init(const struct ble_dis_config *dis_config);

#ifdef __cplusplus
}
#endif

#endif /* BLE_DIS_H__ */

/** @} */
