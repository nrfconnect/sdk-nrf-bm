/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Test utility for dispatching SoftDevice events to registered observers.
 *
 * Allows unit tests to send BLE, SoC, and state events to all observers
 * registered via NRF_SDH_BLE_OBSERVER, NRF_SDH_SOC_OBSERVER, and
 * NRF_SDH_STATE_EVT_OBSERVER without needing to make handler functions
 * non-static or extern.
 */

#ifndef SDH_EVT_DISPATCH_H__
#define SDH_EVT_DISPATCH_H__

#include <ble.h>
#include <bm/softdevice_handler/nrf_sdh.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Dispatch a BLE event to all registered BLE observers.
 *
 * Iterates the nrf_sdh_ble_evt_observers iterable section and invokes
 * each observer's handler in priority order.
 *
 * @param evt BLE event to dispatch.
 */
void sdh_evt_dispatch_ble(const ble_evt_t *evt);

/**
 * @brief Dispatch a SoC event to all registered SoC observers.
 *
 * Iterates the nrf_sdh_soc_evt_observers iterable section and invokes
 * each observer's handler in priority order.
 *
 * @param evt_id SoC event ID to dispatch.
 */
void sdh_evt_dispatch_soc(uint32_t evt_id);

/**
 * @brief Dispatch a state event to all registered state observers.
 *
 * Iterates the nrf_sdh_state_evt_observers iterable section and invokes
 * each observer's handler in priority order.
 *
 * @param state State event to dispatch.
 */
void sdh_evt_dispatch_state(enum nrf_sdh_state_evt state);

#ifdef __cplusplus
}
#endif

#endif /* SDH_EVT_DISPATCH_H__ */
