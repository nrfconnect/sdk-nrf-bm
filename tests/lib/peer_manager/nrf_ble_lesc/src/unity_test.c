/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <unity.h>
#include <stdint.h>
#include <string.h>

#include "cmock_ble_gap.h"
#include "cmock_crypto.h"
#include "cmock_nrf_sdh_ble.h"

#include <bluetooth/peer_manager/nrf_ble_lesc.h>

#define PTR_IGNORE NULL
#define VAL_IGNORE 0

static psa_key_attributes_t *key_attrs;
static const psa_key_attributes_t key_attrs_expected = PSA_KEY_ATTRIBUTES_INIT;

static ble_gap_lesc_oob_data_t *oobd;

static const mbedtls_svc_key_id_t key_pair_id = 0x2A;

static uint8_t own_test_pub_key_psa[] = {
	0x04,

	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
	0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
	0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,

	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
	0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
	0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
};

static uint8_t own_test_pub_key_sd[] = {
	0xCF, 0xCE, 0xCD, 0xCC, 0xCB, 0xCA, 0xC9, 0xC8,
	0xC7, 0xC6, 0xC5, 0xC4, 0xC3, 0xC2, 0xC1, 0xC0,
	0xBF, 0xBE, 0xBD, 0xBC, 0xBB, 0xBA, 0xB9, 0xB8,
	0xB7, 0xB6, 0xB5, 0xB4, 0xB3, 0xB2, 0xB1, 0xB0,

	0xEF, 0xEE, 0xED, 0xEC, 0xEB, 0xEA, 0xE9, 0xE8,
	0xE7, 0xE6, 0xE5, 0xE4, 0xE3, 0xE2, 0xE1, 0xE0,
	0xDF, 0xDE, 0xDD, 0xDC, 0xDB, 0xDA, 0xD9, 0xD8,
	0xD7, 0xD6, 0xD5, 0xD4, 0xD3, 0xD2, 0xD1, 0xD0,
};

static uint8_t peer_test_pub_key_psa[] = {
	0x04,

	0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,

	0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
};

static uint8_t peer_test_pub_key_sd[] = {
	0xA7, 0xA6, 0xA5, 0xA4, 0xA3, 0xA2, 0xA1, 0xA0,
	0xAF, 0xAE, 0xAD, 0xAC, 0xAB, 0xAA, 0xA9, 0xA8,
	0x97, 0x96, 0x95, 0x94, 0x93, 0x92, 0x91, 0x90,
	0x9F, 0x9E, 0x9D, 0x9C, 0x9B, 0x9A, 0x99, 0x98,

	0x87, 0x86, 0x85, 0x84, 0x83, 0x82, 0x81, 0x80,
	0x8F, 0x8E, 0x8D, 0x8C, 0x8B, 0x8A, 0x89, 0x88,
	0x77, 0x76, 0x75, 0x74, 0x73, 0x72, 0x71, 0x70,
	0x7F, 0x7E, 0x7D, 0x7C, 0x7B, 0x7A, 0x79, 0x78,
};

static uint8_t test_pub_key_sd_cleared[] = {
	0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA,
	0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA,
	0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA,
	0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA,

	0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA,
	0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA,
	0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA,
	0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA,
};

static uint8_t test_secret_psa[] = {
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00,
	0x0F, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A, 0x69, 0x78,
	0x87, 0x96, 0xA5, 0xB4, 0xC3, 0xD2, 0xE1, 0xF0,
};

static uint8_t test_secret_sd[] = {
	0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87,
	0x78, 0x69, 0x5A, 0x4B, 0x3C, 0x2D, 0x1E, 0x0F,
	0x00, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99,
	0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
};

BUILD_ASSERT(sizeof(own_test_pub_key_psa) == sizeof(uint8_t) + BLE_GAP_LESC_P256_PK_LEN);
BUILD_ASSERT(sizeof(own_test_pub_key_sd) == BLE_GAP_LESC_P256_PK_LEN);
BUILD_ASSERT(sizeof(peer_test_pub_key_psa) == sizeof(uint8_t) + BLE_GAP_LESC_P256_PK_LEN);
BUILD_ASSERT(sizeof(peer_test_pub_key_sd) == BLE_GAP_LESC_P256_PK_LEN);
BUILD_ASSERT(sizeof(test_secret_psa) == BLE_GAP_LESC_DHKEY_LEN);
BUILD_ASSERT(sizeof(test_secret_sd) == BLE_GAP_LESC_DHKEY_LEN);

#define KEY_ATTRS_CHECK(_attributes)                                                               \
	do {                                                                                       \
		if (key_attrs != NULL) {                                                           \
			TEST_ASSERT_EQUAL_PTR(key_attrs, _attributes);                             \
		} else {                                                                           \
			TEST_ASSERT_EQUAL_MEMORY(&key_attrs_expected, _attributes,                 \
						 sizeof(key_attrs_expected));                      \
			key_attrs = _attributes;                                                   \
		}                                                                                  \
	} while (0)

static void stub_psa_set_key_usage_flags(
	psa_key_attributes_t *attributes, psa_key_usage_t usage_flags, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	TEST_ASSERT_NOT_NULL(attributes);
	KEY_ATTRS_CHECK(attributes);

	TEST_ASSERT_EQUAL(PSA_KEY_USAGE_DERIVE, usage_flags);
}

static void stub_psa_set_key_lifetime(
	psa_key_attributes_t *attributes, psa_key_lifetime_t lifetime, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	TEST_ASSERT_NOT_NULL(attributes);
	KEY_ATTRS_CHECK(attributes);

	TEST_ASSERT_EQUAL(PSA_KEY_LIFETIME_VOLATILE, lifetime);
}

static void stub_psa_set_key_algorithm(
	psa_key_attributes_t *attributes, psa_algorithm_t alg, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	TEST_ASSERT_NOT_NULL(attributes);
	KEY_ATTRS_CHECK(attributes);

	TEST_ASSERT_EQUAL(PSA_ALG_ECDH, alg);
}

static void stub_psa_set_key_type(
	psa_key_attributes_t *attributes, psa_key_type_t type, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	TEST_ASSERT_NOT_NULL(attributes);
	KEY_ATTRS_CHECK(attributes);

	TEST_ASSERT_EQUAL(PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1), type);
}


static void stub_psa_set_key_bits(
	psa_key_attributes_t *attributes, size_t bits, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	TEST_ASSERT_NOT_NULL(attributes);
	KEY_ATTRS_CHECK(attributes);

	TEST_ASSERT_EQUAL(256, bits);
}

static psa_status_t stub_psa_generate_key_success(
	const psa_key_attributes_t *attributes, mbedtls_svc_key_id_t *key, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	TEST_ASSERT_NOT_NULL(attributes);
	TEST_ASSERT_EQUAL_PTR(key_attrs, attributes);

	TEST_ASSERT_NOT_NULL(key);
	*key = key_pair_id;

	return PSA_SUCCESS;
}

static psa_status_t stub_psa_generate_key_failure(
	const psa_key_attributes_t *attributes, mbedtls_svc_key_id_t *key, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	TEST_ASSERT_NOT_NULL(attributes);
	TEST_ASSERT_EQUAL_PTR(key_attrs, attributes);

	TEST_ASSERT_NOT_NULL(key);

	return PSA_ERROR_BAD_STATE;
}

static psa_status_t stub_psa_export_public_key_success(mbedtls_svc_key_id_t key,
	uint8_t *data, size_t data_size, size_t *data_length, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	TEST_ASSERT_EQUAL(key_pair_id, key);
	TEST_ASSERT_EQUAL(sizeof(own_test_pub_key_psa), data_size);

	TEST_ASSERT_NOT_NULL(data);
	memcpy(data, own_test_pub_key_psa, sizeof(own_test_pub_key_psa));

	TEST_ASSERT_NOT_NULL(data_length);
	*data_length = sizeof(own_test_pub_key_psa);

	return PSA_SUCCESS;
}

static psa_status_t stub_psa_export_public_key_failure(mbedtls_svc_key_id_t key,
	uint8_t *data, size_t data_size, size_t *data_length, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	TEST_ASSERT_EQUAL(key_pair_id, key);
	TEST_ASSERT_EQUAL(sizeof(own_test_pub_key_psa), data_size);

	TEST_ASSERT_NOT_NULL(data);
	TEST_ASSERT_NOT_NULL(data_length);

	return PSA_ERROR_BAD_STATE;
}

static void reinitialize(void)
{
	uint32_t err;

	/* Stubs have priority. Clear stubs so that Expect functions are used. */
	__cmock_psa_crypto_init_Stub(NULL);
	__cmock_psa_set_key_usage_flags_Stub(NULL);
	__cmock_psa_set_key_lifetime_Stub(NULL);
	__cmock_psa_set_key_algorithm_Stub(NULL);
	__cmock_psa_set_key_type_Stub(NULL);
	__cmock_psa_set_key_bits_Stub(NULL);
	__cmock_psa_destroy_key_Stub(NULL);
	__cmock_psa_generate_key_Stub(NULL);

	/* Set Expect functions. */
	__cmock_psa_crypto_init_ExpectAndReturn(PSA_SUCCESS);
	__cmock_psa_set_key_usage_flags_ExpectAnyArgs();
	__cmock_psa_set_key_lifetime_ExpectAnyArgs();
	__cmock_psa_set_key_algorithm_ExpectAnyArgs();
	__cmock_psa_set_key_type_ExpectAnyArgs();
	__cmock_psa_set_key_bits_ExpectAnyArgs();
	__cmock_psa_destroy_key_ExpectAnyArgsAndReturn(PSA_ERROR_INVALID_HANDLE);
	__cmock_psa_generate_key_ExpectAnyArgsAndReturn(PSA_ERROR_BAD_STATE);

	err = nrf_ble_lesc_init();
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, err);
}

static void generate_key_pair(void)
{
	uint32_t err;

	__cmock_psa_destroy_key_ExpectAnyArgsAndReturn(PSA_ERROR_INVALID_HANDLE);

	__cmock_psa_set_key_usage_flags_Stub(stub_psa_set_key_usage_flags);
	__cmock_psa_set_key_lifetime_Stub(stub_psa_set_key_lifetime);
	__cmock_psa_set_key_algorithm_Stub(stub_psa_set_key_algorithm);
	__cmock_psa_set_key_type_Stub(stub_psa_set_key_type);
	__cmock_psa_set_key_bits_Stub(stub_psa_set_key_bits);

	__cmock_psa_generate_key_Stub(stub_psa_generate_key_success);
	__cmock_psa_export_public_key_Stub(stub_psa_export_public_key_success);

	err = nrf_ble_lesc_keypair_generate();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	/* Stubs have priority. Reset stubs so they are unset when returning from this function. */
	__cmock_psa_set_key_usage_flags_Stub(NULL);
	__cmock_psa_set_key_lifetime_Stub(NULL);
	__cmock_psa_set_key_algorithm_Stub(NULL);
	__cmock_psa_set_key_type_Stub(NULL);
	__cmock_psa_set_key_bits_Stub(NULL);
	__cmock_psa_destroy_key_Stub(NULL);
	__cmock_psa_generate_key_Stub(NULL);
}

void test_nrf_ble_lesc_init_success(void)
{
	uint32_t err;

	__cmock_psa_crypto_init_ExpectAndReturn(PSA_SUCCESS);
	__cmock_psa_destroy_key_ExpectAnyArgsAndReturn(PSA_ERROR_INVALID_HANDLE);

	__cmock_psa_set_key_usage_flags_Stub(stub_psa_set_key_usage_flags);
	__cmock_psa_set_key_lifetime_Stub(stub_psa_set_key_lifetime);
	__cmock_psa_set_key_algorithm_Stub(stub_psa_set_key_algorithm);
	__cmock_psa_set_key_type_Stub(stub_psa_set_key_type);
	__cmock_psa_set_key_bits_Stub(stub_psa_set_key_bits);

	__cmock_psa_generate_key_Stub(stub_psa_generate_key_success);
	__cmock_psa_export_public_key_Stub(stub_psa_export_public_key_success);

	err = nrf_ble_lesc_init();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_nrf_ble_lesc_init_error_internal(void)
{
	uint32_t err;

	__cmock_psa_crypto_init_ExpectAndReturn(PSA_ERROR_BAD_STATE);

	err = nrf_ble_lesc_init();
	TEST_ASSERT_EQUAL_UINT32(NRF_ERROR_INTERNAL, err);
}

void test_nrf_ble_lesc_keypair_generate_and_public_key_get_success(void)
{
	uint32_t err;
	ble_gap_lesc_p256_pk_t *pub_key;

	__cmock_psa_destroy_key_ExpectAnyArgsAndReturn(PSA_SUCCESS);

	__cmock_psa_set_key_usage_flags_Stub(stub_psa_set_key_usage_flags);
	__cmock_psa_set_key_lifetime_Stub(stub_psa_set_key_lifetime);
	__cmock_psa_set_key_algorithm_Stub(stub_psa_set_key_algorithm);
	__cmock_psa_set_key_type_Stub(stub_psa_set_key_type);
	__cmock_psa_set_key_bits_Stub(stub_psa_set_key_bits);

	__cmock_psa_generate_key_Stub(stub_psa_generate_key_success);
	__cmock_psa_export_public_key_Stub(stub_psa_export_public_key_success);

	err = nrf_ble_lesc_keypair_generate();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	pub_key = nrf_ble_lesc_public_key_get();
	TEST_ASSERT_NOT_NULL(pub_key);
	TEST_ASSERT_EQUAL_MEMORY(own_test_pub_key_sd, pub_key->pk, sizeof(own_test_pub_key_sd));
}

void test_nrf_ble_lesc_keypair_generate_and_public_key_get_error_internal(void)
{
	uint32_t err;
	ble_gap_lesc_p256_pk_t *pub_key;

	__cmock_psa_set_key_usage_flags_Stub(stub_psa_set_key_usage_flags);
	__cmock_psa_set_key_lifetime_Stub(stub_psa_set_key_lifetime);
	__cmock_psa_set_key_algorithm_Stub(stub_psa_set_key_algorithm);
	__cmock_psa_set_key_type_Stub(stub_psa_set_key_type);
	__cmock_psa_set_key_bits_Stub(stub_psa_set_key_bits);

	/* Generate key error. */
	__cmock_psa_destroy_key_ExpectAnyArgsAndReturn(PSA_ERROR_INVALID_HANDLE);

	__cmock_psa_generate_key_Stub(stub_psa_generate_key_failure);

	err = nrf_ble_lesc_keypair_generate();
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, err);

	pub_key = nrf_ble_lesc_public_key_get();
	TEST_ASSERT_NULL(pub_key);

	/* Export key error. */
	__cmock_psa_destroy_key_ExpectAnyArgsAndReturn(PSA_ERROR_INVALID_HANDLE);

	__cmock_psa_generate_key_Stub(stub_psa_generate_key_success);
	__cmock_psa_export_public_key_Stub(stub_psa_export_public_key_failure);

	err = nrf_ble_lesc_keypair_generate();
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, err);

	pub_key = nrf_ble_lesc_public_key_get();
	TEST_ASSERT_NULL(pub_key);
}

static uint32_t stub_sd_ble_gap_lesc_oob_data_get_success(
	uint16_t conn_handle, ble_gap_lesc_p256_pk_t const *p_pk_own,
	ble_gap_lesc_oob_data_t *p_oobd_own, int cmock_num_calls)
{
	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, conn_handle);

	TEST_ASSERT_NOT_NULL(p_pk_own);
	TEST_ASSERT_EQUAL_MEMORY(own_test_pub_key_sd, p_pk_own->pk, sizeof(own_test_pub_key_sd));

	TEST_ASSERT_NOT_NULL(p_oobd_own);
	oobd = p_oobd_own;

	return NRF_SUCCESS;
}

void test_nrf_ble_lesc_own_oob_data_generation_and_get_success(void)
{
	uint32_t err;
	ble_gap_lesc_oob_data_t *lesc_oob_data;

	generate_key_pair();

	__cmock_sd_ble_gap_lesc_oob_data_get_Stub(stub_sd_ble_gap_lesc_oob_data_get_success);

	err = nrf_ble_lesc_own_oob_data_generate();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	lesc_oob_data = nrf_ble_lesc_own_oob_data_get();
	TEST_ASSERT_NOT_NULL(lesc_oob_data);
	TEST_ASSERT_EQUAL_PTR(oobd, lesc_oob_data);
}

void test_nrf_ble_lesc_own_oob_data_generate_error_invalid_state(void)
{
	uint32_t err;
	ble_gap_lesc_oob_data_t *lesc_oob_data;

	err = nrf_ble_lesc_own_oob_data_generate();
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, err);

	lesc_oob_data = nrf_ble_lesc_own_oob_data_get();
	TEST_ASSERT_NULL(lesc_oob_data);
}

static psa_status_t stub_psa_raw_key_agreement_success(psa_algorithm_t alg,
	mbedtls_svc_key_id_t private_key, const uint8_t *peer_key, size_t peer_key_length,
	uint8_t *output, size_t output_size, size_t *output_length, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	TEST_ASSERT_EQUAL(PSA_ALG_ECDH, alg);
	TEST_ASSERT_EQUAL(key_pair_id, private_key);

	TEST_ASSERT_NOT_NULL(peer_key);
	TEST_ASSERT_EQUAL(sizeof(peer_test_pub_key_psa), peer_key_length);
	TEST_ASSERT_EQUAL_MEMORY(peer_test_pub_key_psa, peer_key, sizeof(peer_test_pub_key_psa));

	TEST_ASSERT_EQUAL(sizeof(test_secret_psa), output_size);
	TEST_ASSERT_NOT_NULL(output);
	memcpy(output, test_secret_psa, sizeof(test_secret_psa));
	TEST_ASSERT_NOT_NULL(output_length);
	*output_length = sizeof(test_secret_psa);

	return PSA_SUCCESS;
}

static psa_status_t stub_psa_raw_key_agreement_failure(psa_algorithm_t alg,
	mbedtls_svc_key_id_t private_key, const uint8_t *peer_key, size_t peer_key_length,
	uint8_t *output, size_t output_size, size_t *output_length, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	TEST_ASSERT_EQUAL(PSA_ALG_ECDH, alg);
	TEST_ASSERT_EQUAL(key_pair_id, private_key);

	TEST_ASSERT_NOT_NULL(peer_key);
	TEST_ASSERT_EQUAL(sizeof(peer_test_pub_key_psa), peer_key_length);

	TEST_ASSERT_EQUAL(sizeof(test_secret_psa), output_size);
	TEST_ASSERT_NOT_NULL(output);
	TEST_ASSERT_NOT_NULL(output_length);

	return PSA_ERROR_BAD_STATE;
}

static uint32_t callback_sd_ble_gap_lesc_dhkey_reply_success(
	uint16_t conn_handle, ble_gap_lesc_dhkey_t const *p_dhkey, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);
	ARG_UNUSED(conn_handle);

	TEST_ASSERT_NOT_NULL(p_dhkey);
	TEST_ASSERT_EQUAL_MEMORY(test_secret_sd, p_dhkey->key, sizeof(test_secret_sd));

	return NRF_SUCCESS;
}

void test_nrf_ble_lesc_compute_and_give_dhkey_success(void)
{
	uint32_t err;
	const uint16_t conn_handle = 0x32;
	const int peer_pub_key_idx = 1;
	ble_gap_lesc_p256_pk_t peer_lesc_key;
	ble_evt_t evt = {
		.header = {
			.evt_id = BLE_GAP_EVT_LESC_DHKEY_REQUEST,
		},
		.evt.gap_evt = {
			.conn_handle = conn_handle,
			.params.lesc_dhkey_request.p_pk_peer = &peer_lesc_key,
		},
	};
	memcpy(peer_lesc_key.pk, peer_test_pub_key_sd, sizeof(peer_test_pub_key_sd));

	generate_key_pair();

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(conn_handle, peer_pub_key_idx);

	/* Invoke on_dhkey_request(). */
	nrf_ble_lesc_on_ble_evt(&evt);

	__cmock_psa_raw_key_agreement_Stub(stub_psa_raw_key_agreement_success);
	__cmock_sd_ble_gap_lesc_dhkey_reply_ExpectAndReturn(conn_handle, PTR_IGNORE, VAL_IGNORE);
	__cmock_sd_ble_gap_lesc_dhkey_reply_IgnoreArg_p_dhkey();
	__cmock_sd_ble_gap_lesc_dhkey_reply_AddCallback(
		callback_sd_ble_gap_lesc_dhkey_reply_success);

	/* Invoke compute_and_give_dhkey(). */
	err = nrf_ble_lesc_request_handler();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_nrf_ble_lesc_compute_and_give_dhkey_without_keypair_generation(void)
{
	uint32_t err;
	const uint16_t conn_handle = 0x32;
	const int peer_pub_key_idx = 1;
	ble_gap_lesc_p256_pk_t peer_lesc_key;
	ble_evt_t evt = {
		.header = {
			.evt_id = BLE_GAP_EVT_LESC_DHKEY_REQUEST,
		},
		.evt.gap_evt = {
			.conn_handle = conn_handle,
			.params.lesc_dhkey_request.p_pk_peer = &peer_lesc_key,
		},
	};
	memcpy(peer_lesc_key.pk, peer_test_pub_key_sd, sizeof(peer_test_pub_key_sd));

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(conn_handle, peer_pub_key_idx);

	/* Invoke on_dhkey_request(). */
	nrf_ble_lesc_on_ble_evt(&evt);

	/* Invoke compute_and_give_dhkey(). */
	err = nrf_ble_lesc_request_handler();
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, err);
}

static psa_status_t stub_psa_generate_random_success(
	uint8_t *output, size_t output_size, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	TEST_ASSERT_EQUAL(sizeof(test_secret_sd), output_size);
	TEST_ASSERT_NOT_NULL(output);
	memcpy(output, test_secret_sd, sizeof(test_secret_sd));

	return PSA_SUCCESS;
}

void test_nrf_ble_lesc_compute_and_give_dhkey_with_invalid_peer_key(void)
{
	uint32_t err;
	const uint16_t conn_handle = 0x32;
	const int peer_pub_key_idx = 1;
	ble_gap_lesc_p256_pk_t peer_lesc_key;
	ble_evt_t evt = {
		.header = {
			.evt_id = BLE_GAP_EVT_LESC_DHKEY_REQUEST,
		},
		.evt.gap_evt = {
			.conn_handle = conn_handle,
			.params.lesc_dhkey_request.p_pk_peer = &peer_lesc_key,
		},
	};
	memcpy(peer_lesc_key.pk, peer_test_pub_key_sd, sizeof(peer_test_pub_key_sd));

	generate_key_pair();

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(conn_handle, peer_pub_key_idx);

	/* Invoke on_dhkey_request(). */
	nrf_ble_lesc_on_ble_evt(&evt);

	__cmock_psa_raw_key_agreement_Stub(stub_psa_raw_key_agreement_failure);
	__cmock_psa_generate_random_Stub(stub_psa_generate_random_success);

	__cmock_sd_ble_gap_lesc_dhkey_reply_ExpectAndReturn(conn_handle, PTR_IGNORE, VAL_IGNORE);
	__cmock_sd_ble_gap_lesc_dhkey_reply_IgnoreArg_p_dhkey();
	__cmock_sd_ble_gap_lesc_dhkey_reply_AddCallback(
		callback_sd_ble_gap_lesc_dhkey_reply_success);

	/* Invoke compute_and_give_dhkey(). */
	err = nrf_ble_lesc_request_handler();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_nrf_ble_lesc_compute_and_give_dhkey_with_oob_data_own(void)
{
	uint32_t err;
	const uint16_t conn_handle = 0x32;
	const int peer_pub_key_idx = 1;
	ble_gap_lesc_oob_data_t *lesc_oob_data;
	ble_gap_lesc_p256_pk_t peer_lesc_key;
	ble_evt_t evt = {
		.header = {
			.evt_id = BLE_GAP_EVT_LESC_DHKEY_REQUEST,
		},
		.evt.gap_evt = {
			.conn_handle = conn_handle,
			.params.lesc_dhkey_request.p_pk_peer = &peer_lesc_key,
			.params.lesc_dhkey_request.oobd_req = 1,
		},
	};
	memcpy(peer_lesc_key.pk, peer_test_pub_key_sd, sizeof(peer_test_pub_key_sd));

	generate_key_pair();

	__cmock_sd_ble_gap_lesc_oob_data_get_Stub(stub_sd_ble_gap_lesc_oob_data_get_success);

	/* Prepare own OOB data. */
	err = nrf_ble_lesc_own_oob_data_generate();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	lesc_oob_data = nrf_ble_lesc_own_oob_data_get();
	TEST_ASSERT_NOT_NULL(lesc_oob_data);
	TEST_ASSERT_EQUAL_PTR(oobd, lesc_oob_data);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(conn_handle, peer_pub_key_idx);
	__cmock_sd_ble_gap_lesc_oob_data_set_ExpectAndReturn(conn_handle, lesc_oob_data, NULL,
							     NRF_SUCCESS);

	/* Invoke on_dhkey_request(). */
	nrf_ble_lesc_on_ble_evt(&evt);

	__cmock_psa_raw_key_agreement_Stub(stub_psa_raw_key_agreement_success);
	__cmock_sd_ble_gap_lesc_dhkey_reply_ExpectAndReturn(conn_handle, PTR_IGNORE, VAL_IGNORE);
	__cmock_sd_ble_gap_lesc_dhkey_reply_IgnoreArg_p_dhkey();
	__cmock_sd_ble_gap_lesc_dhkey_reply_AddCallback(
		callback_sd_ble_gap_lesc_dhkey_reply_success);

	/* Invoke compute_and_give_dhkey(). */
	err = nrf_ble_lesc_request_handler();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

static ble_gap_lesc_oob_data_t nfc_oob_data;

static ble_gap_lesc_oob_data_t *nfc_peer_oob_data_get(uint16_t conn_handle)
{
	ARG_UNUSED(conn_handle);

	return &nfc_oob_data;
}

void test_nrf_ble_lesc_compute_and_give_dhkey_with_oob_data_peer(void)
{
	uint32_t err;
	const uint16_t conn_handle = 0x32;
	const int peer_pub_key_idx = 1;
	ble_gap_lesc_p256_pk_t peer_lesc_key;
	ble_evt_t evt = {
		.header = {
			.evt_id = BLE_GAP_EVT_LESC_DHKEY_REQUEST,
		},
		.evt.gap_evt = {
			.conn_handle = conn_handle,
			.params.lesc_dhkey_request.p_pk_peer = &peer_lesc_key,
			.params.lesc_dhkey_request.oobd_req = 1,
		},
	};
	memcpy(peer_lesc_key.pk, peer_test_pub_key_sd, sizeof(peer_test_pub_key_sd));

	generate_key_pair();

	nrf_ble_lesc_peer_oob_data_handler_set(nfc_peer_oob_data_get);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(conn_handle, peer_pub_key_idx);
	__cmock_sd_ble_gap_lesc_oob_data_set_ExpectAndReturn(conn_handle, NULL, &nfc_oob_data,
							     NRF_SUCCESS);

	/* Invoke on_dhkey_request(). */
	nrf_ble_lesc_on_ble_evt(&evt);

	__cmock_psa_raw_key_agreement_Stub(stub_psa_raw_key_agreement_success);
	__cmock_sd_ble_gap_lesc_dhkey_reply_ExpectAndReturn(conn_handle, PTR_IGNORE, VAL_IGNORE);
	__cmock_sd_ble_gap_lesc_dhkey_reply_IgnoreArg_p_dhkey();
	__cmock_sd_ble_gap_lesc_dhkey_reply_AddCallback(
		callback_sd_ble_gap_lesc_dhkey_reply_success);

	/* Invoke compute_and_give_dhkey(). */
	err = nrf_ble_lesc_request_handler();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_nrf_ble_lesc_compute_and_give_dhkey_with_oob_data_fail(void)
{
	uint32_t err;
	const uint16_t conn_handle = 0x32;
	const int peer_pub_key_idx = 1;
	ble_gap_lesc_p256_pk_t peer_lesc_key;
	ble_evt_t evt = {
		.header = {
			.evt_id = BLE_GAP_EVT_LESC_DHKEY_REQUEST,
		},
		.evt.gap_evt = {
			.conn_handle = conn_handle,
			.params.lesc_dhkey_request.p_pk_peer = &peer_lesc_key,
			.params.lesc_dhkey_request.oobd_req = 1,
		},
	};
	memcpy(peer_lesc_key.pk, peer_test_pub_key_sd, sizeof(peer_test_pub_key_sd));

	generate_key_pair();

	nrf_ble_lesc_peer_oob_data_handler_set(nfc_peer_oob_data_get);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(conn_handle, peer_pub_key_idx);
	__cmock_sd_ble_gap_lesc_oob_data_set_ExpectAndReturn(conn_handle, NULL, &nfc_oob_data,
							     NRF_ERROR_INVALID_STATE);

	/* Invoke on_dhkey_request(). */
	nrf_ble_lesc_on_ble_evt(&evt);

	err = nrf_ble_lesc_request_handler();
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, err);
}

void test_nrf_ble_lesc_compute_and_give_dhkey_with_disconnect(void)
{
	uint32_t err;
	const uint16_t conn_handle = 0x32;
	const int peer_pub_key_idx = 1;
	ble_gap_lesc_p256_pk_t peer_lesc_key;
	ble_evt_t evt = {
		.header = {
			.evt_id = BLE_GAP_EVT_LESC_DHKEY_REQUEST,
		},
		.evt.gap_evt = {
			.conn_handle = conn_handle,
			.params.lesc_dhkey_request.p_pk_peer = &peer_lesc_key,
		},
	};
	memcpy(peer_lesc_key.pk, peer_test_pub_key_sd, sizeof(peer_test_pub_key_sd));

	generate_key_pair();

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(conn_handle, peer_pub_key_idx);

	/* Invoke on_dhkey_request(). */
	nrf_ble_lesc_on_ble_evt(&evt);

	/* Disconnect before trying to invoke compute_and_give_dhkey(). */
	const ble_evt_t evt_disconnect = {
		.header = {
			.evt_id = BLE_GAP_EVT_DISCONNECTED,
		},
		.evt.gap_evt.conn_handle = conn_handle,
	};

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(conn_handle, peer_pub_key_idx);

	nrf_ble_lesc_on_ble_evt(&evt_disconnect);

	/* Try to invoke compute_and_give_dhkey(), but nothing to do. */
	err = nrf_ble_lesc_request_handler();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void tearDown(void)
{
	key_attrs = NULL;
	oobd = NULL;

	/* Clear the internal exported public key. */
	ble_gap_lesc_p256_pk_t *pub_key = nrf_ble_lesc_public_key_get();

	if (pub_key != NULL) {
		memcpy(pub_key->pk, test_pub_key_sd_cleared, sizeof(test_pub_key_sd_cleared));
	}

	/* Reset generated key state. */
	reinitialize();
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
