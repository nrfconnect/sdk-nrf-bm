/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/printk.h>
#include <bm_buttons.h>

#if CONFIG_SOFTDEVICE
#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#endif /* CONFIG_SOFTDEVICE */

#include <board-config.h>

#define PIN_BTN_0 BOARD_PIN_BTN_0
#define PIN_BTN_1 BOARD_PIN_BTN_1
#define PIN_BTN_2 BOARD_PIN_BTN_2
#define PIN_BTN_3 BOARD_PIN_BTN_3

static volatile bool running;

static void button_handler(uint8_t pin, uint8_t action)
{
	printk("Button event callback: %d, %d\n", pin, action);

	if (pin == PIN_BTN_3) {
		running = false;
	}
}

int main(void)
{
	int err;

	printk("Buttons sample started\n");

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

	struct bm_buttons_config configs[4] = {
		{
			.pin_number = PIN_BTN_0,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		{
			.pin_number = PIN_BTN_1,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		{
			.pin_number = PIN_BTN_2,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
		{
			.pin_number = PIN_BTN_3,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
	};

	err = bm_buttons_init(configs, ARRAY_SIZE(configs), BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		printk("Failed to initialize buttons, err: %d\n", err);
		return err;
	}

	err = bm_buttons_enable();
	if (err) {
		printk("Failed to enable buttons, err: %d\n", err);
		return err;
	}

	printk("Buttons initialized, press button 3 to terminate\n");

	while (running) {
		/* Sleep */
		__WFE();
	}

	err = bm_buttons_deinit();
	if (err) {
		printk("Failed to deinitialize buttons, err: %d\n", err);
		return err;
	}

	printk("Bye\n");

	return 0;
}
