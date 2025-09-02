/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef PSA_CRYPTO_FOR_MOCKING_H__
#define PSA_CRYPTO_FOR_MOCKING_H__

#include <stddef.h>
#include <stdint.h>

/* Header for mocking the required psa/crypto.h functionality with CMock.
 * For use in nrf_ble_lesc unit tests.
 */

typedef uint32_t psa_key_id_t;
typedef psa_key_id_t mbedtls_svc_key_id_t;
typedef int32_t psa_status_t;
typedef uint16_t psa_key_type_t;

typedef int psa_key_attributes_t;
typedef uint32_t psa_key_usage_t;
typedef uint32_t psa_key_lifetime_t;
typedef uint32_t psa_algorithm_t;
typedef uint8_t psa_ecc_family_t;

#define PSA_KEY_ATTRIBUTES_INIT 0x00C0FFEE

#define PSA_EXPORT_PUBLIC_KEY_OUTPUT_SIZE(key_type, key_bits) (65)
#define PSA_KEY_TYPE_ECC_KEY_PAIR(curve) 0
#define PSA_ECC_FAMILY_SECP_R1 ((psa_ecc_family_t)0x12)

#define PSA_KEY_USAGE_DERIVE ((psa_key_usage_t)0x00004000)
#define PSA_KEY_LIFETIME_VOLATILE ((psa_key_lifetime_t)0x00000000)
#define PSA_ALG_ECDH ((psa_algorithm_t)0x09020000)

#define PSA_SUCCESS ((psa_status_t)0)
#define PSA_ERROR_INVALID_HANDLE ((psa_status_t)-136)
#define PSA_ERROR_BAD_STATE ((psa_status_t)-137)

psa_status_t psa_crypto_init(void);
psa_status_t psa_destroy_key(mbedtls_svc_key_id_t key);
void psa_set_key_usage_flags(psa_key_attributes_t *attributes, psa_key_usage_t usage_flags);
void psa_set_key_lifetime(psa_key_attributes_t *attributes, psa_key_lifetime_t lifetime);
void psa_set_key_algorithm(psa_key_attributes_t *attributes, psa_algorithm_t alg);
void psa_set_key_type(psa_key_attributes_t *attributes, psa_key_type_t type);
void psa_set_key_bits(psa_key_attributes_t *attributes, size_t bits);
psa_status_t psa_generate_key(const psa_key_attributes_t *attributes, mbedtls_svc_key_id_t *key);
psa_status_t psa_export_public_key(mbedtls_svc_key_id_t key, uint8_t *data, size_t data_size,
				   size_t *data_length);
psa_status_t psa_raw_key_agreement(psa_algorithm_t alg, mbedtls_svc_key_id_t private_key,
				   const uint8_t *peer_key, size_t peer_key_length,
				   uint8_t *output, size_t output_size, size_t *output_length);
psa_status_t psa_generate_random(uint8_t *output, size_t output_size);

#endif /* PSA_CRYPTO_FOR_MOCKING_H__ */
