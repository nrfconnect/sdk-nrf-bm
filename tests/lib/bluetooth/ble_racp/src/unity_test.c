/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <unity.h>
#include <stdint.h>
#include <stddef.h>
#include <bm/bluetooth/ble_racp.h>

void test_ble_racp_encode_efault(void)
{
	int ret;
	const struct ble_racp_value racp_val = {};
	uint8_t data[5];

	ret = ble_racp_encode(NULL, data, sizeof(data));
	TEST_ASSERT_EQUAL(-EFAULT, ret);

	ret = ble_racp_encode(&racp_val, NULL, 0);
	TEST_ASSERT_EQUAL(-EFAULT, ret);
}

void test_ble_racp_encode_einval(void)
{
	int ret;
	struct ble_racp_value racp_val = {};
	uint8_t data[5];

	ret = ble_racp_encode(&racp_val, data, 0);
	TEST_ASSERT_EQUAL(-EINVAL, ret);

	ret = ble_racp_encode(&racp_val, data, 1);
	TEST_ASSERT_EQUAL(-EINVAL, ret);

	racp_val.operand_len = 1;

	ret = ble_racp_encode(&racp_val, data, 2);
	TEST_ASSERT_EQUAL(-EINVAL, ret);
}

void test_ble_racp_encode(void)
{
	int ret;

	uint8_t op[] = {3, 4, 5};

	const struct ble_racp_value racp_val = {
		.opcode = RACP_OPCODE_REPORT_RECS,
		.operator = RACP_OPERATOR_LESS_OR_EQUAL,
		.operand_len = sizeof(op),
		.operand = op,
	};

	uint8_t data[5];

	ret = ble_racp_encode(&racp_val, data, sizeof(data));
	TEST_ASSERT_EQUAL(5, ret);

	uint8_t data_expected[] = {RACP_OPCODE_REPORT_RECS, RACP_OPERATOR_LESS_OR_EQUAL, 3, 4, 5};

	TEST_ASSERT_EQUAL_MEMORY(data_expected, data, sizeof(data));
}

void test_ble_racp_decode_efault(void)
{
	int ret;
	struct ble_racp_value racp_val = {};
	uint8_t data[5];

	ret = ble_racp_decode(NULL, 0, &racp_val);
	TEST_ASSERT_EQUAL(-EFAULT, ret);

	ret = ble_racp_decode(data, sizeof(data), NULL);
	TEST_ASSERT_EQUAL(-EFAULT, ret);
}

void test_ble_racp_decode(void)
{
	int ret;

	uint8_t data[] = {RACP_OPCODE_REPORT_RECS, RACP_OPERATOR_LESS_OR_EQUAL, 3, 4, 5};

	struct ble_racp_value racp_val;

	ret = ble_racp_decode(data, sizeof(data), &racp_val);
	TEST_ASSERT_EQUAL(0, ret);

	TEST_ASSERT_EQUAL(RACP_OPCODE_REPORT_RECS, racp_val.opcode);
	TEST_ASSERT_EQUAL(RACP_OPERATOR_LESS_OR_EQUAL, racp_val.operator);
	TEST_ASSERT_EQUAL_PTR(&data[2], racp_val.operand);
	TEST_ASSERT_EQUAL(3, racp_val.operand_len);

	uint8_t empty[] = {};

	ret = ble_racp_decode(empty, 0, &racp_val);
	TEST_ASSERT_EQUAL(0, ret);

	TEST_ASSERT_EQUAL(0xFF, racp_val.opcode);
	TEST_ASSERT_EQUAL(0xFF, racp_val.operator);
	TEST_ASSERT_EQUAL_PTR(NULL, racp_val.operand);
	TEST_ASSERT_EQUAL(0, racp_val.operand_len);

	uint8_t opcode[] = {RACP_OPCODE_DELETE_RECS};

	ret = ble_racp_decode(opcode, sizeof(opcode), &racp_val);
	TEST_ASSERT_EQUAL(0, ret);

	TEST_ASSERT_EQUAL(RACP_OPCODE_DELETE_RECS, racp_val.opcode);
	TEST_ASSERT_EQUAL(0xFF, racp_val.operator);
	TEST_ASSERT_EQUAL_PTR(NULL, racp_val.operand);
	TEST_ASSERT_EQUAL(0, racp_val.operand_len);

	uint8_t opcode_operator[] = {RACP_OPCODE_DELETE_RECS, RACP_OPERATOR_RANGE};

	ret = ble_racp_decode(opcode_operator, sizeof(opcode_operator), &racp_val);
	TEST_ASSERT_EQUAL(0, ret);

	TEST_ASSERT_EQUAL(RACP_OPCODE_DELETE_RECS, racp_val.opcode);
	TEST_ASSERT_EQUAL(RACP_OPERATOR_RANGE, racp_val.operator);
	TEST_ASSERT_EQUAL_PTR(NULL, racp_val.operand);
	TEST_ASSERT_EQUAL(0, racp_val.operand_len);

	uint8_t opcode_operator_data[] = {RACP_OPCODE_DELETE_RECS, RACP_OPERATOR_RANGE, 0xA};

	ret = ble_racp_decode(opcode_operator_data, sizeof(opcode_operator_data), &racp_val);
	TEST_ASSERT_EQUAL(0, ret);

	TEST_ASSERT_EQUAL(RACP_OPCODE_DELETE_RECS, racp_val.opcode);
	TEST_ASSERT_EQUAL(RACP_OPERATOR_RANGE, racp_val.operator);
	TEST_ASSERT_EQUAL_PTR(&opcode_operator_data[2], racp_val.operand);
	TEST_ASSERT_EQUAL(1, racp_val.operand_len);
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
