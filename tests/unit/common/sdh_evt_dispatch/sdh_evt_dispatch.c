/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <sdh_evt_dispatch.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>
#include <zephyr/sys/iterable_sections.h>

void sdh_evt_dispatch_ble(const ble_evt_t *evt)
{
	TYPE_SECTION_FOREACH(struct nrf_sdh_ble_evt_observer,
			     nrf_sdh_ble_evt_observers, obs) {
		obs->handler(evt, obs->context);
	}
}

void sdh_evt_dispatch_soc(uint32_t evt_id)
{
	TYPE_SECTION_FOREACH(struct nrf_sdh_soc_evt_observer,
			     nrf_sdh_soc_evt_observers, obs) {
		obs->handler(evt_id, obs->context);
	}
}

void sdh_evt_dispatch_state(enum nrf_sdh_state_evt state)
{
	TYPE_SECTION_FOREACH(struct nrf_sdh_state_evt_observer,
			     nrf_sdh_state_evt_observers, obs) {
		obs->handler(state, obs->context);
	}
}
