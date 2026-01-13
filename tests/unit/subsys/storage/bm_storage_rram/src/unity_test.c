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

extern const struct bm_storage_api bm_storage_rram_api;

/* RRAM backend uses 16-byte program unit. */
#define BLOCK_SIZE 16

/* Arbitrary partition, must be 32-bit word aligned. */
#define PARTITION_START 0x4200
#define PARTITION_SIZE  (BLOCK_SIZE * 3)

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

void test_bm_storage_init_efault(void)
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

void test_bm_storage_init_eperm(void)
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

	/* Second init on same storage is -EPERM (core layer). */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(-EPERM, err);

	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_init(void)
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

	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_uninit_efault(void)
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

	err = bm_storage_uninit(NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_uninit_eperm(void)
{
	int err;
	struct bm_storage storage = {0};

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_uninit(void)
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

	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_init_uninit_init(void)
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

	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);

	/* Re-init: hardware init runs again (refcount 0 -> 1). */
	__cmock_nrfx_rramc_init_ExpectAnyArgsAndReturn(0);
	__cmock_nrfx_rramc_init_IgnoreArg_p_config();
	__cmock_nrfx_rramc_init_IgnoreArg_handler();

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

/* Refcount: two users init, first uninit does not call nrfx_rramc_uninit. */
void test_bm_storage_refcount_two_users(void)
{
	int err;
	struct bm_storage storage1 = {0};
	struct bm_storage storage2 = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	__cmock_nrfx_rramc_init_ExpectAnyArgsAndReturn(0);
	__cmock_nrfx_rramc_init_IgnoreArg_p_config();
	__cmock_nrfx_rramc_init_IgnoreArg_handler();

	err = bm_storage_init(&storage1, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Second user: no nrfx_rramc_init call (refcount 1 -> 2). */
	err = bm_storage_init(&storage2, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* First user uninit: refcount 2 -> 1, no uninit. */
	err = bm_storage_uninit(&storage1);
	TEST_ASSERT_EQUAL(0, err);

	/* Second user uninit: refcount 1 -> 0, uninit. */
	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage2);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_write_eperm(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_write_einval(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE - 1];
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

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_write_efault(void)
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

	err = bm_storage_write(&storage, PARTITION_START, NULL, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_write(void)
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

	__cmock_nrfx_rramc_init_ExpectAnyArgsAndReturn(0);
	__cmock_nrfx_rramc_init_IgnoreArg_p_config();
	__cmock_nrfx_rramc_init_IgnoreArg_handler();

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_nrfx_rramc_bytes_write_ExpectAnyArgs();
	__cmock_nrfx_rramc_bytes_write_IgnoreArg_address();
	__cmock_nrfx_rramc_bytes_write_IgnoreArg_src();
	__cmock_nrfx_rramc_bytes_write_IgnoreArg_num_bytes();

	err = bm_storage_write(&storage, PARTITION_START, buf, sizeof(buf), NULL);
	TEST_ASSERT_EQUAL(0, err);

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, storage_event.id);
	TEST_ASSERT_EQUAL(0, storage_event.result);
	TEST_ASSERT_EQUAL(PARTITION_START, storage_event.addr);
	TEST_ASSERT_EQUAL_PTR(buf, storage_event.src);
	TEST_ASSERT_EQUAL(sizeof(buf), storage_event.len);

	/* Not busy after sync write completed. */
	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_FALSE(is_busy);

	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_read_eperm(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};

	err = bm_storage_read(&storage, PARTITION_START, buf, sizeof(buf));
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_read_einval(void)
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

	__cmock_nrfx_rramc_init_ExpectAnyArgsAndReturn(0);
	__cmock_nrfx_rramc_init_IgnoreArg_p_config();
	__cmock_nrfx_rramc_init_IgnoreArg_handler();

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_read(&storage, PARTITION_START, buf, 0);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_read(void)
{
	int err;
	uint8_t buf[BLOCK_SIZE] = {0};
	uint32_t dummy_partition[16] = {0x00C0FFEE};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.api = &bm_storage_rram_api,
		.start_addr = (uintptr_t)&dummy_partition,
		.end_addr = (uintptr_t)&dummy_partition + sizeof(dummy_partition),
	};

	__cmock_nrfx_rramc_init_ExpectAnyArgsAndReturn(0);
	__cmock_nrfx_rramc_init_IgnoreArg_p_config();
	__cmock_nrfx_rramc_init_IgnoreArg_handler();

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	__cmock_nrfx_rramc_buffer_read_Expect(buf, config.start_addr, sizeof(buf));

	err = bm_storage_read(&storage, config.start_addr, buf, sizeof(buf));
	TEST_ASSERT_EQUAL(0, err);

	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_read_efault(void)
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

	__cmock_nrfx_rramc_init_ExpectAnyArgsAndReturn(0);
	__cmock_nrfx_rramc_init_IgnoreArg_p_config();
	__cmock_nrfx_rramc_init_IgnoreArg_handler();

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_read(&storage, PARTITION_START - 1, buf, sizeof(buf));
	TEST_ASSERT_EQUAL(-EFAULT, err);

	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_erase_eperm(void)
{
	int err;
	struct bm_storage storage = {0};

	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_erase(void)
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

	__cmock_nrfx_rramc_init_ExpectAnyArgsAndReturn(0);
	__cmock_nrfx_rramc_init_IgnoreArg_p_config();
	__cmock_nrfx_rramc_init_IgnoreArg_handler();

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

	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_is_busy(void)
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

	is_busy = bm_storage_is_busy(NULL);
	TEST_ASSERT_FALSE(is_busy);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_FALSE(is_busy);

	__cmock_nrfx_rramc_init_ExpectAnyArgsAndReturn(0);
	__cmock_nrfx_rramc_init_IgnoreArg_p_config();
	__cmock_nrfx_rramc_init_IgnoreArg_handler();

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_FALSE(is_busy);

	__cmock_nrfx_rramc_uninit_Expect();

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
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
