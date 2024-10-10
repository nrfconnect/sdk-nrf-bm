/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <nrfx_gpiote.h>
#include <zephyr/linker/linker-defs.h>
#include <nrf_sdm.h>

void relocate_vector_table(void)
{
	/** TODO: What is this for? */
}

static int irq_init(void)
{
	int err;

	#define VECTOR_ADDRESS ((uintptr_t)_vector_start)

	/** TODO: rework */
#ifdef CONFIG_NRF5_SDK_IRQ_CONNECT_GPIOTE
        IRQ_CONNECT(GPIOTE_IRQn, 5, nrfx_isr, nrfx_gpiote_0_irq_handler, 0);
#endif

	err = sd_softdevice_vector_table_base_set(VECTOR_ADDRESS);
	__ASSERT("Failed to set the vector table, nrf_error %d", err);

	return (err == NRF_SUCCESS) ? 0 : -EIO;
}

SYS_INIT(irq_init, PRE_KERNEL_1, 0);
