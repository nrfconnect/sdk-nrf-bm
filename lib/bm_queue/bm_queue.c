/*
 * Copyright (c) 2010-2016 Wind River Systems, Inc.
 * Copyright (c) 2025, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Dynamic size queue based on singly linked list.
 */

#include <bm_queue.h>
#include <errno.h>
#include <zephyr/sys/slist.h>
#include <zephyr/sys/sflist.h>
#include <zephyr/toolchain.h>
#if CONFIG_CPU_ARM_M
#include <cmsis_compiler.h> /* PRIMASK */
#endif

/* TODO: perhaps we should use sys_heap instead */
extern void *k_malloc(size_t n);
extern void k_free(void *p);

/* SoftDevice IRQs are defined as Zero latency IRQs.
 * We can't lock those with Zephyr's irq_lock().
 */
#if CONFIG_CPU_ARM_M
static int primask;
#endif

static void bm_queue_critical_region_enter(void)
{
#if CONFIG_CPU_ARM_M
	/* Save PRIMASK */
	primask = __get_PRIMASK();
	__disable_irq();
#endif
}

static void bm_queue_critical_region_exit(void)
{
#if CONFIG_CPU_ARM_M
	/* Restore PRIMASK */
	__set_PRIMASK(primask);
#endif
}

struct alloc_node {
	sys_sfnode_t node;
	void *data;
};

static void *z_queue_node_peek(sys_sfnode_t *node, bool needs_free)
{
	void *ret;

	if ((node != NULL) && (sys_sfnode_flags_get(node) != (uint8_t)0)) {
		/* If the flag is set, then the enqueue operation for this item
		 * did a behind-the scenes memory allocation of an alloc_node
		 * struct, which is what got put in the queue. Free it and pass
		 * back the data pointer.
		 */
		struct alloc_node *anode;

		anode = CONTAINER_OF(node, struct alloc_node, node);
		ret = anode->data;
		if (needs_free) {
			k_free(anode);
		}
	} else {
		/* Data was directly placed in the queue, the first word
		 * reserved for the linked list. User mode isn't allowed to
		 * do this, although it can get data sent this way.
		 */
		ret = (void *)node;
	}

	return ret;
}

static int32_t queue_insert(struct bm_queue *queue, void *prev, void *data,
			    bool alloc, bool is_append)
{
	bm_queue_critical_region_enter();

	if (is_append) {
		prev = sys_sflist_peek_tail(&queue->data_q);
	}

	/* Only need to actually allocate if no threads are pending */
	if (alloc) {
		struct alloc_node *anode;

		anode = k_malloc(sizeof(*anode));
		if (anode == NULL) {
			bm_queue_critical_region_exit();
			return -ENOMEM;
		}
		anode->data = data;
		sys_sfnode_init(&anode->node, 0x1);
		data = anode;
	} else {
		sys_sfnode_init(data, 0x0);
	}

	sys_sflist_insert(&queue->data_q, prev, data);

	bm_queue_critical_region_exit();

	return 0;
}


void bm_queue_init(struct bm_queue *queue)
{
	sys_sflist_init(&queue->data_q);
}

void bm_queue_insert(struct bm_queue *queue, void *prev, void *data)
{
	(void)queue_insert(queue, prev, data, false, false);
}

void bm_queue_append(struct bm_queue *queue, void *data)
{
	(void)queue_insert(queue, NULL, data, false, true);
}

void bm_queue_prepend(struct bm_queue *queue, void *data)
{
	(void)queue_insert(queue, NULL, data, false, false);
}

int32_t bm_queue_alloc_append(struct bm_queue *queue, void *data)
{
	return queue_insert(queue, NULL, data, true, true);
}

int32_t bm_queue_alloc_prepend(struct bm_queue *queue, void *data)
{
	return queue_insert(queue, NULL, data, true, false);
}

int bm_queue_append_list(struct bm_queue *queue, void *head, void *tail)
{
	/* invalid head or tail of list */
	if ((head == NULL) || (tail == NULL)) {
		return -EINVAL;
	}

	if (head != NULL) {
		bm_queue_critical_region_enter();
		sys_sflist_append_list(&queue->data_q, head, tail);
		bm_queue_critical_region_exit();
	}

	return 0;
}

int bm_queue_merge_slist(struct bm_queue *queue, sys_slist_t *list)
{
	int ret;

	/* list must not be empty */
	if (sys_slist_is_empty(list)) {
		return -EINVAL;
	}

	/*
	 * note: this works as long as:
	 * - the slist implementation keeps the next pointer as the first
	 *   field of the node object type
	 * - list->tail->next = NULL.
	 * - sflist implementation only differs from slist by stuffing
	 *   flag bytes in the lower order bits of the data pointer
	 * - source list is really an slist and not an sflist with flags set
	 */
	ret = bm_queue_append_list(queue, list->head, list->tail);
	if (ret != 0) {
		return ret;
	}
	sys_slist_init(list);

	return 0;
}

void *bm_queue_get(struct bm_queue *queue)
{
	void *data = NULL;

	bm_queue_critical_region_enter();
	if (likely(!sys_sflist_is_empty(&queue->data_q))) {
		sys_sfnode_t *node;

		node = sys_sflist_get_not_empty(&queue->data_q);
		data = z_queue_node_peek(node, true);
	}

	bm_queue_critical_region_exit();
	return data;
}

bool bm_queue_remove(struct bm_queue *queue, void *data)
{
	bool removed;

	bm_queue_critical_region_enter();
	removed = sys_sflist_find_and_remove(&queue->data_q, (sys_sfnode_t *)data);
	bm_queue_critical_region_exit();

	return removed;
}

bool bm_queue_unique_append(struct bm_queue *queue, void *data)
{
	bool unique = true;
	sys_sfnode_t *test;

	bm_queue_critical_region_enter();
	SYS_SFLIST_FOR_EACH_NODE(&queue->data_q, test) {
		if (test == (sys_sfnode_t *) data) {
			unique = false;
			break;
		}
	}

	if (unique) {
		/* This is a nested critical region, but that's fine
		 * as long as PRIMASK is restored to the previous value.
		 */
		bm_queue_append(queue, data);
	}

	bm_queue_critical_region_exit();
	return unique;
}

void *bm_queue_peek_head(struct bm_queue *queue)
{
	return z_queue_node_peek(sys_sflist_peek_head(&queue->data_q), false);
}

void *bm_queue_peek_tail(struct bm_queue *queue)
{
	return z_queue_node_peek(sys_sflist_peek_tail(&queue->data_q), false);
}


/* Export k_queue -compatible API */
#if CONFIG_BM_QUEUE_K_QUEUE_COMPAT

#include <zephyr/kernel.h>
#include <zephyr/toolchain.h>
#include <zephyr/sys/__assert.h>

BUILD_ASSERT(
	offsetof(struct bm_queue, data_q) == offsetof(struct k_queue, data_q),
	"Incompatible queues");


inline void z_impl_k_queue_init(struct k_queue *queue)
{
	bm_queue_init((struct bm_queue *)queue);
}

inline void k_queue_insert(struct k_queue *queue, void *prev, void *data)
{
	bm_queue_insert((struct bm_queue *)queue, prev, data);
}

inline void k_queue_append(struct k_queue *queue, void *data)
{
	bm_queue_append((struct bm_queue *)queue, data);
}

inline void k_queue_prepend(struct k_queue *queue, void *data)
{
	bm_queue_prepend((struct bm_queue *)queue, data);
}

inline int32_t z_impl_k_queue_alloc_append(struct k_queue *queue, void *data)
{
	return bm_queue_alloc_append((struct bm_queue *)queue, data);
}

inline int32_t z_impl_k_queue_alloc_prepend(struct k_queue *queue, void *data)
{
	return bm_queue_alloc_prepend((struct bm_queue *)queue, data);
}

inline int k_queue_append_list(struct k_queue *queue, void *head, void *tail)
{
	return bm_queue_append_list((struct bm_queue *)queue, head, tail);
}

inline int k_queue_merge_slist(struct k_queue *queue, sys_slist_t *list)
{
	return bm_queue_merge_slist((struct bm_queue *)queue, list);
}

inline void *z_impl_k_queue_get(struct k_queue *queue, k_timeout_t timeout)
{
	ARG_UNUSED(timeout);
	__ASSERT_NO_MSG(K_TIMEOUT_EQ(timeout, K_NO_WAIT));

	return bm_queue_get((struct bm_queue *)queue);
}

inline bool k_queue_remove(struct k_queue *queue, void *data)
{
	return bm_queue_remove((struct bm_queue *)queue, data);
}

inline bool k_queue_unique_append(struct k_queue *queue, void *data)
{
	return bm_queue_unique_append((struct bm_queue *)queue, data);
}

inline void *z_impl_k_queue_peek_head(struct k_queue *queue)
{
	return bm_queue_peek_head((struct bm_queue *)queue);
}

inline void *z_impl_k_queue_peek_tail(struct k_queue *queue)
{
	return bm_queue_peek_tail((struct bm_queue *)queue);
}

#endif
