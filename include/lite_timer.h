/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup lite_timer NCS-Lite Timer library
 * @{
 */

#ifndef LITE_TIMER_H__
#define LITE_TIMER_H__

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Minimum timeout in microseconds.
 *
 * Calculated from a minimum of 5 ticks with frequency of 32.768 kHz.
 */
#define LITE_TIMER_MIN_TIMEOUT_US (uint32_t)((5 * 1000000) / 32768)

/**
 * @brief Minimum value of the timeout_ticks parameter of @ref lite_timer_start.
 */
#define LITE_TIMER_MIN_TIMEOUT_TICKS k_us_to_ticks_ceil32(LITE_TIMER_MIN_TIMEOUT_US)

/**
 * @brief Convert milliseconds to timer ticks.
 */
#define LITE_TIMER_MS_TO_TICKS(ms) k_ms_to_ticks_floor32(ms)

/**
 * @brief Timer modes.
 */
enum lite_timer_mode {
	/**
	 * @brief The timer will expire only once.
	 */
	LITE_TIMER_MODE_SINGLE_SHOT,
	/**
	 * @brief The timer will restart each time it expires.
	 */
	LITE_TIMER_MODE_REPEATED,
};

/**
 * @brief Application time-out handler type.
 *
 * @param context General purpose pointer. Set when calling @ref lite_timer_start.
 */
typedef void (*lite_timer_timeout_handler_t)(void *context);

/**
 * @brief Timer instance structure.
 */
struct lite_timer {
	struct k_timer timer;
	enum lite_timer_mode mode;
	lite_timer_timeout_handler_t handler;
};

/**
 * @brief Initialize a timer instance.
 *
 * @param timer Pointer to timer instance.
 * @param mode Timer mode.
 * @param timeout_handler Function to be executed when time timer expires.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p timer or @p timeout_handler is @c NULL.
 */
int lite_timer_init(struct lite_timer *timer, enum lite_timer_mode mode,
		    lite_timer_timeout_handler_t timeout_handler);

/**
 * @brief Start a timer.
 *
 * @param timer Pointer to timer instance.
 * @param timeout_ticks Number of ticks to time-out event.
 * @param context General purpose pointer. Will be passed to the time-out handler when
 *                when the timer expires.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p timer is @c NULL.
 * @retval -EINVAL If @p timeout_ticks is less than @ref LITE_TIMER_MIN_TIMEOUT_TICKS.
 */
int lite_timer_start(struct lite_timer *timer, uint32_t timeout_ticks, void *context);

/**
 * @brief Stop a timer.
 *
 * @param timer Pointer to timer instance.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p timer is @c NULL.
 */
int lite_timer_stop(struct lite_timer *timer);

#ifdef __cplusplus
}
#endif

#endif /* LITE_TIMER_H__ */

/** @} */
