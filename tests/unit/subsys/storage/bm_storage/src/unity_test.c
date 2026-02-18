/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <errno.h>

#include <bm/storage/bm_storage.h>

/* Arbitrary block size. */
#define BLOCK_SIZE 16
#define PROGRAM_UNIT 4

/* Arbitrary partition, must be 32-bit word aligned. */
#define PARTITION_START 0x4200
#define PARTITION_SIZE (BLOCK_SIZE * 2)

static const struct bm_storage_info bm_storage_info = {
	.program_unit = BLOCK_SIZE,
	.erase_unit = BLOCK_SIZE,
	.wear_unit = BLOCK_SIZE,
	.is_erase_before_write = true
};

static const struct bm_storage_info bm_storage_info_rram_like = {
	.program_unit = PROGRAM_UNIT,
	.erase_unit = PROGRAM_UNIT,
	.wear_unit = BLOCK_SIZE,
	.is_erase_before_write = false,
};

static int bm_storage_test_api_init(struct bm_storage *storage,
				    const struct bm_storage_config *config)
{
	storage->nvm_info = &bm_storage_info;
	return 0;
}

static int bm_storage_test_api_init_rram_like(struct bm_storage *storage,
					      const struct bm_storage_config *config)
{
	storage->nvm_info = &bm_storage_info_rram_like;
	return 0;
}

static int backend_uninit_retval;

static int bm_storage_test_api_uninit(struct bm_storage *storage)
{
	return backend_uninit_retval;
}

static int bm_storage_test_api_write(const struct bm_storage *storage, uint32_t dest,
				     const void *src, uint32_t len, void *ctx)
{
	return 0;
}

static int bm_storage_test_api_erase(const struct bm_storage *storage, uint32_t addr, uint32_t len,
				     void *ctx)
{
	return 0;
}

static int bm_storage_test_api_read(const struct bm_storage *storage, uint32_t src, void *dest,
				    uint32_t len)
{
	return 0;
}

static bool bm_storage_test_api_is_busy(const struct bm_storage *storage)
{
	return false;
}

static struct bm_storage_api bm_storage_test_api = {
	.init = bm_storage_test_api_init,
	.uninit = bm_storage_test_api_uninit,
	.read = bm_storage_test_api_read,
	.write = bm_storage_test_api_write,
	.erase = bm_storage_test_api_erase,
	.is_busy = bm_storage_test_api_is_busy,
};

static struct bm_storage_api bm_storage_test_api_rram_like = {
	.init = bm_storage_test_api_init_rram_like,
	.uninit = bm_storage_test_api_uninit,
	.read = bm_storage_test_api_read,
	.write = bm_storage_test_api_write,
	.erase = bm_storage_test_api_erase,
	.is_busy = bm_storage_test_api_is_busy,
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
		.api = &bm_storage_test_api,
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

	config.api = NULL;

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_init(void)
{
	int err;

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
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
		.api = &bm_storage_test_api,
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
		.api = &bm_storage_test_api,
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
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_init_uninit_init(void)
{
	int err;

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(0, err);

	/* Re-initialization after uninit must succeed. */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_uninit_outstanding(void)
{
	int err;

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Simulate the backend refusing to uninitialize due to outstanding operations. */
	backend_uninit_retval = -EBUSY;

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(-EBUSY, err);

	/* The instance must remain initialized when the backend refuses. */
	TEST_ASSERT_TRUE(storage.initialized);
}

void test_bm_storage_write_efault(void)
{
	int err;
	char input[BLOCK_SIZE] = "Ciao";
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_write(NULL, PARTITION_START, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_storage_write(&storage, PARTITION_START, NULL, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_write_eperm(void)
{
	int err;
	char input[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_write(&storage, PARTITION_START, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(-EPERM, err);
}



void test_bm_storage_write_einval(void)
{
	int err;
	char input[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Unaligned */

	err = bm_storage_write(&storage, PARTITION_START + 1, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	err = bm_storage_write(&storage, PARTITION_START, input, sizeof(input) - 1, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	/* Out of bounds */

	err = bm_storage_write(&storage, PARTITION_SIZE + BLOCK_SIZE, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	err = bm_storage_write(&storage, PARTITION_START, input, PARTITION_SIZE + BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_write(void)
{
	int err;
	/* Write buffer size must be a multiple of the program unit. */
	char input[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
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
	char output[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
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
}

void test_bm_storage_read_eperm(void)
{
	int err;
	char output[BLOCK_SIZE] = { 0 };
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_read(&storage, PARTITION_START, output, sizeof(output));
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_read_einval(void)
{
	int err;
	char output[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_read(&storage, PARTITION_START, output, 0);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	/* Operation is out of bounds. */
	err = bm_storage_read(&storage, PARTITION_START - 1, output, sizeof(output));
	TEST_ASSERT_EQUAL(-EINVAL, err);

	/* Operation is out of bounds. */
	err = bm_storage_read(&storage, PARTITION_START, output, PARTITION_SIZE + 1);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_read(void)
{
	int err;
	char output[BLOCK_SIZE] = { 0 };
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
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

	err = bm_storage_erase(NULL, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_erase_eperm(void)
{
	int err;
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_erase_einval(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Unaligned */

	err = bm_storage_erase(&storage, PARTITION_START + 1, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE - 1, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	/* Out of bounds */
	err = bm_storage_erase(&storage, PARTITION_START - BLOCK_SIZE, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	err = bm_storage_erase(&storage, PARTITION_START, PARTITION_SIZE + BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_erase(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
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
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	is_busy = bm_storage_is_busy(NULL);
	TEST_ASSERT_FALSE(is_busy);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_FALSE(is_busy);

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	is_busy = bm_storage_is_busy(&storage);
	TEST_ASSERT_FALSE(is_busy);
}

void test_bm_storage_nvm_info_get_null(void)
{
	struct bm_storage storage = {0};
	const struct bm_storage_info *info;

	info = bm_storage_nvm_info_get(NULL);
	TEST_ASSERT_NULL(info);

	info = bm_storage_nvm_info_get(&storage);
	TEST_ASSERT_NULL(info);
}

void test_bm_storage_nvm_info_get(void)
{
	int err;
	struct bm_storage storage = {0};
	const struct bm_storage_info *info;
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	info = bm_storage_nvm_info_get(&storage);
	TEST_ASSERT_NOT_NULL(info);

	TEST_ASSERT_EQUAL_MEMORY(&bm_storage_info, info, sizeof(struct bm_storage_info));
}

void test_bm_storage_write_wear_aligned(void)
{
	int err;
	char input[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api_rram_like,
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
		.flags.is_wear_aligned = true,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Aligned to wear_unit: succeeds */
	err = bm_storage_write(&storage, PARTITION_START, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(0, err);

	/* Aligned to program_unit but not wear_unit: fails */
	err = bm_storage_write(&storage, PARTITION_START + PROGRAM_UNIT, input, PROGRAM_UNIT, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_write_no_wear_aligned(void)
{
	int err;
	char input[PROGRAM_UNIT] = {0};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api_rram_like,
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Without the flag, program_unit alignment is sufficient */
	err = bm_storage_write(&storage, PARTITION_START + PROGRAM_UNIT, input, sizeof(input),
			       NULL);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_erase_wear_aligned(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api_rram_like,
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
		.flags.is_wear_aligned = true,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Aligned to wear_unit: succeeds */
	err = bm_storage_erase(&storage, PARTITION_START, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(0, err);

	/* Aligned to program_unit but not wear_unit: fails */
	err = bm_storage_erase(&storage, PARTITION_START, PROGRAM_UNIT, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_erase_no_wear_aligned(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api_rram_like,
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Without is_wear_aligned: erase_unit alignment is sufficient */
	err = bm_storage_erase(&storage, PARTITION_START, PROGRAM_UNIT, NULL);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_init_wear_aligned_einval(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.start_addr = PARTITION_START,
		.end_addr = PARTITION_START + PARTITION_SIZE,
		.flags.is_wear_aligned = true,
	};

	/* is_erase_before_write backend does not support is_wear_aligned */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void setUp(void)
{
	backend_uninit_retval = 0;
}

void tearDown(void)
{
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
