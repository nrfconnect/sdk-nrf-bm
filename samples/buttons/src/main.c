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

#if CONFIG_SOFTDEVICE
#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#endif /* CONFIG_SOFTDEVICE */

#include <board-config.h>

LOG_MODULE_REGISTER(buttons_sample, LOG_LEVEL_INF);

#define PIN_BTN_0 BOARD_PIN_BTN_0
#define PIN_BTN_1 BOARD_PIN_BTN_1
#define PIN_BTN_2 BOARD_PIN_BTN_2
#define PIN_BTN_3 BOARD_PIN_BTN_3

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

#if CONFIG_SOFTDEVICE
	err = nrf_sdh_enable_request();
	if (err) {
		printk("Failed to enable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice enabled\n");

	err = nrf_sdh_ble_enable(CONFIG_NRF_SDH_BLE_CONN_TAG);
	if (err) {
		printk("Failed to enable BLE, err %d\n", err);
		return -1;
	}
#endif /* CONFIG_SOFTDEVICE */

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
		LOG_ERR("Failed to initialize buttons, err: %d", err);
		return err;
	}

	err = lite_buttons_enable();
	if (err) {
		LOG_ERR("Failed to enable buttons, err: %d", err);
		return err;
	}

	LOG_INF("Buttons initialized, press button 3 to terminate");

	while (running) {
		/* Sleep */
		__WFE();
	}

	err = lite_buttons_deinit();
	if (err) {
		LOG_ERR("Failed to deinitialize buttons, err: %d", err);
		return err;
	}

	LOG_INF("Bye");

	return 0;
}
