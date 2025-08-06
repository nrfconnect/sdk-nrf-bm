/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/printk.h>

#define PERIODIC_TIMER_TICKS K_MSEC(CONFIG_PERIODIC_TIMER_INTERVAL_MS)
#define HELLO_TIMER_TICKS    K_MSEC(CONFIG_HELLO_TIMER_DURATION_MS)
#define WORLD_TIMER_TICKS    K_MSEC(CONFIG_WORLD_TIMER_DURATION_MS)
#define BYE_TIMER_TICKS      K_MSEC(CONFIG_BYE_TIMER_DURATION_MS)

static struct k_timer oneshot_timer;
static struct k_timer periodic_timer;
static bool done;

static const char *const hello_str = "Hello";
static const char *const world_str = "world!";
static const char *const bye_str = "bye!\n";

/**
 * @brief Timeout handler for single-shot timer.
 *
 * Restarts the oneshot timer two times with different strings. Then stops the periodic timer.
 */
static void oneshot_timeout_handler(struct k_timer *timer_id)
{
	static int cnt;
	const char *const str = k_timer_user_data_get(timer_id);

	printk("%s", str);

	if (cnt == 0) {
		k_timer_user_data_set(&oneshot_timer, (void *)world_str);
		k_timer_start(&oneshot_timer, WORLD_TIMER_TICKS, K_FOREVER);
	} else if (cnt == 1) {
		k_timer_user_data_set(&oneshot_timer, (void *)bye_str);
		k_timer_start(&oneshot_timer, BYE_TIMER_TICKS, K_FOREVER);
	} else {
		k_timer_stop(&periodic_timer);
		done = true;
	}

	cnt++;
}

/**
 * @brief Timeout handler for repeated timer.
 *
 * Prints punctuation on timeout.
 */
static void periodic_timeout_handler(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);

	printk(".");
}

int main(void)
{
	printk("Timer sample started\n");

	k_timer_init(&periodic_timer, periodic_timeout_handler, NULL);
	k_timer_init(&oneshot_timer, oneshot_timeout_handler, NULL);

	k_timer_start(&periodic_timer, PERIODIC_TIMER_TICKS, PERIODIC_TIMER_TICKS);

	k_timer_user_data_set(&oneshot_timer, (void *)hello_str);
	k_timer_start(&oneshot_timer, HELLO_TIMER_TICKS, K_FOREVER);

	printk("Timers initialized\n");

	while (!done) {
		while (LOG_PROCESS()) {
			/* Empty. */
		}

		/* Sleep */
		k_cpu_idle();
	}

	return 0;
}
