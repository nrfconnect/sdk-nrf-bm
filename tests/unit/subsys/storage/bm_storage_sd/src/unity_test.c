/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <bm/storage/bm_storage.h>

#include "bm/softdevice_handler/nrf_sdh.h"
#include "cmock_nrf_sdh.h"
#include "cmock_nrf_sdm.h"
#include "cmock_nrf_soc.h"

/* Program unit for the SD backend is 16 bytes (SD_WRITE_BLOCK_SIZE). */
#define BLOCK_SIZE 16

/* Arbitrary partition, must be 32-bit word aligned. */
#define PARTITION_START 0x4200
#define PARTITION_SIZE	(BLOCK_SIZE * 2)

#define WORD_SIZE(a) ((a) / sizeof(uint32_t))

extern const struct bm_storage_info bm_storage_info;

static struct bm_storage_evt storage_event;
static bool storage_event_received;

static void bm_storage_evt_handler(struct bm_storage_evt *evt)
{
	storage_event_received = true;
	memcpy(&storage_event, evt, sizeof(*evt));

	switch (evt->id) {
	case BM_STORAGE_EVT_WRITE_RESULT:
		break;
	case BM_STORAGE_EVT_ERASE_RESULT:
		break;
	default:
		break;
	}
}

void test_bm_storage_sd_init_efault(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(NULL, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_storage_init(&storage, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_storage_init(NULL, &config);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

/* This is the first test that reaches the backend.
 * bm_storage_backend_init() calls sd_softdevice_is_enabled() and then sets the
 * static is_init flag.  All subsequent tests that call bm_storage_init() will
 * hit the early-return path (is_init==true) and will NOT invoke
 * sd_softdevice_is_enabled() again.
 */
void test_bm_storage_sd_init(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_sd_softdevice_is_enabled_ExpectAndReturn(NULL, 0);
	__cmock_sd_softdevice_is_enabled_IgnoreArg_p_softdevice_enabled();
	/* SoftDevice is disabled: writes are synchronous. */
	__cmock_sd_softdevice_is_enabled_ReturnThruPtr_p_softdevice_enabled(&(uint8_t){false});

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);
	TEST_ASSERT_TRUE(storage.initialized);
}

void test_bm_storage_sd_write_eperm(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_sd_write_einval(void)
{
	int err;
	/* Buffer size is not a multiple of the program unit (16). */
	uint8_t buf[BLOCK_SIZE - 1];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Backend already initialized by test_bm_storage_sd_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_sd_write(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Backend already initialized by test_bm_storage_sd_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_sd_flash_write_ExpectAndReturn(
		(uint32_t *)PARTITION_START, (uint32_t *)buf, WORD_SIZE(sizeof(buf)), 0);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(0, err);

	/* With SoftDevice disabled the event is dispatched synchronously. */
	TEST_ASSERT_TRUE(storage_event_received);
	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_DISPATCH_SYNC, storage_event.dispatch_type);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);
}

void test_bm_storage_sd_read_eperm(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_read(&storage, PARTITION_START, buf, sizeof(buf));
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_sd_read_einval(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Backend already initialized by test_bm_storage_sd_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_read(&storage, PARTITION_START, buf, 0);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_sd_read(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	uint32_t dummy_partition[16] = {0x00C0FFEE};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = (uintptr_t)&dummy_partition,
		.end_addr = (uintptr_t)&dummy_partition + sizeof(dummy_partition),
	};

	/* Backend already initialized by test_bm_storage_sd_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_read(&storage, (uintptr_t)&dummy_partition, buf, sizeof(buf));
	TEST_ASSERT_EQUAL(0, err);

	/* The SD backend reads via memcpy; verify the data was copied. */
	TEST_ASSERT_EQUAL_MEMORY(buf, dummy_partition, sizeof(buf));
}

void test_bm_storage_sd_is_busy(void)
{
	int err;
	bool is_busy = false;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* NULL storage. */
	is_busy = bm_storage_is_busy(NULL);
	TEST_ASSERT_TRUE(is_busy);

	/* Uninitialized storage. */
	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_TRUE(is_busy);

	/* Backend already initialized by test_bm_storage_sd_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Initialized and idle. */
	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_FALSE(is_busy);
}

void setUp(void)
{
}

void tearDown(void)
{
	memset(&storage_event, 0x00, sizeof(storage_event));
	storage_event_received = false;
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
