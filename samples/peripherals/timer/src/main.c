/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm/bm_timer.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <hal/nrf_gpio.h>
#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_TIMER_LOG_LEVEL);

#define PERIODIC_TIMER_TICKS BM_TIMER_MS_TO_TICKS(CONFIG_SAMPLE_PERIODIC_TIMER_INTERVAL_MS)
#define HELLO_TIMER_TICKS    BM_TIMER_MS_TO_TICKS(CONFIG_SAMPLE_HELLO_TIMER_DURATION_MS)
#define WORLD_TIMER_TICKS    BM_TIMER_MS_TO_TICKS(CONFIG_SAMPLE_WORLD_TIMER_DURATION_MS)
#define BYE_TIMER_TICKS      BM_TIMER_MS_TO_TICKS(CONFIG_SAMPLE_BYE_TIMER_DURATION_MS)

static struct bm_timer oneshot_timer;
static struct bm_timer periodic_timer;

static const char *const hello_str = "Hello";
static const char *const world_str = "world!";
static const char *const bye_str = "bye!";

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

	LOG_INF("%s", str);

	if (cnt == 0) {
		err = bm_timer_start(&oneshot_timer, WORLD_TIMER_TICKS, (void *)world_str);
		if (err) {
			LOG_ERR("Failed to start oneshot timer, err %d", err);
		}
	} else if (cnt == 1) {
		err = bm_timer_start(&oneshot_timer, BYE_TIMER_TICKS, (void *)bye_str);
		if (err) {
			LOG_ERR("Failed to start oneshot timer, err %d", err);
		}
	} else {
		err = bm_timer_stop(&periodic_timer);
		if (err) {
			LOG_ERR("Failed to stop periodic timer, err %d", err);
		}
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

	LOG_INF(".");
}

int main(void)
{
	int err;

	LOG_INF("Timer sample started");

	err = bm_timer_init(&periodic_timer, BM_TIMER_MODE_REPEATED, periodic_timeout_handler);
	if (err) {
		LOG_ERR("Failed to initialize periodic timer, err %d", err);
		goto idle;
	}

	err = bm_timer_init(&oneshot_timer, BM_TIMER_MODE_SINGLE_SHOT, oneshot_timeout_handler);
	if (err) {
		LOG_ERR("Failed to initialize oneshot timer, err %d", err);
		goto idle;
	}

	err = bm_timer_start(&periodic_timer, PERIODIC_TIMER_TICKS, NULL);
	if (err) {
		LOG_ERR("Failed to start periodic timer, err %d", err);
		goto idle;
	}

	err = bm_timer_start(&oneshot_timer, HELLO_TIMER_TICKS, (void *)hello_str);
	if (err) {
		LOG_ERR("Failed to start oneshot timer, err %d", err);
		goto idle;
	}

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);

	LOG_INF("Timer sample initialized");

idle:
	while (true) {
		log_flush();

		k_cpu_idle();
	}

	return 0;
}
