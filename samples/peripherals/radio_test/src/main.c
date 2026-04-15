/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/printk.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/pm/device_runtime.h>
#if defined(NRF54L15_XXAA)
#include <hal/nrf_clock.h>
#endif /* defined(NRF54L15_XXAA) */
#include <nrfx.h>
/* TODO: DRGN-27733 Remove the alternative condition for nRF54LS05B
 * when the errata has been applied to this chip and the errata
 * check can be used instead.
 */
#if NRF54L_ERRATA_20_PRESENT || defined(NRF54LS05B_ENGA_XXAA)
#include <hal/nrf_power.h>
#endif /* NRF54L_ERRATA_20_PRESENT */
#if defined(NRF54LM20A_XXAA)
#include <hal/nrf_clock.h>
#endif /* defined(NRF54LM20A_XXAA) */

/* Empty trim value */
#define TRIM_VALUE_EMPTY 0xFFFFFFFF

static void clock_init(void)
{
	int err;
	int res;
	struct onoff_manager *clk_mgr;
	struct onoff_client clk_cli;

	clk_mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
	if (!clk_mgr) {
		printk("Unable to get the Clock manager\n");
		return;
	}

	sys_notify_init_spinwait(&clk_cli.notify);

	err = onoff_request(clk_mgr, &clk_cli);
	if (err < 0) {
		printk("Clock request failed: %d\n", err);
		return;
	}

	do {
		err = sys_notify_fetch_result(&clk_cli.notify, &res);
		if (!err && res) {
			printk("Clock could not be started: %d\n", res);
			return;
		}
	} while (err);

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

	printk("Clock has started\n");
}

int main(void)
{
	printk("Starting Radio Test sample\n");

	clock_init();

	return 0;
}
