/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <errno.h>

#include <bm/storage/bm_storage.h>
#include <bm/storage/bm_storage_backend.h>

/* Arbitrary block size. */
#define BLOCK_SIZE 16

/* Arbitrary partition, must be 32-bit word aligned. */
#define PARTITION_START 0x4200
#define PARTITION_SIZE (BLOCK_SIZE * 2)

int bm_storage_backend_init(struct bm_storage *storage)
{
	return 0;
}

int bm_storage_backend_uninit(struct bm_storage *storage)
{
	return 0;
}

int bm_storage_backend_write(const struct bm_storage *storage, uint32_t dest,
			     const void *src, uint32_t len, void *ctx)
{
	return 0;
}

int bm_storage_backend_erase(const struct bm_storage *storage, uint32_t addr,
			     uint32_t len, void *ctx)
{
	return 0;
}

int bm_storage_backend_read(const struct bm_storage *storage, uint32_t src, void *dest,
			    uint32_t len)
{
	return 0;
}

bool bm_storage_backend_is_busy(const struct bm_storage *storage)
{
	return false;
}

/* Implements the exported extern. */
const struct bm_storage_info bm_storage_info = {
	.erase_unit = BLOCK_SIZE,
	.program_unit = BLOCK_SIZE,
	.no_explicit_erase = false
};

static void bm_storage_evt_handler(struct bm_storage_evt *evt)
{
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

	struct bm_storage storage = { 0 };
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

void test_bm_storage_init(void)
{
	int err;

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_init_eperm(void)
{
	int err;

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Double initialization on the same instance is an error. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_uninit_efault(void)
{
	int err;

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_uninit(NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_uninit_eperm(void)
{
	int err;

	struct bm_storage storage = { 0 };

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_uninit(void)
{
	int err;

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_write_efault(void)
{
	int err;
	char input[BLOCK_SIZE] = "Ciao";
	char input_large[BLOCK_SIZE * 4] = "Ciao";

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_write(NULL, PARTITION_START, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_storage_write(&storage, PARTITION_START, NULL, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Operation is out of bounds. */
	err = bm_storage_write(&storage, PARTITION_START - 1, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	/* Operation is out of bounds. */
	err = bm_storage_write(&storage, PARTITION_START, input_large, sizeof(input_large), NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_write_eperm(void)
{
	int err;
	char input[BLOCK_SIZE] = "Ciao";

	struct bm_storage storage = { 0 };

	/* Storage is uninitialized. */
	err = bm_storage_write(&storage, PARTITION_START, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_write_einval(void)
{
	int err;
	/* Write buffer size must be a multiple of the program unit.
	 * This will cause an error.
	 */
	char input[BLOCK_SIZE - 1] = "Ciao";

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_write(&storage, PARTITION_START, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_write(void)
{
	int err;
	/* Write buffer size must be a multiple of the program unit. */
	char input[BLOCK_SIZE] = "Ciao";

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_write(&storage, PARTITION_START, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_read_efault(void)
{
	int err;
	char output[BLOCK_SIZE] = { 0 };
	char output_large[BLOCK_SIZE * 4] = { 0 };

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_read(NULL, PARTITION_START, output, sizeof(output));
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_storage_read(&storage, PARTITION_START, NULL, sizeof(output));
	TEST_ASSERT_EQUAL(-EFAULT, err);

	/* Operation is out of bounds. */
	err = bm_storage_read(&storage, PARTITION_START - 1, output, sizeof(output));
	TEST_ASSERT_EQUAL(-EFAULT, err);

	/* Operation is out of bounds. */
	err = bm_storage_read(&storage, PARTITION_START, output_large, sizeof(output_large));
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_read_eperm(void)
{
	int err;
	char output[BLOCK_SIZE] = { 0 };

	struct bm_storage storage = { 0 };

	/* Storage is uninitialized. */
	err = bm_storage_read(&storage, PARTITION_START, output, sizeof(output));
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_read_einval(void)
{
	int err;
	char output[BLOCK_SIZE] = { 0 };

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_read(&storage, PARTITION_START, output, 0);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_read(void)
{
	int err;
	char output[BLOCK_SIZE] = { 0 };

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_read(&storage, PARTITION_START, output, sizeof(output));
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_erase_efault(void)
{
	int err;

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_erase(NULL, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Operation is out of bounds. */
	err = bm_storage_erase(&storage, PARTITION_START - 1, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	/* Operation is out of bounds. */
	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE * 4, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_erase_eperm(void)
{
	int err;

	struct bm_storage storage = { 0 };

	/* Storage is uninitialized. */
	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_erase_einval(void)
{
	int err;

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE + 1, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_erase(void)
{
	int err;

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_is_busy(void)
{
	int err;
	bool is_busy = false;

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Storage is NULL. */
	is_busy = bm_storage_is_busy(NULL);
	TEST_ASSERT_EQUAL(true, is_busy);

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_EQUAL(false, is_busy);
}

void setUp(void)
{
}

void tearDown(void)
{
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
