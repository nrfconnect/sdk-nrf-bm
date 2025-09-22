/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stddef.h>
#include <stdint.h>
#include <nrf_soc.h>
#include <nrf_sdh.h>
#include <nrf_sdh_soc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nrf_sdh, CONFIG_NRF_SDH_LOG_LEVEL);

static const char *tostr(uint32_t evt)
{
	switch (evt) {
	case NRF_EVT_HFCLKSTARTED:
		return "NRF_EVT_HFCLKSTARTED";
	case NRF_EVT_POWER_FAILURE_WARNING:
		return "NRF_EVT_POWER_FAILURE_WARNING";
	case NRF_EVT_FLASH_OPERATION_SUCCESS:
		return "NRF_EVT_FLASH_OPERATION_SUCCESS";
	case NRF_EVT_FLASH_OPERATION_ERROR:
		return "NRF_EVT_FLASH_OPERATION_ERROR";
	case NRF_EVT_RADIO_BLOCKED:
		return "NRF_EVT_RADIO_BLOCKED";
	case NRF_EVT_RADIO_CANCELED:
		return "NRF_EVT_RADIO_CANCELED";
	case NRF_EVT_RADIO_SIGNAL_CALLBACK_INVALID_RETURN:
		return "NRF_EVT_RADIO_SIGNAL_CALLBACK_INVALID_RETURN";
	case NRF_EVT_RADIO_SESSION_IDLE:
		return "NRF_EVT_RADIO_SESSION_IDLE";
	case NRF_EVT_RADIO_SESSION_CLOSED:
		return "NRF_EVT_RADIO_SESSION_CLOSED";
	case NRF_EVT_RAND_SEED_REQUEST:
		return "NRF_EVT_RAND_SEED_REQUEST";
	default:
		return "Unknown";
	}
}

static void soc_evt_poll(void *context)
{
	uint32_t err;
	uint32_t evt_id;

	while (true) {
		err = sd_evt_get(&evt_id);
		if (err != NRF_SUCCESS) {
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

	__ASSERT(err == NRF_ERROR_NOT_FOUND,
		 "Failed to receive SoftDevice event, nrf_error %#x", err);
}

/* Listen to SoftDevice events */
NRF_SDH_STACK_EVT_OBSERVER(soc_evt_obs, soc_evt_poll, NULL, HIGHEST);
