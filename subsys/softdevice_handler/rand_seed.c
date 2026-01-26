/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_soc.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>
#include <zephyr/logging/log.h>
#include <psa/crypto.h>
#if CONFIG_HAS_HW_NRF_CRACEN
#include <cracen_psa.h>
#else
#include <nrfx_cracen.h>
#endif

LOG_MODULE_DECLARE(nrf_sdh, CONFIG_NRF_SDH_LOG_LEVEL);

/* Extern in nrf_sdh.c */
void sdh_soc_rand_seed(uint32_t evt, void *ctx)
{
	uint32_t nrf_err;
	uint8_t seed[SD_RAND_SEED_SIZE];

	if (evt != NRF_EVT_RAND_SEED_REQUEST) {
		/* Not our business */
		return;
	}

#if CONFIG_HAS_HW_NRF_CRACEN
	/* Selected when we have CRACEN Crypto HW available, like for nRF54L15
	 * In this case the cracen_psa api will be available and can be used
	 */
	psa_status_t status = cracen_get_trng(seed, sizeof(seed));

	if (status != PSA_SUCCESS) {
		LOG_ERR("Failed to generate true random number, psa_status %d", status);
		return;
	}
#else
	/* Selected when we don't have CRACEN Crypto HW available, like for nRF54LS05B
	 * In this case the cracen_psa api isn't available and we should use nrfx api
	 */
	int err = nrfx_cracen_entropy_get(seed, sizeof(seed));

	if (err) {
		LOG_ERR("Failed to generate true random number, err %d", err);
		return;
	}
#endif

	nrf_err = sd_rand_seed_set(seed);

	/* Discard seed immediately */
	memset(seed, 0, sizeof(seed));

	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to seed SoftDevice RNG, nrf_error %#x", nrf_err);
		return;
	}

	LOG_DBG("SoftDevice RNG seeded");
}

NRF_SDH_SOC_OBSERVER(rand_seed, sdh_soc_rand_seed, NULL, HIGH);
