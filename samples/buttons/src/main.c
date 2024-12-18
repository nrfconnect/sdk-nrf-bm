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

/* Buttons #1-4 of the nRF52840 DK. */
#define PIN_BTN_0 11
#define PIN_BTN_1 12
#define PIN_BTN_2 24
#define PIN_BTN_3 25

#define DETECTION_TIME_US (2 * LITE_TIMER_MIN_TIMEOUT_US + 1)

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

	LOG_INF("Buttons sample on %s", CONFIG_BOARD);
	LOG_INF("Press button 4 to terminate.");

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

	err = lite_buttons_init(configs, ARRAY_SIZE(configs), DETECTION_TIME_US);
	if (err) {
		LOG_ERR("lite_buttons_init error: %d", err);
		return err;
	}

	err = lite_buttons_enable();
	if (err) {
		LOG_ERR("lite_buttons_enable error: %d", err);
		return err;
	}

	while (running) {
		/* Highly responsive to detect quickly when to stop running. */
		k_busy_wait(DETECTION_TIME_US);
	}

	err = lite_buttons_deinit();
	if (err) {
		LOG_ERR("lite_buttons_deinit error: %d", err);
		return err;
	}

	LOG_INF("Terminating.");

	return 0;
}
