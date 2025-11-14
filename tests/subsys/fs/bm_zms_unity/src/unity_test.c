/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <errno.h>

#include <bm/fs/bm_zms.h>

#include "cmock_bm_storage.h"
#include "cmock_crc.h"

static struct bm_storage_info info = {
	.erase_unit = 4,
	.erase_value = 0,
	.program_unit = 4,
	.no_explicit_erase = true
};

void bm_zms_callback(bm_zms_evt_t const *p_evt)
{
}

void test_bm_zms_register_efault(void)
{
	int err;

	struct bm_zms_fs fs = { 0 };

	err = bm_zms_register(NULL, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_zms_register(&fs, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_zms_register(NULL, bm_zms_callback);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_zms_register(void)
{
	int err;

	struct bm_zms_fs fs = { 0 };

	err = bm_zms_register(&fs, bm_zms_callback);
	TEST_ASSERT_EQUAL(0, err);
}

void test_bm_zms_mount_efault(void)
{
	int err;

	struct bm_zms_fs fs = { 0 };
	struct bm_zms_fs_config config = {
		.offset = 0,
		.sector_size = 1024,
		.sector_count = 4,
	};

	err = bm_zms_mount(NULL, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_zms_mount(&fs, NULL);
	TEST_ASSERT_EQUAL(-EFAULT, err);

	err = bm_zms_mount(NULL, &config);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_bm_zms_mount(void)
{
	int err;

	struct bm_zms_fs fs = { 0 };
	struct bm_zms_fs_config config = {
		.offset = 0,
		.sector_size = 1024,
		.sector_count = 4,
	};

	fs.zms_bm_storage.nvm_info = &info;

	__cmock_bm_storage_init_ExpectAndReturn(&fs.zms_bm_storage, 0);

	for (int i = 0; i < 13; i++) {
		__cmock_bm_storage_read_ExpectAndReturn(&fs.zms_bm_storage, 0, NULL, 0, 0);
		__cmock_bm_storage_read_IgnoreArg_src();
		__cmock_bm_storage_read_IgnoreArg_dest();
		__cmock_bm_storage_read_IgnoreArg_len();
	}

	__cmock_crc8_ccitt_IgnoreAndReturn(0);

	__cmock_bm_storage_write_ExpectAndReturn(&fs.zms_bm_storage, 0, NULL, 0, NULL, 0);
	__cmock_bm_storage_write_IgnoreArg_dest();
	__cmock_bm_storage_write_IgnoreArg_src();
	__cmock_bm_storage_write_IgnoreArg_len();
	__cmock_bm_storage_write_IgnoreArg_ctx();

	err = bm_zms_mount(&fs, &config);
	TEST_ASSERT_EQUAL(0, err);
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
