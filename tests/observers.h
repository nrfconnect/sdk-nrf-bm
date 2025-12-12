/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdint.h>
#include <ble.h>
#include <bm/softdevice_handler/nrf_sdh.h>

/* Invoke the BLE event handler of each observer, passing BLE event 'evt'. */
void ble_evt_send(const ble_evt_t *evt);

/* Invoke the SoC event handler of each observer, passing SoC event 'evt'. */
void soc_evt_send(uint32_t evt);

/* Invoke the state event handler of each observer, passing state event. */
void state_evt_send(enum nrf_sdh_state_evt state);
