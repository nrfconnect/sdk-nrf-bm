/*
 * Copyright (c) 2018-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <stdlib.h>

#include <ble_radio_notification.h>
#include <zephyr/logging/log.h>

#if CONFIG_UNITY
#include <cmsis.h>
#define STATIC
#else
#define STATIC static
#endif

LOG_MODULE_REGISTER(ble_radio_ntf, CONFIG_BLE_RADIO_NOTIFICATION_LOG_LEVEL);

/* Current radio state. */
#if CONFIG_BLE_RADIO_NOTIFICATION_ON_ACTIVE
static bool radio_active = true;
#else
static bool radio_active;
#endif

/* Application event handler for handling Radio Notification events. */
static ble_radio_notification_evt_handler_t evt_handler;

STATIC void radio_notification_isr(const void *arg)
{
	ARG_UNUSED(arg);

#if defined(CONFIG_BLE_RADIO_NOTIFICATION_ON_BOTH)
	radio_active = !radio_active;
#endif

	evt_handler(radio_active);
}

uint32_t ble_radio_notification_init(uint32_t distance,
				     ble_radio_notification_evt_handler_t _evt_handler)
{
	uint8_t type = NRF_RADIO_NOTIFICATION_TYPE_INT_ON_BOTH;

	if (!_evt_handler) {
		return NRF_ERROR_NULL;
	}

	#if defined(CONFIG_BLE_RADIO_NOTIFICATION_ON_ACTIVE)
	type = NRF_RADIO_NOTIFICATION_TYPE_INT_ON_ACTIVE;
#elif defined(CONFIG_BLE_RADIO_NOTIFICATION_ON_INACTIVE)
	type = NRF_RADIO_NOTIFICATION_TYPE_INT_ON_INACTIVE;
#endif

	evt_handler = _evt_handler;

	/* Initialize Radio Notification software interrupt */
	IRQ_DIRECT_CONNECT(RADIO_NOTIFICATION_IRQn, CONFIG_BLE_RADIO_NOTIFICATION_IRQ_PRIO,
			   radio_notification_isr, 0);

	NVIC_ClearPendingIRQ(RADIO_NOTIFICATION_IRQn);
	NVIC_EnableIRQ(RADIO_NOTIFICATION_IRQn);

	return sd_radio_notification_cfg_set(type, distance);
}
