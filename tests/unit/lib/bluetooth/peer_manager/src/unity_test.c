/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <unity.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <bm/bluetooth/peer_manager/peer_manager.h>
#include <bm/bluetooth/peer_manager/nrf_ble_lesc.h>
#include <zephyr/sys/util.h>

#include "cmock_ble_gap.h"
#include "cmock_ble_gatts.h"
#include "cmock_bm_timer.h"
#include "cmock_bm_zms.h"
#include "cmock_crypto.h"
#include "cmock_nrf_sdh_ble.h"
#include "cmock_nrf_soc.h"

#include <sdh_evt_dispatch.h>

#define PTR_IGNORE NULL
#define VAL_IGNORE 0
#define RET_IGNORE 0

#ifndef ARG_UNUSED
#define ARG_UNUSED(arg) (void)(arg)
#endif

#define PM_PEER_DATA_MAX_SIZE 128

#define CONN_HANDLE_1 (uint16_t)(0x0003)

#define ADDRESS_PUBLIC_1                                                                           \
	(ble_gap_addr_t) {                                                                         \
		.addr_id_peer = 0, .addr_type = BLE_GAP_ADDR_TYPE_PUBLIC,                          \
		.addr = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},                                      \
	}

#define ADDRESS_PUBLIC_2                                                                           \
	(ble_gap_addr_t) {                                                                         \
		.addr_id_peer = 0, .addr_type = BLE_GAP_ADDR_TYPE_PUBLIC,                          \
		.addr = {0xAA, 0xBB, 0x33, 0x44, 0x55, 0x66},                                      \
	}

#define ADDRESS_RANDOM_PRIVATE                                                                     \
	(ble_gap_addr_t) {                                                                         \
		.addr_id_peer = 0,                                                                 \
		.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE,                          \
		.addr = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6},                                      \
	}

#define ADDRESS_RANDOM_STATIC                                                                      \
	(ble_gap_addr_t) {                                                                         \
		.addr_id_peer = 0,                                                                 \
		.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC,                                      \
		.addr = {0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6},                                      \
	}

#define LTK_1                                                                                      \
	(ble_gap_enc_key_t) {                                                                      \
		.enc_info = {                                                                      \
			.ltk = {0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,                    \
				0x58, 0x59, 0x60, 0x61},                                           \
			.lesc = 1, .auth = 1, .ltk_len = 12,                                       \
		},                                                                                 \
		.master_id = {                                                                     \
			.rand = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x48, 0x65, 0x69},                  \
			.ediv = 0x96,                                                              \
		},                                                                                 \
	}

/* Helper union to allocate 'struct pm_peer_data_local_gatt_db' with memory for the
 * flexible array member 'data' at the end of 'struct pm_peer_data_local_gatt_db'.
 */
union local_gatt_db_with_data {
	struct {
		uint8_t header[offsetof(struct pm_peer_data_local_gatt_db, data)];
		uint8_t payload[32];
	};
	struct pm_peer_data_local_gatt_db data;
};

#define STORED_GATT_DATA_1                                                                         \
	(union local_gatt_db_with_data) {                                                          \
		.data = {.flags = 0, .len = 13},                                                   \
		.payload = {0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,                        \
			    0x99, 0x9A, 0x9B, 0x9C, 0x9D},                                         \
	}

/* Helper union for converting between storage entry ID and peer ID + data ID. */
union pm_entry_id {
	struct {
		uint32_t data_id : 16;
		uint32_t peer_id : 16;
	};
	uint32_t id;
};

uint8_t bonding_data_A[PM_PEER_DATA_MAX_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

static int stub_nrf_sdh_ble_idx_get(uint16_t conn_handle, int cmock_num_calls)
{
	int idx = -1;

	ARG_UNUSED(cmock_num_calls);

	switch (conn_handle) {
	case CONN_HANDLE_1:
		idx = 0;
		break;
	default:
		TEST_FAIL();
	}

	return idx;
}

static uint16_t stub_nrf_sdh_ble_conn_handle_get(int idx, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	switch (idx) {
	case 0:
		return CONN_HANDLE_1;
	default:
		return BLE_CONN_HANDLE_INVALID;
	}
}

/* Hold reference to bm_zms instance to check that all bm_zms API calls use the same instance. */
static struct bm_zms_fs *zms_fs;
static bm_zms_evt_handler_t zms_handler;

/* Mock for storage API used by peer manager. */
const struct bm_storage_api bm_storage_sd_api = {0};

static int stub_bm_zms_mount_success(struct bm_zms_fs *fs, const struct bm_zms_fs_config *config,
				     int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);
	TEST_ASSERT_NOT_NULL(fs);

	TEST_ASSERT_NOT_NULL(config);
	TEST_ASSERT_EQUAL(DT_REG_ADDR(DT_NODELABEL(peer_manager_partition)), config->offset);
	TEST_ASSERT_EQUAL(CONFIG_PM_BM_ZMS_SECTOR_SIZE, config->sector_size);
	TEST_ASSERT_EQUAL(
		DT_REG_SIZE(DT_NODELABEL(peer_manager_partition)) / CONFIG_PM_BM_ZMS_SECTOR_SIZE,
		config->sector_count);
	TEST_ASSERT_NOT_NULL(config->evt_handler);
	TEST_ASSERT_EQUAL_PTR(&bm_storage_sd_api, config->storage_api);

	/* Store address of bm_zms file system structure so it can be checked against the
	 * file system structure of later calls to bm_zms.
	 */
	zms_fs = fs;

	/* Store address of the handler function so it can be invoked in later tests. */
	zms_handler = config->evt_handler;

	/* Signal that the fs is initialized. This is usually done asynchronously by bm_zms,
	 * but this will not happen when mocking the bm_zms API, so signal it here.
	 */
	fs->init_flags.initialized = true;

	return 0;
}

static ssize_t stub_bm_zms_read_pm_init(struct bm_zms_fs *fs, uint32_t id, void *data, size_t len,
					int cmock_num_calls)
{
	/* On pm_init, all n = PM_PEER_ID_N_AVAILABLE_IDS 'peer bonding data' entries
	 * are read through, looking for existing bonding data.
	 */

	int ret;
	union pm_entry_id entry = {.id = id};

	TEST_ASSERT_NOT_NULL(fs);
	TEST_ASSERT_EQUAL_PTR(zms_fs, fs);

	if (cmock_num_calls < PM_PEER_ID_N_AVAILABLE_IDS) {
		entry.peer_id = cmock_num_calls;

		TEST_ASSERT_EQUAL(cmock_num_calls, entry.peer_id);
		TEST_ASSERT_EQUAL(PM_PEER_DATA_ID_BONDING, entry.data_id);
		TEST_ASSERT_NOT_NULL(data);
		TEST_ASSERT_EQUAL(PM_PEER_DATA_MAX_SIZE, len);

		/* Return -ENOENT to signal that no data exists for this entry_id. */
		ret = -ENOENT;
	} else {
		TEST_FAIL();
	}

	return ret;
}

#define GEN_MATERIAL(i, start, step) ((start) + (step) * (i))

static uint8_t own_test_pub_key_psa[] = {
	0x04,
	LISTIFY(32, GEN_MATERIAL, (,), 0xB0, 1),
	LISTIFY(32, GEN_MATERIAL, (,), 0xD0, 1),
};
static uint8_t own_test_pub_key_sd[] = {
	LISTIFY(32, GEN_MATERIAL, (,), 0xCF, -1),
	LISTIFY(32, GEN_MATERIAL, (,), 0xEF, -1),
};

static uint8_t peer_test_pub_key_psa[] = {
	0x04,
	LISTIFY(32, GEN_MATERIAL, (,), 0x60, 1),
	LISTIFY(32, GEN_MATERIAL, (,), 0x80, 1),
};
static uint8_t peer_test_pub_key_sd[] = {
	LISTIFY(32, GEN_MATERIAL, (,), 0x7F, -1),
	LISTIFY(32, GEN_MATERIAL, (,), 0x9F, -1),
};

static uint8_t test_secret_psa[] = {
	LISTIFY(32, GEN_MATERIAL, (,), 0x20, 1),
};
static uint8_t test_secret_sd[] = {
	LISTIFY(32, GEN_MATERIAL, (,), 0x3F, -1),
};

BUILD_ASSERT(sizeof(own_test_pub_key_sd) == BLE_GAP_LESC_P256_PK_LEN);
BUILD_ASSERT(sizeof(peer_test_pub_key_sd) == BLE_GAP_LESC_P256_PK_LEN);
BUILD_ASSERT(sizeof(test_secret_sd) == BLE_GAP_LESC_DHKEY_LEN);

static const mbedtls_svc_key_id_t key_pair_id = 0x2A;

#define PSA_GENERATE_ECDH_KEYS_AND_EXPORT_PUBLIC_KEY_MOCKS(_key_pair_id)                           \
	psa_key_attributes_t key_attributes_mock = PSA_KEY_ATTRIBUTES_INIT;                        \
												   \
	__cmock_psa_crypto_init_ExpectAndReturn(PSA_SUCCESS);                                      \
	__cmock_psa_destroy_key_ExpectAnyArgsAndReturn(PSA_ERROR_INVALID_HANDLE);                  \
												   \
	__cmock_psa_set_key_usage_flags_ExpectWithArray(                                           \
		&(psa_key_attributes_t){key_attributes_mock}, 1, PSA_KEY_USAGE_DERIVE);            \
	__cmock_psa_set_key_usage_flags_ReturnMemThruPtr_attributes(                               \
		&(psa_key_attributes_t){(++key_attributes_mock)}, sizeof(key_attributes_mock));    \
												   \
	__cmock_psa_set_key_lifetime_ExpectWithArray(                                              \
		&(psa_key_attributes_t){key_attributes_mock}, 1, PSA_KEY_LIFETIME_VOLATILE);       \
	__cmock_psa_set_key_lifetime_ReturnMemThruPtr_attributes(                                  \
		&(psa_key_attributes_t){(++key_attributes_mock)}, sizeof(key_attributes_mock));    \
												   \
	__cmock_psa_set_key_algorithm_ExpectWithArray(                                             \
		&(psa_key_attributes_t){key_attributes_mock}, 1, PSA_ALG_ECDH);                    \
	__cmock_psa_set_key_algorithm_ReturnMemThruPtr_attributes(                                 \
		&(psa_key_attributes_t){(++key_attributes_mock)}, sizeof(key_attributes_mock));    \
												   \
	__cmock_psa_set_key_type_ExpectWithArray(                                                  \
		&(psa_key_attributes_t){key_attributes_mock}, 1,                                   \
		PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));                                \
	__cmock_psa_set_key_type_ReturnMemThruPtr_attributes(                                      \
		&(psa_key_attributes_t){(++key_attributes_mock)}, sizeof(key_attributes_mock));    \
												   \
	__cmock_psa_set_key_bits_ExpectWithArray(                                                  \
		&(psa_key_attributes_t){key_attributes_mock}, 1, 256);                             \
	__cmock_psa_set_key_bits_ReturnMemThruPtr_attributes(                                      \
		&(psa_key_attributes_t){(++key_attributes_mock)}, sizeof(key_attributes_mock));    \
												   \
	__cmock_psa_generate_key_ExpectWithArrayAndReturn(                                         \
		&(psa_key_attributes_t){key_attributes_mock}, 1, PTR_IGNORE, 0, PSA_SUCCESS);      \
	__cmock_psa_generate_key_IgnoreArg_key();                                                  \
	__cmock_psa_generate_key_ReturnMemThruPtr_key(                                             \
		(mbedtls_svc_key_id_t *)(&(_key_pair_id)), sizeof(_key_pair_id));                  \
												   \
	__cmock_psa_export_public_key_ExpectAndReturn(                                             \
		(_key_pair_id), PTR_IGNORE, sizeof(own_test_pub_key_psa), PTR_IGNORE, PSA_SUCCESS);\
	__cmock_psa_export_public_key_IgnoreArg_data();                                            \
	__cmock_psa_export_public_key_IgnoreArg_data_length();                                     \
	__cmock_psa_export_public_key_ReturnMemThruPtr_data(                                       \
		own_test_pub_key_psa, sizeof(own_test_pub_key_psa));                               \
	__cmock_psa_export_public_key_ReturnMemThruPtr_data_length(                                \
		&(size_t){sizeof(own_test_pub_key_psa)}, sizeof(size_t))

/* Hold reference to Authorization Status Tracker timer instance to be able to check that all
 * auth_status_tracker calls to bm_timer API use the same instance.
 */
static struct bm_timer *ast_timer;
static bm_timer_timeout_handler_t blacklisted_peers_update_handler;

int stub_bm_timer_init_ast(struct bm_timer *timer, enum bm_timer_mode mode,
			   bm_timer_timeout_handler_t timeout_handler, int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	TEST_ASSERT_NOT_NULL(timer);
	TEST_ASSERT_EQUAL(BM_TIMER_MODE_SINGLE_SHOT, mode);
	TEST_ASSERT_NOT_NULL(timeout_handler);

	/* Store address of bm_timer structure so it can be checked against the
	 * bm_timer structure of later auth status tracker related calls to bm_timer.
	 */
	ast_timer = timer;

	/* Store address of the handler function so it can be invoked in later tests. */
	blacklisted_peers_update_handler = timeout_handler;

	return 0;
}

static void invoke_zms_write_success(uint32_t entry_id)
{
	struct bm_zms_evt evt = {
		.evt_type = BM_ZMS_EVT_WRITE,
		.id = entry_id,
		.result = 0,
	};

	TEST_ASSERT_NOT_NULL(zms_handler);
	zms_handler(&evt);
}

static void invoke_zms_delete_success(uint32_t entry_id)
{
	struct bm_zms_evt evt = {
		.evt_type = BM_ZMS_EVT_DELETE,
		.id = entry_id,
		.result = 0,
	};

	TEST_ASSERT_NOT_NULL(zms_handler);
	zms_handler(&evt);
}

static int stub_bm_zms_delete_invoke_handler(struct bm_zms_fs *fs, uint32_t id, int cmock_num_calls)
{
	ARG_UNUSED(fs);
	ARG_UNUSED(cmock_num_calls);

	invoke_zms_delete_success(id);

	return 0;
}

/* Mirrors security_manager.c sec_params_reply_context for pm_conn_sec_params_reply(). */
struct test_sec_params_reply_context {
	ble_gap_sec_params_t *sec_params;
	ble_gap_sec_params_t sec_params_mem;
	bool params_reply_called;
};

static uint32_t stub_sd_ecb_block_encrypt_match_prand(nrf_ecb_hal_data_t *p_ecb_data,
						      int cmock_num_calls)
{
	ARG_UNUSED(cmock_num_calls);

	/* Echo cleartext prand bytes into ciphertext so pm_address_resolve() succeeds. */
	for (uint32_t i = 0; i < 3; i++) {
		p_ecb_data->ciphertext[SOC_ECB_KEY_LENGTH - 1 - i] =
			p_ecb_data->cleartext[SOC_ECB_KEY_LENGTH - 1 - i];
	}

	return NRF_SUCCESS;
}

void peer_manager_initialize_success(void)
{
	uint32_t nrf_err;

	__cmock_bm_zms_mount_Stub(stub_bm_zms_mount_success);
	__cmock_bm_zms_read_Stub(stub_bm_zms_read_pm_init);

	PSA_GENERATE_ECDH_KEYS_AND_EXPORT_PUBLIC_KEY_MOCKS(key_pair_id);

	__cmock_bm_timer_init_Stub(stub_bm_timer_init_ast);

	nrf_err = pm_init();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Stubs have priority. Clear stubs so that Expect functions can be used from now on. */
	__cmock_bm_zms_mount_Stub(NULL);
	__cmock_bm_zms_read_Stub(NULL);

	__cmock_bm_timer_init_Stub(NULL);
}

#define PM_EVT_CAPTURE_MAX 16

/* Helper context for modifying behavior of the on_pm_evt handler function. */
static struct {
	struct {
		bool exclude_connection;
	} conn_config_req;
	struct {
		bool allow_repairing;
	} conn_sec_config_req;
	struct {
		bool enabled;
		uint32_t count;
		struct pm_evt events[PM_EVT_CAPTURE_MAX];
	} capture;
} on_pm_evt_test_ctx;

static void on_pm_evt_capture_restart(void)
{
	on_pm_evt_test_ctx.capture.count = 0;
	on_pm_evt_test_ctx.capture.enabled = true;
}

static const struct pm_evt *on_pm_evt_find_last(enum pm_evt_id evt_id)
{
	const struct pm_evt *found = NULL;

	for (uint32_t i = 0; i < on_pm_evt_test_ctx.capture.count; i++) {
		if (on_pm_evt_test_ctx.capture.events[i].evt_id == evt_id) {
			found = &on_pm_evt_test_ctx.capture.events[i];
		}
	}

	return found;
}

static void on_pm_evt(const struct pm_evt *pm_evt)
{
	uint32_t nrf_err;

	TEST_ASSERT_NOT_NULL(pm_evt);

	switch (pm_evt->evt_id) {
	case PM_EVT_CONN_CONFIG_REQ:
		if (on_pm_evt_test_ctx.conn_config_req.exclude_connection) {
			nrf_err = pm_conn_exclude(pm_evt->conn_handle,
						  pm_evt->conn_config_req.context);
			TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
		}
		break;
	case PM_EVT_CONN_SEC_CONFIG_REQ:
		if (on_pm_evt_test_ctx.conn_sec_config_req.allow_repairing) {
			struct pm_conn_sec_config sec_config = {.allow_repairing = true};

			pm_conn_sec_config_reply(pm_evt->conn_handle, &sec_config);
		}
		break;
	default:
		break;
	}

	if (on_pm_evt_test_ctx.capture.enabled &&
	    (on_pm_evt_test_ctx.capture.count < PM_EVT_CAPTURE_MAX)) {
		on_pm_evt_test_ctx.capture.events[on_pm_evt_test_ctx.capture.count++] = *pm_evt;
	}
}

/* Runs before test_pm_init_* (name sort order) while Peer Manager is still
 * uninitialized. Checks that public APIs return NRF_ERROR_INVALID_STATE, or
 * the documented special-case values, when called before pm_init().
 */
void test_pm_0_module_not_initialized(void)
{
	uint32_t nrf_err;
	uint16_t peer_id;
	uint16_t conn_handle;
	uint16_t next_peer_id;
	uint16_t peer_list[1];
	uint32_t list_size = 1;
	uint32_t peer_count;
	uint32_t data_len = sizeof(uint32_t);
	uint8_t data[4] __aligned(4);
	ble_gap_addr_t addr;
	ble_gap_sec_params_t sec_param;
	struct pm_conn_sec_status sec_status;
	struct pm_peer_data_bonding bonding_data;
	struct ble_gatt_db_srv remote_db __aligned(4);
	ble_gap_privacy_params_t privacy;
	ble_gap_irk_t irk;
	bool excluded;
	bool address_resolved;
	bool sec_sufficient;

	nrf_err = pm_register(on_pm_evt);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_conn_secure(CONN_HANDLE_1, false);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_conn_sec_params_reply(CONN_HANDLE_1, &sec_param, &excluded);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_id_addr_set(&addr);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_id_addr_get(&addr);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_privacy_set(&privacy);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_privacy_get(&privacy);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_allow_list_set(NULL, 0);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_allow_list_get(&addr, &list_size, NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_device_identities_list_set(NULL, 0);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_conn_sec_status_get(CONN_HANDLE_1, &sec_status);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_lesc_public_key_set(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	sec_sufficient = pm_sec_is_sufficient(CONN_HANDLE_1, &sec_status);
	TEST_ASSERT_FALSE(sec_sufficient);

	address_resolved = pm_address_resolve(&addr, &irk);
	TEST_ASSERT_FALSE(address_resolved);

	nrf_err = pm_conn_handle_get(0, &conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_peer_id_get(CONN_HANDLE_1, &peer_id);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_ALL_ID);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	peer_count = pm_peer_count();
	TEST_ASSERT_EQUAL(0, peer_count);

	next_peer_id = pm_next_peer_id_get(PM_PEER_ID_INVALID);
	TEST_ASSERT_EQUAL(PM_PEER_ID_INVALID, next_peer_id);

	nrf_err = pm_peer_data_load(0, PM_PEER_DATA_ID_APPLICATION, data, &data_len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_peer_data_bonding_load(0, &bonding_data);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_peer_data_remote_db_load(0, &remote_db, &data_len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_peer_data_app_data_load(0, data, &data_len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_peer_data_store(0, PM_PEER_DATA_ID_APPLICATION, data, data_len, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_peer_data_bonding_store(0, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_peer_data_remote_db_store(0, &remote_db, sizeof(remote_db), NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_peer_data_app_data_store(0, data, sizeof(data), NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_peer_data_delete(0, PM_PEER_DATA_ID_APPLICATION);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_peer_new(&peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_peer_delete(0);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_peers_delete();
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

#if defined(CONFIG_PM_PEER_RANKS)
	nrf_err = pm_peer_ranks_get(&peer_id, &data_len, NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = pm_peer_rank_highest(0);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
#else
	nrf_err = pm_peer_ranks_get(&peer_id, &data_len, NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_SUPPORTED, nrf_err);

	nrf_err = pm_peer_rank_highest(0);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_SUPPORTED, nrf_err);
#endif

	/* No-op when not initialized; must not crash. */
	pm_local_database_has_changed();
}

void test_pm_init_success(void)
{
	uint32_t nrf_err;

	__cmock_bm_zms_mount_Stub(stub_bm_zms_mount_success);
	__cmock_bm_zms_read_Stub(stub_bm_zms_read_pm_init);

	PSA_GENERATE_ECDH_KEYS_AND_EXPORT_PUBLIC_KEY_MOCKS(key_pair_id);

	__cmock_bm_timer_init_Stub(stub_bm_timer_init_ast);

	nrf_err = pm_init();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_pm_init_zms_error(void)
{
	uint32_t nrf_err;

	__cmock_bm_zms_mount_ExpectAnyArgsAndReturn(-EINVAL);

	nrf_err = pm_init();
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);
}

void test_pm_init_lesc_error(void)
{
	uint32_t nrf_err;

	__cmock_bm_zms_mount_Stub(stub_bm_zms_mount_success);
	__cmock_bm_zms_read_IgnoreAndReturn(-ENOENT);

	__cmock_psa_crypto_init_ExpectAndReturn(PSA_ERROR_BAD_STATE);

	nrf_err = pm_init();
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);

	__cmock_bm_zms_mount_ExpectAnyArgsAndReturn(0);
	__cmock_bm_zms_read_IgnoreAndReturn(-ENOENT);

	__cmock_psa_crypto_init_ExpectAndReturn(PSA_SUCCESS);
	__cmock_psa_destroy_key_ExpectAnyArgsAndReturn(PSA_ERROR_INVALID_HANDLE);
	__cmock_psa_set_key_usage_flags_ExpectAnyArgs();
	__cmock_psa_set_key_lifetime_ExpectAnyArgs();
	__cmock_psa_set_key_algorithm_ExpectAnyArgs();
	__cmock_psa_set_key_type_ExpectAnyArgs();
	__cmock_psa_set_key_bits_ExpectAnyArgs();
	__cmock_psa_generate_key_ExpectAnyArgsAndReturn(PSA_ERROR_BAD_STATE);

	nrf_err = pm_init();
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);

	__cmock_bm_zms_mount_ExpectAnyArgsAndReturn(0);
	__cmock_bm_zms_read_IgnoreAndReturn(-ENOENT);

	__cmock_psa_crypto_init_ExpectAndReturn(PSA_SUCCESS);
	__cmock_psa_destroy_key_ExpectAnyArgsAndReturn(PSA_ERROR_INVALID_HANDLE);
	__cmock_psa_set_key_usage_flags_ExpectAnyArgs();
	__cmock_psa_set_key_lifetime_ExpectAnyArgs();
	__cmock_psa_set_key_algorithm_ExpectAnyArgs();
	__cmock_psa_set_key_type_ExpectAnyArgs();
	__cmock_psa_set_key_bits_ExpectAnyArgs();
	__cmock_psa_generate_key_ExpectAnyArgsAndReturn(PSA_SUCCESS);
	__cmock_psa_export_public_key_ExpectAnyArgsAndReturn(PSA_ERROR_BAD_STATE);

	nrf_err = pm_init();
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);
}

void test_pm_register_success(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	/* Try registering up to the maximum number of event handlers. */
	for (uint32_t i = 0; i < CONFIG_PM_MAX_REGISTRANTS; i++) {
		nrf_err = pm_register(on_pm_evt);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	}
}

void test_pm_register_null(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	nrf_err = pm_register(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_pm_register_no_mem(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	for (uint32_t i = 0; i < CONFIG_PM_MAX_REGISTRANTS; i++) {
		nrf_err = pm_register(on_pm_evt);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	}

	nrf_err = pm_register(on_pm_evt);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_pm_sec_params_set_success(void)
{
	uint32_t nrf_err;
	ble_gap_sec_params_t sec_param;

	/* Bonding with MITM protection. */
	sec_param = (ble_gap_sec_params_t) {
		.bond = 1, .mitm = 1, .lesc = 1, .keypress = 1,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 1,

		.min_key_size = 7, .max_key_size = 16,
		.kdist_own =  {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Bonding without MTIM protection. */
	sec_param = (ble_gap_sec_params_t) {
		.bond = 1, .mitm = 0, .lesc = 0, .keypress = 1,
		.io_caps = BLE_GAP_IO_CAPS_NONE, .oob = 0,

		.min_key_size = 11, .max_key_size = 11,
		.kdist_own =  {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Passing NULL is a valid input where all bonding procedures are rejected. */
	nrf_err = pm_sec_params_set(NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Bonding Not supported. Reject all bonding procedures. */
	sec_param = (ble_gap_sec_params_t) {
		.bond = 0, .mitm = 0, .lesc = 0, .keypress = 0,
		.io_caps = BLE_GAP_IO_CAPS_NONE, .oob = 0,

		.min_key_size = 11, .max_key_size = 11,
		.kdist_own =  {.enc = 0, .id = 0},
		.kdist_peer = {.enc = 0, .id = 0},
	};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_pm_sec_params_set_invalid_param(void)
{
	uint32_t nrf_err;
	ble_gap_sec_params_t sec_param;

	/* Error if OOB flag is set without MITM flag. */
	sec_param = (ble_gap_sec_params_t) {
		.bond = 1, .mitm = 0, .lesc = 1, .keypress = 1,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 1,

		.min_key_size = 7, .max_key_size = 16,
		.kdist_own =  {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	/* Error if IO capabilities are outside of valid values. */
	sec_param = (ble_gap_sec_params_t) {
		.bond = 1, .mitm = 1, .lesc = 1, .keypress = 1,
		.io_caps = -1, .oob = 1,

		.min_key_size = 7, .max_key_size = 16,
		.kdist_own =  {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	/* Error if MITM flag is set and no Out-Of-Band (IO caps or OOB) capability is set. */
	sec_param = (ble_gap_sec_params_t) {
		.bond = 1, .mitm = 1, .lesc = 1, .keypress = 1,
		.io_caps = BLE_GAP_IO_CAPS_NONE, .oob = 0,

		.min_key_size = 7, .max_key_size = 16,
		.kdist_own =  {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	/* Error if min key size is larger than max key size. */
	sec_param = (ble_gap_sec_params_t) {
		.bond = 1, .mitm = 1, .lesc = 1, .keypress = 1,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 1,

		.min_key_size = 12, .max_key_size = 11,
		.kdist_own =  {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	/* Error if minimum key size is lower than 7 bytes. */
	sec_param = (ble_gap_sec_params_t) {
		.bond = 1, .mitm = 1, .lesc = 1, .keypress = 1,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 1,

		.min_key_size = 6, .max_key_size = 16,
		.kdist_own =  {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	/* Error if maximum key size is greater than 16 bytes. */
	sec_param = (ble_gap_sec_params_t) {
		.bond = 1, .mitm = 1, .lesc = 1, .keypress = 1,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 1,

		.min_key_size = 7, .max_key_size = 17,
		.kdist_own =  {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	/* Error if key distribution flags are set but bonding is set to unsupported. */
	sec_param = (ble_gap_sec_params_t) {
		.bond = 0, .mitm = 1, .lesc = 1, .keypress = 1,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 1,

		.min_key_size = 7, .max_key_size = 16,
		.kdist_own = {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	sec_param.kdist_own = (ble_gap_sec_kdist_t) {.enc = 1};
	sec_param.kdist_peer = (ble_gap_sec_kdist_t) {.enc = 0};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	sec_param.kdist_own = (ble_gap_sec_kdist_t) {.enc = 0};
	sec_param.kdist_peer = (ble_gap_sec_kdist_t) {.enc = 1};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	sec_param.kdist_own = (ble_gap_sec_kdist_t) {.id = 1};
	sec_param.kdist_peer = (ble_gap_sec_kdist_t) {.id = 0};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	sec_param.kdist_own = (ble_gap_sec_kdist_t) {.id = 0};
	sec_param.kdist_peer = (ble_gap_sec_kdist_t) {.id = 1};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	/* Error if bonding is set to supported, but no key distribution flags are set. */
	sec_param = (ble_gap_sec_params_t) {
		.bond = 1, .mitm = 1, .lesc = 1, .keypress = 1,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 1,

		.min_key_size = 7, .max_key_size = 16,
		.kdist_own = {.enc = 0, .id = 0},
		.kdist_peer = {.enc = 0, .id = 0},
	};

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_pm_ble_event_connected_not_bonded_success(void)
{
	union pm_entry_id entry;
	ble_evt_t evt = {.evt.gap_evt.conn_handle = CONN_HANDLE_1};

	peer_manager_initialize_success();

	__cmock_nrf_sdh_ble_idx_get_Stub(stub_nrf_sdh_ble_idx_get);
	__cmock_nrf_sdh_ble_conn_handle_get_Stub(stub_nrf_sdh_ble_conn_handle_get);

	/* Expect the bonding data storage to be iterated to find out if the connected peer have
	 * previously bonded. However, no bonding data is found for a peer with the given address.
	 */
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	entry.peer_id = 0;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();

		entry.peer_id++;
	}

	evt.header.evt_id = BLE_GAP_EVT_CONNECTED;
	evt.evt.gap_evt.params.connected = (ble_gap_evt_connected_t) {
		.peer_addr = ADDRESS_PUBLIC_1,
		.role = BLE_GAP_ROLE_PERIPH,
	};

	sdh_evt_dispatch_ble(&evt);
}

void test_pm_ble_event_connected_already_bonded_success(void)
{
	union pm_entry_id entry;
	ble_evt_t evt = {.evt.gap_evt.conn_handle = CONN_HANDLE_1};
	const struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
		.own_ltk = LTK_1,
	};
	union local_gatt_db_with_data local_gatt_db = STORED_GATT_DATA_1;

	peer_manager_initialize_success();

	__cmock_nrf_sdh_ble_idx_get_Stub(stub_nrf_sdh_ble_idx_get);
	__cmock_nrf_sdh_ble_conn_handle_get_Stub(stub_nrf_sdh_ble_conn_handle_get);

	/* Expect the bonding data storage to be iterated to find out if the connected peer have
	 * previously bonded and if so, what peer ID is associated with the peer.
	 */
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    PM_PEER_DATA_MAX_SIZE, -ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();
	entry.peer_id++;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    PM_PEER_DATA_MAX_SIZE, PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&bonding_data,
						  sizeof(struct pm_peer_data_bonding));

	/* Expect a storage read looking for local gatt data for the connected peer,
	 * followed by a restoration of the local gatt database in the SoftDevice.
	 */
	entry.data_id = PM_PEER_DATA_ID_GATT_LOCAL;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    PM_PEER_DATA_MAX_SIZE, PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&local_gatt_db, sizeof(local_gatt_db));

	__cmock_sd_ble_gatts_sys_attr_set_ExpectAndReturn(CONN_HANDLE_1, local_gatt_db.data.data,
							  local_gatt_db.data.len,
							  local_gatt_db.data.flags, NRF_SUCCESS);

	/* Expect a storage read looking for service changed state for the connected peer. */
	entry.data_id = PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(bool), PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&(bool){0}, sizeof(bool));

	/* Expect a storage read looking for central address resolution data for the connected
	 * peer.
	 */
	entry.data_id = PM_PEER_DATA_ID_CENTRAL_ADDR_RES;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(uint32_t), PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&(uint32_t){1}, sizeof(uint32_t));

	evt.header.evt_id = BLE_GAP_EVT_CONNECTED;
	evt.evt.gap_evt.params.connected = (ble_gap_evt_connected_t) {
		.peer_addr = ADDRESS_PUBLIC_1,
		.role = BLE_GAP_ROLE_PERIPH,
	};

	sdh_evt_dispatch_ble(&evt);
}

void test_pm_conn_secure_peripheral_pair_success(void)
{
	uint32_t nrf_err;
	ble_gap_sec_params_t sec_param = {
		.bond = 0, .mitm = 1, .lesc = 1, .keypress = 1,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 0,

		.min_key_size = 7, .max_key_size = 16,
		.kdist_own =  {.enc = 0, .id = 0},
		.kdist_peer = {.enc = 0, .id = 0},
	};
	union pm_entry_id entry;
	const struct pm_evt *pm_evt;
	ble_evt_t evt = {.evt.gap_evt.conn_handle = CONN_HANDLE_1};

	peer_manager_initialize_success();

	nrf_err = pm_register(&on_pm_evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	on_pm_evt_capture_restart();

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_nrf_sdh_ble_idx_get_Stub(stub_nrf_sdh_ble_idx_get);
	__cmock_nrf_sdh_ble_conn_handle_get_Stub(stub_nrf_sdh_ble_conn_handle_get);

	/* Expect the bonding data storage to be iterated to find out if the connected peer have
	 * previously bonded. However, no bonding data is found for a peer with the given address.
	 */
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	entry.peer_id = 0;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();

		entry.peer_id++;
	}

	evt.header.evt_id = BLE_GAP_EVT_CONNECTED;
	evt.evt.gap_evt.params.connected = (ble_gap_evt_connected_t) {
		.peer_addr = ADDRESS_PUBLIC_1,
		.role = BLE_GAP_ROLE_PERIPH,
	};

	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_CONFIG_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(CONN_HANDLE_1, pm_evt->conn_handle);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	__cmock_sd_ble_gap_authenticate_ExpectWithArrayAndReturn(CONN_HANDLE_1, &sec_param, 1,
								 NRF_SUCCESS);

	nrf_err = pm_conn_secure(CONN_HANDLE_1, false);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_PARAMS_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	/* On a BLE_GAP_EVT_SEC_PARAMS_REQUEST event, expect a reply with the sec_param values that
	 * were set during Peer Manager initialization/configuration.
	 */
	__cmock_sd_ble_gap_sec_params_reply_ExpectWithArrayAndReturn(CONN_HANDLE_1,
								     BLE_GAP_SEC_STATUS_SUCCESS,
								     &sec_param, 1, PTR_IGNORE, 0,
								     NRF_SUCCESS);
	__cmock_sd_ble_gap_sec_params_reply_IgnoreArg_p_sec_keyset();

	evt.header.evt_id = BLE_GAP_EVT_SEC_PARAMS_REQUEST;
	evt.evt.gap_evt.params.sec_params_request = (ble_gap_evt_sec_params_request_t) {
		.peer_params = {
			/* Security parameters for central (peer) side. */
			.bond = 0, .mitm = 1, .lesc = 1, .keypress = 1,
			.io_caps = BLE_GAP_IO_CAPS_DISPLAY_ONLY, .oob = 1,

			.min_key_size = 7, .max_key_size = 16,
			.kdist_own =  {.enc = 0, .id = 0},
			.kdist_peer = {.enc = 0, .id = 0},
		},
	};

	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_START);
	TEST_ASSERT_NOT_NULL(pm_evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_PARAMS_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(2, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	/* Here, the SoftDevice would send a BLE_GAP_EVT_AUTH_KEY_REQUEST event and it must be
	 * handled by the application by invoking sd_ble_gap_auth_key_reply.
	 * Peer Manager will not handle this event (except for an edge case with LESC and
	 * identical keys), so nothing to test here.
	 */

	/* On a BLE_GAP_EVT_LESC_DHKEY_REQUEST event, a flag is set to defer the computation
	 * of the DH key (key agreement) to the nrf_ble_lesc_request_handler() function and have
	 * that reply with sd_ble_gap_lesc_dhkey_reply() at a later time (usually from main loop).
	 */
	ble_gap_lesc_p256_pk_t peer_pk;

	memcpy(peer_pk.pk, peer_test_pub_key_sd, BLE_GAP_LESC_P256_PK_LEN);

	evt.header.evt_id = BLE_GAP_EVT_LESC_DHKEY_REQUEST;
	evt.evt.gap_evt.params.lesc_dhkey_request = (ble_gap_evt_lesc_dhkey_request_t) {
		.p_pk_peer = &peer_pk, .oobd_req = 0,
	};

	sdh_evt_dispatch_ble(&evt);

	/* Expect a call to nrf_ble_lesc_request_handler() to compute DH key (key agreement)
	 * with psa_raw_key_agreement() and reply to the SoftDevice with
	 * sd_ble_gap_lesc_dhkey_reply() to complete the BLE_GAP_EVT_LESC_DHKEY_REQUEST request.
	 */
	__cmock_psa_raw_key_agreement_ExpectWithArrayAndReturn(
		PSA_ALG_ECDH, key_pair_id,
		peer_test_pub_key_psa, sizeof(peer_test_pub_key_psa), sizeof(peer_test_pub_key_psa),
		PTR_IGNORE, 0, sizeof(test_secret_psa), PTR_IGNORE, 0, PSA_SUCCESS);
	__cmock_psa_raw_key_agreement_IgnoreArg_output();
	__cmock_psa_raw_key_agreement_IgnoreArg_output_length();
	__cmock_psa_raw_key_agreement_ReturnMemThruPtr_output(
		test_secret_psa, sizeof(test_secret_psa));
	__cmock_psa_raw_key_agreement_ReturnMemThruPtr_output_length(
		&(size_t){sizeof(test_secret_psa)}, sizeof(size_t));

	ble_gap_lesc_dhkey_t secret;

	memcpy(secret.key, test_secret_sd, sizeof(test_secret_sd));
	__cmock_sd_ble_gap_lesc_dhkey_reply_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, BLE_GAP_SEC_STATUS_SUCCESS, &secret, 1, NRF_SUCCESS);

	nrf_err = nrf_ble_lesc_request_handler();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* On a BLE_GAP_EVT_CONN_SEC_UPDATE, expect the connection security state to change. */
	evt.header.evt_id = BLE_GAP_EVT_CONN_SEC_UPDATE;
	evt.evt.gap_evt.params.conn_sec_update = (ble_gap_evt_conn_sec_update_t) {
		.conn_sec.sec_mode = {.sm = 1, .lv = 4},
		.conn_sec.encr_key_size = 16,
	};
	struct pm_conn_sec_status conn_sec_status;

	nrf_err = pm_conn_sec_status_get(CONN_HANDLE_1, &conn_sec_status);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(conn_sec_status.connected);
	TEST_ASSERT_FALSE(conn_sec_status.bonded);
	TEST_ASSERT_FALSE(conn_sec_status.encrypted);
	TEST_ASSERT_FALSE(conn_sec_status.mitm_protected);
	TEST_ASSERT_FALSE(conn_sec_status.lesc);

	sdh_evt_dispatch_ble(&evt);

	nrf_err = pm_conn_sec_status_get(CONN_HANDLE_1, &conn_sec_status);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(conn_sec_status.connected);
	TEST_ASSERT_FALSE(conn_sec_status.bonded);
	TEST_ASSERT_TRUE(conn_sec_status.encrypted);
	TEST_ASSERT_TRUE(conn_sec_status.mitm_protected);
	TEST_ASSERT_TRUE(conn_sec_status.lesc);

	/* On a BLE_GAP_EVT_AUTH_STATUS, expect peer manager to finish up the pairing request.
	 * Not much is done in response to this event when simply pairing (no bonding).
	 */
	evt.header.evt_id = BLE_GAP_EVT_AUTH_STATUS;
	evt.evt.gap_evt.params.auth_status = (ble_gap_evt_auth_status_t) {
		.auth_status = BLE_GAP_SEC_STATUS_SUCCESS,
		.bonded = false,
		.lesc = true,
		.sm1_levels = {.lv1 = 1, .lv2 = 1, .lv3 = 1, .lv4 = 1},
		.kdist_own = {.enc = 0, .id = 0},
		.kdist_peer = {.enc = 0, .id = 0},
	};

	sdh_evt_dispatch_ble(&evt);

	TEST_ASSERT_EQUAL(0, pm_peer_count());

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_SUCCEEDED);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(PM_CONN_SEC_PROCEDURE_PAIRING, pm_evt->conn_sec_succeeded.procedure);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
}

void test_pm_conn_secure_peripheral_pair_and_bond_success(void)
{
	uint32_t nrf_err;
	ble_gap_sec_params_t sec_param = {
		.bond = 1, .mitm = 1, .lesc = 1, .keypress = 1,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 0,

		.min_key_size = 7, .max_key_size = 16,
		.kdist_own =  {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};
	union pm_entry_id entry;
	const struct pm_evt *pm_evt;
	ble_evt_t evt = {.evt.gap_evt.conn_handle = CONN_HANDLE_1};

	peer_manager_initialize_success();

	nrf_err = pm_register(&on_pm_evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	on_pm_evt_capture_restart();

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_nrf_sdh_ble_idx_get_Stub(stub_nrf_sdh_ble_idx_get);
	__cmock_nrf_sdh_ble_conn_handle_get_Stub(stub_nrf_sdh_ble_conn_handle_get);

	/* Expect the bonding data storage to be iterated to find out if the connected peer have
	 * previously bonded. However, no bonding data is found for a peer with the given address.
	 */
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	entry.peer_id = 0;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();

		entry.peer_id++;
	}

	evt.header.evt_id = BLE_GAP_EVT_CONNECTED;
	evt.evt.gap_evt.params.connected = (ble_gap_evt_connected_t) {
		.peer_addr = ADDRESS_PUBLIC_1,
		.role = BLE_GAP_ROLE_PERIPH,
	};

	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_CONFIG_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(CONN_HANDLE_1, pm_evt->conn_handle);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	__cmock_sd_ble_gap_authenticate_ExpectWithArrayAndReturn(CONN_HANDLE_1, &sec_param, 1,
								 NRF_SUCCESS);

	nrf_err = pm_conn_secure(CONN_HANDLE_1, false);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_PARAMS_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	/* On a BLE_GAP_EVT_SEC_PARAMS_REQUEST event, expect a reply with the sec_param values that
	 * were set during Peer Manager initialization/configuration.
	 */
	__cmock_sd_ble_gap_sec_params_reply_ExpectWithArrayAndReturn(CONN_HANDLE_1,
								     BLE_GAP_SEC_STATUS_SUCCESS,
								     &sec_param, 1, PTR_IGNORE, 0,
								     NRF_SUCCESS);
	__cmock_sd_ble_gap_sec_params_reply_IgnoreArg_p_sec_keyset();

	evt.header.evt_id = BLE_GAP_EVT_SEC_PARAMS_REQUEST;
	evt.evt.gap_evt.params.sec_params_request = (ble_gap_evt_sec_params_request_t) {
		.peer_params = {
			/* Security parameters for central (peer) side. */
			.bond = 1, .mitm = 1, .lesc = 1, .keypress = 1,
			.io_caps = BLE_GAP_IO_CAPS_DISPLAY_ONLY, .oob = 1,

			.min_key_size = 7, .max_key_size = 16,
			.kdist_own =  {.enc = 1, .id = 1},
			.kdist_peer = {.enc = 1, .id = 1},
		},
	};

	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_START);
	TEST_ASSERT_NOT_NULL(pm_evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_PARAMS_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(2, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	/* Here, the SoftDevice would send a BLE_GAP_EVT_AUTH_KEY_REQUEST event and it must be
	 * handled by the application by invoking sd_ble_gap_auth_key_reply.
	 * Peer Manager will not handle this event (except for an edge case with LESC and
	 * identical keys), so nothing to test here.
	 */

	/* On a BLE_GAP_EVT_LESC_DHKEY_REQUEST event, a flag is set to defer the computation
	 * of the DH key (key agreement) to the nrf_ble_lesc_request_handler() function and have
	 * that reply with sd_ble_gap_lesc_dhkey_reply() at a later time (usually from main loop).
	 */
	ble_gap_lesc_p256_pk_t peer_pk;

	memcpy(peer_pk.pk, peer_test_pub_key_sd, BLE_GAP_LESC_P256_PK_LEN);

	evt.header.evt_id = BLE_GAP_EVT_LESC_DHKEY_REQUEST;
	evt.evt.gap_evt.params.lesc_dhkey_request = (ble_gap_evt_lesc_dhkey_request_t) {
		.p_pk_peer = &peer_pk, .oobd_req = 0,
	};

	sdh_evt_dispatch_ble(&evt);

	/* Expect a call to nrf_ble_lesc_request_handler() to compute DH key (key agreement)
	 * with psa_raw_key_agreement() and reply to the SoftDevice with
	 * sd_ble_gap_lesc_dhkey_reply() to complete the BLE_GAP_EVT_LESC_DHKEY_REQUEST request.
	 */
	__cmock_psa_raw_key_agreement_ExpectWithArrayAndReturn(
		PSA_ALG_ECDH, key_pair_id,
		peer_test_pub_key_psa, sizeof(peer_test_pub_key_psa), sizeof(peer_test_pub_key_psa),
		PTR_IGNORE, 0, sizeof(test_secret_psa), PTR_IGNORE, 0, PSA_SUCCESS);
	__cmock_psa_raw_key_agreement_IgnoreArg_output();
	__cmock_psa_raw_key_agreement_IgnoreArg_output_length();
	__cmock_psa_raw_key_agreement_ReturnMemThruPtr_output(
		test_secret_psa, sizeof(test_secret_psa));
	__cmock_psa_raw_key_agreement_ReturnMemThruPtr_output_length(
		&(size_t){sizeof(test_secret_psa)}, sizeof(size_t));

	ble_gap_lesc_dhkey_t secret;

	memcpy(secret.key, test_secret_sd, sizeof(test_secret_sd));
	__cmock_sd_ble_gap_lesc_dhkey_reply_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, BLE_GAP_SEC_STATUS_SUCCESS, &secret, 1, NRF_SUCCESS);

	nrf_err = nrf_ble_lesc_request_handler();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* On a BLE_GAP_EVT_CONN_SEC_UPDATE, expect the connection security state to change. */
	evt.header.evt_id = BLE_GAP_EVT_CONN_SEC_UPDATE;
	evt.evt.gap_evt.params.conn_sec_update = (ble_gap_evt_conn_sec_update_t) {
		.conn_sec.sec_mode = {.sm = 1, .lv = 4},
		.conn_sec.encr_key_size = 16,
	};
	struct pm_conn_sec_status conn_sec_status;

	nrf_err = pm_conn_sec_status_get(CONN_HANDLE_1, &conn_sec_status);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(conn_sec_status.connected);
	TEST_ASSERT_FALSE(conn_sec_status.bonded);
	TEST_ASSERT_FALSE(conn_sec_status.encrypted);
	TEST_ASSERT_FALSE(conn_sec_status.mitm_protected);
	TEST_ASSERT_FALSE(conn_sec_status.lesc);

	sdh_evt_dispatch_ble(&evt);

	nrf_err = pm_conn_sec_status_get(CONN_HANDLE_1, &conn_sec_status);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(conn_sec_status.connected);
	TEST_ASSERT_FALSE(conn_sec_status.bonded);
	TEST_ASSERT_TRUE(conn_sec_status.encrypted);
	TEST_ASSERT_TRUE(conn_sec_status.mitm_protected);
	TEST_ASSERT_TRUE(conn_sec_status.lesc);

	/* On a BLE_GAP_EVT_AUTH_STATUS, expect peer manager to finish up the pairing request.
	 *
	 * Expect the bonding data storage to be iterated to check if the paired peer already have
	 * bonding data. No duplicate bonding data is to be found.
	 * Next, expect new bonding data to be stored.
	 */
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	entry.peer_id = 0;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();

		entry.peer_id++;
	}

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					     sizeof(struct pm_peer_data_bonding), 0);
	__cmock_bm_zms_write_IgnoreArg_data();

	evt.header.evt_id = BLE_GAP_EVT_AUTH_STATUS;
	evt.evt.gap_evt.params.auth_status = (ble_gap_evt_auth_status_t) {
		.auth_status = BLE_GAP_SEC_STATUS_SUCCESS,
		.bonded = true,
		.lesc = true,
		.sm1_levels = {.lv1 = 1, .lv2 = 1, .lv3 = 1, .lv4 = 1},
		.kdist_own = {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};

	sdh_evt_dispatch_ble(&evt);

	TEST_ASSERT_EQUAL(1, pm_peer_count());

	nrf_err = pm_conn_sec_status_get(CONN_HANDLE_1, &conn_sec_status);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(conn_sec_status.connected);
	TEST_ASSERT_TRUE(conn_sec_status.bonded);
	TEST_ASSERT_TRUE(conn_sec_status.encrypted);
	TEST_ASSERT_TRUE(conn_sec_status.mitm_protected);
	TEST_ASSERT_TRUE(conn_sec_status.lesc);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_SUCCEEDED);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(PM_CONN_SEC_PROCEDURE_BONDING, pm_evt->conn_sec_succeeded.procedure);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
}

void test_pm_conn_secure_peripheral_with_bonding_data_success(void)
{
	uint32_t nrf_err;
	ble_gap_sec_params_t sec_param = {
		.bond = 1, .mitm = 1, .lesc = 1, .keypress = 1,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 0,

		.min_key_size = 7, .max_key_size = 16,
		.kdist_own =  {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};
	union pm_entry_id entry;
	const struct pm_evt *pm_evt;
	ble_evt_t evt = {.evt.gap_evt.conn_handle = CONN_HANDLE_1};
	const struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
		.own_ltk = LTK_1,
	};
	union local_gatt_db_with_data local_gatt_db = STORED_GATT_DATA_1;

	peer_manager_initialize_success();

	nrf_err = pm_register(&on_pm_evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	on_pm_evt_capture_restart();

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_nrf_sdh_ble_idx_get_Stub(stub_nrf_sdh_ble_idx_get);
	__cmock_nrf_sdh_ble_conn_handle_get_Stub(stub_nrf_sdh_ble_conn_handle_get);

	/* Expect the bonding data storage to be iterated to find out if the connected peer have
	 * previously bonded and if so, what peer ID is associated with the peer.
	 */
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    PM_PEER_DATA_MAX_SIZE, -ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();
	entry.peer_id++;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    PM_PEER_DATA_MAX_SIZE, PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&bonding_data,
						  sizeof(struct pm_peer_data_bonding));

	/* Expect a storage read looking for local gatt data for the connected peer,
	 * followed by a restoration of the local gatt database in the SoftDevice.
	 */
	entry.data_id = PM_PEER_DATA_ID_GATT_LOCAL;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    PM_PEER_DATA_MAX_SIZE, PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&local_gatt_db, sizeof(local_gatt_db));

	__cmock_sd_ble_gatts_sys_attr_set_ExpectAndReturn(CONN_HANDLE_1, local_gatt_db.data.data,
							  local_gatt_db.data.len,
							  local_gatt_db.data.flags, NRF_SUCCESS);

	/* Expect a storage read looking for service changed state for the connected peer. */
	entry.data_id = PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(bool), PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&(bool){0}, sizeof(bool));

	/* Expect a storage read looking for central address resolution data for the connected
	 * peer.
	 */
	entry.data_id = PM_PEER_DATA_ID_CENTRAL_ADDR_RES;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(uint32_t), PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&(uint32_t){1}, sizeof(uint32_t));

	evt.header.evt_id = BLE_GAP_EVT_CONNECTED;
	evt.evt.gap_evt.params.connected = (ble_gap_evt_connected_t) {
		.peer_addr = ADDRESS_PUBLIC_1,
		.role = BLE_GAP_ROLE_PERIPH,
	};

	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_CONFIG_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(CONN_HANDLE_1, pm_evt->conn_handle);

	pm_evt = on_pm_evt_find_last(PM_EVT_BONDED_PEER_CONNECTED);
	TEST_ASSERT_NOT_NULL(pm_evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_LOCAL_DB_CACHE_APPLIED);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(3, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	/* On a BLE_GAP_EVT_SEC_INFO_REQUEST event, expect the bonding data storage to be iterated
	 * in search for a matching master ID. None found. Peer ID is found based on conn_handle.
	 * Next, expect a read of the stored bonding data based on the peer ID.
	 */
	entry = (union pm_entry_id) {.data_id = PM_PEER_DATA_ID_BONDING, .peer_id = 0};
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		if (i == 1) {
			__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
							    PM_PEER_DATA_MAX_SIZE,
							    PM_PEER_DATA_MAX_SIZE);
			__cmock_bm_zms_read_IgnoreArg_data();
			__cmock_bm_zms_read_ReturnMemThruPtr_data(
				(void *)&bonding_data, sizeof(struct pm_peer_data_bonding));
		} else {
			__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
							    PM_PEER_DATA_MAX_SIZE, -ENOENT);
			__cmock_bm_zms_read_IgnoreArg_data();
		}

		entry.peer_id++;
	}

	entry = (union pm_entry_id) {.data_id = PM_PEER_DATA_ID_BONDING, .peer_id = 1};
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(struct pm_peer_data_bonding),
					    sizeof(struct pm_peer_data_bonding));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&bonding_data,
						  sizeof(struct pm_peer_data_bonding));

	/* Expect reply to the BLE_GAP_EVT_SEC_INFO_REQUEST event with ltk. */
	__cmock_sd_ble_gap_sec_info_reply_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, &bonding_data.own_ltk.enc_info, 1, NRF_SUCCESS);

	evt.header.evt_id = BLE_GAP_EVT_SEC_INFO_REQUEST;
	evt.evt.gap_evt.params.sec_info_request = (ble_gap_evt_sec_info_request_t) {
		.peer_addr = ADDRESS_PUBLIC_1,
		.master_id = {0},
		.enc_info = 1,
		.id_info = 0,
	};

	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_START);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
}

void test_pm_conn_secure_peripheral_pair_again_with_bonding_data_present_success(void)
{
	uint32_t nrf_err;
	ble_gap_sec_params_t sec_param = {
		.bond = 1, .mitm = 1, .lesc = 1, .keypress = 0,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 0,

		.min_key_size = 7, .max_key_size = 16,
		.kdist_own =  {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};
	union pm_entry_id entry;
	const struct pm_evt *pm_evt;
	ble_evt_t evt = {.evt.gap_evt.conn_handle = CONN_HANDLE_1};
	const struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
		.own_ltk = LTK_1,
	};
	union local_gatt_db_with_data local_gatt_db = STORED_GATT_DATA_1;

	peer_manager_initialize_success();

	nrf_err = pm_register(&on_pm_evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	on_pm_evt_capture_restart();

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_nrf_sdh_ble_idx_get_Stub(stub_nrf_sdh_ble_idx_get);
	__cmock_nrf_sdh_ble_conn_handle_get_Stub(stub_nrf_sdh_ble_conn_handle_get);

	/* Expect the bonding data storage to be iterated to find out if the connected peer have
	 * previously bonded and if so, what peer ID is associated with the peer.
	 */
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    PM_PEER_DATA_MAX_SIZE, -ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();
	entry.peer_id++;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    PM_PEER_DATA_MAX_SIZE, PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&bonding_data,
						  sizeof(struct pm_peer_data_bonding));

	/* Expect a storage read looking for local gatt data for the connected peer,
	 * followed by a restoration of the local gatt database in the SoftDevice.
	 */
	entry.data_id = PM_PEER_DATA_ID_GATT_LOCAL;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    PM_PEER_DATA_MAX_SIZE, PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&local_gatt_db, sizeof(local_gatt_db));

	__cmock_sd_ble_gatts_sys_attr_set_ExpectAndReturn(CONN_HANDLE_1, local_gatt_db.data.data,
							  local_gatt_db.data.len,
							  local_gatt_db.data.flags, NRF_SUCCESS);

	/* Expect a storage read looking for service changed state for the connected peer. */
	entry.data_id = PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(bool), PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&(bool){0}, sizeof(bool));

	/* Expect a storage read looking for central address resolution data for the connected
	 * peer.
	 */
	entry.data_id = PM_PEER_DATA_ID_CENTRAL_ADDR_RES;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(uint32_t), PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&(uint32_t){1}, sizeof(uint32_t));

	evt.header.evt_id = BLE_GAP_EVT_CONNECTED;
	evt.evt.gap_evt.params.connected = (ble_gap_evt_connected_t) {
		.peer_addr = ADDRESS_PUBLIC_1,
		.role = BLE_GAP_ROLE_PERIPH,
	};

	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_CONFIG_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(CONN_HANDLE_1, pm_evt->conn_handle);

	pm_evt = on_pm_evt_find_last(PM_EVT_BONDED_PEER_CONNECTED);
	TEST_ASSERT_NOT_NULL(pm_evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_LOCAL_DB_CACHE_APPLIED);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(3, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	/* On a BLE_GAP_EVT_SEC_PARAMS_REQUEST event with a bonded peer, expect
	 * PM_EVT_CONN_SEC_CONFIG_REQ and allow repairing, then reply with the configured sec_param
	 * values to start re-pairing.
	 */
	__cmock_sd_ble_gap_sec_params_reply_ExpectWithArrayAndReturn(CONN_HANDLE_1,
								     BLE_GAP_SEC_STATUS_SUCCESS,
								     &sec_param, 1, PTR_IGNORE, 0,
								     NRF_SUCCESS);
	__cmock_sd_ble_gap_sec_params_reply_IgnoreArg_p_sec_keyset();

	evt.header.evt_id = BLE_GAP_EVT_SEC_PARAMS_REQUEST;
	evt.evt.gap_evt.params.sec_params_request = (ble_gap_evt_sec_params_request_t) {
		.peer_params = {
			/* Security parameters for central (peer) side. */
			.bond = 0, .mitm = 1, .lesc = 1, .keypress = 1,
			.io_caps = BLE_GAP_IO_CAPS_DISPLAY_ONLY, .oob = 1,

			.min_key_size = 7, .max_key_size = 16,
			.kdist_own =  {.enc = 1, .id = 1},
			.kdist_peer = {.enc = 1, .id = 1},
		},
	};

	on_pm_evt_test_ctx.conn_sec_config_req.allow_repairing = true;

	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_CONFIG_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_PARAMS_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_START);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(3, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	/* Here, the SoftDevice would send a BLE_GAP_EVT_AUTH_KEY_REQUEST event and it must be
	 * handled by the application by invoking sd_ble_gap_auth_key_reply.
	 * Peer Manager will not handle this event (except for an edge case with LESC and
	 * identical keys), so nothing to test here.
	 */

	/* On a BLE_GAP_EVT_LESC_DHKEY_REQUEST event, a flag is set to defer the computation
	 * of the DH key (key agreement) to the nrf_ble_lesc_request_handler() function and have
	 * that reply with sd_ble_gap_lesc_dhkey_reply() at a later time (usually from main loop).
	 */
	ble_gap_lesc_p256_pk_t peer_pk;

	memcpy(peer_pk.pk, peer_test_pub_key_sd, BLE_GAP_LESC_P256_PK_LEN);

	evt.header.evt_id = BLE_GAP_EVT_LESC_DHKEY_REQUEST;
	evt.evt.gap_evt.params.lesc_dhkey_request = (ble_gap_evt_lesc_dhkey_request_t) {
		.p_pk_peer = &peer_pk, .oobd_req = 0,
	};

	sdh_evt_dispatch_ble(&evt);

	/* Expect a call to nrf_ble_lesc_request_handler() to compute DH key (key agreement)
	 * with psa_raw_key_agreement() and reply to the SoftDevice with
	 * sd_ble_gap_lesc_dhkey_reply() to complete the BLE_GAP_EVT_LESC_DHKEY_REQUEST request.
	 */
	__cmock_psa_raw_key_agreement_ExpectWithArrayAndReturn(
		PSA_ALG_ECDH, key_pair_id,
		peer_test_pub_key_psa, sizeof(peer_test_pub_key_psa), sizeof(peer_test_pub_key_psa),
		PTR_IGNORE, 0, sizeof(test_secret_psa), PTR_IGNORE, 0, PSA_SUCCESS);
	__cmock_psa_raw_key_agreement_IgnoreArg_output();
	__cmock_psa_raw_key_agreement_IgnoreArg_output_length();
	__cmock_psa_raw_key_agreement_ReturnMemThruPtr_output(
		test_secret_psa, sizeof(test_secret_psa));
	__cmock_psa_raw_key_agreement_ReturnMemThruPtr_output_length(
		&(size_t){sizeof(test_secret_psa)}, sizeof(size_t));

	ble_gap_lesc_dhkey_t secret;

	memcpy(secret.key, test_secret_sd, sizeof(test_secret_sd));
	__cmock_sd_ble_gap_lesc_dhkey_reply_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, BLE_GAP_SEC_STATUS_SUCCESS, &secret, 1, NRF_SUCCESS);

	nrf_err = nrf_ble_lesc_request_handler();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* On a BLE_GAP_EVT_CONN_SEC_UPDATE, expect the connection security state to change. */
	evt.header.evt_id = BLE_GAP_EVT_CONN_SEC_UPDATE;
	evt.evt.gap_evt.params.conn_sec_update = (ble_gap_evt_conn_sec_update_t) {
		.conn_sec.sec_mode = {.sm = 1, .lv = 4},
		.conn_sec.encr_key_size = 16,
	};
	struct pm_conn_sec_status conn_sec_status;

	nrf_err = pm_conn_sec_status_get(CONN_HANDLE_1, &conn_sec_status);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(conn_sec_status.connected);
	TEST_ASSERT_TRUE(conn_sec_status.bonded);
	TEST_ASSERT_FALSE(conn_sec_status.encrypted);
	TEST_ASSERT_FALSE(conn_sec_status.mitm_protected);
	TEST_ASSERT_FALSE(conn_sec_status.lesc);

	sdh_evt_dispatch_ble(&evt);

	nrf_err = pm_conn_sec_status_get(CONN_HANDLE_1, &conn_sec_status);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(conn_sec_status.connected);
	TEST_ASSERT_TRUE(conn_sec_status.bonded);
	TEST_ASSERT_TRUE(conn_sec_status.encrypted);
	TEST_ASSERT_TRUE(conn_sec_status.mitm_protected);
	TEST_ASSERT_TRUE(conn_sec_status.lesc);

	/* On a BLE_GAP_EVT_AUTH_STATUS, expect peer manager to finish up the pairing request.
	 * Not much is done in response to this event when simply pairing (no bonding).
	 */
	evt.header.evt_id = BLE_GAP_EVT_AUTH_STATUS;
	evt.evt.gap_evt.params.auth_status = (ble_gap_evt_auth_status_t) {
		.auth_status = BLE_GAP_SEC_STATUS_SUCCESS,
		.bonded = false,
		.lesc = true,
		.sm1_levels = {.lv1 = 1, .lv2 = 1, .lv3 = 1, .lv4 = 1},
		.kdist_own = {.enc = 0, .id = 0},
		.kdist_peer = {.enc = 0, .id = 0},
	};

	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_SUCCEEDED);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(PM_CONN_SEC_PROCEDURE_PAIRING, pm_evt->conn_sec_succeeded.procedure);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
}
void test_pm_conn_secure_central_pair_success(void)
{
	uint32_t nrf_err;
	ble_gap_sec_params_t sec_param = {
		.bond = 0, .mitm = 1, .lesc = 1, .keypress = 1,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 0,

		.min_key_size = 7, .max_key_size = 16,
		.kdist_own =  {.enc = 0, .id = 0},
		.kdist_peer = {.enc = 0, .id = 0},
	};
	union pm_entry_id entry;
	const struct pm_evt *pm_evt;
	ble_evt_t evt = {.evt.gap_evt.conn_handle = CONN_HANDLE_1};

	peer_manager_initialize_success();

	nrf_err = pm_register(&on_pm_evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	on_pm_evt_capture_restart();

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_nrf_sdh_ble_idx_get_Stub(stub_nrf_sdh_ble_idx_get);
	__cmock_nrf_sdh_ble_conn_handle_get_Stub(stub_nrf_sdh_ble_conn_handle_get);

	/* Expect the bonding data storage to be iterated to find out if the connected peer have
	 * previously bonded. However, no bonding data is found for a peer with the given address.
	 */
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	entry.peer_id = 0;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();

		entry.peer_id++;
	}

	evt.header.evt_id = BLE_GAP_EVT_CONNECTED;
	evt.evt.gap_evt.params.connected = (ble_gap_evt_connected_t) {
		.peer_addr = ADDRESS_PUBLIC_1,
		.role = BLE_GAP_ROLE_CENTRAL,
	};

	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_CONFIG_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(CONN_HANDLE_1, pm_evt->conn_handle);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	__cmock_sd_ble_gap_authenticate_ExpectWithArrayAndReturn(CONN_HANDLE_1, &sec_param, 1,
								 NRF_SUCCESS);

	nrf_err = pm_conn_secure(CONN_HANDLE_1, false);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_PARAMS_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_START);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(2, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	/* On a BLE_GAP_EVT_SEC_PARAMS_REQUEST event, expect a reply with sec_param == NULL.
	 * The central's sec_params was passed to the SoftDevice with the call to
	 * sd_ble_gap_authenticate() so no reason to repeat it.
	 */
	__cmock_sd_ble_gap_sec_params_reply_ExpectWithArrayAndReturn(CONN_HANDLE_1,
								     BLE_GAP_SEC_STATUS_SUCCESS,
								     NULL, 0, PTR_IGNORE, 0,
								     NRF_SUCCESS);
	__cmock_sd_ble_gap_sec_params_reply_IgnoreArg_p_sec_keyset();

	evt.header.evt_id = BLE_GAP_EVT_SEC_PARAMS_REQUEST;
	evt.evt.gap_evt.params.sec_params_request = (ble_gap_evt_sec_params_request_t) {
		.peer_params = {
			/* Security parameters for peripheral (peer) side. */
			.bond = 0, .mitm = 1, .lesc = 1, .keypress = 1,
			.io_caps = BLE_GAP_IO_CAPS_DISPLAY_ONLY, .oob = 1,

			.min_key_size = 7, .max_key_size = 16,
			.kdist_own =  {.enc = 0, .id = 0},
			.kdist_peer = {.enc = 0, .id = 0},
		},
	};

	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_PARAMS_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	/* Here, the SoftDevice would send a BLE_GAP_EVT_AUTH_KEY_REQUEST event and it must be
	 * handled by the application by invoking sd_ble_gap_auth_key_reply.
	 * Peer Manager will not handle this event (except for an edge case with LESC and
	 * identical keys), so nothing to test here.
	 */

	/* On a BLE_GAP_EVT_LESC_DHKEY_REQUEST event, a flag is set to defer the computation
	 * of the DH key (key agreement) to the nrf_ble_lesc_request_handler() function and have
	 * that reply with sd_ble_gap_lesc_dhkey_reply() at a later time (usually from main loop).
	 */
	ble_gap_lesc_p256_pk_t peer_pk;

	memcpy(peer_pk.pk, peer_test_pub_key_sd, BLE_GAP_LESC_P256_PK_LEN);

	evt.header.evt_id = BLE_GAP_EVT_LESC_DHKEY_REQUEST;
	evt.evt.gap_evt.params.lesc_dhkey_request = (ble_gap_evt_lesc_dhkey_request_t) {
		.p_pk_peer = &peer_pk, .oobd_req = 0,
	};

	sdh_evt_dispatch_ble(&evt);

	/* Expect a call to nrf_ble_lesc_request_handler() to compute DH key (key agreement)
	 * with psa_raw_key_agreement() and reply to the SoftDevice with
	 * sd_ble_gap_lesc_dhkey_reply() to complete the BLE_GAP_EVT_LESC_DHKEY_REQUEST request.
	 */
	__cmock_psa_raw_key_agreement_ExpectWithArrayAndReturn(
		PSA_ALG_ECDH, key_pair_id,
		peer_test_pub_key_psa, sizeof(peer_test_pub_key_psa), sizeof(peer_test_pub_key_psa),
		PTR_IGNORE, 0, sizeof(test_secret_psa), PTR_IGNORE, 0, PSA_SUCCESS);
	__cmock_psa_raw_key_agreement_IgnoreArg_output();
	__cmock_psa_raw_key_agreement_IgnoreArg_output_length();
	__cmock_psa_raw_key_agreement_ReturnMemThruPtr_output(
		test_secret_psa, sizeof(test_secret_psa));
	__cmock_psa_raw_key_agreement_ReturnMemThruPtr_output_length(
		&(size_t){sizeof(test_secret_psa)}, sizeof(size_t));

	ble_gap_lesc_dhkey_t secret;

	memcpy(secret.key, test_secret_sd, sizeof(test_secret_sd));
	__cmock_sd_ble_gap_lesc_dhkey_reply_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, BLE_GAP_SEC_STATUS_SUCCESS, &secret, 1, NRF_SUCCESS);

	nrf_err = nrf_ble_lesc_request_handler();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* On a BLE_GAP_EVT_CONN_SEC_UPDATE, expect the connection security state to change. */
	evt.header.evt_id = BLE_GAP_EVT_CONN_SEC_UPDATE;
	evt.evt.gap_evt.params.conn_sec_update = (ble_gap_evt_conn_sec_update_t) {
		.conn_sec.sec_mode = {.sm = 1, .lv = 4},
		.conn_sec.encr_key_size = 16,
	};
	struct pm_conn_sec_status conn_sec_status;

	nrf_err = pm_conn_sec_status_get(CONN_HANDLE_1, &conn_sec_status);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(conn_sec_status.connected);
	TEST_ASSERT_FALSE(conn_sec_status.bonded);
	TEST_ASSERT_FALSE(conn_sec_status.encrypted);
	TEST_ASSERT_FALSE(conn_sec_status.mitm_protected);
	TEST_ASSERT_FALSE(conn_sec_status.lesc);

	sdh_evt_dispatch_ble(&evt);

	nrf_err = pm_conn_sec_status_get(CONN_HANDLE_1, &conn_sec_status);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(conn_sec_status.connected);
	TEST_ASSERT_FALSE(conn_sec_status.bonded);
	TEST_ASSERT_TRUE(conn_sec_status.encrypted);
	TEST_ASSERT_TRUE(conn_sec_status.mitm_protected);
	TEST_ASSERT_TRUE(conn_sec_status.lesc);

	/* On a BLE_GAP_EVT_AUTH_STATUS, expect peer manager to finish up the pairing request.
	 * Not much is done in response to this event when simply pairing (no bonding).
	 */
	evt.header.evt_id = BLE_GAP_EVT_AUTH_STATUS;
	evt.evt.gap_evt.params.auth_status = (ble_gap_evt_auth_status_t) {
		.auth_status = BLE_GAP_SEC_STATUS_SUCCESS,
		.bonded = false,
		.lesc = true,
		.sm1_levels = {.lv1 = 1, .lv2 = 1, .lv3 = 1, .lv4 = 1},
		.kdist_own = {.enc = 0, .id = 0},
		.kdist_peer = {.enc = 0, .id = 0},
	};

	sdh_evt_dispatch_ble(&evt);

	TEST_ASSERT_EQUAL(0, pm_peer_count());

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_SUCCEEDED);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(PM_CONN_SEC_PROCEDURE_PAIRING, pm_evt->conn_sec_succeeded.procedure);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
}

void test_pm_conn_secure_central_pair_and_bond_success(void)
{
	uint32_t nrf_err;
	ble_gap_sec_params_t sec_param = {
		.bond = 1, .mitm = 1, .lesc = 1, .keypress = 1,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 0,

		.min_key_size = 7, .max_key_size = 16,
		.kdist_own =  {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};
	union pm_entry_id entry;
	const struct pm_evt *pm_evt;
	ble_evt_t evt = {.evt.gap_evt.conn_handle = CONN_HANDLE_1};

	peer_manager_initialize_success();

	nrf_err = pm_register(&on_pm_evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	on_pm_evt_capture_restart();

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_nrf_sdh_ble_idx_get_Stub(stub_nrf_sdh_ble_idx_get);
	__cmock_nrf_sdh_ble_conn_handle_get_Stub(stub_nrf_sdh_ble_conn_handle_get);

	/* Expect the bonding data storage to be iterated to find out if the connected peer have
	 * previously bonded. However, no bonding data is found for a peer with the given address.
	 */
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	entry.peer_id = 0;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();

		entry.peer_id++;
	}

	evt.header.evt_id = BLE_GAP_EVT_CONNECTED;
	evt.evt.gap_evt.params.connected = (ble_gap_evt_connected_t) {
		.peer_addr = ADDRESS_PUBLIC_1,
		.role = BLE_GAP_ROLE_CENTRAL,
	};

	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_CONFIG_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(CONN_HANDLE_1, pm_evt->conn_handle);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	__cmock_sd_ble_gap_authenticate_ExpectWithArrayAndReturn(CONN_HANDLE_1, &sec_param, 1,
								 NRF_SUCCESS);

	nrf_err = pm_conn_secure(CONN_HANDLE_1, false);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_PARAMS_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_START);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(2, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	/* On a BLE_GAP_EVT_SEC_PARAMS_REQUEST event, expect a reply with sec_param == NULL.
	 * The central's sec_params was passed to the SoftDevice with the call to
	 * sd_ble_gap_authenticate() so no reason to repeat it.
	 */
	__cmock_sd_ble_gap_sec_params_reply_ExpectWithArrayAndReturn(CONN_HANDLE_1,
								     BLE_GAP_SEC_STATUS_SUCCESS,
								     NULL, 0, PTR_IGNORE, 0,
								     NRF_SUCCESS);
	__cmock_sd_ble_gap_sec_params_reply_IgnoreArg_p_sec_keyset();

	evt.header.evt_id = BLE_GAP_EVT_SEC_PARAMS_REQUEST;
	evt.evt.gap_evt.params.sec_params_request = (ble_gap_evt_sec_params_request_t) {
		.peer_params = {
			/* Security parameters for peripheral (peer) side. */
			.bond = 1, .mitm = 1, .lesc = 1, .keypress = 1,
			.io_caps = BLE_GAP_IO_CAPS_DISPLAY_ONLY, .oob = 1,

			.min_key_size = 7, .max_key_size = 16,
			.kdist_own =  {.enc = 1, .id = 1},
			.kdist_peer = {.enc = 1, .id = 1},
		},
	};

	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_PARAMS_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	/* Here, the SoftDevice would send a BLE_GAP_EVT_AUTH_KEY_REQUEST event and it must be
	 * handled by the application by invoking sd_ble_gap_auth_key_reply.
	 * Peer Manager will not handle this event (except for an edge case with LESC and
	 * identical keys), so nothing to test here.
	 */

	/* On a BLE_GAP_EVT_LESC_DHKEY_REQUEST event, a flag is set to defer the computation
	 * of the DH key (key agreement) to the nrf_ble_lesc_request_handler() function and have
	 * that reply with sd_ble_gap_lesc_dhkey_reply() at a later time (usually from main loop).
	 */
	ble_gap_lesc_p256_pk_t peer_pk;

	memcpy(peer_pk.pk, peer_test_pub_key_sd, BLE_GAP_LESC_P256_PK_LEN);

	evt.header.evt_id = BLE_GAP_EVT_LESC_DHKEY_REQUEST;
	evt.evt.gap_evt.params.lesc_dhkey_request = (ble_gap_evt_lesc_dhkey_request_t) {
		.p_pk_peer = &peer_pk, .oobd_req = 0,
	};

	sdh_evt_dispatch_ble(&evt);

	/* Expect a call to nrf_ble_lesc_request_handler() to compute DH key (key agreement)
	 * with psa_raw_key_agreement() and reply to the SoftDevice with
	 * sd_ble_gap_lesc_dhkey_reply() to complete the BLE_GAP_EVT_LESC_DHKEY_REQUEST request.
	 */
	__cmock_psa_raw_key_agreement_ExpectWithArrayAndReturn(
		PSA_ALG_ECDH, key_pair_id,
		peer_test_pub_key_psa, sizeof(peer_test_pub_key_psa), sizeof(peer_test_pub_key_psa),
		PTR_IGNORE, 0, sizeof(test_secret_psa), PTR_IGNORE, 0, PSA_SUCCESS);
	__cmock_psa_raw_key_agreement_IgnoreArg_output();
	__cmock_psa_raw_key_agreement_IgnoreArg_output_length();
	__cmock_psa_raw_key_agreement_ReturnMemThruPtr_output(
		test_secret_psa, sizeof(test_secret_psa));
	__cmock_psa_raw_key_agreement_ReturnMemThruPtr_output_length(
		&(size_t){sizeof(test_secret_psa)}, sizeof(size_t));

	ble_gap_lesc_dhkey_t secret;

	memcpy(secret.key, test_secret_sd, sizeof(test_secret_sd));
	__cmock_sd_ble_gap_lesc_dhkey_reply_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, BLE_GAP_SEC_STATUS_SUCCESS, &secret, 1, NRF_SUCCESS);

	nrf_err = nrf_ble_lesc_request_handler();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* On a BLE_GAP_EVT_CONN_SEC_UPDATE, expect the connection security state to change. */
	evt.header.evt_id = BLE_GAP_EVT_CONN_SEC_UPDATE;
	evt.evt.gap_evt.params.conn_sec_update = (ble_gap_evt_conn_sec_update_t) {
		.conn_sec.sec_mode = {.sm = 1, .lv = 4},
		.conn_sec.encr_key_size = 16,
	};
	struct pm_conn_sec_status conn_sec_status;

	nrf_err = pm_conn_sec_status_get(CONN_HANDLE_1, &conn_sec_status);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(conn_sec_status.connected);
	TEST_ASSERT_FALSE(conn_sec_status.bonded);
	TEST_ASSERT_FALSE(conn_sec_status.encrypted);
	TEST_ASSERT_FALSE(conn_sec_status.mitm_protected);
	TEST_ASSERT_FALSE(conn_sec_status.lesc);

	sdh_evt_dispatch_ble(&evt);

	nrf_err = pm_conn_sec_status_get(CONN_HANDLE_1, &conn_sec_status);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(conn_sec_status.connected);
	TEST_ASSERT_FALSE(conn_sec_status.bonded);
	TEST_ASSERT_TRUE(conn_sec_status.encrypted);
	TEST_ASSERT_TRUE(conn_sec_status.mitm_protected);
	TEST_ASSERT_TRUE(conn_sec_status.lesc);

	/* On a BLE_GAP_EVT_AUTH_STATUS, expect peer manager to finish up the pairing request.
	 * Not much is done in response to this event when simply pairing (no bonding).
	 */
	evt.header.evt_id = BLE_GAP_EVT_AUTH_STATUS;
	evt.evt.gap_evt.params.auth_status = (ble_gap_evt_auth_status_t) {
		.auth_status = BLE_GAP_SEC_STATUS_SUCCESS,
		.bonded = false,
		.lesc = true,
		.sm1_levels = {.lv1 = 1, .lv2 = 1, .lv3 = 1, .lv4 = 1},
		.kdist_own = {.enc = 0, .id = 0},
		.kdist_peer = {.enc = 0, .id = 0},
	};

	sdh_evt_dispatch_ble(&evt);

	TEST_ASSERT_EQUAL(0, pm_peer_count());

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_SUCCEEDED);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(PM_CONN_SEC_PROCEDURE_PAIRING, pm_evt->conn_sec_succeeded.procedure);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
}

void test_pm_conn_secure_central_with_bonding_data_success(void)
{
	uint32_t nrf_err;
	ble_gap_sec_params_t sec_param = {
		.bond = 1, .mitm = 1, .lesc = 0, .keypress = 0,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 0,
		.min_key_size = 7, .max_key_size = 16,
		.kdist_own = {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};
	union pm_entry_id entry;
	const struct pm_evt *pm_evt;
	ble_evt_t evt = {.evt.gap_evt.conn_handle = CONN_HANDLE_1};
	const struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_CENTRAL,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
		.peer_ltk = LTK_1,
	};
	union local_gatt_db_with_data local_gatt_db = STORED_GATT_DATA_1;

	peer_manager_initialize_success();

	nrf_err = pm_register(&on_pm_evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	on_pm_evt_capture_restart();

	nrf_err = pm_sec_params_set(&sec_param);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_nrf_sdh_ble_idx_get_Stub(stub_nrf_sdh_ble_idx_get);
	__cmock_nrf_sdh_ble_conn_handle_get_Stub(stub_nrf_sdh_ble_conn_handle_get);

	/* Find stored bonding for the connected peer address (peer id 1). */
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    PM_PEER_DATA_MAX_SIZE, -ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();
	entry.peer_id = 1;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    PM_PEER_DATA_MAX_SIZE, PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&bonding_data,
						  sizeof(struct pm_peer_data_bonding));

	/* PM_EVT_BONDED_PEER_CONNECTED restores local GATT data and checks SC/CAR state. */
	entry.data_id = PM_PEER_DATA_ID_GATT_LOCAL;
	entry.peer_id = 1;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    PM_PEER_DATA_MAX_SIZE, PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&local_gatt_db, sizeof(local_gatt_db));

	__cmock_sd_ble_gatts_sys_attr_set_ExpectAndReturn(CONN_HANDLE_1, local_gatt_db.data.data,
							  local_gatt_db.data.len,
							  local_gatt_db.data.flags, NRF_SUCCESS);

	entry.data_id = PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(bool), PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&(bool){0}, sizeof(bool));

	entry.data_id = PM_PEER_DATA_ID_CENTRAL_ADDR_RES;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(uint32_t), PM_PEER_DATA_MAX_SIZE);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&(uint32_t){1}, sizeof(uint32_t));

	evt.header.evt_id = BLE_GAP_EVT_CONNECTED;
	evt.evt.gap_evt.params.connected = (ble_gap_evt_connected_t) {
		.peer_addr = ADDRESS_PUBLIC_1,
		.role = BLE_GAP_ROLE_CENTRAL,
	};

	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_CONFIG_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(CONN_HANDLE_1, pm_evt->conn_handle);

	pm_evt = on_pm_evt_find_last(PM_EVT_BONDED_PEER_CONNECTED);
	TEST_ASSERT_NOT_NULL(pm_evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_LOCAL_DB_CACHE_APPLIED);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(3, on_pm_evt_test_ctx.capture.count);
	on_pm_evt_capture_restart();

	/* Central re-encrypts with stored peer LTK (own_ltk.lesc is zero). */
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	entry.peer_id = 1;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(struct pm_peer_data_bonding),
					    sizeof(bonding_data));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data((void *)&bonding_data,
						  sizeof(bonding_data));

	__cmock_sd_ble_gap_encrypt_ExpectWithArrayAndReturn(
		CONN_HANDLE_1, &bonding_data.peer_ltk.master_id, 1,
		&bonding_data.peer_ltk.enc_info, 1, NRF_SUCCESS);

	nrf_err = pm_conn_secure(CONN_HANDLE_1, false);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_PARAMS_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_SEC_START);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(2, on_pm_evt_test_ctx.capture.count);
}

void test_pm_conn_exclude_success(void)
{
	uint32_t nrf_err;
	const struct pm_evt *pm_evt;

	peer_manager_initialize_success();

	__cmock_nrf_sdh_ble_idx_get_Stub(stub_nrf_sdh_ble_idx_get);

	nrf_err = pm_register(on_pm_evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	on_pm_evt_capture_restart();

	on_pm_evt_test_ctx.conn_config_req.exclude_connection = true;

	const ble_evt_t evt = {
		.header.evt_id = BLE_GAP_EVT_CONNECTED,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE_1,
			.params.connected = {
				.peer_addr = ADDRESS_PUBLIC_1,
				.role = BLE_GAP_ROLE_PERIPH,
			},
		},
	};

	/* Expect peer_manager to ignore the connection handle.
	 * No more function calls are expected in response to the connected event.
	 */
	sdh_evt_dispatch_ble(&evt);

	pm_evt = on_pm_evt_find_last(PM_EVT_CONN_CONFIG_REQ);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(CONN_HANDLE_1, pm_evt->conn_handle);

	pm_evt = on_pm_evt_find_last(PM_EVT_BONDED_PEER_CONNECTED);
	TEST_ASSERT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
}

void test_pm_conn_exclude_null(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	nrf_err = pm_conn_exclude(CONN_HANDLE_1, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_pm_conn_sec_config_reply_success(void)
{
	struct pm_conn_sec_config sec_config = {.allow_repairing = true};

	peer_manager_initialize_success();

	__cmock_nrf_sdh_ble_idx_get_Stub(stub_nrf_sdh_ble_idx_get);

	pm_conn_sec_config_reply(CONN_HANDLE_1, &sec_config);
}

void test_pm_conn_sec_config_reply_null(void)
{
	peer_manager_initialize_success();

	/* NULL config is a no-op. */
	pm_conn_sec_config_reply(CONN_HANDLE_1, NULL);
}

void test_pm_conn_sec_params_reply_success(void)
{
	uint32_t nrf_err;
	struct test_sec_params_reply_context context = {0};
	ble_gap_sec_params_t sec_param = {
		.bond = 1, .mitm = 1, .lesc = 1, .keypress = 0,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 1,
		.min_key_size = 7, .max_key_size = 16,
		.kdist_own = {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};

	peer_manager_initialize_success();

	nrf_err = pm_conn_sec_params_reply(CONN_HANDLE_1, &sec_param, &context);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(context.params_reply_called);
	TEST_ASSERT_EQUAL(1, context.sec_params_mem.bond);
}

void test_pm_conn_sec_params_reply_null_context(void)
{
	uint32_t nrf_err;
	ble_gap_sec_params_t sec_param = {
		.bond = 1, .mitm = 1, .lesc = 1, .keypress = 0,
		.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY, .oob = 1,
		.min_key_size = 7, .max_key_size = 16,
		.kdist_own = {.enc = 1, .id = 1},
		.kdist_peer = {.enc = 1, .id = 1},
	};

	peer_manager_initialize_success();

	nrf_err = pm_conn_sec_params_reply(CONN_HANDLE_1, &sec_param, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_pm_conn_sec_status_get_null(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	nrf_err = pm_conn_sec_status_get(CONN_HANDLE_1, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_pm_conn_sec_status_get_invalid_conn_handle(void)
{
	uint32_t nrf_err;
	struct pm_conn_sec_status conn_sec_status;

	peer_manager_initialize_success();

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(BLE_CONN_HANDLE_INVALID, -1);

	nrf_err = pm_conn_sec_status_get(BLE_CONN_HANDLE_INVALID, &conn_sec_status);
	TEST_ASSERT_EQUAL(BLE_ERROR_INVALID_CONN_HANDLE, nrf_err);


}

void test_pm_sec_is_sufficient_invalid_conn_handle(void)
{
	bool sec_sufficient;
	struct pm_conn_sec_status sec_status_req = {0};

	peer_manager_initialize_success();

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(BLE_CONN_HANDLE_INVALID, -1);

	sec_sufficient = pm_sec_is_sufficient(BLE_CONN_HANDLE_INVALID, &sec_status_req);
	TEST_ASSERT_FALSE(sec_sufficient);
}

void test_pm_sec_is_sufficient_connected_unencrypted(void)
{
	bool sec_sufficient;
	union pm_entry_id entry;
	const ble_evt_t evt = {
		.header.evt_id = BLE_GAP_EVT_CONNECTED,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE_1,
			.params.connected = {
				.peer_addr = ADDRESS_PUBLIC_1,
				.role = BLE_GAP_ROLE_PERIPH,
			},
		},
	};
	struct pm_conn_sec_status sec_status_req = {.connected = true, .encrypted = true};

	peer_manager_initialize_success();

	__cmock_nrf_sdh_ble_idx_get_Stub(stub_nrf_sdh_ble_idx_get);
	__cmock_nrf_sdh_ble_conn_handle_get_Stub(stub_nrf_sdh_ble_conn_handle_get);

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	entry.peer_id = 0;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
		entry.peer_id++;
	}

	sdh_evt_dispatch_ble(&evt);

	sec_sufficient = pm_sec_is_sufficient(CONN_HANDLE_1, &sec_status_req);
	TEST_ASSERT_FALSE(sec_sufficient);

	sec_status_req.encrypted = false;

	sec_sufficient = pm_sec_is_sufficient(CONN_HANDLE_1, &sec_status_req);
	TEST_ASSERT_TRUE(sec_sufficient);
}

void test_pm_lesc_public_key_set_forbidden(void)
{
	uint32_t nrf_err;
	ble_gap_lesc_p256_pk_t public_key = {0};

	peer_manager_initialize_success();

	nrf_err = pm_lesc_public_key_set(&public_key);
	TEST_ASSERT_EQUAL(NRF_ERROR_FORBIDDEN, nrf_err);
}

void test_pm_allow_list_clear(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	__cmock_sd_ble_gap_whitelist_set_ExpectAndReturn(NULL, 0, NRF_SUCCESS);

	nrf_err = pm_allow_list_set(NULL, 0);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_pm_allow_list_get_empty(void)
{
	uint32_t nrf_err;
	ble_gap_addr_t addrs[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
	uint32_t addr_cnt = ARRAY_SIZE(addrs);

	peer_manager_initialize_success();

	nrf_err = pm_allow_list_get(addrs, &addr_cnt, NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, addr_cnt);
}

void test_pm_allow_list_get_null_buffers(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	nrf_err = pm_allow_list_get(NULL, NULL, NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_pm_device_identities_list_clear(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	__cmock_sd_ble_gap_device_identities_set_ExpectAndReturn(NULL, NULL, 0, NRF_SUCCESS);

	nrf_err = pm_device_identities_list_set(NULL, 0);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_pm_id_addr_get_null(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	nrf_err = pm_id_addr_get(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_pm_id_addr_set_get_success(void)
{
	uint32_t nrf_err;
	ble_gap_addr_t addr_set = ADDRESS_PUBLIC_1;
	ble_gap_addr_t addr_get;

	peer_manager_initialize_success();

	__cmock_sd_ble_gap_addr_set_ExpectWithArrayAndReturn(&addr_set, 1, NRF_SUCCESS);
	nrf_err = pm_id_addr_set(&addr_set);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gap_addr_get_ExpectAndReturn(&addr_get, NRF_SUCCESS);
	__cmock_sd_ble_gap_addr_get_IgnoreArg_p_addr();
	__cmock_sd_ble_gap_addr_get_ReturnMemThruPtr_p_addr(&addr_set, sizeof(addr_set));

	nrf_err = pm_id_addr_get(&addr_get);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL_MEMORY(&addr_set, &addr_get, sizeof(addr_set));
}

void test_pm_privacy_set_null(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	nrf_err = pm_privacy_set(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_pm_privacy_get_null(void)
{
	uint32_t nrf_err;
	ble_gap_privacy_params_t privacy_params = {0};

	peer_manager_initialize_success();

	nrf_err = pm_privacy_get(&privacy_params);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = pm_privacy_get(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_pm_address_resolve_null(void)
{
	ble_gap_addr_t addr = ADDRESS_PUBLIC_1;
	ble_gap_irk_t irk = {0};

	peer_manager_initialize_success();

	TEST_ASSERT_FALSE(pm_address_resolve(NULL, &irk));
	TEST_ASSERT_FALSE(pm_address_resolve(&addr, NULL));
}

void test_pm_address_resolve_wrong_addr_type(void)
{
	ble_gap_addr_t addr = ADDRESS_PUBLIC_1;
	ble_gap_irk_t irk = {.irk = {1}};

	peer_manager_initialize_success();

	TEST_ASSERT_FALSE(pm_address_resolve(&addr, &irk));
}

void test_pm_address_resolve_success(void)
{
	const uint8_t prand[3] = {0x11, 0x22, 0x33};
	ble_gap_addr_t addr = {
		.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE,
		.addr = {prand[0], prand[1], prand[2], prand[0], prand[1], prand[2]},
	};
	ble_gap_irk_t irk = {.irk = {0x01, 0x02, 0x03, 0x04}};

	peer_manager_initialize_success();

	__cmock_sd_ecb_block_encrypt_Stub(stub_sd_ecb_block_encrypt_match_prand);

	TEST_ASSERT_TRUE(pm_address_resolve(&addr, &irk));
}

void test_pm_address_resolve_mismatch(void)
{
	const uint8_t prand[3] = {0x11, 0x22, 0x33};
	ble_gap_addr_t addr = {
		.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE,
		.addr = {0xAA, 0xBB, 0xCC, prand[0], prand[1], prand[2]},
	};
	ble_gap_irk_t irk = {.irk = {0x01, 0x02, 0x03, 0x04}};

	peer_manager_initialize_success();

	__cmock_sd_ecb_block_encrypt_Stub(stub_sd_ecb_block_encrypt_match_prand);

	TEST_ASSERT_FALSE(pm_address_resolve(&addr, &irk));
}

void test_pm_conn_handle_get_null(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	nrf_err = pm_conn_handle_get(0, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_pm_peer_id_get_null(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	nrf_err = pm_peer_id_get(CONN_HANDLE_1, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_pm_conn_handle_get_and_peer_id_get_not_connected(void)
{
	uint32_t nrf_err;
	uint16_t conn_handle;
	uint16_t peer_id;

	peer_manager_initialize_success();

	/* Use a peer ID that is not associated with any connection. */
	nrf_err = pm_conn_handle_get(1, &conn_handle);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, conn_handle);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE_1, -1);

	nrf_err = pm_peer_id_get(CONN_HANDLE_1, &peer_id);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(PM_PEER_ID_INVALID, peer_id);
}

void test_pm_peer_count_empty(void)
{
	peer_manager_initialize_success();

	TEST_ASSERT_EQUAL(0, pm_peer_count());
}

void test_pm_next_peer_id_get_empty(void)
{
	peer_manager_initialize_success();

	TEST_ASSERT_EQUAL(PM_PEER_ID_INVALID, pm_next_peer_id_get(PM_PEER_ID_INVALID));
}

void test_pm_peer_id_list_null(void)
{
	uint32_t nrf_err;
	uint32_t list_size = 1;
	uint16_t peer_list[1];

	peer_manager_initialize_success();

	nrf_err = pm_peer_id_list(NULL, &list_size, PM_PEER_ID_INVALID, PM_PEER_ID_LIST_ALL_ID);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = pm_peer_id_list(peer_list, NULL, PM_PEER_ID_INVALID, PM_PEER_ID_LIST_ALL_ID);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_pm_peer_id_list_empty(void)
{
	uint32_t nrf_err;
	uint32_t list_size = 4;
	uint16_t peer_list[4] = {0};

	peer_manager_initialize_success();

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_ALL_ID);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, list_size);
}

void test_pm_peer_id_list_invalid_param(void)
{
	uint32_t nrf_err;
	uint32_t list_size = 0;
	uint16_t peer_list[1];

	peer_manager_initialize_success();

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_ALL_ID);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_pm_peer_data_load_null(void)
{
	uint32_t nrf_err;
	uint32_t len = PM_PEER_DATA_MAX_SIZE;
	uint8_t data[PM_PEER_DATA_MAX_SIZE];

	peer_manager_initialize_success();

	nrf_err = pm_peer_data_load(0, PM_PEER_DATA_ID_APPLICATION, NULL, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = pm_peer_data_load(0, PM_PEER_DATA_ID_APPLICATION, data, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_pm_peer_data_load_not_found(void)
{
	uint32_t nrf_err;
	union pm_entry_id entry = {.peer_id = 0, .data_id = PM_PEER_DATA_ID_APPLICATION};
	uint32_t len = PM_PEER_DATA_MAX_SIZE;
	uint8_t data[PM_PEER_DATA_MAX_SIZE];

	peer_manager_initialize_success();

	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, len, -ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();

	nrf_err = pm_peer_data_load(entry.peer_id, entry.data_id, data, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, nrf_err);
}

void test_pm_peer_data_delete_bonding_invalid(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	nrf_err = pm_peer_data_delete(0, PM_PEER_DATA_ID_BONDING);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_pm_peer_new_null(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	struct pm_peer_data_bonding bonding_data = {0};

	peer_manager_initialize_success();

	nrf_err = pm_peer_new(NULL, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = pm_peer_new(&new_peer_id, NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_pm_peer_new_success(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}

	entry.peer_id = 0;
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));

	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, new_peer_id);
	TEST_ASSERT_EQUAL(1, pm_peer_count());
}

void test_pm_peers_delete_empty(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	nrf_err = pm_peers_delete();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_pm_peer_ranks_get_not_supported(void)
{
	uint32_t nrf_err;
	uint16_t highest_peer;
	uint32_t highest_rank;

	peer_manager_initialize_success();

	nrf_err = pm_peer_ranks_get(&highest_peer, &highest_rank, NULL, NULL);
#if defined(CONFIG_PM_PEER_RANKS)
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, nrf_err);
#else
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_SUPPORTED, nrf_err);
#endif
}

void test_pm_local_database_has_changed(void)
{
	peer_manager_initialize_success();

	pm_local_database_has_changed();
}

void test_pm_peer_data_bonding_load_not_found(void)
{
	uint32_t nrf_err;
	union pm_entry_id entry = {.peer_id = 0, .data_id = PM_PEER_DATA_ID_BONDING};
	struct pm_peer_data_bonding bonding_data;

	peer_manager_initialize_success();

	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(bonding_data), -ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();

	nrf_err = pm_peer_data_bonding_load(entry.peer_id, &bonding_data);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, nrf_err);
}

void test_pm_peer_data_store_null(void)
{
	uint32_t nrf_err;
	uint8_t data[4] __aligned(4) = {0};

	peer_manager_initialize_success();

	nrf_err = pm_peer_data_store(0, PM_PEER_DATA_ID_APPLICATION, NULL, sizeof(data), NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_pm_peer_delete_invalid_peer_id(void)
{
	uint32_t nrf_err;

	peer_manager_initialize_success();

	__cmock_nrf_sdh_ble_idx_get_IgnoreAndReturn(-1);

	nrf_err = pm_peer_delete(PM_PEER_ID_INVALID);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_pm_peer_data_store_forwards_update_succeeded_event(void)
{
	uint32_t nrf_err;
	const struct pm_evt *pm_evt;
	union pm_entry_id entry = {.peer_id = 0, .data_id = PM_PEER_DATA_ID_APPLICATION};
	uint8_t app_data[4] __aligned(4) = {0x01, 0x02, 0x03, 0x04};

	peer_manager_initialize_success();

	nrf_err = pm_register(on_pm_evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	on_pm_evt_capture_restart();

	__cmock_nrf_sdh_ble_idx_get_IgnoreAndReturn(-1);

	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, app_data, sizeof(app_data),
					    sizeof(app_data));
	nrf_err = pm_peer_data_app_data_store(entry.peer_id, app_data, sizeof(app_data), NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	invoke_zms_write_success(entry.id);

	pm_evt = on_pm_evt_find_last(PM_EVT_PEER_DATA_UPDATE_SUCCEEDED);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(PM_EVT_PEER_DATA_UPDATE_SUCCEEDED, pm_evt->evt_id);
	TEST_ASSERT_EQUAL(0, pm_evt->peer_id);
	TEST_ASSERT_EQUAL(PM_PEER_DATA_ID_APPLICATION,
			  pm_evt->peer_data_update_succeeded.data_id);
	TEST_ASSERT_EQUAL(PM_PEER_DATA_OP_UPDATE,
			  pm_evt->peer_data_update_succeeded.action);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
}

#if defined(CONFIG_PM_SERVICE_CHANGED)
void test_pm_local_database_has_changed_forwards_unexpected_error(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	union pm_entry_id entry;
	const struct pm_evt *pm_evt;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	nrf_err = pm_register(on_pm_evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	on_pm_evt_capture_restart();

	__cmock_nrf_sdh_ble_idx_get_IgnoreAndReturn(-1);

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.data_id = PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t), -EIO);
	__cmock_bm_zms_write_IgnoreArg_data();

	pm_local_database_has_changed();

	pm_evt = on_pm_evt_find_last(PM_EVT_ERROR_UNEXPECTED);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(PM_EVT_ERROR_UNEXPECTED, pm_evt->evt_id);
	TEST_ASSERT_EQUAL(0, pm_evt->peer_id);
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, pm_evt->error_unexpected.error);
	TEST_ASSERT_EQUAL(1, on_pm_evt_test_ctx.capture.count);
}
#endif /* CONFIG_PM_SERVICE_CHANGED */

void test_pm_privacy_set_get_success(void)
{
	uint32_t nrf_err;
	ble_gap_irk_t device_irk = {.irk = {0x01, 0x02, 0x03, 0x04}};
	ble_gap_privacy_params_t privacy_set = {
		.privacy_mode = BLE_GAP_PRIVACY_MODE_OFF,
		.private_addr_type = BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE,
		.p_device_irk = &device_irk,
	};
	ble_gap_privacy_params_t privacy_get = {
		.p_device_irk = &device_irk,
	};

	peer_manager_initialize_success();

	__cmock_sd_ble_gap_privacy_set_ExpectAndReturn(&privacy_set, NRF_SUCCESS);
	nrf_err = pm_privacy_set(&privacy_set);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gap_privacy_get_ExpectAndReturn(&privacy_get, NRF_SUCCESS);
	nrf_err = pm_privacy_get(&privacy_get);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_pm_peer_data_store_and_load_app_data_success(void)
{
	uint32_t nrf_err;
	union pm_entry_id entry = {.peer_id = 0, .data_id = PM_PEER_DATA_ID_APPLICATION};
	uint8_t app_data[8] __aligned(4) = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	uint8_t read_buf[8] __aligned(4);
	uint32_t len = sizeof(app_data);

	peer_manager_initialize_success();

	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, app_data, sizeof(app_data),
					       sizeof(app_data));
	nrf_err = pm_peer_data_app_data_store(entry.peer_id, app_data, sizeof(app_data), NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, len, len);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(app_data, sizeof(app_data));

	nrf_err = pm_peer_data_app_data_load(entry.peer_id, read_buf, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL_MEMORY(app_data, read_buf, sizeof(app_data));
}

void test_pm_peer_data_remote_db_store_and_load_success(void)
{
	uint32_t nrf_err;
	union pm_entry_id entry = {.peer_id = 0, .data_id = PM_PEER_DATA_ID_GATT_REMOTE};
	struct ble_gatt_db_srv remote_db __aligned(4) = {
		.srv_uuid = {
			.uuid = 0x1800,
			.type = BLE_UUID_TYPE_BLE,
		},
		.char_count = 0,
		.handle_range = {
			.start_handle = 0x0001,
			.end_handle = 0x000A,
		},
	};
	struct ble_gatt_db_srv read_db __aligned(4);
	uint32_t len = sizeof(remote_db);

	peer_manager_initialize_success();

	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &remote_db, len, len);
	nrf_err = pm_peer_data_remote_db_store(entry.peer_id, &remote_db, len, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, len, len);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(&remote_db, sizeof(remote_db));

	nrf_err = pm_peer_data_remote_db_load(entry.peer_id, &read_db, &len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL_MEMORY(&remote_db, &read_db, sizeof(remote_db));
}

void test_pm_peer_data_remote_db_load_not_found(void)
{
	uint32_t nrf_err;
	union pm_entry_id entry = {.peer_id = 0, .data_id = PM_PEER_DATA_ID_GATT_REMOTE};
	struct ble_gatt_db_srv read_db __aligned(4);
	uint32_t len = sizeof(read_db);

	peer_manager_initialize_success();

	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, len, -ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();

	nrf_err = pm_peer_data_remote_db_load(entry.peer_id, &read_db, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_NOT_FOUND, nrf_err);
}

void test_pm_peer_data_store_unaligned(void)
{
	uint32_t nrf_err;
	uint8_t buffer[8] __aligned(4);
	const void *unaligned = &buffer[1];

	peer_manager_initialize_success();

	nrf_err = pm_peer_data_store(0, PM_PEER_DATA_ID_APPLICATION, unaligned,
				     sizeof(buffer) - 1, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_ADDR, nrf_err);
}

void test_pm_peer_data_delete_app_data_success(void)
{
	uint32_t nrf_err;
	union pm_entry_id entry = {.peer_id = 0, .data_id = PM_PEER_DATA_ID_APPLICATION};

	peer_manager_initialize_success();

	__cmock_bm_zms_delete_ExpectAndReturn(zms_fs, entry.id, 0);
	nrf_err = pm_peer_data_delete(entry.peer_id, entry.data_id);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_pm_peer_new_duplicate_returns_existing_peer(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	/* Create the first peer. */
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, new_peer_id);

	/* Search finds the existing bond at peer 0; no new write. */
	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, PM_PEER_DATA_MAX_SIZE,
					    sizeof(bonding_data));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(&bonding_data, sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, new_peer_id);
}

void test_pm_peer_id_list_with_stored_peer(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	uint32_t list_size = 2;
	uint16_t peer_list[2];
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_ALL_ID);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, list_size);
	TEST_ASSERT_EQUAL(0, peer_list[0]);
	TEST_ASSERT_EQUAL(1, pm_peer_count());
}

void test_pm_peers_delete_with_stored_peer(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();
	__cmock_nrf_sdh_ble_idx_get_IgnoreAndReturn(-1);

	/* Add a bonded peer. */
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, pm_peer_count());

	/* pm_peers_delete() scans data IDs 0..7 until bonding is found, then deletes it. */
	entry.peer_id = 0;
	for (uint32_t data_id = 0; data_id <= PM_PEER_DATA_ID_BONDING; data_id++) {
		entry.data_id = data_id;
		if (data_id == PM_PEER_DATA_ID_BONDING) {
			__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
							    PM_PEER_DATA_MAX_SIZE,
							    sizeof(bonding_data));
			__cmock_bm_zms_read_IgnoreArg_data();
			__cmock_bm_zms_read_ReturnMemThruPtr_data(&bonding_data,
								  sizeof(bonding_data));
		} else {
			__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
							    PM_PEER_DATA_MAX_SIZE, -ENOENT);
			__cmock_bm_zms_read_IgnoreArg_data();
		}
	}
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	__cmock_bm_zms_read_IgnoreAndReturn(-ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_delete_Stub(stub_bm_zms_delete_invoke_handler);

	nrf_err = pm_peers_delete();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, pm_peer_count());
}

void test_pm_peer_data_bonding_store_duplicate_forbidden(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, PM_PEER_DATA_MAX_SIZE,
					    sizeof(bonding_data));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(&bonding_data, sizeof(bonding_data));

	nrf_err = pm_peer_data_bonding_store(1, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_FORBIDDEN, nrf_err);
}

void test_pm_peer_data_bonding_store_success(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, PM_PEER_DATA_MAX_SIZE,
					    sizeof(bonding_data));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(&bonding_data, sizeof(bonding_data));
	for (uint32_t i = 1; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_data_bonding_store(0, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_pm_peer_id_list_skip_no_irk_filters_peer(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	uint32_t list_size = 1;
	uint16_t peer_list[1];
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(struct pm_peer_data_bonding),
					    sizeof(bonding_data));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(&bonding_data, sizeof(bonding_data));

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_SKIP_NO_IRK);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, list_size);
}

void test_pm_peer_id_list_includes_peer_with_irk(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	uint32_t list_size = 1;
	uint16_t peer_list[1];
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
		.peer_ble_id.id_info.irk = {0x01, 0x02, 0x03},
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(struct pm_peer_data_bonding),
					    sizeof(bonding_data));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(&bonding_data, sizeof(bonding_data));

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_SKIP_NO_IRK);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, list_size);
	TEST_ASSERT_EQUAL(0, peer_list[0]);
}

void test_pm_peer_id_list_invalid_skip_flags(void)
{
	uint32_t nrf_err;
	uint32_t list_size = 1;
	uint16_t peer_list[1];

	peer_manager_initialize_success();

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID, 8);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_pm_peer_id_list_skip_no_id_addr_filters_non_static_address(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	uint32_t list_size = 1;
	uint16_t peer_list[1];
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_RANDOM_PRIVATE,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(struct pm_peer_data_bonding),
					    sizeof(bonding_data));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(&bonding_data, sizeof(bonding_data));

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, list_size);
}

void test_pm_peer_id_list_skip_no_id_addr_includes_public_address(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	uint32_t list_size = 1;
	uint16_t peer_list[1];
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(struct pm_peer_data_bonding),
					    sizeof(bonding_data));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(&bonding_data, sizeof(bonding_data));

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, list_size);
	TEST_ASSERT_EQUAL(0, peer_list[0]);
}

void test_pm_peer_id_list_skip_no_car_filters_zero(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	uint32_t list_size = 1;
	uint16_t peer_list[1];
	union pm_entry_id entry;
	uint32_t car_zero = 0;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.data_id = PM_PEER_DATA_ID_CENTRAL_ADDR_RES;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &car_zero, sizeof(car_zero),
					    sizeof(car_zero));
	nrf_err = pm_peer_data_store(0, PM_PEER_DATA_ID_CENTRAL_ADDR_RES, &car_zero,
				     sizeof(car_zero), NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.data_id = PM_PEER_DATA_ID_CENTRAL_ADDR_RES;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t),
					    sizeof(car_zero));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(&car_zero, sizeof(car_zero));

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_SKIP_NO_CAR);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, list_size);
}

void test_pm_peer_id_list_skip_no_car_includes_nonzero(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	uint32_t list_size = 1;
	uint16_t peer_list[1];
	union pm_entry_id entry;
	uint32_t car_one = 1;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.data_id = PM_PEER_DATA_ID_CENTRAL_ADDR_RES;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &car_one, sizeof(car_one),
					    sizeof(car_one));
	nrf_err = pm_peer_data_store(0, PM_PEER_DATA_ID_CENTRAL_ADDR_RES, &car_one,
				     sizeof(car_one), NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.data_id = PM_PEER_DATA_ID_CENTRAL_ADDR_RES;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t),
					    sizeof(car_one));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(&car_one, sizeof(car_one));

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_SKIP_NO_CAR);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, list_size);
	TEST_ASSERT_EQUAL(0, peer_list[0]);
}

void test_pm_peer_id_list_starts_from_first_peer_id(void)
{
	uint32_t nrf_err;
	uint16_t peer_id_0;
	uint16_t peer_id_1;
	uint32_t list_size = 2;
	uint16_t peer_list[2];
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_0 = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};
	struct pm_peer_data_bonding bonding_1 = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_2,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_0, sizeof(bonding_0),
					       sizeof(bonding_0));
	nrf_err = pm_peer_new(&peer_id_0, &bonding_0, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, PM_PEER_DATA_MAX_SIZE,
					    sizeof(bonding_0));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(&bonding_0, sizeof(bonding_0));
	for (uint32_t i = 1; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 1;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_1, sizeof(bonding_1),
					       sizeof(bonding_1));
	nrf_err = pm_peer_new(&peer_id_1, &bonding_1, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, peer_id_1);

	nrf_err = pm_peer_id_list(peer_list, &list_size, 1, PM_PEER_ID_LIST_ALL_ID);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, list_size);
	TEST_ASSERT_EQUAL(1, peer_list[0]);
}

void test_pm_peer_id_list_list_size_limits_output(void)
{
	uint32_t nrf_err;
	uint16_t peer_id_0;
	uint16_t peer_id_1;
	uint32_t list_size = 1;
	uint16_t peer_list[2];
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_0 = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};
	struct pm_peer_data_bonding bonding_1 = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_2,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_0, sizeof(bonding_0),
					       sizeof(bonding_0));
	nrf_err = pm_peer_new(&peer_id_0, &bonding_0, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, PM_PEER_DATA_MAX_SIZE,
					    sizeof(bonding_0));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(&bonding_0, sizeof(bonding_0));
	for (uint32_t i = 1; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 1;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_1, sizeof(bonding_1),
					       sizeof(bonding_1));
	nrf_err = pm_peer_new(&peer_id_1, &bonding_1, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_ALL_ID);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, list_size);
	TEST_ASSERT_EQUAL(0, peer_list[0]);
}

void test_pm_peer_id_list_bonding_read_error(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	uint32_t list_size = 1;
	uint16_t peer_list[1];
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(struct pm_peer_data_bonding), -EIO);
	__cmock_bm_zms_read_IgnoreArg_data();

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_SKIP_NO_IRK);
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);
}

void test_pm_peer_id_list_skip_bonding_not_found(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	uint32_t list_size = 1;
	uint16_t peer_list[1];
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(struct pm_peer_data_bonding), -ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_SKIP_NO_IRK);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, list_size);
}

void test_pm_peer_id_list_skip_no_id_addr_includes_random_static(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	uint32_t list_size = 1;
	uint16_t peer_list[1];
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_RANDOM_STATIC,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.peer_id = 0;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
					    sizeof(struct pm_peer_data_bonding),
					    sizeof(bonding_data));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(&bonding_data, sizeof(bonding_data));

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, list_size);
	TEST_ASSERT_EQUAL(0, peer_list[0]);
}

void test_pm_peer_id_list_skip_car_not_found(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	uint32_t list_size = 1;
	uint16_t peer_list[1];
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.peer_id = 0;
	entry.data_id = PM_PEER_DATA_ID_CENTRAL_ADDR_RES;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t),
					    -ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_SKIP_NO_CAR);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, list_size);
}

void test_pm_peer_id_list_car_read_error(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	uint32_t list_size = 1;
	uint16_t peer_list[1];
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.peer_id = 0;
	entry.data_id = PM_PEER_DATA_ID_CENTRAL_ADDR_RES;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t), -EIO);
	__cmock_bm_zms_read_IgnoreArg_data();

	nrf_err = pm_peer_id_list(peer_list, &list_size, PM_PEER_ID_INVALID,
				  PM_PEER_ID_LIST_SKIP_NO_CAR);
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);
}

void test_pm_peers_delete_async_sends_peers_delete_succeeded(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	union pm_entry_id entry;
	const struct pm_evt *pm_evt;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	nrf_err = pm_register(on_pm_evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	on_pm_evt_capture_restart();

	__cmock_nrf_sdh_ble_idx_get_IgnoreAndReturn(-1);

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.peer_id = 0;
	for (uint32_t data_id = 0; data_id <= PM_PEER_DATA_ID_BONDING; data_id++) {
		entry.data_id = data_id;
		if (data_id == PM_PEER_DATA_ID_BONDING) {
			__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
							    PM_PEER_DATA_MAX_SIZE,
							    sizeof(bonding_data));
			__cmock_bm_zms_read_IgnoreArg_data();
			__cmock_bm_zms_read_ReturnMemThruPtr_data(&bonding_data,
								  sizeof(bonding_data));
		} else {
			__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
							    PM_PEER_DATA_MAX_SIZE, -ENOENT);
			__cmock_bm_zms_read_IgnoreArg_data();
		}
	}
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	__cmock_bm_zms_read_IgnoreAndReturn(-ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_delete_Stub(stub_bm_zms_delete_invoke_handler);

	nrf_err = pm_peers_delete();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, pm_peer_count());

	pm_evt = on_pm_evt_find_last(PM_EVT_PEERS_DELETE_SUCCEEDED);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(PM_EVT_PEERS_DELETE_SUCCEEDED, pm_evt->evt_id);
	TEST_ASSERT_EQUAL(2, on_pm_evt_test_ctx.capture.count);
}

void test_pm_peer_ranks_get_success_with_stored_rank(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	uint16_t highest_peer;
	uint32_t highest_rank;
	union pm_entry_id entry;
	uint32_t rank_val = 1;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.data_id = PM_PEER_DATA_ID_PEER_RANK;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t),
					    -ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t),
					    sizeof(uint32_t));
	__cmock_bm_zms_write_IgnoreArg_data();
	nrf_err = pm_peer_rank_highest(0);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.data_id = PM_PEER_DATA_ID_PEER_RANK;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t),
					    sizeof(rank_val));
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_read_ReturnMemThruPtr_data(&rank_val, sizeof(rank_val));

	nrf_err = pm_peer_ranks_get(&highest_peer, &highest_rank, NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, highest_peer);
	TEST_ASSERT_EQUAL(1, highest_rank);
}

void test_pm_peer_rank_highest_success(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	entry.data_id = PM_PEER_DATA_ID_BONDING;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.data_id = PM_PEER_DATA_ID_PEER_RANK;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t),
					    -ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t),
					    sizeof(uint32_t));
	__cmock_bm_zms_write_IgnoreArg_data();

	nrf_err = pm_peer_rank_highest(0);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

#if defined(CONFIG_PM_PEER_RANKS)
void test_pm_peer_rank_highest_busy_while_store_pending(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.data_id = PM_PEER_DATA_ID_PEER_RANK;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t),
					    -ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t),
					    sizeof(uint32_t));
	__cmock_bm_zms_write_IgnoreArg_data();
	nrf_err = pm_peer_rank_highest(0);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Store is still in progress (token not cleared). */
	nrf_err = pm_peer_rank_highest(0);
	TEST_ASSERT_EQUAL(NRF_ERROR_BUSY, nrf_err);
}

void test_pm_peer_rank_highest_already_highest_sends_event(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	union pm_entry_id entry;
	const struct pm_evt *pm_evt;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	nrf_err = pm_register(on_pm_evt);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	on_pm_evt_capture_restart();

	__cmock_nrf_sdh_ble_idx_get_IgnoreAndReturn(-1);

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.data_id = PM_PEER_DATA_ID_PEER_RANK;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t),
					    -ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t),
					    sizeof(uint32_t));
	__cmock_bm_zms_write_IgnoreArg_data();
	nrf_err = pm_peer_rank_highest(0);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Complete the async rank store so peer 0 is recorded as highest. */
	invoke_zms_write_success(entry.id);

	nrf_err = pm_peer_rank_highest(0);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	pm_evt = on_pm_evt_find_last(PM_EVT_PEER_DATA_UPDATE_SUCCEEDED);
	TEST_ASSERT_NOT_NULL(pm_evt);
	TEST_ASSERT_EQUAL(PM_EVT_PEER_DATA_UPDATE_SUCCEEDED, pm_evt->evt_id);
	TEST_ASSERT_EQUAL(0, pm_evt->peer_id);
	TEST_ASSERT_EQUAL(PM_PEER_DATA_ID_PEER_RANK,
			  pm_evt->peer_data_update_succeeded.data_id);
	TEST_ASSERT_FALSE(pm_evt->peer_data_update_succeeded.flash_changed);
	TEST_ASSERT_EQUAL(2, on_pm_evt_test_ctx.capture.count);
}

void test_pm_peer_rank_highest_store_failure(void)
{
	uint32_t nrf_err;
	uint16_t new_peer_id;
	union pm_entry_id entry;
	struct pm_peer_data_bonding bonding_data = {
		.own_role = BLE_GAP_ROLE_PERIPH,
		.peer_ble_id.id_addr_info = ADDRESS_PUBLIC_1,
	};

	peer_manager_initialize_success();

	entry.data_id = PM_PEER_DATA_ID_BONDING;
	for (uint32_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		entry.peer_id = i;
		__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE,
						    PM_PEER_DATA_MAX_SIZE, -ENOENT);
		__cmock_bm_zms_read_IgnoreArg_data();
	}
	entry.peer_id = 0;
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, &bonding_data,
					       sizeof(bonding_data), sizeof(bonding_data));
	nrf_err = pm_peer_new(&new_peer_id, &bonding_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	entry.data_id = PM_PEER_DATA_ID_PEER_RANK;
	__cmock_bm_zms_read_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t),
					    -ENOENT);
	__cmock_bm_zms_read_IgnoreArg_data();
	__cmock_bm_zms_write_ExpectAndReturn(zms_fs, entry.id, PTR_IGNORE, sizeof(uint32_t), -EIO);
	__cmock_bm_zms_write_IgnoreArg_data();
	nrf_err = pm_peer_rank_highest(0);
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);
}

#endif /* CONFIG_PM_PEER_RANKS */

void setUp(void)
{
}

void tearDown(void)
{
	uint32_t nrf_err;

	/* Reset behavior for on_pm_evt function and pm_evt capture before the next test. */
	memset(&on_pm_evt_test_ctx, 0, sizeof(on_pm_evt_test_ctx));

	/* Reset values of zms test variables before the next test. */
	if (zms_fs != NULL) {
		zms_fs->init_flags.initialized = false;
		zms_fs = NULL;
	}
	zms_handler = NULL;

	/* Clear the public key before the next test. */
	ble_gap_lesc_p256_pk_t *pub_key = nrf_ble_lesc_public_key_get();

	if (pub_key != NULL) {
		memset(pub_key->pk, 0, sizeof(pub_key->pk));
	}

	/* Reset the internal nrf_ble_lesc variables before the next test. */
	__cmock_psa_crypto_init_ExpectAndReturn(PSA_SUCCESS);
	__cmock_psa_set_key_usage_flags_ExpectAnyArgs();
	__cmock_psa_set_key_lifetime_ExpectAnyArgs();
	__cmock_psa_set_key_algorithm_ExpectAnyArgs();
	__cmock_psa_set_key_type_ExpectAnyArgs();
	__cmock_psa_set_key_bits_ExpectAnyArgs();
	__cmock_psa_destroy_key_ExpectAnyArgsAndReturn(PSA_ERROR_INVALID_HANDLE);
	__cmock_psa_generate_key_ExpectAnyArgsAndReturn(PSA_ERROR_BAD_STATE);

	nrf_err = nrf_ble_lesc_init();
	TEST_ASSERT_EQUAL(NRF_ERROR_INTERNAL, nrf_err);
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
