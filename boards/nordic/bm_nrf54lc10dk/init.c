/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <hal/nrf_regulators.h>

void board_late_init_hook(void)
{
	nrf_regulators_vreg_enable_set(NRF_REGULATORS, NRF_REGULATORS_VREG_MAIN, true);
}
