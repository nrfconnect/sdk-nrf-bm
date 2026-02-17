/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm/bm_buttons.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <hal/nrf_gpio.h>
#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_BUTTONS_LOG_LEVEL);

static volatile bool running;

static void button_handler(uint8_t pin, uint8_t action)
{
	LOG_INF("Button event callback: %d, %d", pin, action);

	if (pin == BOARD_PIN_BTN_3) {
		running = false;
	}
}

int main(void)
{
	int err;

	LOG_INF("Buttons sample started");

	running = true;

	struct bm_buttons_config configs[] = {
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
		LOG_ERR("Failed to initialize buttons, err: %d", err);
		goto idle;
	}

	err = bm_buttons_enable();
	if (err) {
		LOG_ERR("Failed to enable buttons, err: %d", err);
		goto idle;
	}

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);

	LOG_INF("Buttons sample initialized, press button 3 to terminate");

	while (running) {
		log_flush();

		k_cpu_idle();
	}

	err = bm_buttons_deinit();
	if (err) {
		LOG_ERR("Failed to deinitialize buttons, err: %d", err);
		goto idle;
	}

	LOG_INF("Bye");

idle:
	while (true) {
		log_flush();

		k_cpu_idle();
	}

	return 0;
}
