/*
 * Copyright (c) 2018-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup ble_radio_notification Radio Notification
 * @{
 *
 * @brief Module for propagating Radio Notification events to the application.
 */

#ifndef BLE_RADIO_NOTIFICATION_H__
#define BLE_RADIO_NOTIFICATION_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Application radio notification event handler type. */
typedef void (*ble_radio_notification_evt_handler_t)(bool radio_active);

/**
 * @brief Function for initializing the Radio Notification module.
 *
 * To ensure that the radio notification signal behaves in a consistent way, the radio
 * notifications must be configured when there is no protocol stack or other SoftDevice
 * activity in progress. It is recommended that the radio notification signal is
 * configured directly after the SoftDevice has been enabled.
 *
 * @param[in]  distance    Distance between the ACTIVE notification signal and start of radio event.
 * @param[in]  evt_handler Handler to be called when a radio notification event has been received.
 *
 * @retval NRF_SUCCESS on successful initialization.
 * @retval NRF_ERROR_NULL if @c evt_handler is NULL.
 * @retval NRF_ERROR_INVALID_PARAM if the distance is invalid or radio notification type is not
 *                                 properly configured.
 * @retval NRF_ERROR_INVALID_STATE if the protocol stack or other SoftDevice is running. Stop all
 *                                 running activities and retry.
 */
uint32_t ble_radio_notification_init(uint32_t distance,
				     ble_radio_notification_evt_handler_t evt_handler);

#ifdef __cplusplus
}
#endif

#endif /* BLE_RADIO_NOTIFICATION_H__ */

/** @} */
