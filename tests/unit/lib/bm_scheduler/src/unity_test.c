/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <bm/bm_scheduler.h>

#define MAX_RECORDED_CALLS 16

struct call_record {
	bm_scheduler_fn_t handler;
	size_t len;
	uint8_t data[64];
};

static struct call_record call_log[MAX_RECORDED_CALLS];
static size_t call_count;

static void record_call(bm_scheduler_fn_t handler, void *evt, size_t len)
{
	if (call_count >= MAX_RECORDED_CALLS) {
		return;
	}

	call_log[call_count].handler = handler;
	call_log[call_count].len = len;
	if (len > 0 && len <= sizeof(call_log[call_count].data)) {
		memcpy(call_log[call_count].data, evt, len);
	}
	call_count++;
}

static void handler_a(void *evt, size_t len)
{
	record_call(handler_a, evt, len);
}

static void handler_b(void *evt, size_t len)
{
	record_call(handler_b, evt, len);
}

void test_bm_scheduler_defer_efault(void)
{
	int err;
	uint8_t data = 0;

	err = bm_scheduler_defer(NULL, NULL, 0);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_scheduler_defer(NULL, &data, sizeof(data));
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_scheduler_defer_einval(void)
{
	int err;
	uint8_t data = 0;

	/* data != NULL but len == 0 */
	err = bm_scheduler_defer(handler_a, &data, 0);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	/* data == NULL but len != 0 */
	err = bm_scheduler_defer(handler_a, NULL, sizeof(data));
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_scheduler_defer_enomem(void)
{
	int err;
	uint8_t big_data[CONFIG_BM_SCHEDULER_BUF_SIZE + 1] = {0};

	/* Requesting more than the entire heap is guaranteed to fail. */
	err = bm_scheduler_defer(handler_a, big_data, sizeof(big_data));
	TEST_ASSERT_EQUAL(-ENOMEM, err);
}

void test_bm_scheduler_defer_no_data(void)
{
	int err;

	/* NULL data with 0 length is a valid combination. */
	err = bm_scheduler_defer(handler_a, NULL, 0);
	TEST_ASSERT_EQUAL(0, err);

	/* The deferred event must be invoked when processing runs. */
	(void)bm_scheduler_process();

	TEST_ASSERT_EQUAL(1, call_count);
	TEST_ASSERT_EQUAL_PTR(handler_a, call_log[0].handler);
	TEST_ASSERT_EQUAL(0, call_log[0].len);
}

void test_bm_scheduler_defer(void)
{
	int err;
	uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};

	err = bm_scheduler_defer(handler_a, payload, sizeof(payload));
	TEST_ASSERT_EQUAL(0, err);

	/* Defer must not invoke the handler synchronously. */
	TEST_ASSERT_EQUAL(0, call_count);

	(void)bm_scheduler_process();

	TEST_ASSERT_EQUAL(1, call_count);
	TEST_ASSERT_EQUAL_PTR(handler_a, call_log[0].handler);
	TEST_ASSERT_EQUAL(sizeof(payload), call_log[0].len);
	TEST_ASSERT_EQUAL_MEMORY(payload, call_log[0].data, sizeof(payload));
}

void test_bm_scheduler_process(void)
{
	int err;

	/* Processing with no deferred events is a no-op that returns success. */
	err = bm_scheduler_process();
	TEST_ASSERT_EQUAL(0, err);
	TEST_ASSERT_EQUAL(0, call_count);
}

void test_bm_scheduler_process_dispatches_in_order(void)
{
	int err;
	uint8_t payload_a[] = {1, 2, 3};
	uint8_t payload_b[] = {0xAA, 0xBB};

	/* Defer two events with distinct handlers and payloads, so we can
	 * verify both that each event is routed to its own handler and that
	 * events are dispatched in FIFO order.
	 */
	err = bm_scheduler_defer(handler_a, payload_a, sizeof(payload_a));
	TEST_ASSERT_EQUAL(0, err);

	err = bm_scheduler_defer(handler_b, payload_b, sizeof(payload_b));
	TEST_ASSERT_EQUAL(0, err);

	TEST_ASSERT_EQUAL(0, call_count);

	err = bm_scheduler_process();
	TEST_ASSERT_EQUAL(0, err);

	TEST_ASSERT_EQUAL(2, call_count);

	TEST_ASSERT_EQUAL_PTR(handler_a, call_log[0].handler);
	TEST_ASSERT_EQUAL(sizeof(payload_a), call_log[0].len);
	TEST_ASSERT_EQUAL_MEMORY(payload_a, call_log[0].data, sizeof(payload_a));

	TEST_ASSERT_EQUAL_PTR(handler_b, call_log[1].handler);
	TEST_ASSERT_EQUAL(sizeof(payload_b), call_log[1].len);
	TEST_ASSERT_EQUAL_MEMORY(payload_b, call_log[1].data, sizeof(payload_b));
}

void test_bm_scheduler_process_data_is_copied(void)
{
	int err;
	uint8_t payload[] = {0x55, 0x66, 0x77, 0x88};
	const uint8_t expected[] = {0x55, 0x66, 0x77, 0x88};

	err = bm_scheduler_defer(handler_a, payload, sizeof(payload));
	TEST_ASSERT_EQUAL(0, err);

	/* Mutate the source buffer after deferring. The scheduler must have
	 * captured a snapshot of the data, so the handler should see the
	 * original bytes, not the mutated ones.
	 */
	memset(payload, 0, sizeof(payload));

	(void)bm_scheduler_process();

	TEST_ASSERT_EQUAL(1, call_count);
	TEST_ASSERT_EQUAL(sizeof(payload), call_log[0].len);

	TEST_ASSERT_EQUAL_MEMORY(expected, call_log[0].data, sizeof(expected));
}

void test_bm_scheduler_process_releases_memory(void)
{
	int err;
	uint8_t payload[64] = {0};

	/* Fill the heap until allocation fails, then drain the queue. After
	 * processing, the heap must be free again so a new defer succeeds.
	 */
	do {
		err = bm_scheduler_defer(handler_a, payload, sizeof(payload));
	} while (err == 0);
	TEST_ASSERT_EQUAL(-ENOMEM, err);

	err = bm_scheduler_process();
	TEST_ASSERT_EQUAL(0, err);

	/* The heap must be reusable after processing. */
	err = bm_scheduler_defer(handler_a, payload, sizeof(payload));
	TEST_ASSERT_EQUAL(0, err);
}

void setUp(void)
{
	/* Drain any deferred events that may have leaked from a previous
	 * test, so that each test starts from a clean state.
	 */
	(void)bm_scheduler_process();

	call_count = 0;
	memset(call_log, 0, sizeof(call_log));
}

void tearDown(void)
{
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
