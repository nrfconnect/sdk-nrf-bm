/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/sys/__assert.h> /* __ASSERT */
#include <s115/nrf_error.h>

/* We don't want to include this in unit tests. */
#if CONFIG_HAS_NRFX
#include <nrfx.h>
#else
#define NRFX_CRITICAL_SECTION_ENTER(...)
#define NRFX_CRITICAL_SECTION_EXIT(...)
#endif

#include <bm_fifo.h>

bool bm_fifo_is_full(const struct bm_fifo *fifo)
{
	__ASSERT_NO_MSG(fifo);
	return (fifo->count == fifo->capacity);
}

bool bm_fifo_is_empty(const struct bm_fifo *fifo)
{
	__ASSERT_NO_MSG(fifo);
	return (fifo->count == 0);
}

uint32_t bm_fifo_init(struct bm_fifo *fifo, void *buf, size_t capacity, size_t item_size)
{
	if (!fifo || !buf) {
		return NRF_ERROR_NULL;
	}
	if (!item_size || !capacity) {
		return NRF_ERROR_INVALID_PARAM;
	}

	memset(fifo, 0x00, sizeof(*fifo));

	fifo->buf = buf;
	fifo->item_size = item_size;
	fifo->capacity = capacity;

	return NRF_SUCCESS;
}

uint32_t bm_fifo_enqueue(struct bm_fifo *fifo, void *buf)
{
	uint32_t err;
	void *item;

	if (!fifo || !buf) {
		return NRF_ERROR_NULL;
	}

	NRFX_CRITICAL_SECTION_ENTER();

	if (bm_fifo_is_full(fifo)) {
		err = NRF_ERROR_NO_MEM;
		goto out;
	}

	fifo->count++;
	__ASSERT(fifo->count <= fifo->capacity, "Queue overflow");

	item = (uint8_t *)fifo->buf + (fifo->tail * fifo->item_size);
	fifo->tail = (fifo->tail + 1) % fifo->capacity;
	memcpy(item, buf, fifo->item_size);

	err = NRF_SUCCESS;

out:
	NRFX_CRITICAL_SECTION_EXIT();
	return err;
}

uint32_t bm_fifo_dequeue(struct bm_fifo *fifo, void *buf)
{
	uint32_t err;
	void *item;

	if (!fifo || !buf) {
		return NRF_ERROR_NULL;
	}

	NRFX_CRITICAL_SECTION_ENTER();

	if (bm_fifo_is_empty(fifo)) {
		err = NRF_ERROR_NOT_FOUND;
		goto out;
	}

	fifo->count--;
	__ASSERT(fifo->count <= fifo->capacity, "Queue underflow");

	item = (uint8_t *)fifo->buf + (fifo->head * fifo->item_size);
	fifo->head = (fifo->head + 1) % fifo->capacity;
	memcpy(buf, item, fifo->item_size);

	err = NRF_SUCCESS;

out:
	NRFX_CRITICAL_SECTION_EXIT();
	return err;
}

uint32_t bm_fifo_peek(const struct bm_fifo *fifo, void *buf)
{
	uint32_t err;
	void *item;

	if (!fifo || !buf) {
		return NRF_ERROR_NULL;
	}

	NRFX_CRITICAL_SECTION_ENTER();

	if (bm_fifo_is_empty(fifo)) {
		err = NRF_ERROR_NOT_FOUND;
		goto out;
	}

	item = (uint8_t *)fifo->buf + (fifo->head * fifo->item_size);
	memcpy(buf, item, fifo->item_size);

	err = NRF_SUCCESS;

out:
	NRFX_CRITICAL_SECTION_EXIT();
	return err;
}

uint32_t bm_fifo_discard(struct bm_fifo *fifo)
{
	uint32_t err;

	if (!fifo) {
		return NRF_ERROR_NULL;
	}

	NRFX_CRITICAL_SECTION_ENTER();

	if (bm_fifo_is_empty(fifo)) {
		err = NRF_ERROR_NOT_FOUND;
		goto out;
	}

	fifo->count--;
	__ASSERT(fifo->count <= fifo->capacity, "Queue underflow");

	fifo->head = (fifo->head + 1) % fifo->capacity;

	err = NRF_SUCCESS;

out:
	NRFX_CRITICAL_SECTION_EXIT();
	return err;
}

uint32_t bm_fifo_clear(struct bm_fifo *fifo)
{
	if (!fifo) {
		return NRF_ERROR_NULL;
	}

	NRFX_CRITICAL_SECTION_ENTER();

	fifo->head = 0;
	fifo->tail = 0;
	fifo->count = 0;

	NRFX_CRITICAL_SECTION_EXIT();

	return NRF_SUCCESS;
}
