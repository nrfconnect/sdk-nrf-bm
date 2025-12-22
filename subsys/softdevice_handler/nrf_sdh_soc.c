/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <nrf_soc.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nrf_sdh, CONFIG_NRF_SDH_LOG_LEVEL);

const char *nrf_sdh_soc_evt_to_str(uint32_t evt)
{
	int err;
	static char buf[sizeof("SoC event: 0xFFFFFFFF")];

	switch (evt) {
#if defined(CONFIG_NRF_SDH_STR_TABLES)
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
#endif
	default:
		err = snprintf(buf, sizeof(buf), "SoC event: %#x", evt);
		__ASSERT(err > 0, "Encode error");
		__ASSERT(err < sizeof(buf), "Buffer too small");
		(void)err;
		return buf;
	}
}

void rrw_done(void);

static void soc_evt_poll(void *context)
{
	uint32_t nrf_err;
	uint32_t evt_id;

	while (true) {
		nrf_err = sd_evt_get(&evt_id);
		if (nrf_err) {
			break;
		}

		LOG_DBG("%s", nrf_sdh_soc_evt_to_str(evt_id));

		if (evt_id == NRF_EVT_FLASH_OPERATION_SUCCESS) {
#if defined(CONFIG_SOC_FLASH_NRF_RRAM_BM)
			rrw_done();
#endif
		}
		/* Forward the event to SoC observers. */
		TYPE_SECTION_FOREACH(
			struct nrf_sdh_soc_evt_observer, nrf_sdh_soc_evt_observers, obs) {
			obs->handler(evt_id, obs->context);
		}
	}

	__ASSERT(nrf_err == NRF_ERROR_NOT_FOUND,
		 "Failed to receive SoftDevice SoC event, nrf_error %#x", nrf_err);
}

/* Listen to SoftDevice events */
NRF_SDH_STACK_EVT_OBSERVER(soc_evt_obs, soc_evt_poll, NULL, HIGHEST);
