/*
 * Copyright (c) 2025 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <unity.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include "cmock_kernel.h"
#include <bm_timer.h>

#define TEST_TIMER_TICKS BM_TIMER_MS_TO_TICKS(100)
#define LESS_THAN_MIN_TIMER_TICKS 0

static struct bm_timer test_timer;

static void nocontext_timeout_handler(void *context)
{
}

void test_bm_timer_init_efault(void)
{
	int ret;

	ret = bm_timer_init(NULL, BM_TIMER_MODE_SINGLE_SHOT, nocontext_timeout_handler);
	TEST_ASSERT_EQUAL(-EFAULT, ret);
	ret = bm_timer_init(&test_timer, BM_TIMER_MODE_SINGLE_SHOT, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, ret);
}

void test_bm_timer_init(void)
{
	int ret;

	__cmock_k_timer_init_Ignore();
	ret = bm_timer_init(&test_timer, BM_TIMER_MODE_SINGLE_SHOT, nocontext_timeout_handler);
	TEST_ASSERT_EQUAL(0, ret);
	TEST_ASSERT_EQUAL(BM_TIMER_MODE_SINGLE_SHOT, test_timer.mode);
}

void test_bm_timer_start_efault(void)
{
	int ret;

	ret = bm_timer_start(NULL, TEST_TIMER_TICKS, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, ret);
}

void test_bm_timer_start_einval(void)
{
	int ret;

	ret = bm_timer_start(&test_timer, LESS_THAN_MIN_TIMER_TICKS, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, ret);
}

void test_bm_timer_start(void)
{
	int ret;

	__cmock_k_timer_init_Ignore();
	ret = bm_timer_init(&test_timer, BM_TIMER_MODE_SINGLE_SHOT, nocontext_timeout_handler);
	TEST_ASSERT_EQUAL(0, ret);

	__cmock_k_timer_user_data_set_CMockIgnore();
	__cmock_k_timer_start_Ignore();
	ret = bm_timer_start(&test_timer, TEST_TIMER_TICKS, NULL);
	TEST_ASSERT_EQUAL(0, ret);
}

void test_bm_timer_stop_efault(void)
{
	int ret;

	ret = bm_timer_stop(NULL);
	TEST_ASSERT_EQUAL(-EFAULT, ret);
}

void test_bm_timer_stop(void)
{
	int ret;

	__cmock_k_timer_init_Ignore();
	ret = bm_timer_init(&test_timer, BM_TIMER_MODE_SINGLE_SHOT, nocontext_timeout_handler);
	TEST_ASSERT_EQUAL(0, ret);

	__cmock_k_timer_stop_CMockIgnore();
	ret = bm_timer_stop(&test_timer);
	TEST_ASSERT_EQUAL(0, ret);
}

void setUp(void)
{
	memset(&test_timer, 0, sizeof(test_timer));
}

void tearDown(void) {}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
