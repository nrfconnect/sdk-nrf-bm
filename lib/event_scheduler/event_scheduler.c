/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <errno.h>
#include <nrf_nvic.h>
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

int event_scheduler_defer(evt_handler_t handler, void *data, size_t len)
{
	uint8_t nested;
	struct event_scheduler_event *evt;

	if (!handler) {
		return -EFAULT;
	}
	if ((data && (len == 0)) || (!data && (len != 0))) {
		return -EINVAL;
	}

	(void) sd_nvic_critical_region_enter(&nested);
	evt = sys_heap_alloc(&heap, sizeof(struct event_scheduler_event) + len);
	(void) sd_nvic_critical_region_exit(nested);

	if (!evt) {
		return -ENOMEM;
	}

	evt->handler = handler;
	evt->len = len;

	memcpy(evt->data, data, len);

	(void) sd_nvic_critical_region_enter(&nested);
	sys_slist_append(&event_list, &evt->node);
	(void) sd_nvic_critical_region_exit(nested);

	LOG_DBG("Event %p scheduled for %p", evt, handler);

	return 0;
}

int event_scheduler_process(void)
{
	uint8_t nested;
	sys_snode_t *node;
	struct event_scheduler_event *evt;

	while (!sys_slist_is_empty(&event_list)) {

		(void) sd_nvic_critical_region_enter(&nested);
		node = sys_slist_get_not_empty(&event_list);
		(void) sd_nvic_critical_region_exit(nested);

		evt = CONTAINER_OF(node, struct event_scheduler_event, node);
		LOG_DBG("Dispatching event %p to handler %p", evt, evt->handler);
		evt->handler(evt->data, evt->len);

		(void) sd_nvic_critical_region_enter(&nested);
		sys_heap_free(&heap, evt);
		(void) sd_nvic_critical_region_exit(nested);
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
