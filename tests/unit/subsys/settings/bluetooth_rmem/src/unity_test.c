/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>

#include <bm/settings/bluetooth_name.h>
#include <bm/storage/bm_rmem.h>
#include "bm/storage/cmock_bm_rmem.h"

/* Stub Zephyr logging macros */
#define LOG_MODULE_REGISTER(...)
#define LOG_ERR(...)
#define LOG_INF(...)
#define LOG_WRN(...)
#define LOG_DBG(...)

/* Stub devicetree macro */
#define DT_DRV_COMPAT zephyr_retained_ram

/* Test data */
#define TEST_BLE_NAME "TestDevice"
#define TEST_BLE_NAME_KEY "fw_loader/adv_name"
#define TEST_BLE_NAME_LEN (sizeof(TEST_BLE_NAME) - 1)

/* External function from bluetooth_rmem.c */
extern size_t ble_name_value_get(struct bm_retained_clipboard_ctx *ctx, char const **name);
extern int settings_runtime_set(const char *name, const void *data, size_t len);

void setUp(void)
{
	/* Reset mocks before each test */
}

void tearDown(void)
{
	/* Cleanup after each test */
}

/* Test cases for ble_name_value_get() */

void test_ble_name_value_get_success(void)
{
	size_t len;
	const char *name = NULL;
	struct bm_retained_clipboard_ctx ctx = {0};
	struct bm_rmem_data_desc expected_desc = {
		.type = BM_REM_TLV_TYPE_BLE_NAME,
		.len = TEST_BLE_NAME_LEN,
		.data = (void *)TEST_BLE_NAME,
	};

	/* Expect bm_rmem_data_get to be called and succeed */
	__cmock_bm_rmem_data_get_ExpectAndReturn(&ctx, NULL, 0);
	__cmock_bm_rmem_data_get_IgnoreArg_desc();
	__cmock_bm_rmem_data_get_ReturnThruPtr_desc(&expected_desc);

	len = ble_name_value_get(&ctx, &name);

	TEST_ASSERT_EQUAL(TEST_BLE_NAME_LEN, len);
	TEST_ASSERT_NOT_NULL(name);
	TEST_ASSERT_EQUAL_STRING(TEST_BLE_NAME, name);
}

void test_ble_name_value_get_failure(void)
{
	size_t len;
	const char *name = NULL;
	struct bm_retained_clipboard_ctx ctx = {0};

	/* Expect bm_rmem_data_get to fail */
	__cmock_bm_rmem_data_get_ExpectAndReturn(&ctx, NULL, -ENOENT);
	__cmock_bm_rmem_data_get_IgnoreArg_desc();

	len = ble_name_value_get(&ctx, &name);

	TEST_ASSERT_EQUAL(0, len);
	TEST_ASSERT_NULL(name);
}

void test_ble_name_value_get_null_ctx(void)
{
	size_t len;
	const char *name = NULL;
	struct bm_retained_clipboard_ctx ctx = {0};

	/* Function returns early if ctx is NULL, so bm_rmem_data_get is not called */
	len = ble_name_value_get(NULL, &name);

	TEST_ASSERT_EQUAL(0, len);
	TEST_ASSERT_NULL(name);
}

void test_ble_name_value_get_null_name(void)
{
	size_t len;
	struct bm_retained_clipboard_ctx ctx = {0};

	/* Function returns early if name is NULL, so bm_rmem_data_get is not called */
	len = ble_name_value_get(&ctx, NULL);

	TEST_ASSERT_EQUAL(0, len);
}

/* Test cases for settings_runtime_set() */

void test_settings_runtime_set_success(void)
{
	int err;
	const char test_name[] = "MyDevice";

	err = settings_runtime_set(TEST_BLE_NAME_KEY, test_name, sizeof(test_name) - 1);
	TEST_ASSERT_EQUAL(0, err);
}

void test_settings_runtime_set_null_name(void)
{
	int err;
	const char test_name[] = "MyDevice";

	err = settings_runtime_set(NULL, test_name, sizeof(test_name) - 1);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_settings_runtime_set_null_value(void)
{
	int err;

	err = settings_runtime_set(TEST_BLE_NAME_KEY, NULL, 10);
	TEST_ASSERT_EQUAL(-EFAULT, err);
}

void test_settings_runtime_set_wrong_key(void)
{
	int err;
	const char test_name[] = "MyDevice";
	const char wrong_key[] = "wrong/key";

	err = settings_runtime_set(wrong_key, test_name, sizeof(test_name) - 1);
	TEST_ASSERT_EQUAL(-ENOENT, err);
}

void test_settings_runtime_set_value_too_long(void)
{
	int err;
	char long_name[21] = "VeryLongDeviceName123";

	err = settings_runtime_set(TEST_BLE_NAME_KEY, long_name, sizeof(long_name));
	TEST_ASSERT_EQUAL(-EINVAL, err);
}

void test_settings_runtime_set_valid_length(void)
{
	int err;
	char name[15] = "ValidName123";

	err = settings_runtime_set(TEST_BLE_NAME_KEY, name, sizeof(name) - 1);
	TEST_ASSERT_EQUAL(0, err);
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
