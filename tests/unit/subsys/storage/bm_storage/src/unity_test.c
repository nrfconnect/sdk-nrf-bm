/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <errno.h>
#include <string.h>

#include <bm/storage/bm_storage.h>

#if defined(CONFIG_BM_SCHEDULER)
#include <bm/bm_scheduler.h>
#endif

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

static struct bm_storage_evt dispatch_evt;
static bool dispatch_evt_received;

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

static void dispatch_evt_handler(struct bm_storage_evt *evt)
{
	dispatch_evt_received = true;
	dispatch_evt = *evt;
}

void test_bm_storage_init_efault(void)
{
	int err;

	struct bm_storage storage = { 0 };
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Simulate the backend refusing to uninitialize due to outstanding operations. */
	backend_uninit_retval = -EBUSY;

	err = bm_storage_uninit(&storage);
	TEST_ASSERT_EQUAL(-EBUSY, err);

	/* The instance must remain initialized when the backend refuses. */
	TEST_ASSERT_TRUE(storage.flags.is_initialized);
}

void test_bm_storage_write_efault(void)
{
	int err;
	char input[BLOCK_SIZE] = "Ciao";
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_write(NULL, 0, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_storage_write(&storage, 0, NULL, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_write_eperm(void)
{
	int err;
	char input[BLOCK_SIZE] = {0};
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_write(&storage, 0, input, sizeof(input), NULL);
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Unaligned */

	err = bm_storage_write(&storage, 1, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	err = bm_storage_write(&storage, 0, input, sizeof(input) - 1, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	/* Out of bounds */

	err = bm_storage_write(&storage, PARTITION_SIZE + BLOCK_SIZE, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	err = bm_storage_write(&storage, 0, input, PARTITION_SIZE + BLOCK_SIZE, NULL);
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_write(&storage, 0, input, sizeof(input), NULL);
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_read(NULL, 0, output, sizeof(output));
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_storage_read(&storage, 0, NULL, sizeof(output));
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_read_eperm(void)
{
	int err;
	char output[BLOCK_SIZE] = { 0 };
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_read(&storage, 0, output, sizeof(output));
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_read(&storage, 0, output, 0);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	/* Operation is out of bounds. */
	err = bm_storage_read(&storage, PARTITION_SIZE, output, sizeof(output));
	TEST_ASSERT_EQUAL(-EINVAL, err);

	/* Operation is out of bounds. */
	err = bm_storage_read(&storage, 0, output, PARTITION_SIZE + 1);
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_read(&storage, 0, output, sizeof(output));
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_erase_efault(void)
{
	int err;

	err = bm_storage_erase(NULL, 0, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_storage_erase_eperm(void)
{
	int err;
	struct bm_storage storage = {0};

	/* Storage is uninitialized. */
	err = bm_storage_erase(&storage, 0, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EPERM, err);
}

void test_bm_storage_erase_einval(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Unaligned */

	err = bm_storage_erase(&storage, 1, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	err = bm_storage_erase(&storage, 0, BLOCK_SIZE - 1, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	/* Out of bounds */
	err = bm_storage_erase(&storage, PARTITION_SIZE, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);

	err = bm_storage_erase(&storage, 0, PARTITION_SIZE + BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_erase(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	err = bm_storage_erase(&storage, 0, BLOCK_SIZE, NULL);
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
		.flags.is_wear_aligned = true,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Aligned to wear_unit: succeeds */
	err = bm_storage_write(&storage, 0, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(0, err);

	/* Aligned to program_unit but not wear_unit: fails */
	err = bm_storage_write(&storage, PROGRAM_UNIT, input, PROGRAM_UNIT, NULL);
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
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Without the flag, program_unit alignment is sufficient */
	err = bm_storage_write(&storage, PROGRAM_UNIT, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_write_padded_accepts_unaligned_len(void)
{
	int err;
	char input[BLOCK_SIZE + 3] = {0};
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api_rram_like,
		.evt_handler = bm_storage_evt_handler,
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
		.flags.is_write_padded = true,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* With is_write_padded, length not a multiple of program_unit is accepted */
	err = bm_storage_write(&storage, 0, input, sizeof(input), NULL);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_erase_wear_aligned(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api_rram_like,
		.evt_handler = bm_storage_evt_handler,
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
		.flags.is_wear_aligned = true,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Aligned to wear_unit: succeeds */
	err = bm_storage_erase(&storage, 0, BLOCK_SIZE, NULL);
	TEST_ASSERT_EQUAL(0, err);

	/* Aligned to program_unit but not wear_unit: fails */
	err = bm_storage_erase(&storage, 0, PROGRAM_UNIT, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_erase_no_wear_aligned(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api_rram_like,
		.evt_handler = bm_storage_evt_handler,
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Without is_wear_aligned: erase_unit alignment is sufficient */
	err = bm_storage_erase(&storage, 0, PROGRAM_UNIT, NULL);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_storage_init_wear_aligned_einval(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = bm_storage_evt_handler,
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
		.flags.is_wear_aligned = true,
	};

	/* is_erase_before_write backend does not support is_wear_aligned */
	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_bm_storage_evt_dispatch_null_handler(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
	};
	struct bm_storage_evt evt = {
		.id = BM_STORAGE_EVT_WRITE_RESULT,
		.is_async = false,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	/* Must not crash when the event handler is NULL. */
	bm_storage_evt_dispatch(&storage, &evt);
}

void test_bm_storage_evt_dispatch_async(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = dispatch_evt_handler,
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
	};
	struct bm_storage_evt evt = {
		.id = BM_STORAGE_EVT_WRITE_RESULT,
		.is_async = true,
		.result = 0,
		.addr = 0x100,
		.len = 32,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	bm_storage_evt_dispatch(&storage, &evt);

	/* Async events are always dispatched directly, regardless of scheduler. */
	TEST_ASSERT_TRUE(dispatch_evt_received);
	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_WRITE_RESULT, dispatch_evt.id);
	TEST_ASSERT_EQUAL(true, dispatch_evt.is_async);
	TEST_ASSERT_EQUAL(0, dispatch_evt.result);
	TEST_ASSERT_EQUAL(0x100, dispatch_evt.addr);
	TEST_ASSERT_EQUAL(32, dispatch_evt.len);
}

void test_bm_storage_evt_dispatch_sync(void)
{
	int err;
	struct bm_storage storage = {0};
	struct bm_storage_config config = {
		.api = &bm_storage_test_api,
		.evt_handler = dispatch_evt_handler,
		.addr = PARTITION_START,
		.size = PARTITION_SIZE,
	};
	struct bm_storage_evt evt = {
		.id = BM_STORAGE_EVT_ERASE_RESULT,
		.is_async = false,
		.result = 0,
		.addr = 0x200,
		.len = 64,
	};

	err = bm_storage_init(&storage, &config);
	TEST_ASSERT_EQUAL(0, err);

	bm_storage_evt_dispatch(&storage, &evt);

#if defined(CONFIG_BM_SCHEDULER)
	/* With scheduler: sync events are deferred, not delivered yet. */
	TEST_ASSERT_FALSE(dispatch_evt_received);

	bm_scheduler_process();

	TEST_ASSERT_TRUE(dispatch_evt_received);
	/* The event is marked async since it was deferred. */
	TEST_ASSERT_EQUAL(true, dispatch_evt.is_async);
#else
	/* Without scheduler: sync events are dispatched directly. */
	TEST_ASSERT_TRUE(dispatch_evt_received);
	TEST_ASSERT_EQUAL(false, dispatch_evt.is_async);
#endif

	TEST_ASSERT_EQUAL(BM_STORAGE_EVT_ERASE_RESULT, dispatch_evt.id);
	TEST_ASSERT_EQUAL(0, dispatch_evt.result);
	TEST_ASSERT_EQUAL(0x200, dispatch_evt.addr);
	TEST_ASSERT_EQUAL(64, dispatch_evt.len);
}

void setUp(void)
{
	backend_uninit_retval = 0;
	dispatch_evt_received = false;
	memset(&dispatch_evt, 0, sizeof(dispatch_evt));
}

void tearDown(void)
{
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
