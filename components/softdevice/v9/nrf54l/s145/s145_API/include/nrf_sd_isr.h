/*
 * Copyright (c) Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @addtogroup nrf_sdm_api
  @{ */

#ifndef NRF_SD_ISR_VECTORS_H__
#define NRF_SD_ISR_VECTORS_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup nrf_sd_isr_vectors SoftDevice Interrupt Vector Table Offsets
 * @{
 *
 *  @brief SoftDevice interrupt vector table offsets.
 *         The SoftDevice interrupt vector table contains only the addresses of the interrupt handlers
 *         required by the SoftDevice. The table is located at the SoftDevice base address. When the SoftDevice
 *         is enabled, the application must forward the interrupts corresponding to the defined offsets
 *         to the SoftDevice. The address of the interrupt handler is located at the SoftDevice base address plus the offset.
 *
 *         An example of how to forward an interrupt to the SoftDevice is shown below:
 *
 *         @code
 *         SVC_Handler:
 *           LDR   R0, =NRF_SD_ISR_OFFSET_SVC
 *           LDR   R1, =SOFTDEVICE_BASE_ADDRESS
 *           LDR   R1, [R1, R0]
 *           BX    R1
 *         @endcode
 */
#define NRF_SD_ISR_OFFSET_RESET          (0x0000) /**< SoftDevice Reset Handler address offset */
#define NRF_SD_ISR_OFFSET_HARDFAULT      (0x0004) /**< SoftDevice HardFault Handler address offset */
#define NRF_SD_ISR_OFFSET_SVC            (0x0008) /**< SoftDevice SVC Handler address offset */
#define NRF_SD_ISR_OFFSET_SWI00          (0x000c) /**< SoftDevice SWI00 Handler address offset */
#define NRF_SD_ISR_OFFSET_AAR00_CCM00    (0x0010) /**< SoftDevice AAR00_CCM00 Handler address offset */
#define NRF_SD_ISR_OFFSET_ECB00          (0x0014) /**< SoftDevice ECB00 Handler address offset */
#define NRF_SD_ISR_OFFSET_TIMER10        (0x0018) /**< SoftDevice TIMER10 Handler address offset */
#define NRF_SD_ISR_OFFSET_RADIO_0        (0x001c) /**< SoftDevice RADIO_0 Handler address offset */
#define NRF_SD_ISR_OFFSET_GRTC_3         (0x0020) /**< SoftDevice GRTC_3 Handler address offset */
#define NRF_SD_ISR_OFFSET_CLOCK_POWER    (0x0024) /**< SoftDevice CLOCK_POWER Handler address offset */
/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* NRF_SD_ISR_VECTORS_H__ */

/**
 * @}
 */
