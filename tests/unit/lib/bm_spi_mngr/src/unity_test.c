/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <bm/bm_spi_mngr.h>

#include "cmock_nrfx_spim.h"

/* Queue holds up to 2 pending transactions, not counting the one in progress. */
#define QUEUE_SIZE 2
/* Max number of nrfx_spim_xfer() calls stub_nrfx_spim_xfer_record() can remember. */
#define MAX_RECORDED_XFERS 3

BM_SPI_MNGR_DEF(spi_mgr, QUEUE_SIZE, 0);

static nrfx_spim_config_t default_cfg = NRFX_SPIM_DEFAULT_CONFIG(0, 1, 2, 3);

/* Captured from bm_spi_mngr_init()'s call into nrfx_spim_init(),
 * so tests can fire the "interrupt" manually by calling this handler with a fake event,
 * instead of real hardware.
 */
static nrfx_spim_event_handler_t captured_handler;
static void *captured_context;

/* Records begin/end callback invocations so tests can assert order, args and results. */
struct callback_record {
	bool begin_called;
	bool end_called;
	int end_result;
	void *user_data;
};
static struct callback_record cb_log;

/* Tx buffer pointer of every nrfx_spim_xfer() call, in call order,
 * filled in by stub_nrfx_spim_xfer_record(),
 * so tests can check transactions started in the right order.
 */
static const uint8_t *recorded_tx[MAX_RECORDED_XFERS];
static int recorded_count;

/* Per-transaction end_callback call counts,
 * incremented by end_cb_txn0/1/2(),
 * so tests can check each transaction completed,
 * with no duplicate or missing dispatch.
 */
static int txn_done_count[3];

/* Set by end_cb_set_complete_flag(), so tests can poll it like a caller would,
 * when using the "poll a flag set by end_callback" blocking pattern.
 */
static volatile bool txn_complete_flag;

static int stub_nrfx_spim_init(nrfx_spim_t *p_instance, nrfx_spim_config_t const *p_config,
			       nrfx_spim_event_handler_t handler, void *p_context,
			       int cmock_num_calls)
{
	/* Stub for nrfx_spim_init(). Captures the handler and context so tests can later
	 * simulate the SPIM interrupt firing by calling captured_handler() directly.
	 */
	ARG_UNUSED(p_instance);
	ARG_UNUSED(p_config);
	ARG_UNUSED(cmock_num_calls);

	captured_handler = handler;
	captured_context = p_context;

	return 0;
}

static int stub_nrfx_spim_xfer_record(nrfx_spim_t *p_instance,
				      nrfx_spim_xfer_desc_t const *p_xfer_desc, uint32_t flags,
				      int cmock_num_calls)
{
	/* Stub for nrfx_spim_xfer(). Records the tx buffer pointer of every call, in call order,
	 * into recorded_tx[], so tests can check transactions started in order.
	 */
	ARG_UNUSED(p_instance);
	ARG_UNUSED(flags);
	ARG_UNUSED(cmock_num_calls);

	if (recorded_count < MAX_RECORDED_XFERS) {
		recorded_tx[recorded_count] = p_xfer_desc->p_tx_buffer;
	}
	recorded_count++;

	return 0;
}

static void begin_cb_schedule_next(void *user_data)
{
	/* begin_callback that schedules another transaction (passed in via user_data),
	 * used to simulate a second call site scheduling work while the current one is in flight.
	 */
	const struct bm_spi_mngr_transaction *next =
		(const struct bm_spi_mngr_transaction *)user_data;

	(void)bm_spi_mngr_schedule(&spi_mgr, next);
}

static void begin_cb(void *user_data)
{
	cb_log.begin_called = true;
	cb_log.user_data = user_data;
}

static void end_cb(int result, void *user_data)
{
	cb_log.end_called = true;
	cb_log.end_result = result;
	cb_log.user_data = user_data;
}

static void end_cb_txn0(int result, void *user_data)
{
	ARG_UNUSED(result);
	ARG_UNUSED(user_data);
	txn_done_count[0]++;
}

static void end_cb_txn1(int result, void *user_data)
{
	ARG_UNUSED(result);
	ARG_UNUSED(user_data);
	txn_done_count[1]++;
}

static void end_cb_txn2(int result, void *user_data)
{
	ARG_UNUSED(result);
	ARG_UNUSED(user_data);
	txn_done_count[2]++;
}

static void end_cb_set_complete_flag(int result, void *user_data)
{
	ARG_UNUSED(result);
	ARG_UNUSED(user_data);
	txn_complete_flag = true;
}

/* bm_spi_mngr_init() Unit Tests */
void test_bm_spi_mngr_init_efault(void)
{
	int err;

	err = bm_spi_mngr_init(NULL, &default_cfg);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_spi_mngr_init(&spi_mgr, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_spi_mngr_init_efault_no_queue_storage(void)
{
	int err;
	struct bm_spi_mngr mgr_no_storage = {
		.queue = {
			.size = QUEUE_SIZE,
			.byte_storage = NULL
		},
		.spim = spi_mgr.spim,
	};

	err = bm_spi_mngr_init(&mgr_no_storage, &default_cfg);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_spi_mngr_init_einval_zero_queue_size(void)
{
	int err;
	struct bm_spi_mngr mgr_zero_size = {
		.queue = {
			.size = 0,
			.byte_storage = spi_mgr.queue.byte_storage
		},
		.spim = spi_mgr.spim,
	};

	err = bm_spi_mngr_init(&mgr_zero_size, &default_cfg);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_spi_mngr_init_nrfx_error_propagated(void)
{
	int err;

	__cmock_nrfx_spim_init_ExpectAnyArgsAndReturn(-EIO);

	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(-EIO, err);
}

void test_bm_spi_mngr_init_success(void)
{
	int err;
	bool idle;

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);

	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);
	TEST_ASSERT_NOT_NULL(captured_handler);

	idle = bm_spi_mngr_is_idle(&spi_mgr);
	TEST_ASSERT_TRUE(idle);
}

/* bm_spi_mngr_uninit() Unit Tests */
void test_bm_spi_mngr_uninit_efault(void)
{
	int err;

	err = bm_spi_mngr_uninit(NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_spi_mngr_uninit_success(void)
{
	int err;
	bool idle;

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	idle = bm_spi_mngr_is_idle(&spi_mgr);
	TEST_ASSERT_TRUE(idle);

	__cmock_nrfx_spim_uninit_Expect(spi_mgr.spim);
	err = bm_spi_mngr_uninit(&spi_mgr);
	TEST_ASSERT_EQUAL(0, err);
}

/* bm_spi_mngr_schedule() Unit Tests */
void test_bm_spi_mngr_schedule_efault(void)
{
	int err;
	uint8_t tx[] = { 0 };
	struct bm_spi_mngr_transfer transfer[] = {
		BM_SPI_MNGR_TRANSFER(tx, sizeof(tx), NULL, 0),
	};
	struct bm_spi_mngr_transaction txn = {
		.transfers = transfer,
		.number_of_transfers = 1,
	};

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_spi_mngr_schedule(NULL, &txn);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_spi_mngr_schedule(&spi_mgr, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	txn.transfers = NULL;
	err = bm_spi_mngr_schedule(&spi_mgr, &txn);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_spi_mngr_schedule_einval_zero_transfers(void)
{
	int err;
	uint8_t tx[] = { 0 };
	struct bm_spi_mngr_transfer transfer[] = {
		BM_SPI_MNGR_TRANSFER(tx, sizeof(tx), NULL, 0),
	};
	struct bm_spi_mngr_transaction txn = {
		.transfers = transfer,
		.number_of_transfers = 0,
	};

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_spi_mngr_schedule(&spi_mgr, &txn);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_spi_mngr_schedule_success_starts_transfer(void)
{
	int err;
	bool idle;
	int user_data;
	uint8_t tx[] = { 0xAB };
	struct bm_spi_mngr_transfer transfer[] = {
		BM_SPI_MNGR_TRANSFER(tx, sizeof(tx), NULL, 0),
	};
	struct bm_spi_mngr_transaction txn = {
		.begin_callback = begin_cb,
		.end_callback = end_cb,
		.user_data = &user_data,
		.transfers = transfer,
		.number_of_transfers = 1,
	};

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	/* begin_callback runs synchronously as part of scheduling, before the transfer starts. */
	__cmock_nrfx_spim_xfer_ExpectAnyArgsAndReturn(0);

	err = bm_spi_mngr_schedule(&spi_mgr, &txn);
	TEST_ASSERT_EQUAL(0, err);

	TEST_ASSERT_TRUE(cb_log.begin_called);
	TEST_ASSERT_EQUAL_PTR(&user_data, cb_log.user_data);
	TEST_ASSERT_FALSE(cb_log.end_called);

	idle = bm_spi_mngr_is_idle(&spi_mgr);
	TEST_ASSERT_FALSE(idle);
}

void test_bm_spi_mngr_schedule_completion_runs_end_callback(void)
{
	int err;
	bool idle;
	uint8_t tx[] = { 0xAB };
	struct bm_spi_mngr_transfer transfer[] = {
		BM_SPI_MNGR_TRANSFER(tx, sizeof(tx), NULL, 0),
	};
	struct bm_spi_mngr_transaction txn = {
		.end_callback = end_cb,
		.transfers = transfer,
		.number_of_transfers = 1,
	};
	nrfx_spim_event_t done_evt = {
		.type = NRFX_SPIM_EVENT_DONE
	};

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_nrfx_spim_xfer_ExpectAnyArgsAndReturn(0);
	err = bm_spi_mngr_schedule(&spi_mgr, &txn);
	TEST_ASSERT_EQUAL(0, err);

	/* Simulate the SPIM interrupt firing once the transfer finishes. */
	captured_handler(&done_evt, captured_context);

	TEST_ASSERT_TRUE(cb_log.end_called);
	TEST_ASSERT_EQUAL(0, cb_log.end_result);

	idle = bm_spi_mngr_is_idle(&spi_mgr);
	TEST_ASSERT_TRUE(idle);
}

void test_bm_spi_mngr_schedule_xfer_start_failure(void)
{
	int err;
	bool idle;
	uint8_t tx[] = { 0xAB };
	struct bm_spi_mngr_transfer transfer[] = {
		BM_SPI_MNGR_TRANSFER(tx, sizeof(tx), NULL, 0),
	};
	struct bm_spi_mngr_transaction txn = {
		.end_callback = end_cb,
		.transfers = transfer,
		.number_of_transfers = 1,
	};

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	/* nrfx_spim_xfer() fails to even start the transfer. */
	__cmock_nrfx_spim_xfer_ExpectAnyArgsAndReturn(-EIO);

	err = bm_spi_mngr_schedule(&spi_mgr, &txn);
	TEST_ASSERT_EQUAL(0, err);

	TEST_ASSERT_TRUE(cb_log.end_called);
	TEST_ASSERT_EQUAL(-EIO, cb_log.end_result);

	idle = bm_spi_mngr_is_idle(&spi_mgr);
	TEST_ASSERT_TRUE(idle);
}

void test_bm_spi_mngr_schedule_transfer_failure_event(void)
{
	int err;
	bool idle;
	uint8_t tx[] = { 0xAB };
	struct bm_spi_mngr_transfer transfer[] = {
		BM_SPI_MNGR_TRANSFER(tx, sizeof(tx), NULL, 0),
	};
	struct bm_spi_mngr_transaction txn = {
		.end_callback = end_cb,
		.transfers = transfer,
		.number_of_transfers = 1,
	};
	/* Any event type other than NRFX_SPIM_EVENT_DONE is treated as a failure. */
	nrfx_spim_event_t error_evt = {
		.type = (nrfx_spim_event_type_t)(NRFX_SPIM_EVENT_DONE + 1)
	};

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_nrfx_spim_xfer_ExpectAnyArgsAndReturn(0);
	err = bm_spi_mngr_schedule(&spi_mgr, &txn);
	TEST_ASSERT_EQUAL(0, err);

	/* Simulate a failed transfer, reported by the SPIM interrupt. */
	captured_handler(&error_evt, captured_context);

	TEST_ASSERT_TRUE(cb_log.end_called);
	TEST_ASSERT_EQUAL(-EIO, cb_log.end_result);

	idle = bm_spi_mngr_is_idle(&spi_mgr);
	TEST_ASSERT_TRUE(idle);
}

void test_bm_spi_mngr_schedule_enomem_queue_full(void)
{
	int err;
	uint8_t tx[] = { 0xAB };
	struct bm_spi_mngr_transfer transfer[] = {
		BM_SPI_MNGR_TRANSFER(tx, sizeof(tx), NULL, 0),
	};
	struct bm_spi_mngr_transaction txn = {
		.transfers = transfer,
		.number_of_transfers = 1,
	};

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	/* First transaction starts immediately (becomes "current"), never completes. */
	__cmock_nrfx_spim_xfer_ExpectAnyArgsAndReturn(0);
	err = bm_spi_mngr_schedule(&spi_mgr, &txn);
	TEST_ASSERT_EQUAL(0, err);

	/* Fill the whole pending queue (QUEUE_SIZE slots). */
	for (int i = 0; i < QUEUE_SIZE; i++) {
		err = bm_spi_mngr_schedule(&spi_mgr, &txn);
		TEST_ASSERT_EQUAL(0, err);
	}

	/* Queue is now full, next schedule must fail. */
	err = bm_spi_mngr_schedule(&spi_mgr, &txn);
	TEST_ASSERT_EQUAL(-ENOMEM, err);
}


void test_bm_spi_mngr_schedule_custom_config_reinit(void)
{
	/* Verifies a transaction carrying its own SPIM configuration,
	 * different from the active one,
	 * reconfigures the instance (nrfx_spim_uninit + nrfx_spim_init) before the transfer starts.
	 * This covers both the "transaction has a custom config" and the "config differs" branch.
	 */
	int err;
	bool idle;
	uint8_t tx[] = { 0xAB };

	/* A configuration that differs from default_cfg (different pins),
	 * so the comparison against the currently active configuration is guaranteed to be nonzero.
	 */
	static nrfx_spim_config_t custom_cfg = NRFX_SPIM_DEFAULT_CONFIG(10, 11, 12, 13);
	struct bm_spi_mngr_transfer transfer[] = {
		BM_SPI_MNGR_TRANSFER(tx, sizeof(tx), NULL, 0),
	};
	struct bm_spi_mngr_transaction txn = {
		.end_callback = end_cb,
		.transfers = transfer,
		.number_of_transfers = 1,
		.required_spim_cfg = &custom_cfg,
	};
	nrfx_spim_event_t done_evt = {
		.type = NRFX_SPIM_EVENT_DONE
	};

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	/* Because the transaction's config differs from the active (default) one,
	 * so the manager must uninitialize and reinitialize the SPIM instance before the transfer.
	 */
	__cmock_nrfx_spim_uninit_Expect(spi_mgr.spim);
	__cmock_nrfx_spim_xfer_ExpectAnyArgsAndReturn(0);

	err = bm_spi_mngr_schedule(&spi_mgr, &txn);
	TEST_ASSERT_EQUAL(0, err);

	/* Complete the transaction so the manager returns to idle. */
	captured_handler(&done_evt, captured_context);

	TEST_ASSERT_TRUE(cb_log.end_called);
	TEST_ASSERT_EQUAL(0, cb_log.end_result);

	idle = bm_spi_mngr_is_idle(&spi_mgr);
	TEST_ASSERT_TRUE(idle);
}

void test_bm_spi_mngr_schedule_next_transfer_start_failure(void)
{
	/* Verifies that if a later transfer within a transaction fails to start,
	 * the transaction is finished with that error code and the manager returns to idle.
	 */
	int err;
	bool idle;
	uint8_t tx1[] = { 0x01 };
	uint8_t tx2[] = { 0x02 };
	struct bm_spi_mngr_transfer transfer[] = {
		BM_SPI_MNGR_TRANSFER(tx1, sizeof(tx1), NULL, 0),
		BM_SPI_MNGR_TRANSFER(tx2, sizeof(tx2), NULL, 0),
	};
	struct bm_spi_mngr_transaction txn = {
		.end_callback = end_cb,
		.transfers = transfer,
		.number_of_transfers = 2,
	};
	nrfx_spim_event_t done_evt = {
		.type = NRFX_SPIM_EVENT_DONE
	};

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	/* First transfer starts successfully, the second one fails to start. */
	__cmock_nrfx_spim_xfer_ExpectAnyArgsAndReturn(0);
	__cmock_nrfx_spim_xfer_ExpectAnyArgsAndReturn(-EIO);

	err = bm_spi_mngr_schedule(&spi_mgr, &txn);
	TEST_ASSERT_EQUAL(0, err);

	/* Completing the first transfer makes the manager start the second one, which fails. */
	captured_handler(&done_evt, captured_context);

	TEST_ASSERT_TRUE(cb_log.end_called);
	TEST_ASSERT_EQUAL(-EIO, cb_log.end_result);

	idle = bm_spi_mngr_is_idle(&spi_mgr);
	TEST_ASSERT_TRUE(idle);
}

/* bm_spi_mngr_is_idle() Unit Tests */
void test_bm_spi_mngr_is_idle_true_after_init(void)
{
	int err;
	bool idle;

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	idle = bm_spi_mngr_is_idle(&spi_mgr);
	TEST_ASSERT_TRUE(idle);
}

void test_bm_spi_mngr_is_idle_false_while_busy(void)
{
	int err;
	bool idle;
	uint8_t tx[] = { 0xAB };
	struct bm_spi_mngr_transfer transfer[] = {
		BM_SPI_MNGR_TRANSFER(tx, sizeof(tx), NULL, 0),
	};
	struct bm_spi_mngr_transaction txn = {
		.transfers = transfer,
		.number_of_transfers = 1,
	};

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_nrfx_spim_xfer_ExpectAnyArgsAndReturn(0);
	err = bm_spi_mngr_schedule(&spi_mgr, &txn);
	TEST_ASSERT_EQUAL(0, err);

	idle = bm_spi_mngr_is_idle(&spi_mgr);
	TEST_ASSERT_FALSE(idle);
}

/* Verifies that transactions queued one after another are started in that same order. */
void test_bm_spi_mngr_schedule_fifo_order(void)
{
	/* Schedule three transactions, A, B and C, before any of them get a chance to run.
	 * A starts right away and B, C wait in the queue.
	 * Then simulate the SPIM interrupt firing once per transaction,
	 * and check that nrfx_spim_xfer() was given each transaction's own data in the same
	 * A, B, C order they were scheduled in.
	 */
	int err;
	bool idle;
	uint8_t tx_a[] = { 0xAA };
	uint8_t tx_b[] = { 0xBB };
	uint8_t tx_c[] = { 0xCC };
	struct bm_spi_mngr_transfer transfer_a[] = {
		BM_SPI_MNGR_TRANSFER(tx_a, sizeof(tx_a), NULL, 0),
	};
	struct bm_spi_mngr_transfer transfer_b[] = {
		BM_SPI_MNGR_TRANSFER(tx_b, sizeof(tx_b), NULL, 0),
	};
	struct bm_spi_mngr_transfer transfer_c[] = {
		BM_SPI_MNGR_TRANSFER(tx_c, sizeof(tx_c), NULL, 0),
	};
	struct bm_spi_mngr_transaction txn_a = {
		.transfers = transfer_a,
		.number_of_transfers = 1,
	};
	struct bm_spi_mngr_transaction txn_b = {
		.transfers = transfer_b,
		.number_of_transfers = 1,
	};
	struct bm_spi_mngr_transaction txn_c = {
		.transfers = transfer_c,
		.number_of_transfers = 1,
	};
	nrfx_spim_event_t done_evt = {
		.type = NRFX_SPIM_EVENT_DONE
	};

	recorded_count = 0;

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_nrfx_spim_xfer_Stub(stub_nrfx_spim_xfer_record);
	err = bm_spi_mngr_schedule(&spi_mgr, &txn_a);
	TEST_ASSERT_EQUAL(0, err);
	err = bm_spi_mngr_schedule(&spi_mgr, &txn_b);
	TEST_ASSERT_EQUAL(0, err);
	err = bm_spi_mngr_schedule(&spi_mgr, &txn_c);
	TEST_ASSERT_EQUAL(0, err);

	/* Each "interrupt" finishes the current transaction,
	 * which makes the manager pop and start the next queued one on its own.
	 */
	captured_handler(&done_evt, captured_context);
	captured_handler(&done_evt, captured_context);
	captured_handler(&done_evt, captured_context);

	TEST_ASSERT_EQUAL(3, recorded_count);
	TEST_ASSERT_EQUAL_PTR(tx_a, recorded_tx[0]);
	TEST_ASSERT_EQUAL_PTR(tx_b, recorded_tx[1]);
	TEST_ASSERT_EQUAL_PTR(tx_c, recorded_tx[2]);

	idle = bm_spi_mngr_is_idle(&spi_mgr);
	TEST_ASSERT_TRUE(idle);
}

/* Verifies a transaction scheduled while another is still running still gets its turn. */
void test_bm_spi_mngr_schedule_concurrent_transactions(void)
{
	/* A and B are scheduled up front, same as any normal queueing.
	 * The twist is C: it only gets scheduled from inside B's begin_callback,
	 * i.e. from a second call site while B itself is already "in flight" on the bus.
	 * All three should still start and complete exactly once, in order, whether they were
	 * scheduled up front or added mid-flight like this, with nothing skipped or run twice.
	 */
	int err;
	bool idle;
	uint8_t tx_a[] = { 0xA1 };
	uint8_t tx_b[] = { 0xB2 };
	uint8_t tx_c[] = { 0xC3 };
	struct bm_spi_mngr_transfer transfer_a[] = {
		BM_SPI_MNGR_TRANSFER(tx_a, sizeof(tx_a), NULL, 0),
	};
	struct bm_spi_mngr_transfer transfer_b[] = {
		BM_SPI_MNGR_TRANSFER(tx_b, sizeof(tx_b), NULL, 0),
	};
	struct bm_spi_mngr_transfer transfer_c[] = {
		BM_SPI_MNGR_TRANSFER(tx_c, sizeof(tx_c), NULL, 0),
	};
	struct bm_spi_mngr_transaction txn_c = {
		.end_callback = end_cb_txn2,
		.transfers = transfer_c,
		.number_of_transfers = 1,
	};
	struct bm_spi_mngr_transaction txn_b = {
		.begin_callback = begin_cb_schedule_next,
		.end_callback = end_cb_txn1,
		.user_data = &txn_c,
		.transfers = transfer_b,
		.number_of_transfers = 1,
	};
	struct bm_spi_mngr_transaction txn_a = {
		.end_callback = end_cb_txn0,
		.transfers = transfer_a,
		.number_of_transfers = 1,
	};
	nrfx_spim_event_t done_evt = {
		.type = NRFX_SPIM_EVENT_DONE
	};

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	/* Every nrfx_spim_xfer() call just succeeds, only the dispatch counts matter here. */
	__cmock_nrfx_spim_xfer_IgnoreAndReturn(0);
	err = bm_spi_mngr_schedule(&spi_mgr, &txn_a);
	TEST_ASSERT_EQUAL(0, err);
	err = bm_spi_mngr_schedule(&spi_mgr, &txn_b);
	TEST_ASSERT_EQUAL(0, err);

	/* Finishing A starts B, whose begin_callback schedules C "from the side". */
	captured_handler(&done_evt, captured_context);
	captured_handler(&done_evt, captured_context);
	captured_handler(&done_evt, captured_context);

	TEST_ASSERT_EQUAL(1, txn_done_count[0]);
	TEST_ASSERT_EQUAL(1, txn_done_count[1]);
	TEST_ASSERT_EQUAL(1, txn_done_count[2]);

	idle = bm_spi_mngr_is_idle(&spi_mgr);
	TEST_ASSERT_TRUE(idle);
}

/* Verifies the "poll a flag set by end_callback" pattern works for a multi-transfer transaction. */
void test_bm_spi_mngr_schedule_poll_flag_usage(void)
{
	/* A two transfer transaction only sets the flag once both transfers are done,
	 * so servicing the SPIM interrupt in a loop while polling the flag has to spin twice,
	 * the same way it would while waiting for a real interrupt in between.
	 * Check that the flag reflects completion, and that iterations matches the transfer count.
	 */
	int err;
	int iterations = 0;
	uint8_t tx1[] = { 0x01 };
	uint8_t tx2[] = { 0x02 };
	struct bm_spi_mngr_transfer transfer[] = {
		BM_SPI_MNGR_TRANSFER(tx1, sizeof(tx1), NULL, 0),
		BM_SPI_MNGR_TRANSFER(tx2, sizeof(tx2), NULL, 0),
	};
	struct bm_spi_mngr_transaction txn = {
		.end_callback = end_cb_set_complete_flag,
		.transfers = transfer,
		.number_of_transfers = 2,
	};
	nrfx_spim_event_t done_evt = {
		.type = NRFX_SPIM_EVENT_DONE
	};

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	/* Every nrfx_spim_xfer() call succeeds, only the flag and iteration count matter here. */
	__cmock_nrfx_spim_xfer_IgnoreAndReturn(0);
	err = bm_spi_mngr_schedule(&spi_mgr, &txn);
	TEST_ASSERT_EQUAL(0, err);

	/* Keep servicing the bus until end_callback sets the flag. */
	while (!txn_complete_flag) {
		captured_handler(&done_evt, captured_context);
		iterations++;
	}

	TEST_ASSERT_EQUAL(txn.number_of_transfers, iterations);
	TEST_ASSERT_TRUE(txn_complete_flag);
}

/* Verifies the same transaction object can be scheduled more than once, back-to-back. */
void test_bm_spi_mngr_schedule_repeated_identical_transfers(void)
{
	/* Schedules the same txn struct twice, before either gets a chance to run,
	 * the manager only stores a pointer to the transaction in its queue,
	 * so this pushes the same pointer into two queue slots,
	 * confirming both queued instances still run and complete independently,
	 * with end_callback firing once per schedule call.
	 */
	int err;
	int iterations = 0;
	uint8_t tx[] = { 0x5A };
	struct bm_spi_mngr_transfer transfer[] = {
		BM_SPI_MNGR_TRANSFER(tx, sizeof(tx), NULL, 0),
		BM_SPI_MNGR_TRANSFER(tx, sizeof(tx), NULL, 0),
	};
	struct bm_spi_mngr_transaction txn = {
		.end_callback = end_cb_txn0,
		.transfers = transfer,
		.number_of_transfers = ARRAY_SIZE(transfer),
	};
	nrfx_spim_event_t done_evt = {
		.type = NRFX_SPIM_EVENT_DONE
	};

	__cmock_nrfx_spim_init_Stub(stub_nrfx_spim_init);
	err = bm_spi_mngr_init(&spi_mgr, &default_cfg);
	TEST_ASSERT_EQUAL(0, err);

	/* Every nrfx_spim_xfer() call just succeeds, only the completion count matters here, */
	__cmock_nrfx_spim_xfer_IgnoreAndReturn(0);

	/* Schedule the same txn object twice, back to back, before either one runs. */
	err = bm_spi_mngr_schedule(&spi_mgr, &txn);
	TEST_ASSERT_EQUAL(0, err);
	err = bm_spi_mngr_schedule(&spi_mgr, &txn);
	TEST_ASSERT_EQUAL(0, err);

	/* Drive the bus until both scheduled copies have finished, */
	while (!bm_spi_mngr_is_idle(&spi_mgr)) {
		captured_handler(&done_evt, captured_context);
		iterations++;
	}

	TEST_ASSERT_EQUAL(2 * ARRAY_SIZE(transfer), iterations);
	TEST_ASSERT_EQUAL(2, txn_done_count[0]);
}

/* Unit Test Setup */
void setUp(void)
{
	captured_handler = NULL;
	captured_context = NULL;
	memset(&cb_log, 0, sizeof(cb_log));
	memset(txn_done_count, 0, sizeof(txn_done_count));
}

void tearDown(void)
{
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
