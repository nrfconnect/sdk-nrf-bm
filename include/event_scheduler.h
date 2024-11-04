/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdint.h>
#include <zephyr/sys/slist.h>

/**
 * @brief Event handler prototype.
 */
typedef void (*event_handler_t)(void *evt, size_t len);

/**
 * @brief An event to be scheduled for execution in the main thread.
 *
 * An event consists of a function (handler) and some data that the function has to process.
 */
struct event_scheduler_event {
	/**
	 * @brief Reserved.
	 */
	sys_snode_t node;
	/**
	 * @brief Event handler.
	 */
	event_handler_t handler;
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
 * @return 0 On success.
 * @return -EFAULT @p handler is @c NULL.
 * @return -EINVAL Invalid @p data and @p len combination.
 * @return -ENOMEM No memory to schedule this event.
 */
int event_scheduler_defer(event_handler_t handler, void *data, size_t len);

/**
 * @brief Process deferred events.
 *
 * Process deferred events in the main thread.
 *
 * @return 0 On success.
 */
int event_scheduler_process(void);
