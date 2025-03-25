/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>    /* k_busy_wait() */
#include <zephyr/sys_clock.h> /* K_MSEC() */
#include <zephyr/logging/log.h>

#include <lite_timer.h>
#include <lite_buttons.h>

LOG_MODULE_REGISTER(buttons_sample, LOG_LEVEL_INF);

#if defined(CONFIG_SOC_SERIES_NRF52X)
#define PIN_BTN_0 NRF_PIN_PORT_TO_PIN_NUMBER(11, 0)
#define PIN_BTN_1 NRF_PIN_PORT_TO_PIN_NUMBER(12, 0)
#define PIN_BTN_2 NRF_PIN_PORT_TO_PIN_NUMBER(24, 0)
#define PIN_BTN_3 NRF_PIN_PORT_TO_PIN_NUMBER(25, 0)
#elif defined(CONFIG_SOC_SERIES_NRF54LX)
#define PIN_BTN_0 NRF_PIN_PORT_TO_PIN_NUMBER(13, 1)
#define PIN_BTN_1 NRF_PIN_PORT_TO_PIN_NUMBER(9, 1)
#define PIN_BTN_2 NRF_PIN_PORT_TO_PIN_NUMBER(8, 1)
#define PIN_BTN_3 NRF_PIN_PORT_TO_PIN_NUMBER(4, 0)
#endif

static volatile bool running;

static void button_handler(uint8_t pin, uint8_t action)
{
	LOG_INF("Button event callback: %d, %d", pin, action);

	if (pin == PIN_BTN_3) {
		running = false;
	}
}

int main(void)
{
	int err;

	LOG_INF("Buttons sample started\n");


	running = true;

	struct lite_buttons_config configs[4] = {
		{
			.pin_number = PIN_BTN_0,
			.active_state = LITE_BUTTONS_ACTIVE_LOW,
			.pull_config = LITE_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		{
			.pin_number = PIN_BTN_1,
			.active_state = LITE_BUTTONS_ACTIVE_LOW,
			.pull_config = LITE_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		{
			.pin_number = PIN_BTN_2,
			.active_state = LITE_BUTTONS_ACTIVE_LOW,
			.pull_config = LITE_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		{
			.pin_number = PIN_BTN_3,
			.active_state = LITE_BUTTONS_ACTIVE_LOW,
			.pull_config = LITE_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
	};

	err = lite_buttons_init(configs, ARRAY_SIZE(configs), LITE_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		LOG_ERR("lite_buttons_init error: %d", err);
		return err;
	}

	err = lite_buttons_enable();
	if (err) {
		LOG_ERR("lite_buttons_enable error: %d", err);
		return err;
	}

	LOG_INF("Buttons initialized, press button 4 to terminate");

	while (running) {
		/* Sleep */
		__WFE();
	}

	err = lite_buttons_deinit();
	if (err) {
		LOG_ERR("lite_buttons_deinit error: %d", err);
		return err;
	}

	LOG_INF("Terminating.");

	return 0;
}
