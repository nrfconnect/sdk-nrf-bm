/*
 * Copyright (c) 2016, Wind River Systems, Inc.
 * Copyright (c) 2025, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/sys/slist.h>
#include <zephyr/sys/sflist.h>

/**
 * @brief Queue.
 */
struct bm_queue {
	sys_sflist_t data_q;
};

/**
 * @brief Initialize a queue.
 *
 * This routine initializes a queue, prior to its first use.
 *
 * @param queue Queue.
 */
void bm_queue_init(struct bm_queue *queue);

/**
 * @brief Append an element to the end of a queue.
 *
 * This routine appends a data item to @a queue. A queue data item must be
 * aligned on a word boundary, and the first word of the item is reserved
 * for internal use. The data is not copied.
 *
 * @funcprops \isr_ok
 *
 * @param queue Queue.
 * @param data Data item.
 */
void bm_queue_append(struct bm_queue *queue, void *data);

/**
 * @brief Allocate an element and append it to the queue.
 *
 * This routine appends a data item to @a queue. There is an implicit memory
 * allocation on the system heap to create an additional temporary bookkeeping data structure,
 * which is automatically freed when the item is removed. The data itself is not copied.
 *
 * @funcprops \isr_ok
 *
 * @param queue Queue.
 * @param data Data item.
 *
 * @retval 0 on success
 * @retval -ENOMEM if there isn't sufficient RAM in the system heap
 */
int32_t bm_queue_alloc_append(struct bm_queue *queue, void *data);

/**
 * @brief Prepend an element to the queue.
 *
 * This routine prepends a data item to @a queue. A queue data item must be
 * aligned on a word boundary, and the first word of the item is reserved
 * for internal use. The data is not copied.
 *
 * @funcprops \isr_ok
 *
 * @param queue Queue.
 * @param data Data item.
 */
void bm_queue_prepend(struct bm_queue *queue, void *data);

/**
 * @brief Prepend an element to a queue.
 *
 * This routine prepends a data item to @a queue. There is an implicit memory
 * allocation to create an additional temporary bookkeeping data structure from
 * the system heap, which is automatically freed when the item is removed.
 * The data itself is not copied.
 *
 * @funcprops \isr_ok
 *
 * @param queue Queue.
 * @param data Data item.
 *
 * @retval 0 on success
 * @retval -ENOMEM if there isn't sufficient RAM in the system heap.
 */
int32_t bm_queue_alloc_prepend(struct bm_queue *queue, void *data);

/**
 * @brief Insert an element at a given position in the queue.
 *
 * This routine inserts a data item to @a queue after previous item. A queue
 * data item must be aligned on a word boundary, and the first word of
 * the item is reserved for internal use.
 *
 * @funcprops \isr_ok
 *
 * @param queue Queue.
 * @param prev Previous element.
 * @param data Data item.
 */
void bm_queue_insert(struct bm_queue *queue, void *prev, void *data);

/**
 * @brief Atomically append a list of elements to a queue.
 *
 * This routine adds a list of data items to @a queue in one operation.
 * The data items must be in a singly-linked list, with the first word
 * in each data item pointing to the next data item; the list must be
 * NULL-terminated.
 *
 * @funcprops \isr_ok
 *
 * @param queue Queue.
 * @param head Pointer to first node in singly-linked list.
 * @param tail Pointer to last node in singly-linked list.
 *
 * @retval 0 on success
 * @retval -EINVAL on invalid supplied data
 */
int bm_queue_append_list(struct bm_queue *queue, void *head, void *tail);

/**
 * @brief Atomically add a list of elements to a queue.
 *
 * This routine adds a list of data items to @a queue in one operation.
 * The data items must be in a singly-linked list implemented using a
 * sys_slist_t object. Upon completion, the original list is empty.
 *
 * @funcprops \isr_ok
 *
 * @param queue Queue.
 * @param list Pointer to sys_slist_t object.
 *
 * @retval 0 on success
 * @retval -EINVAL on invalid data
 */
int bm_queue_merge_slist(struct bm_queue *queue, sys_slist_t *list);

/**
 * @brief Get an element from a queue.
 *
 * This routine removes first data item from @a queue. The first word of the
 * data item is reserved for internal use.
 *
 * @funcprops \isr_ok
 *
 * @param queue Queue.
 *
 * @return Address of the data item if successful; NULL otherwise.
 */
void *bm_queue_get(struct bm_queue *queue);

/**
 * @brief Remove an element from a queue.
 *
 * This routine removes data item from @a queue. The first word of the
 * data item is reserved for internal use. Removing elements from bm_queue
 * rely on sys_slist_find_and_remove which is not a constant time operation.
 *
 * @funcprops \isr_ok
 *
 * @param queue Queue.
 * @param data Data item.
 *
 * @return true if data item was removed
 */
bool bm_queue_remove(struct bm_queue *queue, void *data);

/**
 * @brief Append an element to a queue only if it's not present already.
 *
 * This routine appends data item to @a queue. The first word of the data
 * item is reserved for internal use. Appending elements to bm_queue
 * relies on sys_slist_is_node_in_list which is not a constant time operation.
 *
 * @funcprops \isr_ok
 *
 * @param queue Queue.
 * @param data Data item.
 *
 * @return true if data item was added, false if not
 */
bool bm_queue_unique_append(struct bm_queue *queue, void *data);

/**
 * @brief Query a queue to see if it has data available.
 *
 * @funcprops \isr_ok
 *
 * @param queue Queue.
 *
 * @return Non-zero if the queue is empty.
 * @return 0 if data is available.
 */
static inline bool bm_queue_is_empty(struct bm_queue *queue)
{
	return sys_sflist_is_empty(&queue->data_q);
}

/**
 * @brief Peek element at the head of queue.
 *
 * Return element from the head of queue without removing it.
 *
 * @param queue Queue.
 *
 * @return Head element, or NULL if queue is empty.
 */
void *bm_queue_peek_head(struct bm_queue *queue);

/**
 * @brief Peek element at the tail of queue.
 *
 * Return element from the tail of queue without removing it.
 *
 * @param queue Queue.
 *
 * @return Tail element, or NULL if queue is empty.
 */
void *bm_queue_peek_tail(struct bm_queue *queue);
