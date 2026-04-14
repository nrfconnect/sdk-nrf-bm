/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/shell/shell.h>
#include <bm/shell/backend_bm_uarte.h>
#include <nrfx.h>
#include <hal/nrf_clock.h>
/* TODO: DRGN-27733 Remove the alternative condition for nRF54LS05B
 * when the errata has been applied to this chip and the errata
 * check can be used instead.
 */
#if NRF54L_ERRATA_20_PRESENT || defined(NRF54LS05B_ENGA_XXAA)
#include <hal/nrf_power.h>
#endif /* NRF54L_ERRATA_20_PRESENT */

#include "radio_test.h"

const struct shell *sh;

static void clock_init(void)
{
	nrf_clock_event_clear(NRF_CLOCK, NRF_CLOCK_EVENT_HFCLKSTARTED);
	nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTART);
	while (!nrf_clock_event_check(NRF_CLOCK, NRF_CLOCK_EVENT_HFCLKSTARTED)) {
		/* spin until HFXO is up */
	}
	nrf_clock_event_clear(NRF_CLOCK, NRF_CLOCK_EVENT_HFCLKSTARTED);

#if NRF54L_ERRATA_20_PRESENT
	if (nrf54l_errata_20()) {
		nrf_power_task_trigger(NRF_POWER, NRF_POWER_TASK_CONSTLAT);
	}
	/* TODO: DRGN-27733 Remove the elif block for nRF54LS05B when the errata has
	 * been applied to this chip and the errata check can be used instead.
	 */
#elif defined(NRF54LS05B_ENGA_XXAA)
	nrf_power_task_trigger(NRF_POWER, NRF_POWER_TASK_CONSTLAT);
#endif /* NRF54L_ERRATA_20_PRESENT */

#if defined(NRF54LM20A_XXAA)
	/* MLTPAN-39 */
	nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_PLLSTART);
#endif /* defined(NRF54LM20A_XXAA) */
}

int main(void)
{
	sh = shell_backend_bm_uarte_get_ptr();
	const struct shell_backend_config_flags cfg_flags =
		SHELL_DEFAULT_BACKEND_CONFIG_FLAGS;

	shell_init(sh, NULL, cfg_flags, false, 0);
	shell_start(sh);

	shell_print(sh, "Starting Radio Test sample");

	clock_init();

	struct radio_test_config *radio_test_conf = radio_cmd_get_test_config();
	int err = radio_test_init(radio_test_conf);

	if (err) {
		shell_error(sh, "radio_test_init() failed: %d", err);
	}

	while (true) {
		/*
		 * Process all received data from the shell backend, calling registered command
		 * handlers like sample_terminate_cmd if the command and arguments match.
		 */
		shell_process(sh);

		/*
		 * Ensure no events are missed while we check if there is pending RX data to
		 * be processed.
		 */
		unsigned int key = irq_lock();

		radio_test_process_rx_timeout();

		if (shell_backend_bm_uarte_rx_ready()) {
			/* Process pending RX data */
			irq_unlock(key);
			continue;
		}

		/* Idle until next event */
		k_cpu_atomic_idle(key);
	}

	return 0;
}
