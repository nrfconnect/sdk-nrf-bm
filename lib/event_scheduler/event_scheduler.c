/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <errno.h>
#include <event_scheduler.h>
#include <zephyr/logging/log.h>
#include <zephyr/init.h>
#include <zephyr/sys/sys_heap.h>
#include <zephyr/sys/slist.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(event_scheduler, CONFIG_EVENT_SCHEDULER_LOG_LEVEL);

static sys_slist_t event_list;
static struct sys_heap heap;
static uint8_t buf[CONFIG_EVENT_SCHEDULER_BUF_SIZE];

int event_scheduler_defer(event_handler_t handler, void *data, size_t len)
{
	uint32_t pm;
	struct event_scheduler_event *evt;

	if (!handler) {
		return -EFAULT;
	}
	if ((data && (len == 0)) || (!data && (len != 0))) {
		return -EINVAL;
	}

	pm = __get_PRIMASK();
	__disable_irq();
	evt = sys_heap_alloc(&heap, sizeof(struct event_scheduler_event) + len);
	if (!pm) {
		__enable_irq();
	}

	if (!evt) {
		return -ENOMEM;
	}

	evt->handler = handler;
	evt->len = len;

	memcpy(evt->data, data, len);

	pm = __get_PRIMASK();
	__disable_irq();
	sys_slist_append(&event_list, &evt->node);
	if (!pm) {
		__enable_irq();
	}

	LOG_DBG("Event %p scheduled for %p", evt, handler);

	return 0;
}

int event_scheduler_process(void)
{
	uint32_t pm;
	sys_snode_t *node;
	struct event_scheduler_event *evt;

	while (!sys_slist_is_empty(&event_list)) {

		pm = __get_PRIMASK();
		__disable_irq();
		node = sys_slist_get_not_empty(&event_list);
		if (!pm) {
			__enable_irq();
		}

		evt = CONTAINER_OF(node, struct event_scheduler_event, node);
		LOG_DBG("Dispatching event %p to handler %p", evt, evt->handler);
		evt->handler(evt->data, evt->len);

		pm = __get_PRIMASK();
		__disable_irq();
		sys_heap_free(&heap, evt);
		if (!pm) {
			__enable_irq();
		}
	}

	return 0;
}

static int event_scheduler_init(void)
{
	sys_slist_init(&event_list);
	sys_heap_init(&heap, buf, sizeof(buf));

	LOG_DBG("Event scheduler initialized");

	return 0;
}

SYS_INIT(event_scheduler_init, APPLICATION, 0);
