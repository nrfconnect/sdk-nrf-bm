/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <errno.h>
#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <bm/bluetooth/services/ble_dis.h>
#include <bm/bluetooth/services/uuid.h>
#include <nrf_error.h>

#include "cmock_ble_gatts.h"

#define HANDLE 0xa4

uint32_t stub_sd_ble_gatts_service_add_invalid_param(uint8_t type, ble_uuid_t const *p_uuid,
						     uint16_t *p_handle, int cmock_num_calls)
{
	return NRF_ERROR_INVALID_PARAM;
}

uint32_t stub_sd_ble_gatts_service_add(uint8_t type, ble_uuid_t const *p_uuid, uint16_t *p_handle,
				       int cmock_num_calls)
{
	TEST_ASSERT_EQUAL(BLE_GATTS_SRVC_TYPE_PRIMARY, type);
	TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_uuid->type);
	TEST_ASSERT_EQUAL(BLE_UUID_DEVICE_INFORMATION_SERVICE, p_uuid->uuid);

	*p_handle = HANDLE;

	return NRF_SUCCESS;
}

uint32_t stub_sd_ble_gatts_characteristic_add_invalid_param(
	uint16_t service_handle, ble_gatts_char_md_t const *p_char_md,
	ble_gatts_attr_t const *p_attr_char_value, ble_gatts_char_handles_t *p_handles,
	int cmock_num_calls)
{
	return NRF_ERROR_INVALID_PARAM;
}

uint32_t stub_sd_ble_gatts_characteristic_add(
	uint16_t service_handle, ble_gatts_char_md_t const *p_char_md,
	ble_gatts_attr_t const *p_attr_char_value, ble_gatts_char_handles_t *p_handles,
	int cmock_num_calls)
{
	ble_gatts_char_md_t char_md_expected = {
		.char_props = {
			.read = true,
		},
	};
	ble_gatts_char_handles_t char_handles_expected = {0};
	uint8_t sys_id_expected[8] = {0x15, 0x14, 0x13, 0x12, 0x11, 0x03, 0x02, 0x01};
	uint8_t pnp_id_expected[7] = {0x01, 0x03, 0x02, 0x05, 0x04, 0x07, 0x06};
	uint8_t regulatory_certifications_expected[8] = {0x08, 0x07, 0x06, 0x05,
							 0x04, 0x03, 0x02, 0x01};

	TEST_ASSERT_EQUAL(HANDLE, service_handle);

	TEST_ASSERT_EQUAL_MEMORY(&char_md_expected, p_char_md, sizeof(ble_gatts_char_md_t));
	TEST_ASSERT_EQUAL_MEMORY(&char_handles_expected, p_handles,
				 sizeof(ble_gatts_char_handles_t));

	switch (cmock_num_calls) {
	case 0:
		TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_attr_char_value->p_uuid->type);
		TEST_ASSERT_EQUAL(BLE_UUID_MANUFACTURER_NAME_STRING_CHAR,
				  p_attr_char_value->p_uuid->uuid);
		TEST_ASSERT_EQUAL_STRING("Manufacturer", p_attr_char_value->p_value);
		TEST_ASSERT_EQUAL(12, p_attr_char_value->max_len);
		TEST_ASSERT_EQUAL(12, p_attr_char_value->init_len);
		break;
	case 1:
		TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_attr_char_value->p_uuid->type);
		TEST_ASSERT_EQUAL(BLE_UUID_MODEL_NUMBER_STRING_CHAR,
				  p_attr_char_value->p_uuid->uuid);
		TEST_ASSERT_EQUAL_STRING("Model", p_attr_char_value->p_value);
		TEST_ASSERT_EQUAL(5, p_attr_char_value->max_len);
		TEST_ASSERT_EQUAL(5, p_attr_char_value->init_len);
		break;
	case 2:
		TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_attr_char_value->p_uuid->type);
		TEST_ASSERT_EQUAL(BLE_UUID_SERIAL_NUMBER_STRING_CHAR,
				  p_attr_char_value->p_uuid->uuid);
		TEST_ASSERT_EQUAL_STRING("Serial", p_attr_char_value->p_value);
		TEST_ASSERT_EQUAL(6, p_attr_char_value->max_len);
		TEST_ASSERT_EQUAL(6, p_attr_char_value->init_len);
		break;
	case 3:
		TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_attr_char_value->p_uuid->type);
		TEST_ASSERT_EQUAL(BLE_UUID_HARDWARE_REVISION_STRING_CHAR,
				  p_attr_char_value->p_uuid->uuid);
		TEST_ASSERT_EQUAL_STRING("HWrev", p_attr_char_value->p_value);
		TEST_ASSERT_EQUAL(5, p_attr_char_value->max_len);
		TEST_ASSERT_EQUAL(5, p_attr_char_value->init_len);
		break;
	case 4:
		TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_attr_char_value->p_uuid->type);
		TEST_ASSERT_EQUAL(BLE_UUID_FIRMWARE_REVISION_STRING_CHAR,
				  p_attr_char_value->p_uuid->uuid);
		TEST_ASSERT_EQUAL_STRING("FWrev", p_attr_char_value->p_value);
		TEST_ASSERT_EQUAL(5, p_attr_char_value->max_len);
		TEST_ASSERT_EQUAL(5, p_attr_char_value->init_len);
		break;
	case 5:
		TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_attr_char_value->p_uuid->type);
		TEST_ASSERT_EQUAL(BLE_UUID_SOFTWARE_REVISION_STRING_CHAR,
				  p_attr_char_value->p_uuid->uuid);
		TEST_ASSERT_EQUAL_STRING("SWrev", p_attr_char_value->p_value);
		TEST_ASSERT_EQUAL(5, p_attr_char_value->max_len);
		TEST_ASSERT_EQUAL(5, p_attr_char_value->init_len);
		break;
	case 6:
		TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_attr_char_value->p_uuid->type);
		TEST_ASSERT_EQUAL(BLE_UUID_SYSTEM_ID_CHAR, p_attr_char_value->p_uuid->uuid);
		TEST_ASSERT_EQUAL_MEMORY(sys_id_expected, p_attr_char_value->p_value,
					 sizeof(sys_id_expected));
		TEST_ASSERT_EQUAL(8, p_attr_char_value->max_len);
		TEST_ASSERT_EQUAL(8, p_attr_char_value->init_len);
		break;
	case 7:
		TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_attr_char_value->p_uuid->type);
		TEST_ASSERT_EQUAL(BLE_UUID_PNP_ID_CHAR, p_attr_char_value->p_uuid->uuid);
		TEST_ASSERT_EQUAL_MEMORY(pnp_id_expected, p_attr_char_value->p_value,
					 sizeof(pnp_id_expected));
		TEST_ASSERT_EQUAL(7, p_attr_char_value->max_len);
		TEST_ASSERT_EQUAL(7, p_attr_char_value->init_len);
		break;
	case 8:
		TEST_ASSERT_EQUAL(BLE_UUID_TYPE_BLE, p_attr_char_value->p_uuid->type);
		TEST_ASSERT_EQUAL(BLE_UUID_IEEE_REGULATORY_CERTIFICATION_DATA_LIST_CHAR,
				  p_attr_char_value->p_uuid->uuid);
		TEST_ASSERT_EQUAL_MEMORY(regulatory_certifications_expected,
					 p_attr_char_value->p_value,
					 sizeof(regulatory_certifications_expected));
		TEST_ASSERT_EQUAL(8, p_attr_char_value->max_len);
		TEST_ASSERT_EQUAL(8, p_attr_char_value->init_len);
		break;
	default:
		TEST_ASSERT_TRUE_MESSAGE(false, "Unreachable");
		break;
	}

	return NRF_SUCCESS;
}

void test_ble_dis_init_error_invalid_param(void)
{
	uint32_t nrf_err;

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add_invalid_param);

	nrf_err = ble_dis_init();
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(
		stub_sd_ble_gatts_characteristic_add_invalid_param);

	nrf_err = ble_dis_init();
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_dis_init(void)
{
	uint32_t nrf_err;

	__cmock_sd_ble_gatts_service_add_Stub(stub_sd_ble_gatts_service_add);
	__cmock_sd_ble_gatts_characteristic_add_Stub(stub_sd_ble_gatts_characteristic_add);

	nrf_err = ble_dis_init();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
