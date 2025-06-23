/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef CONFIG_SOC_NRF54L15
#include <nrf54l15_application.h>
#elif CONFIG_SOC_NRF54L10
#include <nrf54l10_application.h>
#elif CONFIG_SOC_NRF54L05
#include <nrf54l05_application.h>
#endif

#include <bm_util.h>

static uint8_t critical_section_count;

void bm_util_critical_section_enter(bool *is_nested)
{
#if defined(CONFIG_SOFTDEVICE)
	/* Disable all interrupts with priorities above 0 (used by SoftDevice, time-sensitive). */
	__set_BASEPRI(1 << (8 - __NVIC_PRIO_BITS));
#else
	/* Disable all interrupts. */
	__disable_irq();
#endif

	*is_nested = (critical_section_count != 0);
	critical_section_count++;
}

void bm_util_critical_section_exit(bool is_nested)
{
	critical_section_count--;

	if (!is_nested) {
#if defined(CONFIG_SOFTDEVICE)
		/* Enable all interrupts. */
		__set_BASEPRI(0);
#else
		/* Enable all interrupts. */
		__enable_irq();
#endif
	}
}
