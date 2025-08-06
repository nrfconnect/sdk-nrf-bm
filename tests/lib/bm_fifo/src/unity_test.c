/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/util.h>
#include <s115/nrf_error.h>

#include <bm_fifo.h>

struct foo {
	int data;
} foo;

static struct foo buffer[4];

void test_bm_fifo_init(void)
{
	uint32_t err;
	struct bm_fifo fifo;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

#define CAP1 1
#define CAP2 2
#define SIZE1 3
#define SIZE2 4

	BM_FIFO_INIT(fifo1, CAP1, SIZE1);
	TEST_ASSERT_EQUAL(CAP1, fifo1.capacity);
	TEST_ASSERT_EQUAL(SIZE1, fifo1.item_size);
	TEST_ASSERT_NOT_NULL(fifo1.buf);

	BM_FIFO_INIT(fifo2, CAP2, SIZE2);
	TEST_ASSERT_EQUAL(CAP2, fifo2.capacity);
	TEST_ASSERT_EQUAL(SIZE2, fifo2.item_size);
	TEST_ASSERT_NOT_NULL(fifo2.buf);

	TEST_ASSERT_TRUE(fifo1.buf != fifo2.buf);
}

void test_bm_fifo_init_error_null(void)
{
	uint32_t err;
	struct bm_fifo fifo;

	err = bm_fifo_init(NULL, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	err = bm_fifo_init(&fifo, NULL, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_bm_fifo_init_error_invalid_param(void)
{
	uint32_t err;
	struct bm_fifo fifo;

	err = bm_fifo_init(&fifo, buffer, 0, sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, err);

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), 0);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, err);
}

void test_bm_fifo_enqueue(void)
{
	uint32_t err;
	struct bm_fifo fifo;
	struct foo item;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_fifo_enqueue(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_bm_fifo_enqueue_error_null(void)
{
	uint32_t err;
	struct bm_fifo fifo;
	struct foo item;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_fifo_enqueue(NULL, &item);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	err = bm_fifo_enqueue(&fifo, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_bm_fifo_enqueue_error_no_mem(void)
{
	uint32_t err;
	struct bm_fifo fifo;
	struct foo item;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	for (size_t i = 0; i < ARRAY_SIZE(buffer); i++) {
		err = bm_fifo_enqueue(&fifo, &item);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
	}

	err = bm_fifo_enqueue(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, err);
}

void test_bm_fifo_dequeue(void)
{
	uint32_t err;
	struct bm_fifo fifo;
	struct foo item;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	item.data = 0xbeef;
	err = bm_fifo_enqueue(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	item.data = 0x00;
	err = bm_fifo_dequeue(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	TEST_ASSERT_EQUAL_HEX(0xbeef, item.data);
}

void test_bm_fifo_dequeue_error_null(void)
{
	uint32_t err;
	struct bm_fifo fifo;
	struct foo item;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_fifo_enqueue(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_fifo_dequeue(NULL, &item);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	err = bm_fifo_dequeue(&fifo, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_bm_fifo_dequeue_error_not_found(void)
{
	uint32_t err;
	struct bm_fifo fifo;
	struct foo item;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_fifo_dequeue(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, err);
}

void test_bm_fifo_dequeue_data(void)
{
	uint32_t err;
	struct bm_fifo fifo;
	struct foo item;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	for (size_t i = 0; i < ARRAY_SIZE(buffer); i++) {
		item.data = i * 2;

		err = bm_fifo_enqueue(&fifo, &item);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
	}

	for (size_t i = 0; i < ARRAY_SIZE(buffer); i++) {
		err = bm_fifo_dequeue(&fifo, &item);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

		TEST_ASSERT_EQUAL(i * 2, item.data);
	}
}

void test_bm_fifo_circular(void)
{
	uint32_t err;
	size_t i;
	struct bm_fifo fifo;
	struct foo item;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	/* Queue leaving room for one */
	for (i = 0; i < ARRAY_SIZE(buffer) - 1; i++) {
		item.data = i * 2;

		err = bm_fifo_enqueue(&fifo, &item);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
	}

	item.data = 0xbeef;

	/* Make room for one more, dequeue item 0 */
	err = bm_fifo_dequeue(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, item.data);

	/* Continue the numbering */
	for (; i < ARRAY_SIZE(buffer); i++) {
		item.data = i * 2;

		err = bm_fifo_enqueue(&fifo, &item);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
	}

	for (i = 0; i < ARRAY_SIZE(buffer) - 1; i++) {
		err = bm_fifo_dequeue(&fifo, &item);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

		TEST_ASSERT_EQUAL((i + 1) * 2, item.data);
	}

	err = bm_fifo_dequeue(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, err);
}

void test_bm_fifo_discard(void)
{
	uint32_t err;
	struct bm_fifo fifo;
	struct foo item;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	item.data = 1;
	err = bm_fifo_enqueue(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
	item.data = 2;
	err = bm_fifo_enqueue(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_fifo_discard(&fifo);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	item.data = 0;
	err = bm_fifo_dequeue(&fifo, &item);
	TEST_ASSERT_EQUAL(2, item.data);

	err = bm_fifo_dequeue(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, err);
}

void test_bm_fifo_discard_error_null(void)
{
	uint32_t err;
	struct bm_fifo fifo;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_fifo_discard(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_bm_fifo_discard_error_not_found(void)
{
	uint32_t err;
	struct bm_fifo fifo;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_fifo_discard(&fifo);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, err);
}

void test_bm_fifo_peek(void)
{
	uint32_t err;
	struct bm_fifo fifo;
	struct foo item;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	item.data = 1;
	err = bm_fifo_enqueue(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
	item.data = 2;
	err = bm_fifo_enqueue(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	item.data = 0;
	err = bm_fifo_peek(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
	TEST_ASSERT_EQUAL(1, item.data);

	item.data = 0;
	err = bm_fifo_dequeue(&fifo, &item);
	TEST_ASSERT_EQUAL(1, item.data);

	item.data = 0;
	err = bm_fifo_dequeue(&fifo, &item);
	TEST_ASSERT_EQUAL(2, item.data);
}

void test_bm_fifo_peek_error_null(void)
{
	uint32_t err;
	struct bm_fifo fifo;
	struct foo item;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	item.data = 1;
	err = bm_fifo_enqueue(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_fifo_peek(NULL, &item);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	err = bm_fifo_peek(&fifo, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_bm_fifo_peek_error_not_found(void)
{
	uint32_t err;
	struct bm_fifo fifo;
	struct foo item;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_fifo_peek(&fifo, &item);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, err);
}

void test_bm_fifo_is_full_or_empty(void)
{
	uint32_t err;
	struct bm_fifo fifo;
	struct foo item;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	TEST_ASSERT_TRUE(bm_fifo_is_empty(&fifo));
	TEST_ASSERT_FALSE(bm_fifo_is_full(&fifo));

	for (size_t i = 0; i < ARRAY_SIZE(buffer); i++) {
		err = bm_fifo_enqueue(&fifo, &item);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
	}

	TEST_ASSERT_TRUE(bm_fifo_is_full(&fifo));
	TEST_ASSERT_FALSE(bm_fifo_is_empty(&fifo));
}

void test_bm_fifo_is_empty(void)
{
	uint32_t err;
	struct bm_fifo fifo;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_bm_fifo_clear(void)
{
	uint32_t err;
	struct bm_fifo fifo;
	struct foo item;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	item.data = 0xbeef;
	for (size_t i = 0; i < ARRAY_SIZE(buffer); i++) {
		err = bm_fifo_enqueue(&fifo, &item);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
	}

	TEST_ASSERT_TRUE(bm_fifo_is_full(&fifo));

	err = bm_fifo_clear(&fifo);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	for (size_t i = 0; i < ARRAY_SIZE(buffer); i++) {
		item.data = i;
		err = bm_fifo_enqueue(&fifo, &item);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
	}

	for (size_t i = 0; i < ARRAY_SIZE(buffer); i++) {
		err = bm_fifo_dequeue(&fifo, &item);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

		TEST_ASSERT_EQUAL(i, item.data);
	}
}

void test_bm_fifo_clear_error_null(void)
{
	uint32_t err;
	struct bm_fifo fifo;

	err = bm_fifo_init(&fifo, buffer, ARRAY_SIZE(buffer), sizeof(struct foo));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_fifo_clear(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void setUp(void)
{
	memset(buffer, 0x00, sizeof(buffer));
}

void tearDown(void)
{
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
