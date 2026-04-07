/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <stdint.h>
#include <string.h>
#include <unity.h>

#include "cmock_ble.h"
#include "cmock_ble_gap.h"

#include <bm/bluetooth/ble_adv_data.h>

/* Fill pattern to detect unwanted writes. */
#define BUF_FILL_PATTERN 0xAA

/* UUID encode constants. */
#define TEST_UUID_16_VAL   0x1234
#define TEST_UUID_128_VAL  0xABCD
#define TEST_UUID_128_TYPE BLE_UUID_TYPE_VENDOR_BEGIN

static const uint8_t test_uuid_128_encoded[] = {
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0A, 0x0B, 0x0C, 0xCD, 0xAB, 0x0E, 0x0F,
};

/* Nordic Semiconductor company identifier (Bluetooth SIG assigned). */
#define BLE_COMPANY_ID_NORDIC 0x0059

/* Name encode constants. */
#define TEST_DEVICE_NAME     "TestDev"
#define TEST_DEVICE_NAME_LEN strlen(TEST_DEVICE_NAME)

/* Global variables to test with */
static uint8_t buf[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
static uint16_t len;

/* Stub functions for SoftDevice specific functionalities */
static uint32_t stub_uuid_encode(const ble_uuid_t *p_uuid, uint8_t *p_uuid_le_len,
				 uint8_t *p_uuid_le, int cmock_num_calls)
{
	/*
	 * Stub for sd_ble_uuid_encode(). Returns the encoded size via p_uuid_le_len
	 * (2 for BLE_UUID_TYPE_BLE, 16 otherwise).
	 * When p_uuid_le is non-NULL the actual bytes are written:
	 * little-endian 16-bit value for standard UUIDs,
	 * or the test_uuid_128_encoded array for vendor UUIDs.
	 */
	(void)cmock_num_calls;

	if (p_uuid->type == BLE_UUID_TYPE_BLE) {
		*p_uuid_le_len = 2;
		if (p_uuid_le) {
			p_uuid_le[0] = (uint8_t)(p_uuid->uuid & 0xFF);
			p_uuid_le[1] = (uint8_t)(p_uuid->uuid >> 8);
		}
	} else {
		*p_uuid_le_len = 16;
		if (p_uuid_le) {
			memcpy(p_uuid_le, test_uuid_128_encoded, 16);
		}
	}
	return NRF_SUCCESS;
}

static uint32_t stub_uuid_encode_fail_on_write(const ble_uuid_t *p_uuid, uint8_t *p_uuid_le_len,
					       uint8_t *p_uuid_le, int cmock_num_calls)
{
	/*
	 * Stub for sd_ble_uuid_encode() that succeeds on the size query (p_uuid_le is NULL)
	 * but fails with NRF_ERROR_INTERNAL on the actual encode call (p_uuid_le is non-NULL).
	 * Used to test the sd_ble_uuid_encode() error path.
	 */
	(void)cmock_num_calls;
	*p_uuid_le_len = 2;
	if (p_uuid_le) {
		return NRF_ERROR_INTERNAL;
	}
	return NRF_SUCCESS;
}

static uint32_t stub_device_name_get(uint8_t *p_dev_name, uint16_t *p_len, int cmock_num_calls)
{
	/*
	 * Stub for sd_ble_gap_device_name_get().
	 * Copies TEST_DEVICE_NAME into the output buffer and sets the length to
	 * TEST_DEVICE_NAME_LEN.
	 */
	(void)cmock_num_calls;
	memcpy(p_dev_name, TEST_DEVICE_NAME, TEST_DEVICE_NAME_LEN);
	*p_len = TEST_DEVICE_NAME_LEN;
	return NRF_SUCCESS;
}

/* Helper functions */
static void assert_encoded(const uint8_t *expected, uint16_t expected_len)
{
	TEST_ASSERT_EQUAL(expected_len, len);
	/* Unity Tests rejects array comparison with length 0. */
	if (expected_len > 0) {
		TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, expected_len);
	}
	TEST_ASSERT_EACH_EQUAL_HEX8(BUF_FILL_PATTERN, &buf[expected_len],
				    sizeof(buf) - expected_len);
}

static void common_test_data_encode(void)
{
	/*
	 * Encodes 6 AD fields of varying sizes that are use by all parse/find tests.
	 *   appearance:    4 bytes (1+1+2)
	 *   flags:         3 bytes (1+1+1)
	 *   tx_power:      3 bytes (1+1+1)
	 *   uuid 16-bit:   4 bytes (1+1+2)
	 *   conn_int:      6 bytes (1+1+4)
	 *   device_name:   9 bytes (1+1+7)
	 *   Total: 29 bytes (fits in 31)
	 */
	uint32_t nrf_err;
	uint16_t appearance = BLE_APPEARANCE_GENERIC_HID;
	int8_t tx_power = 4;
	ble_uuid_t uuids[] = {
		{
			.uuid = TEST_UUID_16_VAL,
			.type = BLE_UUID_TYPE_BLE
		},
	};
	struct ble_adv_data_conn_int conn_int = {
		.min_conn_interval = 0x0006,
		.max_conn_interval = 0x0C80,
	};
	struct ble_adv_data adv_data = {
		.include_appearance = true,
		.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		.tx_power_level = &tx_power,
		.uuid_lists.complete = {
			.uuid = uuids,
			.len = 1
		},
		.periph_conn_int = &conn_int,
		.name_type = BLE_ADV_DATA_FULL_NAME,
	};

	__cmock_sd_ble_gap_appearance_get_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gap_appearance_get_ReturnThruPtr_p_appearance(&appearance);
	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);
	__cmock_sd_ble_gap_device_name_get_Stub(stub_device_name_get);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

static void assert_parsed(const uint8_t *result, const uint8_t *expected_value,
			   uint16_t expected_value_len)
{
	/* NULL expected_value means we expect the AD type to not be found. */
	if (expected_value == NULL) {
		TEST_ASSERT_NULL(result);
		return;
	}
	TEST_ASSERT_NOT_NULL(result);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_value, result, expected_value_len);
}

static void encode_short_name_test_data(void)
{
	/*
	 * Separate from common_test_data_encode() because that encodes a full name
	 * (COMPLETE_LOCAL_NAME), but ble_adv_data_short_name_find() only searches
	 * for SHORT_LOCAL_NAME fields. This encodes "Test" (first 4 chars of TEST_DEVICE_NAME)
	 * as a SHORT_LOCAL_NAME AD field.
	 */
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {
		.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		.name_type = BLE_ADV_DATA_SHORT_NAME,
		.short_name_len = 4,
	};

	__cmock_sd_ble_gap_device_name_get_Stub(stub_device_name_get);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

static void encode_128bit_uuid_test_data(void)
{
	/*
	 * Separate from common_test_data_encode() because adding a 128-bit UUID (18 bytes)
	 * to the common data (29 bytes) would exceed BLE_GAP_ADV_SET_DATA_SIZE_MAX (31 bytes).
	 * This encodes a 128-bit UUID to test the UUID128_SIZE switch case path in
	 * ble_adv_data_uuid_find().
	 */
	uint32_t nrf_err;
	ble_uuid_t uuids[] = {
		{.uuid = TEST_UUID_128_VAL, .type = TEST_UUID_128_TYPE},
	};
	struct ble_adv_data adv_data = {
		.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		.uuid_lists.complete = {.uuid = uuids, .len = 1},
	};

	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

/* ble_adv_data_encode() => Generic Unit Tests */
void test_ble_adv_data_encode_error_null(void)
{
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {0};

	nrf_err = ble_adv_data_encode(NULL, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_data_encode(&adv_data, NULL, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_data_encode(&adv_data, buf, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_data_encode(NULL, NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_adv_data_encode_empty(void)
{
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {0};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_zero_length_buf(void)
{
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {0};

	len = 0;

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_all_fields_overflow(void)
{
	/*
	 * Configure every possible field. Buffer is 31 bytes.
	 * Encoding order and sizes:
	 *   device_addr:  9 bytes (offset  0 -> 9)
	 *   appearance:   4 bytes (offset  9 -> 13)
	 *   flags:        3 bytes (offset 13 -> 16)
	 *   tx_power:     3 bytes (offset 16 -> 19)
	 *   uuid 16-bit:  4 bytes (offset 19 -> 23)
	 *   conn_int:     6 bytes (offset 23 -> 29)
	 *   manuf_data:   4 bytes needed, only 2 left -> NRF_ERROR_DATA_SIZE
	 *   srv_list:     not reached
	 *   device_name:  not reached
	 *
	 * Verify the 29 bytes encoded before the overflow are correct.
	 */
	uint32_t nrf_err;

	/* len stays at 29, the offset after the last successful encode (conn_int). */
	const uint8_t expected_buf[] = {
		/* Device Address (public) */
		8,
		BLE_GAP_AD_TYPE_LE_BLUETOOTH_DEVICE_ADDRESS,
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x00,
		/* Appearance */
		3,
		BLE_GAP_AD_TYPE_APPEARANCE,
		0xC0, 0x03, /* 0x03C0 little-endian */
		/* Flags */
		2,
		BLE_GAP_AD_TYPE_FLAGS,
		BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		/* TX Power Level */
		2,
		BLE_GAP_AD_TYPE_TX_POWER_LEVEL,
		0x04,
		/* 16-bit UUID complete */
		3,
		BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE,
		0x34, 0x12, /* 0x1234 little-endian (BLE byte order) */
		/* Connection Interval Range */
		5,
		BLE_GAP_AD_TYPE_SLAVE_CONNECTION_INTERVAL_RANGE,
		0x06, 0x00, /* 0x0006 little-endian */
		0x80, 0x0C, /* 0x0C80 little-endian */
	};

	ble_gap_addr_t addr = {
		.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC,
		.addr = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
	};
	uint16_t appearance = BLE_APPEARANCE_GENERIC_HID;
	int8_t tx_power = 4;
	ble_uuid_t uuids[] = {
		{
			.uuid = TEST_UUID_16_VAL,
			.type = BLE_UUID_TYPE_BLE
		},
	};
	struct ble_adv_data_conn_int conn_int = {
		.min_conn_interval = 0x0006,
		.max_conn_interval = 0x0C80,
	};
	struct ble_adv_data_manufacturer manuf = {
		.company_identifier = BLE_COMPANY_ID_NORDIC,
		.data = NULL,
		.len = 0,
	};
	uint8_t srv_payload[] = {0x01};
	struct ble_adv_data_service services[] = {
		{
			.service_uuid = 0x180D,
			.data = srv_payload,
			.len = sizeof(srv_payload)
		},
	};

	struct ble_adv_data adv_data = {
		.include_ble_device_addr = true,
		.include_appearance = true,
		.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		.tx_power_level = &tx_power,
		.uuid_lists.complete = {
			.uuid = uuids,
			.len = 1
		},
		.periph_conn_int = &conn_int,
		.manufacturer_data = &manuf,
		.srv_list = {
			.service = services,
			.len = 1
		},
		.name_type = BLE_ADV_DATA_FULL_NAME,
	};

	__cmock_sd_ble_gap_addr_get_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gap_addr_get_ReturnThruPtr_p_addr(&addr);
	__cmock_sd_ble_gap_appearance_get_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gap_appearance_get_ReturnThruPtr_p_appearance(&appearance);
	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);
	/* device_name_get() not mocked, never reached due to overflow. */

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

/* ble_adv_data_encode() => device_addr_encode() Unit Tests */
void test_ble_adv_data_encode_device_addr_public(void)
{
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		8,
		BLE_GAP_AD_TYPE_LE_BLUETOOTH_DEVICE_ADDRESS,
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x00, /* public */
	};
	struct ble_adv_data adv_data = {
		.include_ble_device_addr = true
	};
	ble_gap_addr_t addr = {
		.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC,
		.addr = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
	};

	__cmock_sd_ble_gap_addr_get_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gap_addr_get_ReturnThruPtr_p_addr(&addr);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_device_addr_random(void)
{
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		8,
		BLE_GAP_AD_TYPE_LE_BLUETOOTH_DEVICE_ADDRESS,
		0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, /* random */
	};
	struct ble_adv_data adv_data = {
		.include_ble_device_addr = true
	};
	ble_gap_addr_t addr = {
		.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC,
		.addr = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
	};

	__cmock_sd_ble_gap_addr_get_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gap_addr_get_ReturnThruPtr_p_addr(&addr);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_device_addr_sd_error(void)
{
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {
		.include_ble_device_addr = true
	};

	__cmock_sd_ble_gap_addr_get_ExpectAnyArgsAndReturn(NRF_ERROR_INTERNAL);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_device_addr_buf_too_small(void)
{
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {
		.include_ble_device_addr = true
	};
	ble_gap_addr_t addr = {
		.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC,
		.addr = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
	};

	len = 4;

	__cmock_sd_ble_gap_addr_get_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gap_addr_get_ReturnThruPtr_p_addr(&addr);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, nrf_err);
	assert_encoded(NULL, 0);
}

/* ble_adv_data_encode() => appearance_encode() Unit Tests */
void test_ble_adv_data_encode_appearance_success(void)
{
	/* Appearance encoded successfully in little-endian. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		3,
		BLE_GAP_AD_TYPE_APPEARANCE,
		0xC0, 0x03, /* 0x03C0 little-endian */
	};
	struct ble_adv_data adv_data = {
		.include_appearance = true
	};
	uint16_t appearance = BLE_APPEARANCE_GENERIC_HID; /* 0x03C0 */

	__cmock_sd_ble_gap_appearance_get_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gap_appearance_get_ReturnThruPtr_p_appearance(&appearance);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_appearance_sd_error(void)
{
	/* sd_ble_gap_appearance_get() fails, error propagated. */
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {
		.include_appearance = true
	};

	__cmock_sd_ble_gap_appearance_get_ExpectAnyArgsAndReturn(NRF_ERROR_INTERNAL);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_appearance_buf_too_small(void)
{
	/* Buffer too small for the 4-byte appearance AD field. */
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {
		.include_appearance = true
	};
	uint16_t appearance = BLE_APPEARANCE_GENERIC_HID; /* 0x03C0 */

	len = 3; /* Need 1 (len) + 1 (type) + 2 (value) = 4. */

	__cmock_sd_ble_gap_appearance_get_ExpectAnyArgsAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gap_appearance_get_ReturnThruPtr_p_appearance(&appearance);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, nrf_err);
	assert_encoded(NULL, 0);
}

/* ble_adv_data_encode() => flags_encode() Unit Tests */
void test_ble_adv_data_encode_flags_success(void)
{
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		2,
		BLE_GAP_AD_TYPE_FLAGS,
		BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
	};
	struct ble_adv_data adv_data = {
		.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_flags_buf_too_small(void)
{
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {
		.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
	};

	len = 2; /* Need 1 (len) + 1 (type) + 1 (value) = 3. */

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, nrf_err);
	assert_encoded(NULL, 0);
}

/* ble_adv_data_encode() => tx_power_level_encode() Unit Tests */
void test_ble_adv_data_encode_tx_power_level_success(void)
{
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		2,
		BLE_GAP_AD_TYPE_TX_POWER_LEVEL,
		0x04,
	};
	int8_t tx_power = 4;
	struct ble_adv_data adv_data = {
		.tx_power_level = &tx_power,
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_tx_power_level_negative(void)
{
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		2,
		BLE_GAP_AD_TYPE_TX_POWER_LEVEL,
		0xEC, /* -20 as uint8_t */
	};
	int8_t tx_power = -20;
	struct ble_adv_data adv_data = {
		.tx_power_level = &tx_power,
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_tx_power_level_buf_too_small(void)
{
	uint32_t nrf_err;
	int8_t tx_power = 4;
	struct ble_adv_data adv_data = {
		.tx_power_level = &tx_power,
	};

	len = 2; /* Need 1 (len) + 1 (type) + 1 (value) = 3. */

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, nrf_err);
	assert_encoded(NULL, 0);
}

/* ble_adv_data_encode() => uuid_list_encode() Unit Tests */
void test_ble_adv_data_encode_uuid_more_available_16bit(void)
{
	/* "More available" list uses different AD type than "complete". */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		3,
		BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_MORE_AVAILABLE,
		0x34, 0x12, /* 0x1234 little-endian (BLE byte order) */
	};
	ble_uuid_t uuids[] = {
		{
			.uuid = TEST_UUID_16_VAL,
			.type = BLE_UUID_TYPE_BLE
		},
	};
	struct ble_adv_data adv_data = {
		.uuid_lists.more_available = {
			.uuid = uuids,
			.len = 1
		},
	};

	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_uuid_more_available_sd_error(void)
{
	/* sd_ble_uuid_encode() fails when encoding the "more available" UUID list. */
	uint32_t nrf_err;
	ble_uuid_t uuids[] = {
		{
			.uuid = TEST_UUID_16_VAL,
			.type = BLE_UUID_TYPE_BLE
		},
	};
	struct ble_adv_data adv_data = {
		.uuid_lists.more_available = {
			.uuid = uuids,
			.len = 1
		},
	};

	__cmock_sd_ble_uuid_encode_ExpectAnyArgsAndReturn(NRF_ERROR_INTERNAL);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_uuid_complete_16bit(void)
{
	/* Single 16-bit UUID in the complete list. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		3,
		BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE,
		0x34, 0x12, /* 0x1234 little-endian (BLE byte order) */
	};
	ble_uuid_t uuids[] = {
		{
			.uuid = TEST_UUID_16_VAL,
			.type = BLE_UUID_TYPE_BLE
		},
	};
	struct ble_adv_data adv_data = {
		.uuid_lists.complete = {
			.uuid = uuids,
			.len = 1
		},
	};

	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_uuid_complete_128bit(void)
{
	/* Single 128-bit UUID in the complete list. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		17,
		BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE,
		/* test_uuid_128_encoded[] bytes as defined in test constants */
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0A, 0x0B, 0x0C, 0xCD, 0xAB, 0x0E, 0x0F,
	};
	ble_uuid_t uuids[] = {
		{
			.uuid = TEST_UUID_128_VAL,
			.type = TEST_UUID_128_TYPE
		},
	};
	struct ble_adv_data adv_data = {
		.uuid_lists.complete = {
			.uuid = uuids,
			.len = 1
		},
	};

	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_uuid_complete_mixed(void)
{
	/* Mixed 16-bit and 128-bit UUIDs produce two separate AD fields. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		/* 16-bit AD field */
		3,
		BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE,
		0x34, 0x12, /* 0x1234 little-endian (BLE byte order) */
		/* 128-bit AD field */
		17,
		BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE,
		/* test_uuid_128_encoded[] bytes as defined in test constants */
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0A, 0x0B, 0x0C, 0xCD, 0xAB, 0x0E, 0x0F,
	};
	ble_uuid_t uuids[] = {
		{
			.uuid = TEST_UUID_16_VAL,
			.type = BLE_UUID_TYPE_BLE
		},
		{
			.uuid = TEST_UUID_128_VAL,
			.type = TEST_UUID_128_TYPE
		},
	};
	struct ble_adv_data adv_data = {
		.uuid_lists.complete = {
			.uuid = uuids,
			.len = 2
		},
	};

	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_uuid_solicited_success(void)
{
	/* Solicited 16-bit UUID encoded with the correct AD type. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		3,
		BLE_GAP_AD_TYPE_SOLICITED_SERVICE_UUIDS_16BIT,
		0x34, 0x12, /* 0x1234 little-endian (BLE byte order) */
	};
	ble_uuid_t uuids[] = {
		{
			.uuid = TEST_UUID_16_VAL,
			.type = BLE_UUID_TYPE_BLE
		},
	};
	struct ble_adv_data adv_data = {
		.uuid_lists.solicited = {
			.uuid = uuids,
			.len = 1
		},
	};

	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_uuid_solicited_sd_error(void)
{
	/* sd_ble_uuid_encode() fails when encoding the solicited UUID list. */
	uint32_t nrf_err;
	ble_uuid_t uuids[] = {
		{
			.uuid = TEST_UUID_16_VAL,
			.type = BLE_UUID_TYPE_BLE
		},
	};
	struct ble_adv_data adv_data = {
		.uuid_lists.solicited = {
			.uuid = uuids,
			.len = 1
		},
	};

	__cmock_sd_ble_uuid_encode_ExpectAnyArgsAndReturn(NRF_ERROR_INTERNAL);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_uuid_sd_error_size_query(void)
{
	/* sd_ble_uuid_encode() fails on size query (first call), error propagated. */
	uint32_t nrf_err;
	ble_uuid_t uuids[] = {
		{
			.uuid = TEST_UUID_16_VAL,
			.type = BLE_UUID_TYPE_BLE
		},
	};
	struct ble_adv_data adv_data = {
		.uuid_lists.complete = {
			.uuid = uuids,
			.len = 1
		},
	};

	__cmock_sd_ble_uuid_encode_ExpectAnyArgsAndReturn(NRF_ERROR_INTERNAL);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_uuid_sd_error_encode(void)
{
	/* sd_ble_uuid_encode() succeeds on size query but fails on actual encode. */
	uint32_t nrf_err;
	ble_uuid_t uuids[] = {
		{
			.uuid = TEST_UUID_16_VAL,
			.type = BLE_UUID_TYPE_BLE
		},
	};
	struct ble_adv_data adv_data = {
		.uuid_lists.complete = {
			.uuid = uuids,
			.len = 1
		},
	};

	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode_fail_on_write);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_uuid_internal_buf_overflow(void)
{
	/*
	 * The internal uuid_buf in uuid_list_sized_encode() is BLE_GAP_ADV_SET_DATA_SIZE_MAX
	 * (31 bytes). 16 UUIDs × 2 bytes each = 32 bytes, which exceeds the internal buffer
	 * and triggers the uuid_buf overflow check before ad_field_encode() is reached.
	 */
	uint32_t nrf_err;
	ble_uuid_t uuids[16];

	for (uint8_t i = 0; i < 16; i++) {
		uuids[i].uuid = 0x1800 + i;
		uuids[i].type = BLE_UUID_TYPE_BLE;
	}
	struct ble_adv_data adv_data = {
		.uuid_lists.complete = {.uuid = uuids, .len = 16},
	};

	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_uuid_128bit_buf_too_small(void)
{
	/*
	 * Buffer has enough room for the 16-bit UUID AD field (4 bytes) but not for the
	 * 128-bit UUID AD field (18 bytes). Triggers the error path on the second
	 * uuid_list_sized_encode() call inside uuid_list_encode().
	 */
	uint32_t nrf_err;
	ble_uuid_t uuids[] = {
		{
			.uuid = TEST_UUID_16_VAL,
			.type = BLE_UUID_TYPE_BLE
		},
		{
			.uuid = TEST_UUID_128_VAL,
			.type = TEST_UUID_128_TYPE
		},
	};
	struct ble_adv_data adv_data = {
		.uuid_lists.complete = {.uuid = uuids, .len = 2},
	};

	len = 10; /* Fits 16-bit field (4 bytes), not 128-bit field (18 bytes). */

	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, nrf_err);
}

void test_ble_adv_data_encode_uuid_buf_too_small(void)
{
	/* Buffer too small for the 16-bit UUID AD field. */
	uint32_t nrf_err;
	ble_uuid_t uuids[] = {
		{
			.uuid = TEST_UUID_16_VAL,
			.type = BLE_UUID_TYPE_BLE
		},
	};
	struct ble_adv_data adv_data = {
		.uuid_lists.complete = {
			.uuid = uuids,
			.len = 1
		},
	};

	len = 3; /* Need 1 (len) + 1 (type) + 2 (uuid) = 4. */

	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, nrf_err);
	assert_encoded(NULL, 0);
}

/* ble_adv_data_encode() => conn_int_encode() Unit Tests */
void test_ble_adv_data_encode_conn_int_success(void)
{
	/* Valid min and max intervals encoded in little-endian. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		5,
		BLE_GAP_AD_TYPE_SLAVE_CONNECTION_INTERVAL_RANGE,
		0x06, 0x00, /* 0x0006 little-endian */
		0x80, 0x0C, /* 0x0C80 little-endian */
	};
	struct ble_adv_data_conn_int conn_int = {
		.min_conn_interval = 0x0006,
		.max_conn_interval = 0x0C80,
	};
	struct ble_adv_data adv_data = {
		.periph_conn_int = &conn_int,
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_conn_int_no_preference(void)
{
	/* 0xFFFF means "no specific preference", valid for both min and max. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		5,
		BLE_GAP_AD_TYPE_SLAVE_CONNECTION_INTERVAL_RANGE,
		0xFF, 0xFF, /* 0xFFFF little-endian */
		0xFF, 0xFF, /* 0xFFFF little-endian */
	};
	struct ble_adv_data_conn_int conn_int = {
		.min_conn_interval = 0xFFFF,
		.max_conn_interval = 0xFFFF,
	};
	struct ble_adv_data adv_data = {
		.periph_conn_int = &conn_int,
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_conn_int_min_too_low(void)
{
	/* min_conn_interval below 0x0006 is invalid. */
	uint32_t nrf_err;
	struct ble_adv_data_conn_int conn_int = {
		.min_conn_interval = 0x0005,
		.max_conn_interval = 0x0C80,
	};
	struct ble_adv_data adv_data = {
		.periph_conn_int = &conn_int,
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_conn_int_min_too_high(void)
{
	/* min_conn_interval above 0x0C80 (and not 0xFFFF) is invalid. */
	uint32_t nrf_err;
	struct ble_adv_data_conn_int conn_int = {
		.min_conn_interval = 0x0C81,
		.max_conn_interval = 0x0C81,
	};
	struct ble_adv_data adv_data = {
		.periph_conn_int = &conn_int,
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_conn_int_max_too_low(void)
{
	/* max_conn_interval below 0x0006 is invalid. */
	uint32_t nrf_err;
	struct ble_adv_data_conn_int conn_int = {
		.min_conn_interval = 0x0006,
		.max_conn_interval = 0x0005,
	};
	struct ble_adv_data adv_data = {
		.periph_conn_int = &conn_int,
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_conn_int_min_greater_than_max(void)
{
	/* min > max (both valid individually) is invalid. */
	uint32_t nrf_err;
	struct ble_adv_data_conn_int conn_int = {
		.min_conn_interval = 0x0100,
		.max_conn_interval = 0x0006,
	};
	struct ble_adv_data adv_data = {
		.periph_conn_int = &conn_int,
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_conn_int_buf_too_small(void)
{
	/* Buffer too small for the 6-byte connection interval AD field. */
	uint32_t nrf_err;
	struct ble_adv_data_conn_int conn_int = {
		.min_conn_interval = 0x0006,
		.max_conn_interval = 0x0C80,
	};
	struct ble_adv_data adv_data = {
		.periph_conn_int = &conn_int,
	};

	len = 5; /* Need 1 (len) + 1 (type) + 4 (value) = 6. */

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, nrf_err);
	assert_encoded(NULL, 0);
}

/* ble_adv_data_encode() => manuf_specific_data_encode() Unit Tests */
void test_ble_adv_data_encode_manuf_data_id_only(void)
{
	/* Company ID only, no additional data. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		3,
		BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA,
		0x59, 0x00, /* 0x0059 little-endian */
	};
	struct ble_adv_data_manufacturer manuf = {
		.company_identifier = BLE_COMPANY_ID_NORDIC,
		.data = NULL,
		.len = 0,
	};
	struct ble_adv_data adv_data = {
		.manufacturer_data = &manuf,
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_manuf_data_with_payload(void)
{
	/* Company ID with additional payload bytes. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		7,
		BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA,
		0x59, 0x00, /* 0x0059 little-endian */
		0xDE, 0xAD, 0xBE, 0xEF,
	};
	uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
	struct ble_adv_data_manufacturer manuf = {
		.company_identifier = BLE_COMPANY_ID_NORDIC,
		.data = payload,
		.len = sizeof(payload),
	};
	struct ble_adv_data adv_data = {
		.manufacturer_data = &manuf,
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_manuf_data_len_nonzero_null_data(void)
{
	/* len > 0 but data pointer is NULL is invalid. */
	uint32_t nrf_err;
	struct ble_adv_data_manufacturer manuf = {
		.company_identifier = BLE_COMPANY_ID_NORDIC,
		.data = NULL,
		.len = 5,
	};
	struct ble_adv_data adv_data = {
		.manufacturer_data = &manuf,
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_manuf_data_buf_too_small(void)
{
	/* Buffer too small for company ID + payload. */
	uint32_t nrf_err;
	uint8_t payload[] = {0xDE, 0xAD};
	struct ble_adv_data_manufacturer manuf = {
		.company_identifier = BLE_COMPANY_ID_NORDIC,
		.data = payload,
		.len = sizeof(payload),
	};
	struct ble_adv_data adv_data = {
		.manufacturer_data = &manuf,
	};

	len = 4; /* Need 1 (len) + 1 (type) + 2 (id) + 2 (payload) = 6. */

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, nrf_err);
	assert_encoded(NULL, 0);
}

/* ble_adv_data_encode() => service_data_encode() Unit Tests */
void test_ble_adv_data_encode_service_data_uuid_only(void)
{
	/* Single service with UUID only, no additional data. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		3,
		BLE_GAP_AD_TYPE_SERVICE_DATA,
		0x0D, 0x18, /* 0x180D little-endian */
	};
	struct ble_adv_data_service services[] = {
		{
			.service_uuid = 0x180D,
			.data = NULL,
			.len = 0
		},
	};
	struct ble_adv_data adv_data = {
		.srv_list = {
			.service = services,
			.len = 1
		},
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_service_data_with_payload(void)
{
	/* Single service with UUID and additional payload. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		6,
		BLE_GAP_AD_TYPE_SERVICE_DATA,
		0x0D, 0x18, /* 0x180D little-endian */
		0xAA, 0xBB, 0xCC,
	};
	uint8_t payload[] = {0xAA, 0xBB, 0xCC};
	struct ble_adv_data_service services[] = {
		{
			.service_uuid = 0x180D,
			.data = payload,
			.len = sizeof(payload)
		},
	};
	struct ble_adv_data adv_data = {
		.srv_list = {
			.service = services,
			.len = 1
		},
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_service_data_multiple(void)
{
	/* Two services each produce a separate AD field. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		/* First service */
		4,
		BLE_GAP_AD_TYPE_SERVICE_DATA,
		0x0D, 0x18, /* 0x180D little-endian */
		0x01,
		/* Second service */
		5,
		BLE_GAP_AD_TYPE_SERVICE_DATA,
		0x0F, 0x18, /* 0x180F little-endian */
		0x02, 0x03,
	};
	uint8_t payload1[] = {0x01};
	uint8_t payload2[] = {0x02, 0x03};
	struct ble_adv_data_service services[] = {
		{
			.service_uuid = 0x180D,
			.data = payload1,
			.len = sizeof(payload1)
		},
		{
			.service_uuid = 0x180F,
			.data = payload2,
			.len = sizeof(payload2)
		},
	};
	struct ble_adv_data adv_data = {
		.srv_list = {.service = services, .len = 2},
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_service_data_null_service_ptr(void)
{
	/* srv_list.len > 0 but service pointer is NULL. */
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {
		.srv_list = {
			.service = NULL,
			.len = 1
		},
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_service_data_len_nonzero_null_data(void)
{
	/* Service has len > 0 but data pointer is NULL. */
	uint32_t nrf_err;
	struct ble_adv_data_service services[] = {
		{
			.service_uuid = 0x180D,
			.data = NULL,
			.len = 3
		},
	};
	struct ble_adv_data adv_data = {
		.srv_list = {
			.service = services,
			.len = 1
		},
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_service_data_buf_too_small(void)
{
	/* Buffer too small for service UUID AD field. */
	uint32_t nrf_err;
	struct ble_adv_data_service services[] = {
		{
			.service_uuid = 0x180D,
			.data = NULL,
			.len = 0
		},
	};
	struct ble_adv_data adv_data = {
		.srv_list = {
			.service = services,
			.len = 1
		},
	};

	len = 3; /* Need 1 (len) + 1 (type) + 2 (uuid) = 4. */

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_service_data_many_overflow(void)
{
	/*
	 * 255 services with UUID-only each need 4 bytes = 1020 bytes total.
	 * BLE_GAP_ADV_SET_DATA_SIZE_MAX (31) can't fit that, so it overflows after a few entries.
	 */
	uint32_t nrf_err;
	struct ble_adv_data_service services[UINT8_MAX];

	for (uint16_t i = 0; i < UINT8_MAX; i++) {
		services[i].service_uuid = 0x1800 + i;
		services[i].data = NULL;
		services[i].len = 0;
	}
	struct ble_adv_data adv_data = {
		.srv_list = {
			.service = services,
			.len = UINT8_MAX
		},
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, nrf_err);
}

/* ble_adv_data_encode() => device_name_encode() Unit Tests */
void test_ble_adv_data_encode_device_name_full(void)
{
	/* Full name fits in buffer, encoded as COMPLETE_LOCAL_NAME. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		8,
		BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
		'T', 'e', 's', 't', 'D', 'e', 'v',
	};
	struct ble_adv_data adv_data = {
		.name_type = BLE_ADV_DATA_FULL_NAME
	};

	__cmock_sd_ble_gap_device_name_get_Stub(stub_device_name_get);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_device_name_short(void)
{
	/* Explicit short name with short_name_len=4, encoded as SHORT_LOCAL_NAME. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		5,
		BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME,
		'T', 'e', 's', 't',
	};
	struct ble_adv_data adv_data = {
		.name_type = BLE_ADV_DATA_SHORT_NAME,
		.short_name_len = 4,
	};

	__cmock_sd_ble_gap_device_name_get_Stub(stub_device_name_get);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_device_name_short_zero_len(void)
{
	/* Short name with short_name_len=0 is invalid. */
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {
		.name_type = BLE_ADV_DATA_SHORT_NAME,
		.short_name_len = 0,
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_device_name_sd_error(void)
{
	/* sd_ble_gap_device_name_get() fails, error propagated. */
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {
		.name_type = BLE_ADV_DATA_FULL_NAME
	};

	__cmock_sd_ble_gap_device_name_get_ExpectAnyArgsAndReturn(NRF_ERROR_INTERNAL);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_device_name_buf_too_small(void)
{
	/* Buffer too small to fit even the AD header (2 bytes). */
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {
		.name_type = BLE_ADV_DATA_FULL_NAME
	};

	len = 1;

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_device_name_short_buf_too_small(void)
{
	/* Buffer too small to fit the requested short name. */
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {
		.name_type = BLE_ADV_DATA_SHORT_NAME,
		.short_name_len = 4,
	};

	len = 4; /* Need 2 (length + type) + 4 (value) = 6. */

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_DATA_SIZE, nrf_err);
	assert_encoded(NULL, 0);
}

void test_ble_adv_data_encode_device_name_full_truncated(void)
{
	/* Full name requested but buffer only fits 4 name bytes, truncated to SHORT_LOCAL_NAME. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		5,
		BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME,
		'T', 'e', 's', 't',
	};
	struct ble_adv_data adv_data = {
		.name_type = BLE_ADV_DATA_FULL_NAME
	};

	len = 6; /* rem = 6 - 2 = 4, name is 7, truncated to 4. */
	__cmock_sd_ble_gap_device_name_get_Stub(stub_device_name_get);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

void test_ble_adv_data_encode_device_name_short_becomes_complete(void)
{
	/* Short name requested but actual name is shorter than short_name_len, becomes COMPLETE. */
	uint32_t nrf_err;
	const uint8_t expected_buf[] = {
		8,
		BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
		'T', 'e', 's', 't', 'D', 'e', 'v',
	};
	struct ble_adv_data adv_data = {
		.name_type = BLE_ADV_DATA_SHORT_NAME,
		.short_name_len = 10, /* Larger than actual name (7). */
	};

	__cmock_sd_ble_gap_device_name_get_Stub(stub_device_name_get);

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	assert_encoded(expected_buf, sizeof(expected_buf));
}

/* ble_adv_data_parse() Unit Tests */
void test_ble_adv_data_parse_find_small(void)
{
	/* Find flags (1 byte value) among 3 differently sized AD fields. */
	const uint8_t expected[] = {BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE};

	common_test_data_encode();

	uint8_t *result = ble_adv_data_parse(buf, len, BLE_GAP_AD_TYPE_FLAGS);

	assert_parsed(result, expected, sizeof(expected));
}

void test_ble_adv_data_parse_find_medium(void)
{
	/* Find connection interval (4 byte value) among 3 differently sized AD fields. */
	const uint8_t expected[] = {
		0x06, 0x00, /* 0x0006 little-endian */
		0x80, 0x0C, /* 0x0C80 little-endian */
	};

	common_test_data_encode();

	uint8_t *result = ble_adv_data_parse(buf, len,
					     BLE_GAP_AD_TYPE_SLAVE_CONNECTION_INTERVAL_RANGE);

	assert_parsed(result, expected, sizeof(expected));
}

void test_ble_adv_data_parse_find_large(void)
{
	/* Find device name (7 byte value) among 3 differently sized AD fields. */
	const uint8_t expected[] = {'T', 'e', 's', 't', 'D', 'e', 'v'};

	common_test_data_encode();

	uint8_t *result = ble_adv_data_parse(buf, len, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME);

	assert_parsed(result, expected, sizeof(expected));
}

void test_ble_adv_data_parse_not_found(void)
{
	/* Search for an AD type not present in the encoded data. */
	common_test_data_encode();

	uint8_t *result = ble_adv_data_parse(buf, len,
					     BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA);

	assert_parsed(result, NULL, 0);
}

void test_ble_adv_data_parse_null_data(void)
{
	uint8_t *result = ble_adv_data_parse(NULL, 10, BLE_GAP_AD_TYPE_FLAGS);

	assert_parsed(result, NULL, 0);
}

void test_ble_adv_data_parse_zero_length(void)
{
	uint8_t *result = ble_adv_data_parse(buf, 0, BLE_GAP_AD_TYPE_FLAGS);

	assert_parsed(result, NULL, 0);
}

void test_ble_adv_data_parse_malformed_length_zero(void)
{
	/* AD field with length byte = 0 is malformed. */
	const uint8_t malformed[] = {0x00, BLE_GAP_AD_TYPE_FLAGS};

	uint8_t *result = ble_adv_data_parse(malformed, sizeof(malformed), BLE_GAP_AD_TYPE_FLAGS);

	assert_parsed(result, NULL, 0);
}

void test_ble_adv_data_parse_malformed_extends_beyond(void)
{
	/*
	 * AD field claims length = 5 (4 bytes of value) but only 3 bytes of value exist.
	 * Data extends beyond the provided buffer.
	 */
	const uint8_t malformed[] = {5, BLE_GAP_AD_TYPE_FLAGS, 0x01, 0x02, 0x03};

	uint8_t *result = ble_adv_data_parse(malformed, sizeof(malformed), BLE_GAP_AD_TYPE_FLAGS);

	assert_parsed(result, NULL, 0);
}

void test_ble_adv_data_parse_single_byte_data(void)
{
	/* 1 byte buffer, not enough for any valid AD field. */
	const uint8_t tiny[] = {0x02};

	uint8_t *result = ble_adv_data_parse(tiny, sizeof(tiny), BLE_GAP_AD_TYPE_FLAGS);

	assert_parsed(result, NULL, 0);
}

/* ble_adv_data_name_find() Unit Tests */
void test_ble_adv_data_name_find_success(void)
{
	/* Exact match of the full device name in encoded data. */
	bool found;

	common_test_data_encode();

	found = ble_adv_data_name_find(buf, len, TEST_DEVICE_NAME);
	TEST_ASSERT_TRUE(found);
}

void test_ble_adv_data_name_find_wrong_name(void)
{
	/* Name present in data but searched name doesn't match. */
	bool found;

	common_test_data_encode();

	found = ble_adv_data_name_find(buf, len, "WrongName");
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_name_find_partial_name(void)
{
	/* Searched name is a prefix of the encoded name, not an exact match. */
	bool found;

	common_test_data_encode();

	found = ble_adv_data_name_find(buf, len, "Test");
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_name_find_longer_name(void)
{
	/* Searched name is longer than the encoded name, not a match. */
	bool found;

	common_test_data_encode();

	found = ble_adv_data_name_find(buf, len, "TestDevExtra");
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_name_find_no_name_in_data(void)
{
	/* Data has no COMPLETE_LOCAL_NAME field. */
	uint32_t nrf_err;
	bool found;
	struct ble_adv_data adv_data = {
		.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
	};

	nrf_err = ble_adv_data_encode(&adv_data, buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	found = ble_adv_data_name_find(buf, len, TEST_DEVICE_NAME);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_name_find_null_name(void)
{
	bool found;

	common_test_data_encode();

	found = ble_adv_data_name_find(buf, len, NULL);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_name_find_empty_name(void)
{
	bool found;

	common_test_data_encode();

	found = ble_adv_data_name_find(buf, len, "");
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_name_find_null_data(void)
{
	bool found = ble_adv_data_name_find(NULL, 10, TEST_DEVICE_NAME);

	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_name_find_zero_length(void)
{
	bool found = ble_adv_data_name_find(buf, 0, TEST_DEVICE_NAME);

	TEST_ASSERT_FALSE(found);
}

/* ble_adv_data_short_name_find() Unit Tests */
void test_ble_adv_data_short_name_find_success(void)
{
	/* Encoded short name "Test" (4 chars), search with full name and min_len=4. */
	bool found;

	encode_short_name_test_data();

	found = ble_adv_data_short_name_find(buf, len, TEST_DEVICE_NAME, 4);
	TEST_ASSERT_TRUE(found);
}

void test_ble_adv_data_short_name_find_min_len_shorter(void)
{
	/* Encoded short name is 4 chars, min_len=2, still a valid match. */
	bool found;

	encode_short_name_test_data();

	found = ble_adv_data_short_name_find(buf, len, TEST_DEVICE_NAME, 2);
	TEST_ASSERT_TRUE(found);
}

void test_ble_adv_data_short_name_find_min_len_too_long(void)
{
	/* Encoded short name is 4 chars, min_len = 5, parsed_name_len < min_len, no match. */
	bool found;

	encode_short_name_test_data();

	found = ble_adv_data_short_name_find(buf, len, TEST_DEVICE_NAME, 5);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_short_name_find_wrong_name(void)
{
	/* Encoded short name is "Test", searched name starts differently. */
	bool found;

	encode_short_name_test_data();

	found = ble_adv_data_short_name_find(buf, len, "Wrong", 4);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_short_name_find_name_shorter_than_encoded(void)
{
	/* Searched name "Te" is shorter than encoded short name "Test", no match. */
	bool found;

	encode_short_name_test_data();

	found = ble_adv_data_short_name_find(buf, len, "Te", 1);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_short_name_find_no_short_name_in_data(void)
{
	/* Data has COMPLETE_LOCAL_NAME but no SHORT_LOCAL_NAME. */
	bool found;

	common_test_data_encode();

	found = ble_adv_data_short_name_find(buf, len, TEST_DEVICE_NAME, 4);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_short_name_find_null_data(void)
{
	bool found = ble_adv_data_short_name_find(NULL, 10, TEST_DEVICE_NAME, 4);

	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_short_name_find_null_name(void)
{
	bool found;

	encode_short_name_test_data();

	found = ble_adv_data_short_name_find(buf, len, NULL, 4);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_short_name_find_zero_length_data(void)
{
	bool found = ble_adv_data_short_name_find(buf, 0, TEST_DEVICE_NAME, 4);

	TEST_ASSERT_FALSE(found);
}

/* ble_adv_data_uuid_find() Unit Tests */
void test_ble_adv_data_uuid_find_16bit_success(void)
{
	/* Find the 16-bit UUID that was encoded by common_test_data_encode(). */
	bool found;
	ble_uuid_t uuid = {
		.uuid = TEST_UUID_16_VAL,
		.type = BLE_UUID_TYPE_BLE
	};

	common_test_data_encode();

	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);

	found = ble_adv_data_uuid_find(buf, len, &uuid);
	TEST_ASSERT_TRUE(found);
}

void test_ble_adv_data_uuid_find_128bit_success(void)
{
	/* Encode a 128-bit UUID, then find it. */
	bool found;
	ble_uuid_t search = {
		.uuid = TEST_UUID_128_VAL,
		.type = TEST_UUID_128_TYPE
	};

	encode_128bit_uuid_test_data();

	found = ble_adv_data_uuid_find(buf, len, &search);
	TEST_ASSERT_TRUE(found);
}

void test_ble_adv_data_uuid_find_16bit_wrong_uuid(void)
{
	/* Search for a 16-bit UUID that doesn't match the encoded one. */
	bool found;
	ble_uuid_t uuid = {
		.uuid = TEST_UUID_16_VAL + 0x01,
		.type = BLE_UUID_TYPE_BLE
	};

	common_test_data_encode();

	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);

	found = ble_adv_data_uuid_find(buf, len, &uuid);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_uuid_find_no_uuid_in_data(void)
{
	/* Data has no UUID AD field. */
	bool found;
	ble_uuid_t uuid = {
		.uuid = TEST_UUID_16_VAL,
		.type = BLE_UUID_TYPE_BLE
	};

	encode_short_name_test_data();

	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);

	found = ble_adv_data_uuid_find(buf, len, &uuid);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_uuid_find_sd_encode_error(void)
{
	/* sd_ble_uuid_encode() fails during the find, returns false. */
	bool found;
	ble_uuid_t uuid = {
		.uuid = TEST_UUID_16_VAL,
		.type = BLE_UUID_TYPE_BLE
	};

	common_test_data_encode();

	/* Reset the stub set by common_test_data_encode() before setting the error expectation. */
	__cmock_sd_ble_uuid_encode_Stub(NULL);
	__cmock_sd_ble_uuid_encode_ExpectAnyArgsAndReturn(NRF_ERROR_INTERNAL);

	found = ble_adv_data_uuid_find(buf, len, &uuid);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_uuid_find_null_data(void)
{
	bool found;
	ble_uuid_t uuid = {
		.uuid = TEST_UUID_16_VAL,
		.type = BLE_UUID_TYPE_BLE
	};

	found = ble_adv_data_uuid_find(NULL, 10, &uuid);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_uuid_find_null_uuid(void)
{
	bool found;

	common_test_data_encode();

	found = ble_adv_data_uuid_find(buf, len, NULL);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_uuid_find_zero_length_data(void)
{
	bool found;
	ble_uuid_t uuid = {
		.uuid = TEST_UUID_16_VAL,
		.type = BLE_UUID_TYPE_BLE
	};

	__cmock_sd_ble_uuid_encode_Stub(stub_uuid_encode);

	found = ble_adv_data_uuid_find(buf, 0, &uuid);
	TEST_ASSERT_FALSE(found);
}

/* ble_adv_data_appearance_find() Unit Tests */
void test_ble_adv_data_appearance_find_success(void)
{
	/* Find the appearance value encoded by common_test_data_encode(). */
	bool found;
	uint16_t target = BLE_APPEARANCE_GENERIC_HID;

	common_test_data_encode();

	found = ble_adv_data_appearance_find(buf, len, &target);
	TEST_ASSERT_TRUE(found);
}

void test_ble_adv_data_appearance_find_wrong_value(void)
{
	/* Appearance is encoded but searched value doesn't match. */
	bool found;
	uint16_t target = BLE_APPEARANCE_GENERIC_CLOCK;

	common_test_data_encode();

	found = ble_adv_data_appearance_find(buf, len, &target);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_appearance_find_no_appearance_in_data(void)
{
	/* Data has no appearance AD field. */
	bool found;
	uint16_t target = BLE_APPEARANCE_GENERIC_HID;

	encode_short_name_test_data();

	found = ble_adv_data_appearance_find(buf, len, &target);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_appearance_find_null_data(void)
{
	bool found;
	uint16_t target = BLE_APPEARANCE_GENERIC_HID;

	found = ble_adv_data_appearance_find(NULL, 10, &target);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_appearance_find_null_target(void)
{
	bool found;

	common_test_data_encode();

	found = ble_adv_data_appearance_find(buf, len, NULL);
	TEST_ASSERT_FALSE(found);
}

void test_ble_adv_data_appearance_find_zero_length_data(void)
{
	bool found;
	uint16_t target = BLE_APPEARANCE_GENERIC_HID;

	found = ble_adv_data_appearance_find(buf, 0, &target);
	TEST_ASSERT_FALSE(found);
}

/* Unit Test Setup */
void setUp(void)
{
	memset(buf, BUF_FILL_PATTERN, sizeof(buf));
	len = sizeof(buf);
}

void tearDown(void) {}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
