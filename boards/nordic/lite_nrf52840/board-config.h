/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup Lite nRF52840 Board Configuration
 * @{
 */

#ifndef __LITE_NRF52840_BOARD_CONFIG
#define __LITE_NRF52840_BOARD_CONFIG

#include <hal/nrf_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This board is for internal development use only.
 * nRF52840 is not planned to be supported in the final version of NCS Lite.
 */

#ifndef BOARD_PIN_BTN_0
#define BOARD_PIN_BTN_0 NRF_PIN_PORT_TO_PIN_NUMBER(11, 0)
#endif
#ifndef BOARD_PIN_BTN_1
#define BOARD_PIN_BTN_1 NRF_PIN_PORT_TO_PIN_NUMBER(12, 0)
#endif
#ifndef BOARD_PIN_BTN_2
#define BOARD_PIN_BTN_2 NRF_PIN_PORT_TO_PIN_NUMBER(24, 0)
#endif
#ifndef BOARD_PIN_BTN_3
#define BOARD_PIN_BTN_3 NRF_PIN_PORT_TO_PIN_NUMBER(25, 0)
#endif

#ifndef BOARD_PIN_LED_0
#define BOARD_PIN_LED_0 NRF_PIN_PORT_TO_PIN_NUMBER(13, 0)
#endif
#ifndef BOARD_PIN_LED_1
#define BOARD_PIN_LED_1 NRF_PIN_PORT_TO_PIN_NUMBER(14, 0)
#endif
#ifndef BOARD_PIN_LED_2
#define BOARD_PIN_LED_2 NRF_PIN_PORT_TO_PIN_NUMBER(15, 0)
#endif
#ifndef BOARD_PIN_LED_3
#define BOARD_PIN_LED_3 NRF_PIN_PORT_TO_PIN_NUMBER(16, 0)
#endif

/* UART Logger configuration */
#ifndef LITE_UARTE_CONSOLE_UARTE_INST
#define LITE_UARTE_CONSOLE_UARTE_INST 0
#endif

#ifndef LITE_UARTE_CONSOLE_PIN_TX
#define LITE_UARTE_CONSOLE_PIN_TX NRF_PIN_PORT_TO_PIN_NUMBER(6, 0)
#endif
#ifndef LITE_UARTE_CONSOLE_PIN_RTS
#define LITE_UARTE_CONSOLE_PIN_RTS NRF_PIN_PORT_TO_PIN_NUMBER(5, 0)
#endif
#ifndef LITE_UARTE_CONSOLE_PIN_CTS
#define LITE_UARTE_CONSOLE_PIN_CTS NRF_PIN_PORT_TO_PIN_NUMBER(7, 0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __LITE_NRF52840_BOARD_CONFIG */

/** @} */
