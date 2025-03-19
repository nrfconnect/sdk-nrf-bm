/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <lite_timer.h>
#include <zephyr/sys/printk.h>

#if CONFIG_SOFTDEVICE
#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#endif /* CONFIG_SOFTDEVICE */

#define PERIODIC_TIMER_TICKS LITE_TIMER_MS_TO_TICKS(CONFIG_PERIODIC_TIMER_INTERVAL_MS)
#define HELLO_TIMER_TICKS    LITE_TIMER_MS_TO_TICKS(CONFIG_HELLO_TIMER_DURATION_MS)
#define WORLD_TIMER_TICKS    LITE_TIMER_MS_TO_TICKS(CONFIG_WORLD_TIMER_DURATION_MS)
#define BYE_TIMER_TICKS      LITE_TIMER_MS_TO_TICKS(CONFIG_BYE_TIMER_DURATION_MS)

static struct lite_timer oneshot_timer;
static struct lite_timer periodic_timer;
static bool done;

static const char *const hello_str = "Hello";
static const char *const world_str = "world!";
static const char *const bye_str = "bye!\n";

/**
 * @brief Timeout handler for single-shot timer.
 *
 * Restarts the oneshot timer two times with different strings. Then stops the periodic timer.
 */
static void oneshot_timeout_handler(void *context)
{
	static int cnt;
	int err;
	const char *const str = (const char *const)context;

	printk("%s", str);

	if (cnt == 0) {
		err = lite_timer_start(&oneshot_timer, WORLD_TIMER_TICKS, (void *)world_str);
		if (err) {
			printk("Failed to start oneshot timer, err %d\n", err);
		}
	} else if (cnt == 1) {
		err = lite_timer_start(&oneshot_timer, BYE_TIMER_TICKS, (void *)bye_str);
		if (err) {
			printk("Failed to start oneshot timer, err %d\n", err);
		}
	} else {
		err = lite_timer_stop(&periodic_timer);
		if (err) {
			printk("Failed to stop periodic timer, err %d\n", err);
		}

		done = true;
	}

	cnt++;
}

/**
 * @brief Timeout handler for repeated timer.
 *
 * Prints punctuation on timeout.
 */
static void periodic_timeout_handler(void *context)
{
	ARG_UNUSED(context);

	printk(".");
}

int main(void)
{
	int err;

#if CONFIG_SOFTDEVICE
#define CONN_TAG 1
	uint32_t ram_start;

	err = nrf_sdh_enable_request();
	if (err) {
		printk("Failed to enable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice enabled\n");

	nrf_sdh_ble_app_ram_start_get(&ram_start);

	err = nrf_sdh_ble_default_cfg_set(CONN_TAG);
	if (err) {
		printk("Failed to setup default configuration, err %d\n", err);
		return -1;
	}

	err = nrf_sdh_ble_enable(ram_start);
	if (err) {
		printk("Failed to enable BLE, err %d\n", err);
		return -1;
	}
#endif /* CONFIG_SOFTDEVICE */

	err = lite_timer_init(&periodic_timer, LITE_TIMER_MODE_REPEATED, periodic_timeout_handler);
	if (err) {
		printk("Failed to initialize periodic timer, err %d\n", err);
		return -1;
	}

	err = lite_timer_init(&oneshot_timer, LITE_TIMER_MODE_SINGLE_SHOT, oneshot_timeout_handler);
	if (err) {
		printk("Failed to initialize oneshot timer, err %d\n", err);
		return -1;
	}

	err = lite_timer_start(&periodic_timer, PERIODIC_TIMER_TICKS, NULL);
	if (err) {
		printk("Failed to start periodic timer, err %d\n", err);
		return -1;
	}

	err = lite_timer_start(&oneshot_timer, HELLO_TIMER_TICKS, (void *)hello_str);
	if (err) {
		printk("Failed to start oneshot timer, err %d\n", err);
		return -1;
	}

	printk("Timer sample started\n");

	while (!done) {
		/* Sleep */
		__WFE();
	}

#if CONFIG_SOFTDEVICE
	err = nrf_sdh_disable_request();
	if (err) {
		printk("Failed to disable SoftDevice, err %d\n", err);
		return -1;
	}

	printk("SoftDevice disabled\n");
#endif /* CONFIG_SOFTDEVICE */

	return 0;
}
