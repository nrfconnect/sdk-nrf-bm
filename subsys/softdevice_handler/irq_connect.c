/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/storage/flash_map.h>

#if CONFIG_SOC_SERIES_NRF52X
#include <zephyr/linker/linker-defs.h>
#include <nrf_sdm.h>
#include <nrfx_gpiote.h>

void relocate_vector_table(void)
{
	/* Empty, but needed */
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
	__ASSERT(err == NRF_SUCCESS, "Failed to set the vector table, nrf_error %#x", err);

	return (err == NRF_SUCCESS) ? 0 : -EIO;
}

#elif CONFIG_SOC_SERIES_NRF54LX
#include "irq_connect.h"

extern void CLOCK_POWER_IRQHandler(void);
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

	CallSoftDeviceResetHandler();
	irq_forwarding_enabled_magic_number_holder = IRQ_FORWARDING_ENABLED_MAGIC_NUMBER;
}

static int irq_init(void)
{
#define PRIO_HIGH 0	/* SoftDevice high priority interrupt */
#define PRIO_LOW 4	/* SoftDevice low priority interrupt */

	IRQ_DIRECT_CONNECT(CLOCK_POWER_IRQn, PRIO_LOW, CLOCK_POWER_IRQHandler, IRQ_ZERO_LATENCY);
	IRQ_DIRECT_CONNECT(RADIO_0_IRQn, PRIO_HIGH, RADIO_0_IRQHandler, IRQ_ZERO_LATENCY);
	IRQ_DIRECT_CONNECT(TIMER10_IRQn, PRIO_HIGH, TIMER10_IRQHandler, IRQ_ZERO_LATENCY);
	IRQ_DIRECT_CONNECT(GRTC_3_IRQn, PRIO_HIGH, GRTC_3_IRQHandler, IRQ_ZERO_LATENCY);
	IRQ_DIRECT_CONNECT(ECB00_IRQn, PRIO_LOW, ECB00_IRQHandler, IRQ_ZERO_LATENCY);
	IRQ_DIRECT_CONNECT(AAR00_CCM00_IRQn, PRIO_LOW, AAR00_CCM00_IRQHandler, IRQ_ZERO_LATENCY);
	IRQ_DIRECT_CONNECT(SWI00_IRQn, PRIO_LOW, SWI00_IRQHandler, IRQ_ZERO_LATENCY);

	NVIC_SetPriority(SVCall_IRQn, PRIO_LOW);

	sd_enable_irq_forwarding();

	return 0;
}

__attribute__((weak)) void C_HardFault_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_TIMER0_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_RTC0_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_SIGNALLING_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_RADIO_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_RNG_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_ECB_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_CCM_Handler(void)
{
	__asm__("SVC 255");
}

__attribute__((weak)) void C_POWER_CLOCK_Handler(void)
{
	__asm__("SVC 255");
}

#endif

SYS_INIT(irq_init, PRE_KERNEL_1, 0);
