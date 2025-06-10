/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stddef.h>
#include <stdint.h>

struct bm_fifo {
	/**
	 * @brief FIFO buffer.
	 */
	void *buf;
	/**
	 * @brief FIFO maximum capacity (number of items).
	 */
	uint32_t capacity;
	/**
	 * @brief FIFO item size.
	 */
	size_t item_size;
	/**
	 * @brief Number of items in the queue.
	 */
	uint32_t count;
	/**
	 * @brief FIFO head.
	 */
	int head;
	/**
	 * @brief FIFO tail.
	 */
	int tail;
};

/**
 * @brief Statically define a FIFO.
 *
 * Avoids the @ref bm_fifo_init() call.
 */
#define BM_FIFO_INIT(_name, _capacity, _item_size)                                                 \
	static uint8_t _name##_buf[(_item_size) * (_capacity)];                                    \
	static struct bm_fifo _name = {                                                            \
		.buf = _name##_buf,                                                                \
		.item_size = _item_size,                                                           \
		.capacity = _capacity,                                                             \
	}

/**
 * @brief Initialize a queue.
 *
 * @param fifo FIFO queue.
 * @param buf Queue buffer.
 * @param capacity Buffer capacity, in number of items.
 * @param item_size Size of a queue element, in bytes.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_NULL If @p fifo or @p buf are @c NULL.
 * @retval NRF_ERROR_INVALID_PARAM If @p capacity or @p item_size are 0.
 */
uint32_t bm_fifo_init(struct bm_fifo *fifo, void *buf, size_t capacity, size_t item_size);

/**
 * @brief Check whether the queue is full.
 *
 * @param fifo FIFO queue.
 *
 * @return true Queue is full.
 * @return false Queue is not full.
 */
bool bm_fifo_is_full(const struct bm_fifo *fifo);

/**
 * @brief Check whether the queue is empty.
 *
 * @param fifo FIFO queue.
 *
 * @return true Queue is empty.
 * @return false Queue is not empty.
 */
bool bm_fifo_is_empty(const struct bm_fifo *fifo);

/**
 * @brief Queue an element.
 *
 * The element is copied into the queue's own buffer.
 * Interrupts are disabled during the copy.
 *
 * @param fifo FIFO queue.
 * @param buf Buffer pointing to the element.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_NULL If @p fifo or @p buf are @c NULL.
 * @retval NRF_ERROR_NO_MEM If there are no buffers available in the queue.
 */
uint32_t bm_fifo_enqueue(struct bm_fifo *fifo, void *buf);

/**
 * @brief Dequeue an element.
 *
 * Dequeue an element from the queue's head.
 *
 * @param fifo FIFO queue.
 * @param buf Buffer to copy the element into.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_NULL If @p fifo or @p buf are @c NULL.
 * @retval NRF_ERROR_NOT_FOUND If the queue is empty.
 */
uint32_t bm_fifo_dequeue(struct bm_fifo *fifo, void *buf);

/**
 * @brief Peek at the queue.
 *
 * Peek at the queue's head.
 *
 * @param fifo FIFO queue.
 * @param buf Buffer to copy the element into.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_NULL If @p fifo or @p buf are @c NULL.
 * @retval NRF_ERROR_NOT_FOUND If the queue is empty.
 */
uint32_t bm_fifo_peek(const struct bm_fifo *fifo, void *buf);

/**
 * @brief Dequeue one element and discard it.
 *
 * Dequeue an element and discard it.
 *
 * @param fifo FIFO queue.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_NULL If @p fifo is @c NULL.
 * @retval NRF_ERROR_NOT_FOUND If the queue is empty.
 */
uint32_t bm_fifo_discard(struct bm_fifo *fifo);

/**
 * @brief Clear the queue, discarding all elements.
 *
 * @param fifo FIFO queue.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_NULL If @p fifo is @c NULL.
 */
uint32_t bm_fifo_clear(struct bm_fifo *fifo);
