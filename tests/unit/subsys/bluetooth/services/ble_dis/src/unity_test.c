/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <bm/bluetooth/services/ble_dis.h>
#include <bm/bluetooth/services/uuid.h>
#include <nrf_error.h>

#include "cmock_ble_gatts.h"

/* Arbitrary service handle returned by the stubbed service add. */
#define SERVICE_HANDLE 0xA4

/* Runtime test values. */
#define TEST_MANUFACTURER_NAME "Manufacturer"
#define TEST_MODEL_NUMBER      "Model"
#define TEST_SERIAL_NUMBER     "Serial"
#define TEST_HW_REVISION       "HWrev"
#define TEST_FW_REVISION       "FWrev"
#define TEST_SW_REVISION       "SWrev"

/* Expected on-air encodings for the binary characteristics. */
static const uint8_t sys_id_expected[8]   = {0x15, 0x14, 0x13, 0x12, 0x11, 0x03, 0x02, 0x01};
static const uint8_t pnp_id_expected[7]   = {0x01, 0x03, 0x02, 0x05, 0x04, 0x07, 0x06};
static const uint8_t reg_cert_expected[8] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};

/* Runtime config that exercises every characteristic. */
static const struct ble_dis_sys_id test_sys_id = {
	.manufacturer_id            = 0x1112131415, /* -> 0x15 0x14 0x13 0x12 0x11 */
	.organizationally_unique_id = 0x010203,     /* -> 0x03 0x02 0x01 */
};

static const struct ble_dis_pnp_id test_pnp_id = {
	.vendor_id_source = 0x01,
	.vendor_id        = 0x0203,
	.product_id       = 0x0405,
	.product_version  = 0x0607,
};

static const struct ble_dis_reg_cert test_reg_cert = {
	.list = reg_cert_expected,
	.len  = sizeof(reg_cert_expected),
};

static const struct ble_dis_values test_values = {
	.manufacturer_name = TEST_MANUFACTURER_NAME,
	.model_number      = TEST_MODEL_NUMBER,
	.serial_number     = TEST_SERIAL_NUMBER,
	.hw_revision       = TEST_HW_REVISION,
	.fw_revision       = TEST_FW_REVISION,
	.sw_revision       = TEST_SW_REVISION,
	.sys_id            = &test_sys_id,
	.pnp_id            = &test_pnp_id,
	.reg_cert          = &test_reg_cert,
};

static const struct ble_dis_config config_runtime = {
	.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
	.values   = &test_values,
};

static const struct ble_dis_config config_defaults = {
	.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
	/* .values left NULL -> Kconfig defaults. */
};

/* Number of characteristic_add calls observed, reset in setUp(). */
static int char_add_call_count;
/* Set to true by stub_char_add_track_serial_number when serial number is added. */
static bool serial_number_was_added;

/* Stub functions for SoftDevice specific functionalities */
static uint32_t stub_service_add(uint8_t type, const ble_uuid_t *p_uuid, uint16_t *p_handle,
				 int cmock_num_calls)
{
	/* Verify the DIS service is added as a primary service with the correct UUID. */
	(void)cmock_num_calls;
	TEST_ASSERT_EQUAL(BLE_GATTS_SRVC_TYPE_PRIMARY, type);
	TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_uuid->type);
	TEST_ASSERT_EQUAL(BLE_UUID_DEVICE_INFORMATION_SERVICE, p_uuid->uuid);

	*p_handle = SERVICE_HANDLE;
	return NRF_SUCCESS;
}

/* Assert the common, characteristic-independent properties of every add call. */
static void assert_common_char(uint16_t service_handle, const ble_gatts_char_md_t *p_char_md,
			       const ble_gatts_attr_t *p_attr_char_value,
			       ble_gatts_char_handles_t *p_handles)
{
	const ble_gatts_char_md_t char_md_expected = {
		.char_props = {
			.read = true,
		},
	};
	const ble_gatts_char_handles_t char_handles_expected = {0};

	TEST_ASSERT_EQUAL(SERVICE_HANDLE, service_handle);
	TEST_ASSERT_EQUAL_MEMORY(&char_md_expected, p_char_md, sizeof(char_md_expected));
	TEST_ASSERT_EQUAL_MEMORY(&char_handles_expected, p_handles, sizeof(char_handles_expected));
	TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_attr_char_value->p_uuid->type);
}

/* Assert a string characteristic: UUID, value and (init/max) length. */
static void assert_string_char(const ble_gatts_attr_t *attr, uint16_t uuid, const char *value)
{
	TEST_ASSERT_EQUAL(uuid, attr->p_uuid->uuid);
	TEST_ASSERT_EQUAL_STRING(value, attr->p_value);
	TEST_ASSERT_EQUAL(strlen(value), attr->max_len);
	TEST_ASSERT_EQUAL(strlen(value), attr->init_len);
}

/* Assert a binary characteristic: UUID, value bytes and (init/max) length. */
static void assert_binary_char(const ble_gatts_attr_t *attr, uint16_t uuid, const uint8_t *value,
			       uint16_t value_len)
{
	TEST_ASSERT_EQUAL(uuid, attr->p_uuid->uuid);
	TEST_ASSERT_EQUAL_MEMORY(value, attr->p_value, value_len);
	TEST_ASSERT_EQUAL(value_len, attr->max_len);
	TEST_ASSERT_EQUAL(value_len, attr->init_len);
}

/* Dispatch by UUID so the stub works for any subset of characteristics.
 * Each call is matched against its expected value,
 * and the call count lets tests assert exactly how many were added.
 */
static uint32_t stub_char_add(uint16_t service_handle, const ble_gatts_char_md_t *p_char_md,
			      const ble_gatts_attr_t *p_attr_char_value,
			      ble_gatts_char_handles_t *p_handles, int cmock_num_calls)
{
	(void)cmock_num_calls;

	assert_common_char(service_handle, p_char_md, p_attr_char_value, p_handles);
	char_add_call_count++;

	switch (p_attr_char_value->p_uuid->uuid) {
	case BLE_UUID_MANUFACTURER_NAME_STRING_CHAR:
		assert_string_char(p_attr_char_value, BLE_UUID_MANUFACTURER_NAME_STRING_CHAR,
				   TEST_MANUFACTURER_NAME);
		break;
	case BLE_UUID_MODEL_NUMBER_STRING_CHAR:
		assert_string_char(p_attr_char_value, BLE_UUID_MODEL_NUMBER_STRING_CHAR,
				   TEST_MODEL_NUMBER);
		break;
	case BLE_UUID_SERIAL_NUMBER_STRING_CHAR:
		assert_string_char(p_attr_char_value, BLE_UUID_SERIAL_NUMBER_STRING_CHAR,
				   TEST_SERIAL_NUMBER);
		break;
	case BLE_UUID_HARDWARE_REVISION_STRING_CHAR:
		assert_string_char(p_attr_char_value, BLE_UUID_HARDWARE_REVISION_STRING_CHAR,
				   TEST_HW_REVISION);
		break;
	case BLE_UUID_FIRMWARE_REVISION_STRING_CHAR:
		assert_string_char(p_attr_char_value, BLE_UUID_FIRMWARE_REVISION_STRING_CHAR,
				   TEST_FW_REVISION);
		break;
	case BLE_UUID_SOFTWARE_REVISION_STRING_CHAR:
		assert_string_char(p_attr_char_value, BLE_UUID_SOFTWARE_REVISION_STRING_CHAR,
				   TEST_SW_REVISION);
		break;
	case BLE_UUID_SYSTEM_ID_CHAR:
		assert_binary_char(p_attr_char_value, BLE_UUID_SYSTEM_ID_CHAR,
				   sys_id_expected, sizeof(sys_id_expected));
		break;
	case BLE_UUID_PNP_ID_CHAR:
		assert_binary_char(p_attr_char_value, BLE_UUID_PNP_ID_CHAR,
				   pnp_id_expected, sizeof(pnp_id_expected));
		break;
	case BLE_UUID_IEEE_REGULATORY_CERTIFICATION_DATA_LIST_CHAR:
		assert_binary_char(p_attr_char_value,
				   BLE_UUID_IEEE_REGULATORY_CERTIFICATION_DATA_LIST_CHAR,
				   reg_cert_expected, sizeof(reg_cert_expected));
		break;
	default:
		TEST_ASSERT_TRUE_MESSAGE(false, "Unexpected characteristic UUID");
		break;
	}

	return NRF_SUCCESS;
}

/* Accepts any characteristic, sets serial_number_was_added if serial number is seen. */
static uint32_t stub_char_add_track_serial_number(uint16_t service_handle,
						  const ble_gatts_char_md_t *p_char_md,
						  const ble_gatts_attr_t *p_attr_char_value,
						  ble_gatts_char_handles_t *p_handles,
						  int cmock_num_calls)
{
	(void)cmock_num_calls;
	assert_common_char(service_handle, p_char_md, p_attr_char_value, p_handles);
	char_add_call_count++;

	if (p_attr_char_value->p_uuid->uuid == BLE_UUID_SERIAL_NUMBER_STRING_CHAR) {
		serial_number_was_added = true;
	}
	return NRF_SUCCESS;
}


/* ble_dis_init() => Error path Unit Tests */
void test_ble_dis_init_error_null(void)
{
	/* A NULL config is rejected, security is mandatory. */
	uint32_t nrf_err;

	nrf_err = ble_dis_init(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_dis_init_error_service_add(void)
{
	/* sd_ble_gatts_service_add() fails, error propagated. */
	uint32_t nrf_err;

	__cmock_sd_ble_gatts_service_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);

	nrf_err = ble_dis_init(&config_runtime);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_dis_init_error_char_add(void)
{
	/* sd_ble_gatts_characteristic_add() fails, error propagated. */
	uint32_t nrf_err;

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);

	nrf_err = ble_dis_init(&config_runtime);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

/* ble_dis_init() => Runtime config Unit Tests */
void test_ble_dis_init_runtime_all_chars(void)
{
	/* All 9 characteristics added with the supplied runtime values. */
	uint32_t nrf_err;

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add);

	nrf_err = ble_dis_init(&config_runtime);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(9, char_add_call_count);
}

void test_ble_dis_init_runtime_omits_null_fields(void)
{
	/* A runtime config with everything NULL/empty must omit every characteristic:
	 * the service is added but sd_ble_gatts_characteristic_add() is never called.
	 */
	uint32_t nrf_err;
	static const struct ble_dis_values empty_values = {0};
	static const struct ble_dis_config config_empty = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &empty_values,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add);

	nrf_err = ble_dis_init(&config_empty);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, char_add_call_count);
}

/* ble_dis_init() => Partial runtime config Unit Tests */
void test_ble_dis_init_runtime_partial_strings(void)
{
	/* Only a few string characteristics are set, the rest are omitted. */
	uint32_t nrf_err;
	static const struct ble_dis_values values = {
		.manufacturer_name = TEST_MANUFACTURER_NAME,
		.serial_number     = TEST_SERIAL_NUMBER,
		.sw_revision       = TEST_SW_REVISION,
	};
	static const struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &values,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(3, char_add_call_count);
}

void test_ble_dis_init_runtime_partial_binaries(void)
{
	/* Only the System ID and Regulatory Cert binaries are set. */
	uint32_t nrf_err;
	static const struct ble_dis_values values = {
		.sys_id   = &test_sys_id,
		.reg_cert = &test_reg_cert,
	};
	static const struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &values,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(2, char_add_call_count);
}

void test_ble_dis_init_runtime_partial_mixed(void)
{
	/* A mix of strings and binaries with gaps in between. */
	uint32_t nrf_err;
	static const struct ble_dis_values values = {
		.model_number = TEST_MODEL_NUMBER,
		.hw_revision  = TEST_HW_REVISION,
		.sys_id       = &test_sys_id,
		.pnp_id       = &test_pnp_id,
	};
	static const struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &values,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(4, char_add_call_count);
}

/* ble_dis_init() => Single characteristic Unit Tests */
void test_ble_dis_init_runtime_only_manufacturer_name(void)
{
	uint32_t nrf_err;
	static const struct ble_dis_values values = {
		.manufacturer_name = TEST_MANUFACTURER_NAME,
	};
	static const struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &values,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, char_add_call_count);
}

void test_ble_dis_init_runtime_only_model_number(void)
{
	uint32_t nrf_err;
	static const struct ble_dis_values values = {
		.model_number = TEST_MODEL_NUMBER,
	};
	static const struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &values,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, char_add_call_count);
}

void test_ble_dis_init_runtime_only_serial_number(void)
{
	uint32_t nrf_err;
	static const struct ble_dis_values values = {
		.serial_number = TEST_SERIAL_NUMBER,
	};
	static const struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &values,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, char_add_call_count);
}

void test_ble_dis_init_runtime_only_hw_revision(void)
{
	uint32_t nrf_err;
	static const struct ble_dis_values values = {
		.hw_revision = TEST_HW_REVISION,
	};
	static const struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &values,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, char_add_call_count);
}

void test_ble_dis_init_runtime_only_fw_revision(void)
{
	uint32_t nrf_err;
	static const struct ble_dis_values values = {
		.fw_revision = TEST_FW_REVISION,
	};
	static const struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &values,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, char_add_call_count);
}

void test_ble_dis_init_runtime_only_sw_revision(void)
{
	uint32_t nrf_err;
	static const struct ble_dis_values values = {
		.sw_revision = TEST_SW_REVISION,
	};
	static const struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &values,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, char_add_call_count);
}

void test_ble_dis_init_runtime_only_sys_id(void)
{
	uint32_t nrf_err;
	static const struct ble_dis_values values = {
		.sys_id = &test_sys_id,
	};
	static const struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &values,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, char_add_call_count);
}

void test_ble_dis_init_runtime_only_pnp_id(void)
{
	uint32_t nrf_err;
	static const struct ble_dis_values values = {
		.pnp_id = &test_pnp_id,
	};
	static const struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &values,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, char_add_call_count);
}

void test_ble_dis_init_runtime_only_reg_cert(void)
{
	uint32_t nrf_err;
	static const struct ble_dis_values values = {
		.reg_cert = &test_reg_cert,
	};
	static const struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &values,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, char_add_call_count);
}

/* ble_dis_init() => BLE_DIS_VALUES_DEFAULTS macro Unit Tests */
void test_ble_dis_init_defaults_macro_succeeds(void)
{
	/* BLE_DIS_VALUES_DEFAULTS produces a valid config and initializes successfully.
	 * Exact characteristic values depend on Kconfig, so only success is asserted.
	 */
	uint32_t nrf_err;
	struct ble_dis_values vals = BLE_DIS_VALUES_DEFAULTS;
	struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &vals,
	};

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreAndReturn(NRF_SUCCESS);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_dis_init_defaults_macro_override_serial_number(void)
{
	/* Override serial_number after loading defaults: must be added. */
	uint32_t nrf_err;
	struct ble_dis_values vals = BLE_DIS_VALUES_DEFAULTS;
	struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &vals,
	};

	vals.serial_number = TEST_SERIAL_NUMBER;

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add_track_serial_number);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(serial_number_was_added);
}

void test_ble_dis_init_defaults_macro_null_field_omits_char(void)
{
	/* Clear serial_number after loading defaults: must not be added. */
	uint32_t nrf_err;
	struct ble_dis_values vals = BLE_DIS_VALUES_DEFAULTS;
	struct ble_dis_config config = {
		.sec_mode = BLE_DIS_CONFIG_SEC_MODE_DEFAULT,
		.values   = &vals,
	};

	vals.serial_number = NULL;

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_char_add_track_serial_number);

	nrf_err = ble_dis_init(&config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_FALSE(serial_number_was_added);
}

/* ble_dis_init() => Kconfig default Unit Tests */
void test_ble_dis_init_defaults(void)
{
	/* With values == NULL the service is built from the Kconfig defaults.
	 * We don't assert the exact values here (they depend on Kconfig),
	 * only that initialization succeeds end to end.
	 */
	uint32_t nrf_err;

	__cmock_sd_ble_gatts_service_add_Stub(stub_service_add);
	__cmock_sd_ble_gatts_characteristic_add_IgnoreAndReturn(NRF_SUCCESS);

	nrf_err = ble_dis_init(&config_defaults);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

/* Unit Test Setup */
void setUp(void)
{
	__cmock_sd_ble_gatts_service_add_Stub(NULL);
	__cmock_sd_ble_gatts_characteristic_add_Stub(NULL);
	char_add_call_count = 0;
	serial_number_was_added = false;
}

void tearDown(void) {}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
