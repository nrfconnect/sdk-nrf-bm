/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * SoftDevice Interrupts
 * This section should contain handlers for all interrupts needed by the SoftDevice.
 *
 * The handlers below are used by the SoftDevice implementation.
 * When the SoftDevice is enabled, the interrupts are consumed by the SoftDevice.
 * When the SoftDevice is disabled, all interrupts should be forwarded to the
 * application, this functionality is handled in the ConsumeOrForwardIRQ function.
 */

    .syntax unified
    .arch armv8-m.main

#include "irq_connect.h"
#include "nrf_sd_isr.h"

    .section .text.STACK_INTERRUPTS, "x"

    /* Checks if the SoftDevice is enabled/disabled.
     * If the SoftDevice is enabled the IRQ is forwarded to it.
     * Otherwise, it is handled as normal by the application.
     *
     * Precondition: R3 contains the address to jump if the SoftDevice is disabled
     */
    .thumb
    .thumb_func
    .align 1
    .globl  ConsumeOrForwardIRQ
    .type   ConsumeOrForwardIRQ, %function
ConsumeOrForwardIRQ:
    LDR   R2, =irq_forwarding_enabled_magic_number_holder
    LDR   R2, [R2]
    LDR   R1, =IRQ_FORWARDING_ENABLED_MAGIC_NUMBER
    CMP   R2, R1
    /* Forward the IRQ if the SoftDevice is enabled */
    BEQ   ForwardIRQ_ForwardToSoftDevice
    /* Jump to the application address if SoftDevice disabled */
    BX    R3

    .thumb
    .thumb_func
    .align 1
    .globl  SVC_Handler
    .type   SVC_Handler, %function
SVC_Handler:
    LDR   R0, =NRF_SD_ISR_OFFSET_SVC
ForwardIRQ_ForwardToSoftDevice:
    /* SoftDevice interrupt vector is located in softdevice_vector_forward_address */
    LDR   R1, =softdevice_vector_forward_address
    LDR   R1, [R1]
    LDR   R1, [R1, R0]  /* Vector value (with Thumb marker in bit 0) */
    BX    R1            /* Forward */

    .size   SVC_Handler, . - SVC_Handler
    .balign


    /* Call SoftDevice reset handler. */
    .thumb
    .thumb_func
    .align 1
    .globl  CallSoftDeviceResetHandler
    .type   CallSoftDeviceResetHandler, %function
CallSoftDeviceResetHandler:
    LDR   R0, =NRF_SD_ISR_OFFSET_RESET
    /* SoftDevice interrupt vector is located in softdevice_vector_forward_address */
    LDR   R1, =softdevice_vector_forward_address
    LDR   R1, [R1]
    /* Vector value (with Thumb marker in bit 0) */
    LDR   R1, [R1, R0]
    PUSH {LR}
    /* Forward */
    BLX   R1
    POP {LR}
    BX    LR

    .thumb
    .thumb_func
    .align 1
    .global HardFault_Handler
    .type HardFault_Handler, "function"
HardFault_Handler:
    LDR   R3, =C_HardFault_Handler
    LDR   R0, =NRF_SD_ISR_OFFSET_HARDFAULT
    LDR   R1, =ConsumeOrForwardIRQ
    BX    R1

    .thumb
    .thumb_func
    .align 1
    .global CLOCK_POWER_IRQHandler
    .type CLOCK_POWER_IRQHandler, "function"
CLOCK_POWER_IRQHandler:
    LDR   R3, =C_POWER_CLOCK_Handler
    LDR   R0, =NRF_SD_ISR_OFFSET_CLOCK_POWER
    LDR   R1, =ConsumeOrForwardIRQ
    BX    R1

    .thumb
    .thumb_func
    .align 1
    .global RADIO_0_IRQHandler
    .type RADIO_0_IRQHandler, "function"
RADIO_0_IRQHandler:
    LDR   R3, =C_RADIO_Handler
    LDR   R0, =NRF_SD_ISR_OFFSET_RADIO_0
    LDR   R1, =ConsumeOrForwardIRQ
    BX    R1

    .thumb
    .thumb_func
    .align 1
    .global TIMER10_IRQHandler
    .type TIMER10_IRQHandler, "function"
TIMER10_IRQHandler:
    LDR   R3, =C_TIMER0_Handler
    LDR   R0, =NRF_SD_ISR_OFFSET_TIMER10
    LDR   R1, =ConsumeOrForwardIRQ
    BX    R1

    .thumb
    .thumb_func
    .align 1
    .global GRTC_3_IRQHandler
    .type GRTC_3_IRQHandler, "function"
GRTC_3_IRQHandler:
    LDR   R3, =C_RTC0_Handler
    LDR   R0, =NRF_SD_ISR_OFFSET_GRTC_3
    LDR   R1, =ConsumeOrForwardIRQ
    BX    R1

    .thumb
    .thumb_func
    .align 1
    .global ECB00_IRQHandler
    .type ECB00_IRQHandler, "function"
ECB00_IRQHandler:
    LDR   R3, =C_ECB_Handler
    LDR   R0, =NRF_SD_ISR_OFFSET_ECB00
    LDR   R1, =ConsumeOrForwardIRQ
    BX    R1

    .thumb
    .thumb_func
    .align 1
    .global AAR00_CCM00_IRQHandler
    .type AAR00_CCM00_IRQHandler, "function"
AAR00_CCM00_IRQHandler:
    LDR   R3, =C_CCM_Handler
    LDR   R0, =NRF_SD_ISR_OFFSET_AAR00_CCM00
    LDR   R1, =ConsumeOrForwardIRQ
    BX    R1

    .thumb
    .thumb_func
    .align 1
    .global SWI00_IRQHandler
    .type SWI00_IRQHandler, "function"
SWI00_IRQHandler:
    LDR   R3, =C_SIGNALLING_Handler
    LDR   R0, =NRF_SD_ISR_OFFSET_SWI00
    LDR   R1, =ConsumeOrForwardIRQ
    BX    R1

    .balign
    .end   /* End of file */
