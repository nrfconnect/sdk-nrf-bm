/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/printk.h>
#include <bm_buttons.h>

#include <board-config.h>
#include <zephyr/logging/log_ctrl.h>

static volatile bool running;

static void button_handler(uint8_t pin, uint8_t action)
{
	printk("Button event callback: %d, %d\n", pin, action);

	if (pin == BOARD_PIN_BTN_3) {
		running = false;
	}
}

int main(void)
{
	int err;

	printk("Buttons sample started\n");

	running = true;

	struct bm_buttons_config configs[4] = {
		{
			.pin_number = BOARD_PIN_BTN_0,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		{
			.pin_number = BOARD_PIN_BTN_1,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		{
			.pin_number = BOARD_PIN_BTN_2,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		{
			.pin_number = BOARD_PIN_BTN_3,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
	};

	err = bm_buttons_init(configs, ARRAY_SIZE(configs), BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		printk("Failed to initialize buttons, err: %d\n", err);
		goto idle;
	}

	err = bm_buttons_enable();
	if (err) {
		printk("Failed to enable buttons, err: %d\n", err);
		goto idle;
	}

	printk("Buttons initialized, press button 3 to terminate\n");

	while (running) {
		while (LOG_PROCESS()) {
		}

		/* Sleep */
		__WFE();
	}

	err = bm_buttons_deinit();
	if (err) {
		printk("Failed to deinitialize buttons, err: %d\n", err);
		goto idle;
	}

	printk("Bye\n");

idle:
	while (true) {
		while (LOG_PROCESS()) {
		}

		/* Sleep */
		__WFE();
	}

	return 0;
}
