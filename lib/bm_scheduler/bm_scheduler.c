/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <string.h>
#include <bm/bm_scheduler.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>	/* k_heap */
#include <zephyr/sys/slist.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bm_scheduler, CONFIG_BM_SCHEDULER_LOG_LEVEL);

static sys_slist_t event_list;
static K_HEAP_DEFINE(heap, CONFIG_BM_SCHEDULER_BUF_SIZE);

int bm_scheduler_defer(bm_scheduler_fn_t handler, void *data, size_t len)
{
	uint32_t pm;
	struct bm_scheduler_event *evt;

	if (!handler) {
		return -EFAULT;
	}
	if ((data && (len == 0)) || (!data && (len != 0))) {
		return -EINVAL;
	}

	evt = k_heap_alloc(&heap, sizeof(struct bm_scheduler_event) + len, K_NO_WAIT);
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

int bm_scheduler_process(void)
{
	uint32_t pm;
	sys_snode_t *node;
	struct bm_scheduler_event *evt;

	while (!sys_slist_is_empty(&event_list)) {

		pm = __get_PRIMASK();
		__disable_irq();
		node = sys_slist_get_not_empty(&event_list);
		if (!pm) {
			__enable_irq();
		}

		evt = CONTAINER_OF(node, struct bm_scheduler_event, node);
		LOG_DBG("Dispatching event %p to handler %p", evt, evt->handler);
		evt->handler(evt->data, evt->len);

		k_heap_free(&heap, evt);
	}

	return 0;
}

static int bm_scheduler_init(void)
{
	sys_slist_init(&event_list);
	LOG_DBG("Event scheduler initialized");

	return 0;
}

SYS_INIT(bm_scheduler_init, APPLICATION, 0);
