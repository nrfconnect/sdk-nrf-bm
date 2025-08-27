/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_KERNEL_H_
#define ZEPHYR_INCLUDE_KERNEL_H_

typedef int k_timeout_t;

struct k_timer {
	int foo[52];
};

typedef void (*k_timer_expiry_t)(struct k_timer *timer);

typedef void (*k_timer_stop_t)(struct k_timer *timer);

void k_timer_init(struct k_timer *timer,
			 k_timer_expiry_t expiry_fn,
			 k_timer_stop_t stop_fn);

void k_timer_start(struct k_timer *timer,
			     k_timeout_t duration, k_timeout_t period);

void k_timer_stop(struct k_timer *timer);

void k_timer_user_data_set(struct k_timer *timer, void *user_data);

void *k_timer_user_data_get(const struct k_timer *timer);

#endif /* ZEPHYR_INCLUDE_KERNEL_H_ */
