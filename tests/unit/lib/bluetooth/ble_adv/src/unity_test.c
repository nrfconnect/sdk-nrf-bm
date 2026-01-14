/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unity.h>

#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/ble_adv_data.h>
#include <bm/bluetooth/services/common.h>

#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include "cmock_ble.h"
#include "cmock_ble_gap.h"

/* Macro for skipping a test if a condition is met.
 *
 * Place at the start of a test function.
 * Use with IS_ENABLED(...) to filter tests based on Kconfigs.
 */
#define TEST_SKIP_IF(conditional)                                                                  \
	if (conditional) {                                                                         \
		TEST_IGNORE();                                                                     \
	}

/* Macro for running a test only if a condition is met.
 *
 * Place at the start of a test function.
 * Use with IS_ENABLED(...) to filter tests based on Kconfigs.
 */
#define TEST_RUN_ONLY_IF(conditional) TEST_SKIP_IF(!(conditional))

/* Number of ble_adv event types. */
#define NUM_ADV_EVT_TYPES (BLE_ADV_EVT_ERROR + 1)

/* Values for testing. */
#define TEST_CONN_CFG_TAG (uint8_t)(42)
#define TEST_CONN_CFG_TAG_2 (uint8_t)(43)
#define TEST_ADV_SET_HANDLE (uint8_t)(93)
#define TEST_APPEARANCE BLE_APPEARANCE_GENERIC_HID
#define TEST_CONN_HANDLE (uint16_t)(74)
#define TEST_CONN_HANDLE_2 (uint16_t)(83)
#define TEST_UUID_1 (uint16_t)(0xCAFE)
#define TEST_UUID_2 (uint16_t)(0xBEEF)
#define TEST_UUID_3 (uint16_t)(0x1337)
#define TEST_UUID_4 (uint16_t)(0x5ACE)
#define TEST_UUID_TYPE_1 (uint8_t)(BLE_UUID_TYPE_VENDOR_BEGIN + 42)
#define TEST_UUID_1_BYTES 0xCA, 0xFE
#define TEST_UUID_2_BYTES 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,                          \
			  0x28, 0x29, 0x2A, 0x2B, 0xBE, 0xEF, 0x2E, 0x2F
#define TEST_UUID_3_BYTES 0x13, 0x37
#define TEST_UUID_4_BYTES 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,                          \
			  0x28, 0x29, 0x2A, 0x2B, 0xCE, 0x2E, 0x2E, 0x2F
#define AD_LENGTH_FIELD_SIZE sizeof(uint8_t)
#define AD_TYPE_FIELD_SIZE sizeof(uint8_t)
#define AD_FLAGS_DATA_SIZE sizeof(uint8_t)
#define AD_UUID_16_DATA_SIZE sizeof(uint16_t)
#define AD_UUID_128_DATA_SIZE 8*sizeof(uint16_t)

static const ble_gap_conn_sec_mode_t sec_mode_open = BLE_GAP_CONN_SEC_MODE_OPEN;

static const ble_gap_adv_params_t init_adv_params = {
	.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED,
	.interval = BLE_GAP_ADV_INTERVAL_MAX,
	.duration = BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED,
	.filter_policy = BLE_GAP_ADV_FP_ANY,
	.primary_phy = BLE_GAP_PHY_AUTO,
};

static const ble_gap_addr_t test_addr_invalid = {
	.addr_id_peer = false,
	.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC,
	.addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

static const ble_gap_addr_t test_addr1 = {
	.addr_id_peer = false,
	.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC,
	.addr = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11},
};

static const ble_gap_addr_t test_addr2 = {
	.addr_id_peer = false,
	.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC,
	.addr = {0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77},
};

static const ble_gap_addr_t test_addr3 = {
	.addr_id_peer = false,
	.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC,
	.addr = {0xf1, 0xe2, 0xd3, 0xc4, 0xb5, 0xa6},
};

static const ble_gap_addr_t test_addrs[] = {
	test_addr2, test_addr3,
};

#if defined(CONFIG_BLE_ADV_FAST_ADVERTISING) || defined(CONFIG_BLE_ADV_SLOW_ADVERTISING)
static const uint8_t ad_flags_le_only[] = {
	/* AD length, AD type, AD data. */
	(AD_TYPE_FIELD_SIZE + AD_FLAGS_DATA_SIZE), BLE_GAP_AD_TYPE_FLAGS,
	BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED,
};
#endif /* CONFIG_BLE_ADV_FAST_ADVERTISING || CONFIG_BLE_ADV_SLOW_ADVERTISING */

static ble_uuid_t test_uuid_list_1[] = {
	{.uuid = TEST_UUID_1, .type = BLE_UUID_TYPE_BLE},
	{.uuid = TEST_UUID_2, .type = TEST_UUID_TYPE_1},
};
static ble_uuid_t test_uuid_list_2[] = {
	{.uuid = TEST_UUID_3, .type = BLE_UUID_TYPE_BLE},
	{.uuid = TEST_UUID_4, .type = TEST_UUID_TYPE_1},
};

static const uint8_t ad_uuid_list_1[] = {
	/* AD length, AD type, AD data. */
	(AD_TYPE_FIELD_SIZE + AD_UUID_16_DATA_SIZE), BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE,
	TEST_UUID_1_BYTES,
	/* AD length, AD type, AD data. */
	(AD_TYPE_FIELD_SIZE + AD_UUID_128_DATA_SIZE), BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE,
	TEST_UUID_2_BYTES,
};

static const uint8_t ad_uuid_list_2[] = {
	/* AD length, AD type, AD data. */
	(AD_TYPE_FIELD_SIZE + AD_UUID_16_DATA_SIZE), BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE,
	TEST_UUID_3_BYTES,
	/* AD length, AD type, AD data. */
	(AD_TYPE_FIELD_SIZE + AD_UUID_128_DATA_SIZE), BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE,
	TEST_UUID_4_BYTES,
};

/* Instance data used in all tests. */
static struct ble_adv ble_adv;
/* Keep a list of how many times each event have been raised. */
static unsigned int evts_raised_cnt[NUM_ADV_EVT_TYPES];
/* Keep a list of expected values to be compared to evts_raised_cnt. */
static unsigned int evts_raised_cnt_expectations[NUM_ADV_EVT_TYPES];

/* For checking how many times the specific stub have been called from the test function. */
static int stub_sd_ble_gap_adv_set_configure_num_calls;

/* Behavioral options for ble_adv_evt_handler(). */
static struct {
	uint8_t reply_with_peer_addr : 1;
	uint8_t reply_with_peer_addr_valid : 1;
	uint8_t reply_with_allow_list : 1;
} ble_adv_evt_handler_options;

static void ble_adv_evt_handler(struct ble_adv *adv, const struct ble_adv_evt *adv_evt)
{
	TEST_ASSERT_NOT_NULL(adv);
	TEST_ASSERT_NOT_NULL(adv_evt);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(0, adv_evt->evt_type);
	TEST_ASSERT_LESS_THAN_INT(NUM_ADV_EVT_TYPES, adv_evt->evt_type);

	evts_raised_cnt[adv_evt->evt_type] += 1;

	switch (adv_evt->evt_type) {
	case BLE_ADV_EVT_PEER_ADDR_REQUEST:
		if (ble_adv_evt_handler_options.reply_with_peer_addr) {
			if (ble_adv_evt_handler_options.reply_with_peer_addr_valid) {
				ble_adv_peer_addr_reply(adv, &test_addr1);
			} else {
				ble_adv_peer_addr_reply(adv, &test_addr_invalid);
			}
		}
		break;
	case BLE_ADV_EVT_ALLOW_LIST_REQUEST:
		if (ble_adv_evt_handler_options.reply_with_allow_list) {
			ble_adv_allow_list_reply(adv, test_addrs, ARRAY_SIZE(test_addrs), NULL, 0);
		}
	default:
		break;
	}
}

static void evts_raised_cnt_reset(void)
{
	for (uint32_t i = 0; i < NUM_ADV_EVT_TYPES; ++i) {
		evts_raised_cnt[i] = 0;
		evts_raised_cnt_expectations[i] = 0;
	}
}

static void evts_raised_cnt_expectation_set(enum ble_adv_evt_type adv_evt_type, unsigned int num)
{
	if (adv_evt_type >= 0 && adv_evt_type < NUM_ADV_EVT_TYPES) {
		evts_raised_cnt_expectations[adv_evt_type] = num;
	}
}

static bool evts_raised_cnt_expectations_met(void)
{
	for (uint32_t i = 0; i < NUM_ADV_EVT_TYPES; ++i) {
		if (evts_raised_cnt[i] != evts_raised_cnt_expectations[i]) {
			printk("Adv evt %d was raised %u time(s). Expected raised %u time(s).\n",
			       i, evts_raised_cnt[i], evts_raised_cnt_expectations[i]);
			return false;
		}
	}

	return true;
}

static void init_success(void)
{
	uint32_t nrf_err;
	struct ble_adv_config cfg = {
		.conn_cfg_tag = TEST_CONN_CFG_TAG,
		.evt_handler = ble_adv_evt_handler,
		.adv_data = {
			.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		},
	};

	__cmock_sd_ble_gap_device_name_set_ExpectWithArrayAndReturn(
		&sec_mode_open, 1, CONFIG_BLE_ADV_NAME, sizeof(CONFIG_BLE_ADV_NAME),
		sizeof(CONFIG_BLE_ADV_NAME) - 1, NRF_SUCCESS);

	__cmock_sd_ble_gap_adv_set_configure_ExpectWithArrayAndReturn(
		&(uint8_t){BLE_GAP_ADV_SET_HANDLE_NOT_SET}, 1,
		NULL, 0,
		&init_adv_params, 1,
		NRF_SUCCESS);
	__cmock_sd_ble_gap_adv_set_configure_ReturnMemThruPtr_p_adv_handle(
		&(uint8_t){TEST_ADV_SET_HANDLE}, sizeof(uint8_t));

	nrf_err = ble_adv_init(&ble_adv, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

static void init_without_ad_flags(void)
{
	uint32_t nrf_err;
	struct ble_adv_config cfg = {
		.conn_cfg_tag = TEST_CONN_CFG_TAG,
		.evt_handler = ble_adv_evt_handler,
	};

	__cmock_sd_ble_gap_device_name_set_ExpectWithArrayAndReturn(
		&sec_mode_open, 1, CONFIG_BLE_ADV_NAME, sizeof(CONFIG_BLE_ADV_NAME),
		sizeof(CONFIG_BLE_ADV_NAME) - 1, NRF_SUCCESS);

	__cmock_sd_ble_gap_adv_set_configure_ExpectWithArrayAndReturn(
		&(uint8_t){BLE_GAP_ADV_SET_HANDLE_NOT_SET}, 1,
		NULL, 0,
		&init_adv_params, 1,
		NRF_SUCCESS);
	__cmock_sd_ble_gap_adv_set_configure_ReturnMemThruPtr_p_adv_handle(
		&(uint8_t){TEST_ADV_SET_HANDLE}, sizeof(uint8_t));

	nrf_err = ble_adv_init(&ble_adv, &cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

#define TEST_ASSERT_ADV_MODE_DIRECTED_HD(_adv_handle, _adv_data, _adv_params)                      \
	/* Validate advertising handle. */                                                         \
	TEST_ASSERT_NOT_NULL(_adv_handle);                                                         \
	TEST_ASSERT_EQUAL(TEST_ADV_SET_HANDLE, *(_adv_handle));                                    \
	/* Validate advertising data. */                                                           \
	TEST_ASSERT_NULL(_adv_data);                                                               \
	/* Validate advertising parameters. */                                                     \
	TEST_ASSERT_NOT_NULL(_adv_params);                                                         \
	TEST_ASSERT_EQUAL(BLE_GAP_ADV_TYPE_CONNECTABLE_NONSCANNABLE_DIRECTED_HIGH_DUTY_CYCLE,      \
			  (_adv_params)->properties.type);                                         \
	TEST_ASSERT_EQUAL_MEMORY(&test_addr1, (_adv_params)->p_peer_addr, sizeof(test_addr1));     \
	TEST_ASSERT_EQUAL(0, (_adv_params)->interval);                                             \
	TEST_ASSERT_EQUAL(BLE_GAP_ADV_TIMEOUT_HIGH_DUTY_MAX, (_adv_params)->duration);             \
	TEST_ASSERT_EQUAL(BLE_GAP_ADV_FP_ANY, (_adv_params)->filter_policy);                       \
	TEST_ASSERT_EQUAL(CONFIG_BLE_ADV_PRIMARY_PHY, (_adv_params)->primary_phy);                 \
	TEST_ASSERT_EQUAL(CONFIG_BLE_ADV_SECONDARY_PHY, (_adv_params)->secondary_phy)

#define TEST_ASSERT_ADV_MODE_DIRECTED(_adv_handle, _adv_data, _adv_params)                         \
	/* Validate advertising handle. */                                                         \
	TEST_ASSERT_NOT_NULL(_adv_handle);                                                         \
	TEST_ASSERT_EQUAL(TEST_ADV_SET_HANDLE, *(_adv_handle));                                    \
	/* Validate advertising data. */                                                           \
	TEST_ASSERT_NULL(_adv_data);                                                               \
	/* Validate advertising parameters. */                                                     \
	TEST_ASSERT_NOT_NULL(_adv_params);                                                         \
	TEST_ASSERT_EQUAL(BLE_GAP_ADV_TYPE_CONNECTABLE_NONSCANNABLE_DIRECTED,                      \
			  (_adv_params)->properties.type);                                         \
	TEST_ASSERT_EQUAL_MEMORY(&test_addr1, (_adv_params)->p_peer_addr, sizeof(test_addr1));     \
	TEST_ASSERT_EQUAL(CONFIG_BLE_ADV_DIRECTED_ADVERTISING_INTERVAL, (_adv_params)->interval);  \
	TEST_ASSERT_EQUAL(CONFIG_BLE_ADV_DIRECTED_ADVERTISING_TIMEOUT, (_adv_params)->duration);   \
	TEST_ASSERT_EQUAL(BLE_GAP_ADV_FP_ANY, (_adv_params)->filter_policy);                       \
	TEST_ASSERT_EQUAL(CONFIG_BLE_ADV_PRIMARY_PHY, (_adv_params)->primary_phy);                 \
	TEST_ASSERT_EQUAL(CONFIG_BLE_ADV_SECONDARY_PHY, (_adv_params)->secondary_phy)

#define TEST_ASSERT_ADV_MODE_FAST(_adv_handle, _adv_data, _adv_params, _expect_use_allow_list)     \
	/* Validate advertising handle. */                                                         \
	TEST_ASSERT_NOT_NULL(_adv_handle);                                                         \
	TEST_ASSERT_EQUAL(TEST_ADV_SET_HANDLE, *(_adv_handle));                                    \
	/* Validate advertising data. */                                                           \
	TEST_ASSERT_NOT_NULL(_adv_data);                                                           \
	if (_expect_use_allow_list) {                                                              \
		TEST_ASSERT_EQUAL_MEMORY(ad_flags_le_only, (_adv_data)->adv_data.p_data,           \
					 sizeof(ad_flags_le_only));                                \
		TEST_ASSERT_EQUAL(sizeof(ad_flags_le_only), (_adv_data)->adv_data.len);            \
	}                                                                                          \
	/* Validate advertising parameters. */                                                     \
	TEST_ASSERT_NOT_NULL(_adv_params);                                                         \
	TEST_ASSERT_EQUAL(BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED,                       \
			  (_adv_params)->properties.type);                                         \
	TEST_ASSERT_EQUAL(CONFIG_BLE_ADV_FAST_ADVERTISING_INTERVAL, (_adv_params)->interval);      \
	TEST_ASSERT_EQUAL(CONFIG_BLE_ADV_FAST_ADVERTISING_TIMEOUT, (_adv_params)->duration);       \
	TEST_ASSERT_EQUAL((_expect_use_allow_list ? BLE_GAP_ADV_FP_FILTER_CONNREQ :                \
			   BLE_GAP_ADV_FP_ANY), (_adv_params)->filter_policy);                     \
	TEST_ASSERT_EQUAL(CONFIG_BLE_ADV_PRIMARY_PHY, (_adv_params)->primary_phy);                 \
	TEST_ASSERT_EQUAL(CONFIG_BLE_ADV_SECONDARY_PHY, (_adv_params)->secondary_phy)

#define TEST_ASSERT_ADV_MODE_SLOW(_adv_handle, _adv_data, _adv_params, _expect_use_allow_list)     \
	/* Validate advertising handle. */                                                         \
	TEST_ASSERT_NOT_NULL(_adv_handle);                                                         \
	TEST_ASSERT_EQUAL(TEST_ADV_SET_HANDLE, *(_adv_handle));                                    \
	/* Validate advertising data. */                                                           \
	TEST_ASSERT_NOT_NULL(p_adv_data);                                                          \
	if (_expect_use_allow_list) {                                                              \
		TEST_ASSERT_EQUAL_MEMORY(ad_flags_le_only, (_adv_data)->adv_data.p_data,           \
					 sizeof(ad_flags_le_only));                                \
		TEST_ASSERT_EQUAL(sizeof(ad_flags_le_only), (_adv_data)->adv_data.len);            \
	}                                                                                          \
	/* Validate advertising parameters. */                                                     \
	TEST_ASSERT_NOT_NULL(_adv_params);                                                         \
	TEST_ASSERT_EQUAL(BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED,                       \
			  (_adv_params)->properties.type);                                         \
	TEST_ASSERT_EQUAL(CONFIG_BLE_ADV_SLOW_ADVERTISING_INTERVAL, (_adv_params)->interval);      \
	TEST_ASSERT_EQUAL(CONFIG_BLE_ADV_SLOW_ADVERTISING_TIMEOUT, (_adv_params)->duration);       \
	TEST_ASSERT_EQUAL((_expect_use_allow_list ? BLE_GAP_ADV_FP_FILTER_CONNREQ :                \
			   BLE_GAP_ADV_FP_ANY), (_adv_params)->filter_policy);                     \
	TEST_ASSERT_EQUAL(CONFIG_BLE_ADV_PRIMARY_PHY, (_adv_params)->primary_phy);                 \
	TEST_ASSERT_EQUAL(CONFIG_BLE_ADV_SECONDARY_PHY, (_adv_params)->secondary_phy)

#define TEST_ASSERT_ADV_DATA_UPDATE(_adv_handle, _adv_data, _adv_params, _check_adv, _check_sr)    \
	/* Validate advertising handle. */                                                         \
	TEST_ASSERT_NOT_NULL(_adv_handle);                                                         \
	TEST_ASSERT_EQUAL(TEST_ADV_SET_HANDLE, *(_adv_handle));                                    \
	/* Validate advertising data. */                                                           \
	TEST_ASSERT_NOT_NULL(p_adv_data);                                                          \
	if (_check_adv) {                                                                          \
		TEST_ASSERT_EQUAL_MEMORY(ad_uuid_list_1, (_adv_data)->adv_data.p_data,             \
					 sizeof(ad_uuid_list_1));                                  \
		TEST_ASSERT_EQUAL(sizeof(ad_uuid_list_2), (_adv_data)->adv_data.len);              \
	}                                                                                          \
	if (_check_sr) {                                                                           \
		TEST_ASSERT_EQUAL_MEMORY(ad_uuid_list_2, (_adv_data)->scan_rsp_data.p_data,        \
					 sizeof(ad_uuid_list_2));                                  \
		TEST_ASSERT_EQUAL(sizeof(ad_uuid_list_2), (_adv_data)->scan_rsp_data.len);         \
	}                                                                                          \
	/* Validate advertising parameters. */                                                     \
	TEST_ASSERT_NULL(_adv_params)

/* Helper macro for deciding if specific fields related to using allow list should be checked. */
#define AL_CHECK_EVAL() (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST) &&                              \
			 ble_adv_evt_handler_options.reply_with_allow_list)

static uint32_t stub_sd_ble_gap_adv_set_configure_directed_hd_success(
	uint8_t *p_adv_handle, const ble_gap_adv_data_t *p_adv_data,
	const ble_gap_adv_params_t *p_adv_params, int cmock_num_calls)
{
	stub_sd_ble_gap_adv_set_configure_num_calls = cmock_num_calls + 1;

	switch (cmock_num_calls) {
	case 0:
#if defined(CONFIG_BLE_ADV_USE_ALLOW_LIST)
	case 1:
#endif /* CONFIG_BLE_ADV_USE_ALLOW_LIST */
#if defined(CONFIG_BLE_ADV_DIRECTED_ADVERTISING_HIGH_DUTY)
		TEST_ASSERT_ADV_MODE_DIRECTED_HD(p_adv_handle, p_adv_data, p_adv_params);
#elif defined(CONFIG_BLE_ADV_DIRECTED_ADVERTISING)
		TEST_ASSERT_ADV_MODE_DIRECTED(p_adv_handle, p_adv_data, p_adv_params);
#elif defined(CONFIG_BLE_ADV_FAST_ADVERTISING)
		TEST_ASSERT_ADV_MODE_FAST(p_adv_handle, p_adv_data, p_adv_params, AL_CHECK_EVAL());
#elif defined(CONFIG_BLE_ADV_SLOW_ADVERTISING)
		TEST_ASSERT_ADV_MODE_SLOW(p_adv_handle, p_adv_data, p_adv_params, AL_CHECK_EVAL());
#endif /* CONFIG_BLE_ADV_xxx */
		break;

	default:
		TEST_FAIL();
	}

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gap_adv_set_configure_directed_success(
	uint8_t *p_adv_handle, const ble_gap_adv_data_t *p_adv_data,
	const ble_gap_adv_params_t *p_adv_params, int cmock_num_calls)
{
	stub_sd_ble_gap_adv_set_configure_num_calls = cmock_num_calls + 1;

	switch (cmock_num_calls) {
	case 0:
#if defined(CONFIG_BLE_ADV_USE_ALLOW_LIST)
	case 1:
#endif /* CONFIG_BLE_ADV_USE_ALLOW_LIST */
#if defined(CONFIG_BLE_ADV_DIRECTED_ADVERTISING)
		TEST_ASSERT_ADV_MODE_DIRECTED(p_adv_handle, p_adv_data, p_adv_params);
#elif defined(CONFIG_BLE_ADV_FAST_ADVERTISING)
		TEST_ASSERT_ADV_MODE_FAST(p_adv_handle, p_adv_data, p_adv_params, AL_CHECK_EVAL());
#elif defined(CONFIG_BLE_ADV_SLOW_ADVERTISING)
		TEST_ASSERT_ADV_MODE_SLOW(p_adv_handle, p_adv_data, p_adv_params, AL_CHECK_EVAL());
#endif /* CONFIG_BLE_ADV_xxx */
		break;

	default:
		TEST_FAIL();
	}

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gap_adv_set_configure_fast_success(
	uint8_t *p_adv_handle, const ble_gap_adv_data_t *p_adv_data,
	const ble_gap_adv_params_t *p_adv_params, int cmock_num_calls)
{
	stub_sd_ble_gap_adv_set_configure_num_calls = cmock_num_calls + 1;

	switch (cmock_num_calls) {
	case 0:
#if defined(CONFIG_BLE_ADV_USE_ALLOW_LIST)
	case 1:
#endif /* CONFIG_BLE_ADV_USE_ALLOW_LIST */
#if defined(CONFIG_BLE_ADV_FAST_ADVERTISING)
		TEST_ASSERT_ADV_MODE_FAST(p_adv_handle, p_adv_data, p_adv_params, AL_CHECK_EVAL());
#elif defined(CONFIG_BLE_ADV_SLOW_ADVERTISING)
		TEST_ASSERT_ADV_MODE_SLOW(p_adv_handle, p_adv_data, p_adv_params, AL_CHECK_EVAL());
#endif /* CONFIG_BLE_ADV_xxx */
		break;

	default:
		TEST_FAIL();
	}

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gap_adv_set_configure_slow_success(
	uint8_t *p_adv_handle, const ble_gap_adv_data_t *p_adv_data,
	const ble_gap_adv_params_t *p_adv_params, int cmock_num_calls)
{
	stub_sd_ble_gap_adv_set_configure_num_calls = cmock_num_calls + 1;

	switch (cmock_num_calls) {
	case 0:
#if defined(CONFIG_BLE_ADV_USE_ALLOW_LIST)
	case 1:
#endif /* CONFIG_BLE_ADV_USE_ALLOW_LIST */
#if defined(CONFIG_BLE_ADV_SLOW_ADVERTISING)
		TEST_ASSERT_ADV_MODE_SLOW(p_adv_handle, p_adv_data, p_adv_params, AL_CHECK_EVAL());
#endif /* CONFIG_BLE_ADV_xxx */
		break;

	default:
		TEST_FAIL();
	}

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gap_adv_set_configure_restart_slow_without_allow_list_success(
	uint8_t *p_adv_handle, const ble_gap_adv_data_t *p_adv_data,
	const ble_gap_adv_params_t *p_adv_params, int cmock_num_calls)
{
	stub_sd_ble_gap_adv_set_configure_num_calls = cmock_num_calls + 1;

	switch (cmock_num_calls) {
	case 0:
#if defined(CONFIG_BLE_ADV_USE_ALLOW_LIST)
	case 1:
#endif /* CONFIG_BLE_ADV_USE_ALLOW_LIST */
#if defined(CONFIG_BLE_ADV_SLOW_ADVERTISING)
		TEST_ASSERT_ADV_MODE_SLOW(p_adv_handle, p_adv_data, p_adv_params, AL_CHECK_EVAL());
#endif /* CONFIG_BLE_ADV_xxx */
		break;

	case 2:
#if defined(CONFIG_BLE_ADV_USE_ALLOW_LIST)
	case 3:
#else
	case 1:
#endif /* CONFIG_BLE_ADV_USE_ALLOW_LIST */
#if defined(CONFIG_BLE_ADV_SLOW_ADVERTISING)
		TEST_ASSERT_ADV_MODE_SLOW(p_adv_handle, p_adv_data, p_adv_params, false);
#endif /* CONFIG_BLE_ADV_xxx */
		break;

	default:
		TEST_FAIL();
	}

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gap_adv_set_configure_adv_set_terminated_fast_to_slow_success(
	uint8_t *p_adv_handle, const ble_gap_adv_data_t *p_adv_data,
	const ble_gap_adv_params_t *p_adv_params, int cmock_num_calls)
{
	stub_sd_ble_gap_adv_set_configure_num_calls = cmock_num_calls + 1;

	switch (cmock_num_calls) {
	case 0:
#if defined(CONFIG_BLE_ADV_USE_ALLOW_LIST)
	case 1:
#endif /* CONFIG_BLE_ADV_USE_ALLOW_LIST */
#if defined(CONFIG_BLE_ADV_FAST_ADVERTISING)
		TEST_ASSERT_ADV_MODE_FAST(p_adv_handle, p_adv_data, p_adv_params, AL_CHECK_EVAL());
#endif /* CONFIG_BLE_ADV_xxx */
		break;

#if defined(CONFIG_BLE_ADV_USE_ALLOW_LIST)
	case 2:
	case 3:
#else
	case 1:
#endif /* CONFIG_BLE_ADV_USE_ALLOW_LIST */
#if defined(CONFIG_BLE_ADV_SLOW_ADVERTISING)
		TEST_ASSERT_ADV_MODE_SLOW(p_adv_handle, p_adv_data, p_adv_params, AL_CHECK_EVAL());
#endif /* CONFIG_BLE_ADV_xxx */
		break;

	default:
		TEST_FAIL();
	}

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_gap_adv_set_configure_adv_data_update_success(
	uint8_t *p_adv_handle, const ble_gap_adv_data_t *p_adv_data,
	const ble_gap_adv_params_t *p_adv_params, int cmock_num_calls)
{
	stub_sd_ble_gap_adv_set_configure_num_calls = cmock_num_calls + 1;

	switch (cmock_num_calls) {
	case 0:
		TEST_ASSERT_ADV_DATA_UPDATE(p_adv_handle, p_adv_data, p_adv_params, true, true);
		break;

	case 1:
		TEST_ASSERT_ADV_DATA_UPDATE(p_adv_handle, p_adv_data, p_adv_params, true, false);
		break;

	case 2:
		TEST_ASSERT_ADV_DATA_UPDATE(p_adv_handle, p_adv_data, p_adv_params, false, true);
		break;

	default:
		TEST_FAIL();
	}

	return NRF_SUCCESS;
}

static uint32_t stub_sd_ble_uuid_encode_adv_data_update_success(
	const ble_uuid_t *p_uuid, uint8_t *p_uuid_le_len, uint8_t *p_uuid_le, int cmock_num_calls)
{
	TEST_ASSERT_NOT_NULL(p_uuid);
	TEST_ASSERT_NOT_NULL(p_uuid_le_len);

	*p_uuid_le_len = (p_uuid->type == BLE_UUID_TYPE_BLE) ? AD_UUID_16_DATA_SIZE :
							       AD_UUID_128_DATA_SIZE;

	/* Runs through checks two times. First both Advertising checks and scan response checks,
	 * then only advertising checks, and lastly only scan response checks.
	 */
	const int call_num = cmock_num_calls % 12;

	switch (call_num) {
	/* Advertising UUID data checks. */
	/* 16 bit UUIDs. */
	case 0:
	case 1:
	case 3:
		TEST_ASSERT_EQUAL_HEX(TEST_UUID_1, p_uuid->uuid);
		TEST_ASSERT_EQUAL_HEX(BLE_UUID_TYPE_BLE, p_uuid->type);
		if (call_num == 1) {
			TEST_ASSERT_NOT_NULL(p_uuid_le);
			memcpy(p_uuid_le, &(uint8_t[]){TEST_UUID_1_BYTES}, *p_uuid_le_len);
		} else {
			TEST_ASSERT_NULL(p_uuid_le);
		}
		break;
	/* 128 bit UUIDs. */
	case 2:
	case 4:
	case 5:
		TEST_ASSERT_EQUAL_HEX(TEST_UUID_2, p_uuid->uuid);
		TEST_ASSERT_EQUAL_HEX(TEST_UUID_TYPE_1, p_uuid->type);
		if (call_num == 5) {
			TEST_ASSERT_NOT_NULL(p_uuid_le);
			memcpy(p_uuid_le, &(uint8_t[]){TEST_UUID_2_BYTES}, *p_uuid_le_len);
		} else {
			TEST_ASSERT_NULL(p_uuid_le);
		}
		break;

	/* Scan response UUID data checks. */
	/* 16 bit UUIDs. */
	case 6:
	case 7:
	case 9:
		TEST_ASSERT_EQUAL_HEX(TEST_UUID_3, p_uuid->uuid);
		TEST_ASSERT_EQUAL_HEX(BLE_UUID_TYPE_BLE, p_uuid->type);
		if (call_num == 7) {
			TEST_ASSERT_NOT_NULL(p_uuid_le);
			memcpy(p_uuid_le, &(uint8_t[]){TEST_UUID_3_BYTES}, *p_uuid_le_len);
		} else {
			TEST_ASSERT_NULL(p_uuid_le);
		}
		break;
	/* 128 bit UUIDs. */
	case 8:
	case 10:
	case 11:
		TEST_ASSERT_EQUAL_HEX(TEST_UUID_4, p_uuid->uuid);
		TEST_ASSERT_EQUAL_HEX(TEST_UUID_TYPE_1, p_uuid->type);
		if (call_num == 11) {
			TEST_ASSERT_NOT_NULL(p_uuid_le);
			memcpy(p_uuid_le, &(uint8_t[]){TEST_UUID_4_BYTES}, *p_uuid_le_len);
		} else {
			TEST_ASSERT_NULL(p_uuid_le);
		}
		break;

	default:
		/* Should never be reached. */
		TEST_FAIL();
		break;
	}

	return NRF_SUCCESS;
}

void test_ble_adv_on_ble_evt_adv_set_terminated_fast_to_slow_success(void)
{
	TEST_RUN_ONLY_IF(IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING) &&
			 IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING));

	uint32_t nrf_err;

	init_success();

	__cmock_sd_ble_gap_adv_set_configure_Stub(
		stub_sd_ble_gap_adv_set_configure_adv_set_terminated_fast_to_slow_success);
	__cmock_sd_ble_gap_adv_start_ExpectAndReturn(TEST_ADV_SET_HANDLE, TEST_CONN_CFG_TAG,
						     NRF_SUCCESS);

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
		/* When using allow list, the adv flags in the AD data must be updated.
		 * This is done with an additional call to sd_ble_gap_adv_set_configure().
		 * Expect the function to be called twice if allow list is enabled.
		 */
		TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_FAST_ALLOW_LIST, 1);
	} else {
		TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_FAST, 1);
	}
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());

	evts_raised_cnt_reset();

	/* Raise BLE_GAP_EVT_ADV_SET_TERMINATED to switch advertising mode from fast to slow. */
	const ble_evt_t ble_evt_adv_set_terminated = {
		.header.evt_id = BLE_GAP_EVT_ADV_SET_TERMINATED,
		.evt.gap_evt.conn_handle = BLE_CONN_HANDLE_INVALID,
		.evt.gap_evt.params.adv_set_terminated = {
			.reason = BLE_GAP_EVT_ADV_SET_TERMINATED_REASON_TIMEOUT,
			.adv_handle = TEST_ADV_SET_HANDLE,
		},
	};

	__cmock_sd_ble_gap_adv_start_ExpectAndReturn(TEST_ADV_SET_HANDLE, TEST_CONN_CFG_TAG,
						     NRF_SUCCESS);

	ble_adv_on_ble_evt(&ble_evt_adv_set_terminated, (void *)&ble_adv);

	if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
		TEST_ASSERT_EQUAL(4, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW_ALLOW_LIST, 1);
	} else {
		TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW, 1);
	}
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());
}

void test_ble_adv_on_ble_evt_restart_advertising_on_disconnect_success(void)
{
	TEST_RUN_ONLY_IF(IS_ENABLED(CONFIG_BLE_ADV_RESTART_ON_DISCONNECT));

	const ble_evt_t ble_evt_connected = {
		.header.evt_id = BLE_GAP_EVT_CONNECTED,
		.evt.gap_evt.conn_handle = TEST_CONN_HANDLE,
		.evt.gap_evt.params.connected = {
			.role = BLE_GAP_ROLE_PERIPH,
		},
	};
	const ble_evt_t ble_evt_disconnected = {
		.header.evt_id = BLE_GAP_EVT_DISCONNECTED,
		.evt.gap_evt.conn_handle = TEST_CONN_HANDLE,
	};

	init_success();

	ble_adv_on_ble_evt(&ble_evt_connected, (void *)&ble_adv);

	__cmock_sd_ble_gap_adv_set_configure_Stub(
		stub_sd_ble_gap_adv_set_configure_directed_hd_success);

	if (IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING_HIGH_DUTY) ||
	    IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING) ||
	    IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING) ||
	    IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		__cmock_sd_ble_gap_adv_start_ExpectAndReturn(TEST_ADV_SET_HANDLE, TEST_CONN_CFG_TAG,
							     NRF_SUCCESS);
	}

	ble_adv_on_ble_evt(&ble_evt_disconnected, (void *)&ble_adv);

	if (IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING_HIGH_DUTY)) {
		TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_PEER_ADDR_REQUEST, 1);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_DIRECTED_HIGH_DUTY, 1);
	} else if (IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING)) {
		TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_PEER_ADDR_REQUEST, 1);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_DIRECTED, 1);
	} else if (IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING)) {
		if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
			TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_FAST_ALLOW_LIST, 1);
		} else {
			TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_FAST, 1);
		}
	} else if (IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
			TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW_ALLOW_LIST, 1);
		} else {
			TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW, 1);
		}
	} else {
		TEST_ASSERT_EQUAL(0, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_IDLE, 1);
	}
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());
}

void test_ble_adv_on_ble_evt_restart_advertising_on_disconnect_incorrect_conn_handle(void)
{
	const ble_evt_t ble_evt_connected = {
		.header.evt_id = BLE_GAP_EVT_CONNECTED,
		.evt.gap_evt.conn_handle = TEST_CONN_HANDLE_2,
		.evt.gap_evt.params.connected = {
			.role = BLE_GAP_ROLE_PERIPH,
		},
	};
	const ble_evt_t ble_evt_disconnected = {
		.header.evt_id = BLE_GAP_EVT_DISCONNECTED,
		.evt.gap_evt.conn_handle = TEST_CONN_HANDLE,
	};

	init_success();

	__cmock_sd_ble_gap_adv_set_configure_Stub(
		stub_sd_ble_gap_adv_set_configure_directed_hd_success);

	ble_adv_on_ble_evt(&ble_evt_disconnected, (void *)&ble_adv);

	TEST_ASSERT_EQUAL(0, stub_sd_ble_gap_adv_set_configure_num_calls);
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());

	ble_adv_on_ble_evt(&ble_evt_connected, (void *)&ble_adv);

	ble_adv_on_ble_evt(&ble_evt_disconnected, (void *)&ble_adv);

	TEST_ASSERT_EQUAL(0, stub_sd_ble_gap_adv_set_configure_num_calls);
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());
}

void test_ble_adv_on_ble_evt_restart_advertising_on_disconnect_error(void)
{
	TEST_RUN_ONLY_IF(IS_ENABLED(CONFIG_BLE_ADV_RESTART_ON_DISCONNECT));

	const ble_evt_t ble_evt_connected = {
		.header.evt_id = BLE_GAP_EVT_CONNECTED,
		.evt.gap_evt.conn_handle = TEST_CONN_HANDLE,
		.evt.gap_evt.params.connected = {
			.role = BLE_GAP_ROLE_PERIPH,
		},
	};
	const ble_evt_t ble_evt_disconnected = {
		.header.evt_id = BLE_GAP_EVT_DISCONNECTED,
		.evt.gap_evt.conn_handle = TEST_CONN_HANDLE,
	};

	init_success();
	ble_adv_evt_handler_options.reply_with_allow_list = false;

	ble_adv_on_ble_evt(&ble_evt_connected, (void *)&ble_adv);

	__cmock_sd_ble_gap_adv_set_configure_Stub(
		stub_sd_ble_gap_adv_set_configure_directed_hd_success);
	if (IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING_HIGH_DUTY) ||
	    IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING) ||
	    IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING) ||
	    IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		__cmock_sd_ble_gap_adv_start_ExpectAndReturn(TEST_ADV_SET_HANDLE, TEST_CONN_CFG_TAG,
							     NRF_ERROR_INVALID_STATE);
	}

	ble_adv_on_ble_evt(&ble_evt_disconnected, (void *)&ble_adv);

	if (IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING)) {
		TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_PEER_ADDR_REQUEST, 1);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_ERROR, 1);
	} else if (IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING) ||
		   IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
		if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
		}
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_ERROR, 1);
	} else {
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_IDLE, 1);
		TEST_ASSERT_EQUAL(0, stub_sd_ble_gap_adv_set_configure_num_calls);
	}
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());
}

void test_ble_adv_init_success(void)
{
	init_success();
}

void test_ble_adv_init_error_null(void)
{
	uint32_t nrf_err;
	struct ble_adv_config cfg;

	memset(&cfg, 0xA2, sizeof(cfg));

	nrf_err = ble_adv_init(NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_init(NULL, &cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_init(&ble_adv, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	cfg.evt_handler = NULL;

	nrf_err = ble_adv_init(&ble_adv, &cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_adv_init_error_invalid_param(void)
{
	uint32_t nrf_err;
	struct ble_adv_config cfg = {
		.conn_cfg_tag = TEST_CONN_CFG_TAG,
		.evt_handler = ble_adv_evt_handler,
	};

	__cmock_sd_ble_gap_device_name_set_ExpectWithArrayAndReturn(
		&sec_mode_open, 1, CONFIG_BLE_ADV_NAME, sizeof(CONFIG_BLE_ADV_NAME),
		sizeof(CONFIG_BLE_ADV_NAME) - 1, NRF_ERROR_INVALID_ADDR);

	nrf_err = ble_adv_init(&ble_adv, &cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_adv_conn_cfg_tag_set_success(void)
{
	uint32_t nrf_err;

	nrf_err = ble_adv_conn_cfg_tag_set(&ble_adv, TEST_CONN_CFG_TAG);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_adv_conn_cfg_tag_set_error_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_adv_conn_cfg_tag_set(NULL, TEST_CONN_CFG_TAG);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_adv_start_error_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_adv_start(NULL, BLE_ADV_MODE_FAST);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_adv_start_error_invalid_state(void)
{
	uint32_t nrf_err;

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_adv_start_directed_hd_success(void)
{
	/* Modes can be individually enabled and disabled with Kconfig.
	 * The test is written to support all combinations of modes.
	 */
	uint32_t nrf_err;

	init_success();

	__cmock_sd_ble_gap_adv_set_configure_Stub(
		stub_sd_ble_gap_adv_set_configure_directed_hd_success);

	if (IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING_HIGH_DUTY) ||
	    IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING) ||
	    IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING) ||
	    IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		__cmock_sd_ble_gap_adv_start_ExpectAndReturn(TEST_ADV_SET_HANDLE, TEST_CONN_CFG_TAG,
							     NRF_SUCCESS);
	}

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_DIRECTED_HIGH_DUTY);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	if (IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING_HIGH_DUTY)) {
		TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_PEER_ADDR_REQUEST, 1);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_DIRECTED_HIGH_DUTY, 1);
	} else if (IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING)) {
		TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_PEER_ADDR_REQUEST, 1);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_DIRECTED, 1);
	} else if (IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING)) {
		if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
			TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_FAST_ALLOW_LIST, 1);
		} else {
			TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_FAST, 1);
		}
	} else if (IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
			TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW_ALLOW_LIST, 1);
		} else {
			TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW, 1);
		}
	} else {
		TEST_ASSERT_EQUAL(0, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_IDLE, 1);
	}
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());
}

void test_ble_adv_start_directed_success(void)
{
	/* Modes can be individually enabled and disabled with Kconfig.
	 * The test is written to support all combinations of modes.
	 */
	uint32_t nrf_err;

	init_success();

	__cmock_sd_ble_gap_adv_set_configure_Stub(
		stub_sd_ble_gap_adv_set_configure_directed_success);

	if (IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING) ||
	    IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING) ||
	    IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		__cmock_sd_ble_gap_adv_start_ExpectAndReturn(TEST_ADV_SET_HANDLE, TEST_CONN_CFG_TAG,
							     NRF_SUCCESS);
	}

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_DIRECTED);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	if (IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING)) {
		TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_PEER_ADDR_REQUEST, 1);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_DIRECTED, 1);
	} else if (IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING)) {
		if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
			TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_FAST_ALLOW_LIST, 1);
		} else {
			TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_FAST, 1);
		}
	} else if (IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
			TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW_ALLOW_LIST, 1);
		} else {
			TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW, 1);
		}
	} else {
		TEST_ASSERT_EQUAL(0, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_IDLE, 1);
	}
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());
}

void test_ble_adv_start_fast_success(void)
{
	/* Modes can be individually enabled and disabled with Kconfig.
	 * The test is written to support all combinations of modes.
	 */
	uint32_t nrf_err;

	init_success();

	/* Change tag and expect the new tag value to be passed to sd_ble_gap_adv_start(). */
	nrf_err = ble_adv_conn_cfg_tag_set(&ble_adv, TEST_CONN_CFG_TAG_2);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_gap_adv_set_configure_Stub(stub_sd_ble_gap_adv_set_configure_fast_success);

	if (IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING) ||
	    IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		__cmock_sd_ble_gap_adv_start_ExpectAndReturn(TEST_ADV_SET_HANDLE,
							     TEST_CONN_CFG_TAG_2, NRF_SUCCESS);
	}

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	if (IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING)) {
		if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
			/* When using allow list, the adv flags in the AD data must be updated.
			 * This is done with an additional call to sd_ble_gap_adv_set_configure().
			 * Expect the function to be called twice if allow list is enabled.
			 */
			TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_FAST_ALLOW_LIST, 1);
		} else {
			TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_FAST, 1);
		}
	} else if (IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
			TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW_ALLOW_LIST, 1);
		} else {
			TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW, 1);
		}
	} else {
		TEST_ASSERT_EQUAL(0, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_IDLE, 1);
	}
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());
}

void test_ble_adv_start_fast_error_invalid_param(void)
{
	TEST_RUN_ONLY_IF(IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST) &&
			 (IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING) ||
			  IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)));

	uint32_t nrf_err;

	/* When using allow list, the AD flags must be updated to match using allow list.
	 * If there are no flags data in the advertising data, an error is returned.
	 */
	init_without_ad_flags();

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	TEST_ASSERT_EQUAL(0, stub_sd_ble_gap_adv_set_configure_num_calls);
}

void test_ble_adv_start_slow_success(void)
{
	/* Modes can be individually enabled and disabled with Kconfig.
	 * The test is written to support all combinations of modes.
	 */
	uint32_t nrf_err;

	init_success();

	__cmock_sd_ble_gap_adv_set_configure_Stub(stub_sd_ble_gap_adv_set_configure_slow_success);

	if (IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		__cmock_sd_ble_gap_adv_start_ExpectAndReturn(TEST_ADV_SET_HANDLE, TEST_CONN_CFG_TAG,
							     NRF_SUCCESS);
	}

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_SLOW);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	if (IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
			/* When using allow list, the adv flags in the AD data must be updated.
			 * This is done with an additional call to sd_ble_gap_adv_set_configure().
			 * Expect the function to be called twice if allow list is enabled.
			 */
			TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW_ALLOW_LIST, 1);
		} else {
			TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW, 1);
		}
	} else {
		TEST_ASSERT_EQUAL(0, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_IDLE, 1);
	}
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());
}

void test_ble_adv_start_slow_error_invalid_param(void)
{
	TEST_RUN_ONLY_IF(IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST) &&
			 IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING));

	uint32_t nrf_err;

	/* When using allow list, the AD flags must be updated to match using allow list.
	 * If there are no flags data in the advertising data, an error is returned.
	 */
	init_without_ad_flags();

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_SLOW);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	TEST_ASSERT_EQUAL(0, stub_sd_ble_gap_adv_set_configure_num_calls);
}

void test_ble_adv_start_idle_success(void)
{
	uint32_t nrf_err;

	init_success();

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_IDLE);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evts_raised_cnt_expectation_set(BLE_ADV_EVT_IDLE, 1);
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());
}

void test_ble_adv_start_mode_out_of_range_success(void)
{
	uint32_t nrf_err;

	init_success();

	nrf_err = ble_adv_start(&ble_adv, NUM_ADV_EVT_TYPES);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evts_raised_cnt_expectation_set(BLE_ADV_EVT_IDLE, 1);
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());
}

void test_ble_adv_peer_addr_reply_error_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_adv_peer_addr_reply(NULL, &test_addr2);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_peer_addr_reply(&ble_adv, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_peer_addr_reply(NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_adv_peer_addr_reply_error_invalid_state(void)
{
	uint32_t nrf_err;

	nrf_err = ble_adv_peer_addr_reply(&ble_adv, &test_addr2);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	init_success();

	nrf_err = ble_adv_peer_addr_reply(&ble_adv, &test_addr2);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_adv_peer_addr_reply_error_invalid_param(void)
{
	TEST_RUN_ONLY_IF(IS_ENABLED(CONFIG_BLE_ADV_DIRECTED_ADVERTISING));

	uint32_t nrf_err;

	init_success();

	__cmock_sd_ble_gap_adv_set_configure_Stub(
		stub_sd_ble_gap_adv_set_configure_fast_success);
	if (IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING) ||
	    IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		__cmock_sd_ble_gap_adv_start_ExpectAndReturn(TEST_ADV_SET_HANDLE, TEST_CONN_CFG_TAG,
							     NRF_SUCCESS);
	}

	ble_adv_evt_handler_options.reply_with_peer_addr_valid = false;

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_DIRECTED_HIGH_DUTY);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	evts_raised_cnt_expectation_set(BLE_ADV_EVT_PEER_ADDR_REQUEST, 1);

	if (IS_ENABLED(CONFIG_BLE_ADV_FAST_ADVERTISING)) {
		if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
			TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_FAST_ALLOW_LIST, 1);
		} else {
			TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_FAST, 1);
		}
	} else if (IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
			TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW_ALLOW_LIST, 1);
		} else {
			TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW, 1);
		}
	} else {
		TEST_ASSERT_EQUAL(0, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_IDLE, 1);
	}
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());
}

void test_ble_adv_allow_list_reply_error_null(void)
{
	uint32_t nrf_err;
	ble_gap_irk_t test_irks[] = {
		{.irk = {0xAA}}, {.irk = {0xBB}}, {.irk = {0xCC}},
	};

	nrf_err = ble_adv_allow_list_reply(NULL, test_addrs, ARRAY_SIZE(test_addrs),
					   test_irks, ARRAY_SIZE(test_irks));
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_adv_allow_list_reply_error_invalid_state(void)
{
	uint32_t nrf_err;
	ble_gap_irk_t test_irks[] = {
		{.irk = {0xAA}}, {.irk = {0xBB}}, {.irk = {0xCC}},
	};

	nrf_err = ble_adv_allow_list_reply(&ble_adv, test_addrs, ARRAY_SIZE(test_addrs),
					   test_irks, ARRAY_SIZE(test_irks));
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	init_success();

	nrf_err = ble_adv_allow_list_reply(&ble_adv, test_addrs, ARRAY_SIZE(test_addrs),
					   test_irks, ARRAY_SIZE(test_irks));
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_adv_restart_without_allow_list_slow_success(void)
{
	TEST_RUN_ONLY_IF(IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING));

	uint32_t nrf_err;

	init_success();

	__cmock_sd_ble_gap_adv_set_configure_Stub(
		stub_sd_ble_gap_adv_set_configure_restart_slow_without_allow_list_success);
	__cmock_sd_ble_gap_adv_start_ExpectAndReturn(TEST_ADV_SET_HANDLE, TEST_CONN_CFG_TAG,
						     NRF_SUCCESS);

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_SLOW);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	if (IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
			/* When using allow list, the adv flags in the AD data must be updated.
			 * This is done with an additional call to sd_ble_gap_adv_set_configure().
			 * Expect the function to be called twice if allow list is enabled.
			 */
			TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_ALLOW_LIST_REQUEST, 1);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW_ALLOW_LIST, 1);
		} else {
			TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);
			evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW, 1);
		}
	} else {
		TEST_ASSERT_EQUAL(0, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_IDLE, 1);
	}
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());

	evts_raised_cnt_reset();

	/* Try restarting without allow list and expect:
	 *     - advertising data to have changed slightly.
	 *     - Event BLE_ADV_EVT_SLOW is now raised (both with and without
	 *       CONFIG_BLE_ADV_USE_ALLOW_LIST enabled).
	 */
	__cmock_sd_ble_gap_adv_start_ExpectAndReturn(TEST_ADV_SET_HANDLE, TEST_CONN_CFG_TAG,
						     NRF_SUCCESS);
	__cmock_sd_ble_gap_adv_stop_ExpectAndReturn(TEST_ADV_SET_HANDLE, NRF_SUCCESS);

	nrf_err = ble_adv_restart_without_allow_list(&ble_adv);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	if (IS_ENABLED(CONFIG_BLE_ADV_SLOW_ADVERTISING)) {
		if (IS_ENABLED(CONFIG_BLE_ADV_USE_ALLOW_LIST)) {
			/* When restarting without allow list, the adv flags in the AD data
			 * must be updated. This is done with an additional call to
			 * sd_ble_gap_adv_set_configure(), in addition to the one in
			 * ble_adv_start(). Therefore, expect the sd_ble_gap_adv_set_configure()
			 * function to be called two more times.
			 */
			TEST_ASSERT_EQUAL(4, stub_sd_ble_gap_adv_set_configure_num_calls);
		} else {
			TEST_ASSERT_EQUAL(3, stub_sd_ble_gap_adv_set_configure_num_calls);
		}
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_SLOW, 1);
	} else {
		TEST_ASSERT_EQUAL(0, stub_sd_ble_gap_adv_set_configure_num_calls);
		evts_raised_cnt_expectation_set(BLE_ADV_EVT_IDLE, 1);
	}
	TEST_ASSERT_TRUE(evts_raised_cnt_expectations_met());
}

void test_ble_adv_restart_without_allow_list_error_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_adv_restart_without_allow_list(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_adv_restart_without_allow_list_error_invalid_state(void)
{
	uint32_t nrf_err;

	nrf_err = ble_adv_restart_without_allow_list(&ble_adv);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_adv_data_update_success(void)
{
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {
		.uuid_lists.complete = {
			.uuid = test_uuid_list_1, .len = ARRAY_SIZE(test_uuid_list_1),
		},
	};
	struct ble_adv_data sr_data = {
		.uuid_lists.complete = {
			.uuid = test_uuid_list_2, .len = ARRAY_SIZE(test_uuid_list_2),
		},
	};

	init_success();

	__cmock_sd_ble_uuid_encode_Stub(stub_sd_ble_uuid_encode_adv_data_update_success);
	__cmock_sd_ble_gap_adv_set_configure_Stub(
		stub_sd_ble_gap_adv_set_configure_adv_data_update_success);

	nrf_err = ble_adv_data_update(&ble_adv, &adv_data, &sr_data);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, stub_sd_ble_gap_adv_set_configure_num_calls);

	nrf_err = ble_adv_data_update(&ble_adv, &adv_data, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(2, stub_sd_ble_gap_adv_set_configure_num_calls);

	nrf_err = ble_adv_data_update(&ble_adv, NULL, &sr_data);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(3, stub_sd_ble_gap_adv_set_configure_num_calls);
}

void test_ble_adv_data_update_error_null(void)
{
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {0};
	struct ble_adv_data sr_data = {0};

	nrf_err = ble_adv_data_update(&ble_adv, NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_data_update(NULL, &adv_data, &sr_data);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_data_update(NULL, &adv_data, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_data_update(NULL, NULL, &sr_data);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_data_update(NULL, NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_adv_data_update_error_invalid_state(void)
{
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {0};
	struct ble_adv_data sr_data = {0};

	nrf_err = ble_adv_data_update(&ble_adv, &adv_data, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = ble_adv_data_update(&ble_adv, NULL, &sr_data);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	nrf_err = ble_adv_data_update(&ble_adv, &adv_data, &sr_data);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_adv_data_update_error_invalid_param(void)
{
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {
		.uuid_lists.complete = {
			.uuid = test_uuid_list_1, .len = ARRAY_SIZE(test_uuid_list_1),
		},
	};
	struct ble_adv_data sr_data = {
		.uuid_lists.complete = {
			.uuid = test_uuid_list_2, .len = ARRAY_SIZE(test_uuid_list_2),
		},
	};

	init_success();

	__cmock_sd_ble_uuid_encode_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);

	nrf_err = ble_adv_data_update(&ble_adv, &adv_data, &sr_data);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_adv_data_update_error_invalid_param_2(void)
{
	uint32_t nrf_err;
	struct ble_adv_data adv_data = {
		.uuid_lists.complete = {
			.uuid = test_uuid_list_1, .len = ARRAY_SIZE(test_uuid_list_1),
		},
	};
	struct ble_adv_data sr_data = {
		.uuid_lists.complete = {
			.uuid = test_uuid_list_2, .len = ARRAY_SIZE(test_uuid_list_2),
		},
	};

	init_success();

	__cmock_sd_ble_uuid_encode_Stub(stub_sd_ble_uuid_encode_adv_data_update_success);
	__cmock_sd_ble_gap_adv_set_configure_ExpectAnyArgsAndReturn(NRF_ERROR_INVALID_PARAM);

	nrf_err = ble_adv_data_update(&ble_adv, &adv_data, &sr_data);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void setUp(void)
{
	/* Clear the instance data before each test. */
	ble_adv = (struct ble_adv){0};

	/* Reset the event count list before each test. */
	evts_raised_cnt_reset();

	/* Reset the behavior of ble_adv_evt_handler() before each test. */
	ble_adv_evt_handler_options.reply_with_peer_addr = true;
	ble_adv_evt_handler_options.reply_with_peer_addr_valid = true;
	ble_adv_evt_handler_options.reply_with_allow_list = true;

	/* Reset global stub num_calls variables. */
	stub_sd_ble_gap_adv_set_configure_num_calls = 0;
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
