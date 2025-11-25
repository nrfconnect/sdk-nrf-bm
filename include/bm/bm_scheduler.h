/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup bm_scheduler NCS Bare Metal Event Scheduler library
 * @{
 */

#ifndef BM_SCHEDULER_H__
#define BM_SCHEDULER_H__

#include <stdint.h>
#include <zephyr/sys/slist.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Event handler prototype.
 */
typedef void (*bm_scheduler_fn_t)(void *evt, size_t len);

/**
 * @brief An event to be scheduled for execution in the main thread.
 *
 * An event consists of a function (handler) and some data that the function has to process.
 */
struct bm_scheduler_event {
	/**
	 * @brief Reserved.
	 */
	sys_snode_t node;
	/**
	 * @brief Event handler.
	 */
	bm_scheduler_fn_t handler;
	/**
	 * @brief Event length.
	 */
	size_t len;
	/**
	 * @brief Event data.
	 */
	uint8_t data[];
};

/**
 * @brief Schedule an event for execution in the main thread.
 *
 * This function can be called from an ISR to defer code execution to the main thread.
 *
 * @param handler Event handler.
 * @param data Event data.
 * @param len Event data length.
 *
 * @retval 0 On success.
 * @retval -EFAULT @p handler is @c NULL.
 * @retval -EINVAL Invalid @p data and @p len combination.
 * @retval -ENOMEM No memory to schedule this event.
 */
int bm_scheduler_defer(bm_scheduler_fn_t handler, void *data, size_t len);

/**
 * @brief Process deferred events.
 *
 * Process deferred events in the main thread.
 *
 * @retval 0 On success.
 */
int bm_scheduler_process(void);

#ifdef __cplusplus
}
#endif

#endif /* BM_SCHEDULER_H__ */

/** @} */
