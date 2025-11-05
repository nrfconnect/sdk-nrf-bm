/*
 * Copyright (c) 2018-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <nrf_soc.h>
#include <stdlib.h>

#include <bm/bluetooth/ble_radio_notification.h>
#include <zephyr/logging/log.h>

#if CONFIG_UNITY
#include <cmsis.h>
#endif

LOG_MODULE_REGISTER(ble_radio_ntf, CONFIG_BLE_RADIO_NOTIFICATION_LOG_LEVEL);

/* Radio notification type. */
#if defined(CONFIG_BLE_RADIO_NOTIFICATION_ON_ACTIVE)
#define NOTIFICATION_TYPE NRF_RADIO_NOTIFICATION_TYPE_INT_ON_ACTIVE
#elif defined(CONFIG_BLE_RADIO_NOTIFICATION_ON_INACTIVE)
#define NOTIFICATION_TYPE NRF_RADIO_NOTIFICATION_TYPE_INT_ON_INACTIVE
#else
#define NOTIFICATION_TYPE NRF_RADIO_NOTIFICATION_TYPE_INT_ON_BOTH
#endif

/* Application event handler for handling Radio Notification events. */
static ble_radio_notification_evt_handler_t evt_handler;

ISR_DIRECT_DECLARE(radio_notification_isr)
{
	static bool radio_active = IS_ENABLED(CONFIG_BLE_RADIO_NOTIFICATION_ON_ACTIVE);

#if defined(CONFIG_BLE_RADIO_NOTIFICATION_ON_BOTH)
	radio_active = !radio_active;
#endif

	if (evt_handler) {
		evt_handler(radio_active);
	}

	return 0;
}

uint32_t ble_radio_notification_init(uint32_t distance,
				     ble_radio_notification_evt_handler_t notif_evt_handler)
{
	if (!notif_evt_handler) {
		return NRF_ERROR_NULL;
	}

	evt_handler = notif_evt_handler;

	/* Initialize Radio Notification software interrupt */
	IRQ_DIRECT_CONNECT(RADIO_NOTIFICATION_IRQn, CONFIG_BLE_RADIO_NOTIFICATION_IRQ_PRIO,
			   radio_notification_isr, 0);

	NVIC_ClearPendingIRQ(RADIO_NOTIFICATION_IRQn);
	NVIC_EnableIRQ(RADIO_NOTIFICATION_IRQn);

	return sd_radio_notification_cfg_set(NOTIFICATION_TYPE, distance);
}
