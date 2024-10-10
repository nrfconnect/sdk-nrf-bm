/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stddef.h>
#include <stdint.h>
#include <nrf_sdh.h>
#include <nrf_sdh_soc.h>
#include <nrf_soc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nrf_sdh, CONFIG_NRF_SDH_LOG_LEVEL);

static const char *tostr(uint32_t evt)
{
	switch (evt) {
	case NRF_EVT_HFCLKSTARTED:
		return "The HFCLK has started";
	case NRF_EVT_POWER_FAILURE_WARNING:
		return "A power failure warning has occurred";
	case NRF_EVT_FLASH_OPERATION_SUCCESS:
		return "Flash operation has completed successfully";
	case NRF_EVT_FLASH_OPERATION_ERROR:
		return "Flash operation has timed out with an error";
	case NRF_EVT_RADIO_BLOCKED:
		return "A radio timeslot was blocked";
	case NRF_EVT_RADIO_CANCELED:
		return "A radio timeslot was canceled by SoftDevice";
	case NRF_EVT_RADIO_SIGNAL_CALLBACK_INVALID_RETURN:
		return "A radio timeslot signal callback handler return was invalid";
	case NRF_EVT_RADIO_SESSION_IDLE:
		return "A radio timeslot session is idle";
	case NRF_EVT_RADIO_SESSION_CLOSED:
		return "A radio timeslot session is closed";
	case NRF_EVT_POWER_USB_POWER_READY:
		return "A USB 3.3 V supply is ready";
	case NRF_EVT_POWER_USB_DETECTED:
		return "Voltage supply is detected on VBUS";
	case NRF_EVT_POWER_USB_REMOVED:
		return "Voltage supply is removed from VBUS";
	default:
		return "Unknown";
	}
}

static void soc_evt_poll(void *context)
{
	int err;
	uint32_t evt_id;

	while (true) {
		err = sd_evt_get(&evt_id);
		if (err) {
			break;
		}

		if (IS_ENABLED(CONFIG_NRF_SDH_STR_TABLES)) {
			LOG_DBG("SoC event: %s", tostr(evt_id));
		} else {
			LOG_DBG("SoC event: 0x%x", evt_id);
		}

		/* Forward the event to SoC observers. */
		TYPE_SECTION_FOREACH(
			struct nrf_sdh_soc_evt_observer, nrf_sdh_soc_evt_observers, obs) {
			obs->handler(evt_id, obs->context);
		}
	}

	__ASSERT(err != NRF_ERROR_NOT_FOUND,
		"Failed to receive SoftDevice event, nrf_error %d", err);
}

/* Listen to SoftDevice events */
NRF_SDH_STACK_EVT_OBSERVER(soc_evt_obs, soc_evt_poll, NULL, 0);
