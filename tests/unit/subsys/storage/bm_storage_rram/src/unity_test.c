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

#include "cmock_nrfx_rramc.h"

/* RRAM backend uses 16-byte program unit. */
#define BLOCK_SIZE 16

/* Arbitrary partition, must be 32-bit word aligned. */
#define PARTITION_START 0x4200
#define PARTITION_SIZE  (BLOCK_SIZE * 2)

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

void test_bm_storage_rram_init_efault(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
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
 * The RRAM backend init increments the refcount and, since refcount == 1,
 * calls nrfx_rramc_init() to initialize the hardware.
 * All subsequent tests that call bm_storage_init() will increment the refcount
 * past 1 and skip the hardware initialization path, so they will NOT invoke
 * nrfx_rramc_init() again.
 */
void test_bm_storage_rram_init(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_nrfx_rramc_init_ExpectAnyArgsAndReturn(0);
	__cmock_nrfx_rramc_init_IgnoreArg_p_config();
	__cmock_nrfx_rramc_init_IgnoreArg_handler();

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);
	TEST_ASSERT_TRUE(storage.initialized);
}

void test_bm_storage_rram_init_eperm(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Backend already initialized by test_bm_storage_rram_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Double initialization on the same instance is an error. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_rram_uninit_efault(void)
{
	int err;

	err = bm_storage_uninit(NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_rram_uninit_eperm(void)
{
	int err;
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_rram_uninit(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Backend already initialized by test_bm_storage_rram_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
	TEST_ASSERT_FALSE(storage.initialized);
}

void test_bm_storage_rram_init_uninit_init(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Backend already initialized by test_bm_storage_rram_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);

	/* Re-initialization after uninit must succeed. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);
	TEST_ASSERT_TRUE(storage.initialized);
}

void test_bm_storage_rram_write_eperm(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_rram_write_einval(void)
{
	int err;
	/* Buffer size is not a multiple of the program unit (16). */
	uint8_t buf[BLOCK_SIZE - 1];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Backend already initialized by test_bm_storage_rram_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_rram_write_efault(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Backend already initialized by test_bm_storage_rram_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_write(&storage, PARTITION_START, NULL, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_rram_write(void)
{
	int err;
	bool is_busy;
	uint8_t buf[BLOCK_SIZE];
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Backend already initialized by test_bm_storage_rram_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_nrfx_rramc_bytes_write_ExpectAnyArgs();
	__cmock_nrfx_rramc_bytes_write_IgnoreArg_address();
	__cmock_nrfx_rramc_bytes_write_IgnoreArg_src();
	__cmock_nrfx_rramc_bytes_write_IgnoreArg_num_bytes();

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(0, err);

	/* RRAM backend dispatches events synchronously. */
	TEST_ASSERT_TRUE(storage_event_received);
	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(false, storage_event.is_async);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);

	/* Not busy after sync write completed. */
	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_FALSE(is_busy);
}

void test_bm_storage_rram_read_eperm(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_read(&storage, PARTITION_START, buf, sizeof(buf));
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_rram_read_einval(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Backend already initialized by test_bm_storage_rram_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_read(&storage, PARTITION_START, buf, 0);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_rram_read(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Backend already initialized by test_bm_storage_rram_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_nrfx_rramc_buffer_read_Expect(buf, PARTITION_START, sizeof(buf));

	err = bm_storage_read(&storage, PARTITION_START, buf, sizeof(buf));
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_rram_read_efault(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Backend already initialized by test_bm_storage_rram_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Read address is past the end of the partition. */
	err = bm_storage_read(&storage, PARTITION_START + PARTITION_SIZE, buf, sizeof(buf));
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_rram_erase_eperm(void)
{
	int err;
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_rram_erase(void)
{
	int err;
	bool is_busy;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Backend already initialized by test_bm_storage_rram_init. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Erase writes erase_value (0xFF) in BLOCK_SIZE chunks via nrfx_rramc_bytes_write. */
	__cmock_nrfx_rramc_bytes_write_ExpectAnyArgs();
	__cmock_nrfx_rramc_bytes_write_IgnoreArg_address();
	__cmock_nrfx_rramc_bytes_write_IgnoreArg_src();
	__cmock_nrfx_rramc_bytes_write_IgnoreArg_num_bytes();

	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(0, err);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_ERASE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL(BLOCK_SIZE, storage_event.len);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_FALSE(is_busy);
}

void test_bm_storage_rram_is_busy(void)
{
	int err;
	bool is_busy = false;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	is_busy = bm_storage_is_busy(NULL);
	TEST_ASSERT_FALSE(is_busy);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_FALSE(is_busy);

	/* Backend already initialized by test_bm_storage_rram_init. */
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
