/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h> /* k_busy_wait() */
#include <zephyr/sys_clock.h> /* USEC_PER_MSEC */
#include <zephyr/logging/log_ctrl.h>

#include <hal/nrf_gpio.h>

#include <board-config.h>

static void led_init(void)
{
	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
}

static void led_on(void)
{
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
}

static void led_off(void)
{
	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);
}

int main(void)
{
	printk("LEDs sample started\n");

	/* Initialize the LED */
	led_init();

	while (true) {
		while (LOG_PROCESS()) {
		}

		/* Turn the LED on */
		led_on();

		k_busy_wait(500 * USEC_PER_MSEC);

		/* Turn the LED off */
		led_off();

		k_busy_wait(500 * USEC_PER_MSEC);
	}

	return 0;
}
