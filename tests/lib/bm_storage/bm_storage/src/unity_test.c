/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <nrf_error.h>

#include <bm/bm_storage.h>

/* Arbitrary block size. */
#define BLOCK_SIZE 16

/* Arbitrary partition, must be 32-bit word aligned. */
#define PARTITION_START 0x4200
#define PARTITION_SIZE (BLOCK_SIZE * 2)

uint32_t bm_storage_backend_init(struct bm_storage *storage)
{
	return NRF_SUCCESS;
}

uint32_t bm_storage_backend_uninit(struct bm_storage *storage)
{
	return NRF_SUCCESS;
}

uint32_t bm_storage_backend_write(const struct bm_storage *storage, uint32_t dest,
				       const void *src, uint32_t len, void *ctx)
{
	return NRF_SUCCESS;
}

uint32_t bm_storage_backend_erase(const struct bm_storage *storage, uint32_t addr,
				       uint32_t len, void *ctx)
{
	return NRF_SUCCESS;
}

uint32_t bm_storage_backend_read(const struct bm_storage *storage, uint32_t src, void *dest,
				      uint32_t len)
{
	return NRF_SUCCESS;
}

bool bm_storage_backend_is_busy(const struct bm_storage *storage)
{
	return false;
}

/* Implements the exported extern. */
const struct bm_storage_info bm_storage_info = {
	.erase_unit = BLOCK_SIZE,
	.program_unit = BLOCK_SIZE,
	.no_explicit_erase = true
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

void test_bm_storage_init_error_null(void)
{
	uint32_t err;

	err = bm_storage_init(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_bm_storage_init(void)
{
	uint32_t err;

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_bm_storage_uninit_error_null(void)
{
	uint32_t err;

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_storage_uninit(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_bm_storage_uninit_error_invalid_state(void)
{
	uint32_t err;

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, err);
}

void test_bm_storage_uninit(void)
{
	uint32_t err;

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_bm_storage_write_error_null(void)
{
	uint32_t err;
	char input[BLOCK_SIZE] = "Ciao";

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_write(NULL, PARTITION_START, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	err = bm_storage_write(&storage, PARTITION_START, NULL, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_bm_storage_write_error_invalid_state(void)
{
	uint32_t err;
	char input[BLOCK_SIZE] = "Ciao";

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Storage is uninitialized. */
	err = bm_storage_write(&storage, PARTITION_START, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, err);
}

void test_bm_storage_write_error_invalid_length(void)
{
	uint32_t err;
	/* Write buffer size must be a multiple of the program unit.
	 * This will cause an error.
	 */
	char input[BLOCK_SIZE - 1] = "Ciao";

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_storage_write(&storage, PARTITION_START, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_LENGTH, err);
}

void test_bm_storage_write_error_invalid_addr(void)
{
	uint32_t err;
	char input[BLOCK_SIZE] = "Ciao";
	char input_large[BLOCK_SIZE * 4] = "Ciao";

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	/* Operation is out of bounds. */
	err = bm_storage_write(&storage, PARTITION_START - 1, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_ADDR, err);

	/* Operation is out of bounds. */
	err = bm_storage_write(&storage, PARTITION_START, input_large, sizeof(input_large), NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_ADDR, err);
}

void test_bm_storage_write(void)
{
	uint32_t err;
	/* Write buffer size must be a multiple of the program unit. */
	char input[BLOCK_SIZE] = "Ciao";

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_storage_write(&storage, PARTITION_START, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_bm_storage_read_error_null(void)
{
	uint32_t err;
	char output[BLOCK_SIZE] = { 0 };

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_storage_read(NULL, PARTITION_START, output, sizeof(output));
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	err = bm_storage_read(&storage, PARTITION_START, NULL, sizeof(output));
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_bm_storage_read_error_invalid_state(void)
{
	uint32_t err;
	char output[BLOCK_SIZE] = { 0 };

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Storage is uninitialized. */
	err = bm_storage_read(&storage, PARTITION_START, output, sizeof(output));
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, err);
}

void test_bm_storage_read_error_invalid_length(void)
{
	uint32_t err;
	char output[BLOCK_SIZE] = { 0 };

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_storage_read(&storage, PARTITION_START, output, 0);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_LENGTH, err);
}

void test_bm_storage_read_error_invalid_addr(void)
{
	uint32_t err;
	char output[BLOCK_SIZE] = { 0 };
	char output_large[BLOCK_SIZE * 4] = { 0 };

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	/* Operation is out of bounds. */
	err = bm_storage_read(&storage, PARTITION_START - 1, output, sizeof(output));
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_ADDR, err);

	/* Operation is out of bounds. */
	err = bm_storage_read(&storage, PARTITION_START, output_large, sizeof(output_large));
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_ADDR, err);
}

void test_bm_storage_read(void)
{
	uint32_t err;
	char output[BLOCK_SIZE] = { 0 };

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_storage_read(&storage, PARTITION_START, output, sizeof(output));
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_bm_storage_erase_error_null(void)
{
	uint32_t err;

	err = bm_storage_erase(NULL, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_bm_storage_erase_error_invalid_state(void)
{
	uint32_t err;

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Storage is uninitialized. */
	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, err);
}

void test_bm_storage_erase_error_invalid_length(void)
{
	uint32_t err;

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE + 1, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_LENGTH, err);
}

void test_bm_storage_erase_error_invalid_addr(void)
{
	uint32_t err;

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	/* Operation is out of bounds. */
	err = bm_storage_erase(&storage, PARTITION_START - 1, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_ADDR, err);

	/* Operation is out of bounds. */
	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE * 4, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_ADDR, err);
}

void test_bm_storage_erase(void)
{
	uint32_t err;

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_bm_storage_is_busy(void)
{
	uint32_t err;
	bool is_busy = false;

	struct bm_storage storage = {
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	/* Storage is NULL. */
	is_busy = bm_storage_is_busy(NULL);
	TEST_ASSERT_EQUAL(true, is_busy);

	err = bm_storage_init(&storage);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

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
