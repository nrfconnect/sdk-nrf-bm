/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <lite_timer.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/__assert.h>

static void lite_timer_handler(struct k_timer *timer)
{
	__ASSERT(timer, "timer is NULL");

	struct lite_timer *lite_timer = CONTAINER_OF(timer, struct lite_timer, timer);
	void *context = k_timer_user_data_get(timer);

	lite_timer->handler(context);
}

int lite_timer_init(struct lite_timer *timer, enum lite_timer_mode mode,
		    lite_timer_timeout_handler_t timeout_handler)
{
	if (timer == NULL || timeout_handler == NULL) {
		return -EFAULT;
	}

	timer->mode = mode;
	timer->handler = timeout_handler;
	k_timer_init(&timer->timer, lite_timer_handler, NULL);

	return 0;
}

int lite_timer_start(struct lite_timer *timer, uint32_t timeout_ticks, void *context)
{
	if (timer == NULL) {
		return -EFAULT;
	}

	k_timeout_t duration = { .ticks = timeout_ticks };
	k_timeout_t period = (timer->mode == LITE_TIMER_MODE_SINGLE_SHOT) ? K_NO_WAIT : duration;

	k_timer_user_data_set(&timer->timer, context);
	k_timer_start(&timer->timer, duration, period);

	return 0;
}

int lite_timer_stop(struct lite_timer *timer)
{
	if (timer == NULL) {
		return -EFAULT;
	}

	k_timer_stop(&timer->timer);

	return 0;
}
