/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_soc.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>
#include <zephyr/logging/log.h>
#include <psa/crypto.h>
#include <cracen_psa.h>

LOG_MODULE_DECLARE(nrf_sdh, CONFIG_NRF_SDH_LOG_LEVEL);

static void on_rand_seed_evt(uint32_t evt, void *ctx)
{
	uint32_t nrf_err;
	psa_status_t status;
	uint8_t seed[SD_RAND_SEED_SIZE];

	if (evt != NRF_EVT_RAND_SEED_REQUEST) {
		/* Not our business */
		return;
	}

	status = cracen_get_trng(seed, sizeof(seed));
	if (status != PSA_SUCCESS) {
		LOG_ERR("Failed to generate true random number, psa_status %d", status);
		return;
	}

	nrf_err = sd_rand_seed_set(seed);

	/* Discard seed immediately */
	memset(seed, 0, sizeof(seed));

	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to seed SoftDevice RNG, nrf_error %#x", nrf_err);
		return;
	}

	LOG_DBG("SoftDevice RNG seeded");
}

NRF_SDH_SOC_OBSERVER(rand_seed, on_rand_seed_evt, NULL, HIGH);
