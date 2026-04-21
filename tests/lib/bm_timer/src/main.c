/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/ztest.h>

#include <bm/bm_timer.h>

/* Timer duration that is comfortably above BM_TIMER_MIN_TIMEOUT_TICKS but short enough
 * to keep the tests fast. ~50 ms is plenty for both single shot and repeated tests.
 */
#define TEST_TIMER_MS         50U
#define TEST_TIMER_TICKS      BM_TIMER_MS_TO_TICKS(TEST_TIMER_MS)

/* Slack to allow the timer to fire before the test thread checks the result. */
#define TEST_TIMER_WAIT_MS    (TEST_TIMER_MS + 50U)

/* Magic value used to verify the context pointer is forwarded to the handler. */
#define TEST_CONTEXT_MAGIC    ((void *)0xDEADBEEFUL)

/* Shared test state updated from the timer expiry handler (interrupt context). */
static atomic_t handler_call_count;
static void *last_context;

static void timeout_handler(void *context)
{
	last_context = context;
	atomic_inc(&handler_call_count);
}

static void reset_handler_state(void)
{
	atomic_set(&handler_call_count, 0);
	last_context = NULL;
}

static void before(void *fixture)
{
	ARG_UNUSED(fixture);
	reset_handler_state();
}

ZTEST(bm_timer, test_bm_timer_init_efault)
{
	int err;
	struct bm_timer timer;

	err = bm_timer_init(NULL, BM_TIMER_MODE_SINGLE_SHOT, timeout_handler);
	zassert_equal(err, -EFAULT, "expected -EFAULT for NULL timer, got %d", err);

	err = bm_timer_init(&timer, BM_TIMER_MODE_SINGLE_SHOT, NULL);
	zassert_equal(err, -EFAULT, "expected -EFAULT for NULL handler, got %d", err);

	err = bm_timer_init(NULL, BM_TIMER_MODE_REPEATED, NULL);
	zassert_equal(err, -EFAULT, "expected -EFAULT when both args are NULL, got %d", err);
}

ZTEST(bm_timer, test_bm_timer_start_min_timeout_einval)
{
	int err;
	struct bm_timer timer;

	err = bm_timer_init(&timer, BM_TIMER_MODE_SINGLE_SHOT, timeout_handler);
	zassert_ok(err, "bm_timer_init failed, err %d", err);

	err = bm_timer_start(&timer, 0, NULL);
	zassert_equal(err, -EINVAL, "expected -EINVAL for 0 ticks, got %d", err);

	if (BM_TIMER_MIN_TIMEOUT_TICKS > 1U) {
		err = bm_timer_start(&timer, BM_TIMER_MIN_TIMEOUT_TICKS - 1U, NULL);
		zassert_equal(err, -EINVAL,
			      "expected -EINVAL for ticks below min, got %d", err);
	}
}

ZTEST(bm_timer, test_bm_timer_stop_efault)
{
	int err;

	err = bm_timer_stop(NULL);
	zassert_equal(err, -EFAULT, "expected -EFAULT for NULL timer, got %d", err);
}

ZTEST(bm_timer, test_bm_timer_single_shot)
{
	int err;
	struct bm_timer timer;

	err = bm_timer_init(&timer, BM_TIMER_MODE_SINGLE_SHOT, timeout_handler);
	zassert_ok(err, "bm_timer_init failed, err %d", err);

	err = bm_timer_start(&timer, TEST_TIMER_TICKS, NULL);
	zassert_ok(err, "bm_timer_start failed, err %d", err);

	/* Wait long enough for the timer to expire at least three times if it
	 * was incorrectly running in repeated mode.
	 */
	k_sleep(K_MSEC(TEST_TIMER_WAIT_MS * 3));

	zassert_equal(atomic_get(&handler_call_count), 1,
		      "single shot timer expired %ld times, expected 1",
		      (long)atomic_get(&handler_call_count));
}

ZTEST(bm_timer, test_bm_timer_repeated)
{
	int err;
	struct bm_timer timer;
	atomic_val_t count;

	err = bm_timer_init(&timer, BM_TIMER_MODE_REPEATED, timeout_handler);
	zassert_ok(err, "bm_timer_init failed, err %d", err);

	err = bm_timer_start(&timer, TEST_TIMER_TICKS, NULL);
	zassert_ok(err, "bm_timer_start failed, err %d", err);

	/* Wait long enough for the repeated timer to fire at least 3 times. */
	k_sleep(K_MSEC(TEST_TIMER_MS * 3 + 50U));

	err = bm_timer_stop(&timer);
	zassert_ok(err, "bm_timer_stop failed, err %d", err);

	count = atomic_get(&handler_call_count);
	zassert_true(count >= 3,
		     "repeated timer expired only %ld times, expected at least 3",
		     (long)count);
}

ZTEST(bm_timer, test_bm_timer_stop)
{
	int err;
	struct bm_timer timer;
	atomic_val_t count_after_stop;

	err = bm_timer_init(&timer, BM_TIMER_MODE_REPEATED, timeout_handler);
	zassert_ok(err, "bm_timer_init failed, err %d", err);

	err = bm_timer_start(&timer, TEST_TIMER_TICKS, NULL);
	zassert_ok(err, "bm_timer_start failed, err %d", err);

	/* Stop the timer well before it has a chance to expire. */
	err = bm_timer_stop(&timer);
	zassert_ok(err, "bm_timer_stop failed, err %d", err);

	/* Sleep long enough that the timer would have fired multiple times if
	 * stopping had failed.
	 */
	k_sleep(K_MSEC(TEST_TIMER_WAIT_MS * 3));
	count_after_stop = atomic_get(&handler_call_count);

	zassert_equal(count_after_stop, 0,
		      "timer fired %ld times after being stopped, expected 0",
		      (long)count_after_stop);

	/* Stopping an already stopped timer must still succeed. */
	err = bm_timer_stop(&timer);
	zassert_ok(err, "stopping an inactive timer failed, err %d", err);
}

ZTEST(bm_timer, test_bm_timer_context)
{
	int err;
	struct bm_timer timer;

	err = bm_timer_init(&timer, BM_TIMER_MODE_SINGLE_SHOT, timeout_handler);
	zassert_ok(err, "bm_timer_init failed, err %d", err);

	err = bm_timer_start(&timer, TEST_TIMER_TICKS, TEST_CONTEXT_MAGIC);
	zassert_ok(err, "bm_timer_start failed, err %d", err);

	k_sleep(K_MSEC(TEST_TIMER_WAIT_MS));

	zassert_equal(atomic_get(&handler_call_count), 1,
		      "expected exactly one expiry, got %ld",
		      (long)atomic_get(&handler_call_count));
	zassert_equal_ptr(last_context, TEST_CONTEXT_MAGIC,
			  "context not propagated to handler");
}

ZTEST(bm_timer, test_bm_timer_restart)
{
	int err;
	struct bm_timer timer;
	atomic_val_t count;

	err = bm_timer_init(&timer, BM_TIMER_MODE_SINGLE_SHOT, timeout_handler);
	zassert_ok(err, "bm_timer_init failed, err %d", err);

	/* Start, then restart with a fresh duration before the first expiry. */
	err = bm_timer_start(&timer, TEST_TIMER_TICKS, NULL);
	zassert_ok(err, "first bm_timer_start failed, err %d", err);

	err = bm_timer_start(&timer, TEST_TIMER_TICKS, TEST_CONTEXT_MAGIC);
	zassert_ok(err, "restart bm_timer_start failed, err %d", err);

	k_sleep(K_MSEC(TEST_TIMER_WAIT_MS * 2));

	count = atomic_get(&handler_call_count);
	zassert_equal(count, 1,
		      "single-shot timer expired %ld times after restart, expected 1",
		      (long)count);
	zassert_equal_ptr(last_context, TEST_CONTEXT_MAGIC,
			  "context not propagated to handler");
}

ZTEST_SUITE(bm_timer, NULL, NULL, before, NULL, NULL);
