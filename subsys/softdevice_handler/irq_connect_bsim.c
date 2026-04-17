/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Register all SoftDevice IRQ handlers with the native simulator.
 * SD_EVT_IRQn (SWI01) is connected in nrf_sdh.c (sd_irq_init), same as on ARM.
 */

#include <nrfx.h>
#include <zephyr/init.h>
#include <zephyr/irq.h>

/* Handlers provided by the linked SoftDevice library */
extern void C_RTC0_Handler(void);
extern void C_SIGNALLING_Handler(void);
extern void C_RADIO_Handler(void);
extern void C_POWER_CLOCK_Handler(void);
extern void C_TIMER0_Handler(void);
extern void C_ECB_Handler(void);
extern void C_CCM_Handler(void);

#define SD_IRQ_PRIO_HIGH 0
#define SD_IRQ_PRIO_LOW 4

#define IRQ_WRAPPER(name, handler) \
	static void name##_wrapper(const void *arg) { ARG_UNUSED(arg); handler(); }

IRQ_WRAPPER(grtc3, C_RTC0_Handler);
IRQ_WRAPPER(swi00, C_SIGNALLING_Handler);
IRQ_WRAPPER(radio0, C_RADIO_Handler);

IRQ_WRAPPER(clock_power, C_POWER_CLOCK_Handler);
IRQ_WRAPPER(timer10, C_TIMER0_Handler);
IRQ_WRAPPER(ecb00, C_ECB_Handler);
IRQ_WRAPPER(aar00_ccm00, C_CCM_Handler);

#define REGISTER(irqn, irqp, wrapper) do { \
	(void)irq_connect_dynamic(irqn, irqp, wrapper##_wrapper, NULL, 0); \
	irq_enable(irqn); \
} while (0)

static int softdevice_bsim_irq_init(void)
{
	REGISTER(GRTC_3_IRQn, SD_IRQ_PRIO_HIGH, grtc3);
	REGISTER(TIMER10_IRQn, SD_IRQ_PRIO_HIGH, timer10);
	REGISTER(RADIO_0_IRQn, SD_IRQ_PRIO_HIGH, radio0);

	REGISTER(SWI00_IRQn, SD_IRQ_PRIO_LOW, swi00);
	REGISTER(CLOCK_POWER_IRQn, SD_IRQ_PRIO_LOW, clock_power);
	REGISTER(ECB00_IRQn, SD_IRQ_PRIO_LOW, ecb00);
	REGISTER(AAR00_CCM00_IRQn, SD_IRQ_PRIO_LOW, aar00_ccm00);

	return 0;
}

SYS_INIT(softdevice_bsim_irq_init, APPLICATION, 0);
