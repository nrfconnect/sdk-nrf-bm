/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h> /* k_busy_wait() */
#include <zephyr/sys_clock.h> /* USEC_PER_MSEC */

#if CONFIG_SOFTDEVICE
#include <bm_sdh.h>
#include <bm_sdh_ble.h>
#endif /* CONFIG_SOFTDEVICE */

#include <hal/nrf_gpio.h>

#include <board-config.h>

static void led_init(void)
{
	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
}

static void led_on(void)
{
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE ? 1 : 0);
}

static void led_off(void)
{
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE ? 0 : 1);
}

int main(void)
{
	printk("LEDs sample started\n");

#if CONFIG_SOFTDEVICE
	int err;

	err = bm_sdh_enable_request();
	if (err) {
		printk("Failed to enable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice enabled\n");

	err = bm_sdh_ble_enable(CONFIG_BM_SDH_BLE_CONN_TAG);
	if (err) {
		printk("Failed to enable BLE, err %d\n", err);
		return -1;
	}
#endif /* CONFIG_SOFTDEVICE */

	/* Initialize the LED */
	led_init();

	while (true) {
		/* Turn the LED on */
		led_on();

		k_busy_wait(500 * USEC_PER_MSEC);

		/* Turn the LED off */
		led_off();

		k_busy_wait(500 * USEC_PER_MSEC);


	}

	return 0;
}
