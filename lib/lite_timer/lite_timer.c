/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <lite_timer.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_REGISTER(lite_timer, CONFIG_LITE_TIMER_LOG_LEVEL);

#if CONFIG_SOFTDEVICE
#include <nrf_nvic.h>
#endif /* CONFIG_SOFTDEVICE */

#if CONFIG_SOC_SERIES_NRF52X
#define LITE_TIMER_IRQn RTC1_IRQn
#elif CONFIG_SOC_SERIES_NRF54LX
#include <hal/nrf_grtc.h>
#define LITE_TIMER_IRQn GRTC_IRQn
#else
#define LITE_TIMER_IRQn
#error "Unsupported"
#endif /* CONFIG_SOC_SERIES_NRF5xx */

static void irq_prio_lvl_configure(void)
{
#if CONFIG_SOFTDEVICE
	int ret = sd_nvic_SetPriority(LITE_TIMER_IRQn, CONFIG_LITE_TIMER_IRQ_PRIO);

	if (ret != NRF_SUCCESS) {
		LOG_WRN("Failed to set IRQ priority, nrf_error %#x", ret);
		__ASSERT(false, "Failed to set IRQ priority, nrf_error %#x", ret);
		return;
	}
#else
	NVIC_SetPriority(LITE_TIMER_IRQn, (uint32_t)CONFIG_LITE_TIMER_IRQ_PRIO);
#endif

	LOG_DBG("Timer IRQ priority level set to %d", CONFIG_LITE_TIMER_IRQ_PRIO);
}

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

	if (timeout_ticks < LITE_TIMER_MIN_TIMEOUT_TICKS) {
		return -EINVAL;
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

static int lite_timer_sys_init(void)
{
	irq_prio_lvl_configure();

	return 0;
}

SYS_INIT(lite_timer_sys_init, APPLICATION, 0);
