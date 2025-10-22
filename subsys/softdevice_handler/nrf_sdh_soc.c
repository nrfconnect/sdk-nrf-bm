/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stddef.h>
#include <stdint.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>
#include <nrf_soc.h>
#include <psa/crypto.h>
#include <cracen_psa.h>
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
	case NRF_EVT_RAND_SEED_REQUEST:
		return "SoftDevice RNG needs to be seeded";
	default:
		return "Unknown";
	}
}

static void softdevice_rng_seed(void)
{
	uint32_t nrf_err = NRF_ERROR_INVALID_DATA;
	psa_status_t status;
	uint8_t seed[SD_RAND_SEED_SIZE];

	status = cracen_get_trng(seed, sizeof(seed));
	if (status == PSA_SUCCESS) {
		nrf_err = sd_rand_seed_set(seed);
		memset(seed, 0, sizeof(seed));
		if (nrf_err == NRF_SUCCESS) {
			LOG_DBG("SoftDevice RNG seeded");
			return;
		}
	} else {
		LOG_ERR("Generate random failed, psa status %d", status);
	}

	LOG_ERR("Failed to seed SoftDevice RNG, nrf_error %#x", nrf_err);
}

static void soc_evt_poll(void *context)
{
	uint32_t nrf_err;
	uint32_t evt_id;

	while (true) {
		nrf_err = sd_evt_get(&evt_id);
		if (nrf_err) {
			break;
		}

		if (IS_ENABLED(CONFIG_NRF_SDH_STR_TABLES)) {
			LOG_DBG("SoC event: %s", tostr(evt_id));
		} else {
			LOG_DBG("SoC event: 0x%x", evt_id);
		}

		if (evt_id == NRF_EVT_RAND_SEED_REQUEST) {
			softdevice_rng_seed();
		}

		/* Forward the event to SoC observers. */
		TYPE_SECTION_FOREACH(
			struct nrf_sdh_soc_evt_observer, nrf_sdh_soc_evt_observers, obs) {
			obs->handler(evt_id, obs->context);
		}
	}

	__ASSERT(nrf_err == NRF_ERROR_NOT_FOUND,
		 "Failed to receive SoftDevice event, nrf_error %#x", nrf_err);
}

/* Listen to SoftDevice events */
NRF_SDH_STACK_EVT_OBSERVER(soc_evt_obs, soc_evt_poll, NULL, 0);
