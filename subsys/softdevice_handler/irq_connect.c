/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

LOG_MODULE_REGISTER(sdh_irq_connect, CONFIG_NRF_SDH_LOG_LEVEL);

#if CONFIG_SOC_SERIES_NRF54LX
#include "irq_connect.h"

extern void CLOCK_POWER_SD_IRQHandler(void);
extern void RADIO_0_IRQHandler(void);
extern void TIMER10_IRQHandler(void);
extern void GRTC_3_IRQHandler(void);
extern void ECB00_IRQHandler(void);
extern void AAR00_CCM00_IRQHandler(void);
extern void SWI00_IRQHandler(void);

/* In irq_forward.s */
extern void SVC_Handler(void);
extern void CallSoftDeviceResetHandler(void);

uint32_t irq_forwarding_enabled_magic_number_holder;
uint32_t softdevice_vector_forward_address;

static void sd_enable_irq_forwarding(void)
{
	softdevice_vector_forward_address = FIXED_PARTITION_OFFSET(softdevice_partition);
#ifdef CONFIG_BOOTLOADER_MCUBOOT
	softdevice_vector_forward_address += CONFIG_ROM_START_OFFSET;
#endif

	LOG_INF("SoftDevice forward address: 0x%x", softdevice_vector_forward_address);

	while (LOG_PROCESS()) {
	}

	CallSoftDeviceResetHandler();
	irq_forwarding_enabled_magic_number_holder = IRQ_FORWARDING_ENABLED_MAGIC_NUMBER;
}

static int irq_init(void)
{
#define PRIO_HIGH 0	/* SoftDevice high priority interrupt */
#define PRIO_LOW 4	/* SoftDevice low priority interrupt */

	/* IRQ_ZERO_LATENCY with CONFIG_ZERO_LATENCY_LEVELS equal to 1 (default) forces the priority
	 * level to 0, ignoring the specified priority.
	 * On `sd_softdevice_enable()`, the SoftDevice will override the necessary interrupts it
	 * uses internally with the priority levels it needs.
	 */
	IRQ_DIRECT_CONNECT(RADIO_0_IRQn, PRIO_HIGH, RADIO_0_IRQHandler, IRQ_ZERO_LATENCY);
	IRQ_DIRECT_CONNECT(TIMER10_IRQn, PRIO_HIGH, TIMER10_IRQHandler, IRQ_ZERO_LATENCY);
	IRQ_DIRECT_CONNECT(GRTC_3_IRQn, PRIO_HIGH, GRTC_3_IRQHandler, IRQ_ZERO_LATENCY);

	/* These are not zero latency. */
	IRQ_DIRECT_CONNECT(AAR00_CCM00_IRQn, PRIO_LOW, AAR00_CCM00_IRQHandler, 0);
	IRQ_DIRECT_CONNECT(CLOCK_POWER_IRQn, PRIO_LOW, CLOCK_POWER_SD_IRQHandler, 0);
	IRQ_DIRECT_CONNECT(ECB00_IRQn, PRIO_LOW, ECB00_IRQHandler, 0);
	IRQ_DIRECT_CONNECT(SWI00_IRQn, PRIO_LOW, SWI00_IRQHandler, 0);

	NVIC_SetPriority(SVCall_IRQn, PRIO_LOW);

	sd_enable_irq_forwarding();

	return 0;
}

__attribute__((weak)) void C_HardFault_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_TIMER10_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_GRTC_3_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_SWI00_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_RADIO_0_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_ECB00_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_AAR00_CCM00_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_CLOCK_POWER_SD_Handler(void)
{
#if defined(CONFIG_NRFX_POWER) || defined(CONFIG_NRFX_CLOCK)
	extern void CLOCK_POWER_IRQHandler(void);
	CLOCK_POWER_IRQHandler();
#else
	__asm__("SVC 255");
#endif
}

#endif /* CONFIG_SOC_SERIES_NRF54LX */

SYS_INIT(irq_init, APPLICATION, 0);
