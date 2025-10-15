/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm/bm_timer.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_REGISTER(bm_timer, CONFIG_BM_TIMER_LOG_LEVEL);

#if CONFIG_SOC_SERIES_NRF54LX
#include <hal/nrf_grtc.h>
#define BM_TIMER_IRQn GRTC_IRQn
#else
#define BM_TIMER_IRQn
#error "Unsupported"
#endif /* CONFIG_SOC_SERIES_NRF5xx */

static void irq_prio_lvl_configure(void)
{
	NVIC_SetPriority(BM_TIMER_IRQn, (uint32_t)CONFIG_BM_TIMER_IRQ_PRIO);

	LOG_DBG("Timer IRQ priority level set to %d", CONFIG_BM_TIMER_IRQ_PRIO);
}

static void bm_timer_handler(struct k_timer *timer)
{
	__ASSERT(timer, "timer is NULL");

	struct bm_timer *bm_timer = CONTAINER_OF(timer, struct bm_timer, timer);
	void *context = k_timer_user_data_get(timer);

	bm_timer->handler(context);
}

int bm_timer_init(struct bm_timer *timer, enum bm_timer_mode mode,
		    bm_timer_timeout_handler_t timeout_handler)
{
	if (timer == NULL || timeout_handler == NULL) {
		return -EFAULT;
	}

	timer->mode = mode;
	timer->handler = timeout_handler;
	k_timer_init(&timer->timer, bm_timer_handler, NULL);

	return 0;
}

int bm_timer_start(struct bm_timer *timer, uint32_t timeout_ticks, void *context)
{
	if (timer == NULL) {
		return -EFAULT;
	}

	if (timeout_ticks < BM_TIMER_MIN_TIMEOUT_TICKS) {
		return -EINVAL;
	}

	k_timeout_t duration = { .ticks = timeout_ticks };
	k_timeout_t period = (timer->mode == BM_TIMER_MODE_SINGLE_SHOT) ? K_NO_WAIT : duration;

	k_timer_user_data_set(&timer->timer, context);
	k_timer_start(&timer->timer, duration, period);

	return 0;
}

int bm_timer_stop(struct bm_timer *timer)
{
	if (timer == NULL) {
		return -EFAULT;
	}

	k_timer_stop(&timer->timer);

	return 0;
}

static int bm_timer_sys_init(void)
{
	irq_prio_lvl_configure();

	return 0;
}

SYS_INIT(bm_timer_sys_init, APPLICATION, 0);
