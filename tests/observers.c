/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <observers.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>

void ble_evt_send(const ble_evt_t *evt)
{
	TYPE_SECTION_FOREACH(struct nrf_sdh_ble_evt_observer, nrf_sdh_ble_evt_observers, obs) {
		obs->handler(evt, obs->context);
	}
}

void soc_evt_send(uint32_t evt)
{
	TYPE_SECTION_FOREACH(struct nrf_sdh_soc_evt_observer, nrf_sdh_soc_evt_observers, obs) {
		obs->handler(evt, obs->context);
	}
}

void state_evt_send(enum nrf_sdh_state_evt state)
{
	TYPE_SECTION_FOREACH(struct nrf_sdh_state_evt_observer, nrf_sdh_state_evt_observers, obs) {
		obs->handler(state, obs->context);
	}
}
