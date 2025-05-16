/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup nRF54L15 Template Board Configuration
 * @{
 */

#ifndef __BM_NRF54L15_TEMPLATE_BOARD_CONFIG
#define __BM_NRF54L15_TEMPLATE_BOARD_CONFIG

#include <hal/nrf_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPIO_ACTIVE_HIGH 1

/* Change this file to match your own board. */

#ifndef BOARD_PIN_BTN_0
#define BOARD_PIN_BTN_0 NRF_PIN_PORT_TO_PIN_NUMBER(13, 1)
#endif
#ifndef BOARD_PIN_BTN_1
#define BOARD_PIN_BTN_1 NRF_PIN_PORT_TO_PIN_NUMBER(9, 1)
#endif
#ifndef BOARD_PIN_BTN_2
#define BOARD_PIN_BTN_2 NRF_PIN_PORT_TO_PIN_NUMBER(8, 1)
#endif
#ifndef BOARD_PIN_BTN_3
#define BOARD_PIN_BTN_3 NRF_PIN_PORT_TO_PIN_NUMBER(4, 0)
#endif

#ifndef BOARD_PIN_LED_0
#define BOARD_PIN_LED_0 NRF_PIN_PORT_TO_PIN_NUMBER(9, 2)
#endif
#ifndef BOARD_PIN_LED_1
#define BOARD_PIN_LED_1 NRF_PIN_PORT_TO_PIN_NUMBER(10, 1)
#endif
#ifndef BOARD_PIN_LED_2
#define BOARD_PIN_LED_2 NRF_PIN_PORT_TO_PIN_NUMBER(7, 2)
#endif
#ifndef BOARD_PIN_LED_3
#define BOARD_PIN_LED_3 NRF_PIN_PORT_TO_PIN_NUMBER(14, 1)
#endif

#ifndef BOARD_LED_ACTIVE_STATE
#define BOARD_LED_ACTIVE_STATE GPIO_ACTIVE_HIGH
#endif

/* UART Logger configuration */
#ifndef BOARD_CONSOLE_UARTE_INST
#define BOARD_CONSOLE_UARTE_INST 20
#endif

#ifndef BOARD_CONSOLE_UARTE_PIN_TX
#define BOARD_CONSOLE_UARTE_PIN_TX NRF_PIN_PORT_TO_PIN_NUMBER(4, 1)
#endif
#ifndef BOARD_CONSOLE_UARTE_PIN_CTS
#define BOARD_CONSOLE_UARTE_PIN_CTS NRF_PIN_PORT_TO_PIN_NUMBER(7, 1)
#endif

/* Application UART configuration */
#ifndef BOARD_APP_UARTE_INST
#define BOARD_APP_UARTE_INST 30
#endif

#ifndef BOARD_APP_UARTE_PIN_TX
#define BOARD_APP_UARTE_PIN_TX NRF_PIN_PORT_TO_PIN_NUMBER(6, 0)
#endif
#ifndef BOARD_APP_UARTE_PIN_RX
#define BOARD_APP_UARTE_PIN_RX NRF_PIN_PORT_TO_PIN_NUMBER(8, 0)
#endif
#ifndef BOARD_APP_UARTE_PIN_RTS
#define BOARD_APP_UARTE_PIN_RTS NRF_PIN_PORT_TO_PIN_NUMBER(5, 0)
#endif
#ifndef BOARD_APP_UARTE_PIN_CTS
#define BOARD_APP_UARTE_PIN_CTS NRF_PIN_PORT_TO_PIN_NUMBER(7, 0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BM_NRF54L15_TEMPLATE_BOARD_CONFIG */

/** @} */
