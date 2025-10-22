/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <unity.h>
#include <stdint.h>
#include <stddef.h>
#include <bm/bluetooth/ble_racp.h>

void test_ble_racp_encode_error_invalid(void)
{
	int ret;
	struct ble_racp_value racp_val = {0};
	uint8_t data[5];

	ret = ble_racp_encode(NULL, data, sizeof(data));
	TEST_ASSERT_EQUAL(0, ret);

	ret = ble_racp_encode(&racp_val, NULL, 0);
	TEST_ASSERT_EQUAL(0, ret);

	ret = ble_racp_encode(&racp_val, data, 0);
	TEST_ASSERT_EQUAL(0, ret);

	ret = ble_racp_encode(&racp_val, data, 1);
	TEST_ASSERT_EQUAL(0, ret);

	racp_val.operand_len = 1;

	ret = ble_racp_encode(&racp_val, data, 2);
	TEST_ASSERT_EQUAL(0, ret);
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

void test_ble_racp_decode_error_null(void)
{
	uint32_t nrf_err;
	struct ble_racp_value racp_val = {0};
	uint8_t data[5];

	nrf_err = ble_racp_decode(NULL, 0, &racp_val);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_racp_decode(data, sizeof(data), NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_racp_decode(void)
{
	uint32_t nrf_err;

	uint8_t data[] = {RACP_OPCODE_REPORT_RECS, RACP_OPERATOR_LESS_OR_EQUAL, 3, 4, 5};

	struct ble_racp_value racp_val;

	nrf_err = ble_racp_decode(data, sizeof(data), &racp_val);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	TEST_ASSERT_EQUAL(RACP_OPCODE_REPORT_RECS, racp_val.opcode);
	TEST_ASSERT_EQUAL(RACP_OPERATOR_LESS_OR_EQUAL, racp_val.operator);
	TEST_ASSERT_EQUAL_PTR(&data[2], racp_val.operand);
	TEST_ASSERT_EQUAL(3, racp_val.operand_len);

	uint8_t empty[] = {0};

	nrf_err = ble_racp_decode(empty, 0, &racp_val);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	TEST_ASSERT_EQUAL(0xFF, racp_val.opcode);
	TEST_ASSERT_EQUAL(0xFF, racp_val.operator);
	TEST_ASSERT_EQUAL_PTR(NULL, racp_val.operand);
	TEST_ASSERT_EQUAL(0, racp_val.operand_len);

	uint8_t opcode[] = {RACP_OPCODE_DELETE_RECS};

	nrf_err = ble_racp_decode(opcode, sizeof(opcode), &racp_val);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	TEST_ASSERT_EQUAL(RACP_OPCODE_DELETE_RECS, racp_val.opcode);
	TEST_ASSERT_EQUAL(0xFF, racp_val.operator);
	TEST_ASSERT_EQUAL_PTR(NULL, racp_val.operand);
	TEST_ASSERT_EQUAL(0, racp_val.operand_len);

	uint8_t opcode_operator[] = {RACP_OPCODE_DELETE_RECS, RACP_OPERATOR_RANGE};

	nrf_err = ble_racp_decode(opcode_operator, sizeof(opcode_operator), &racp_val);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	TEST_ASSERT_EQUAL(RACP_OPCODE_DELETE_RECS, racp_val.opcode);
	TEST_ASSERT_EQUAL(RACP_OPERATOR_RANGE, racp_val.operator);
	TEST_ASSERT_EQUAL_PTR(NULL, racp_val.operand);
	TEST_ASSERT_EQUAL(0, racp_val.operand_len);

	uint8_t opcode_operator_data[] = {RACP_OPCODE_DELETE_RECS, RACP_OPERATOR_RANGE, 0xA};

	nrf_err = ble_racp_decode(opcode_operator_data, sizeof(opcode_operator_data), &racp_val);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

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
