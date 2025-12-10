/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/ble_adv_data.h>

#include "cmock_ble.h"
#include "cmock_ble_gap.h"

#define  MAX_ADV_MODES 5
uint16_t ble_adv_evt_type;

const ble_gap_addr_t test_addr = {
	.addr_id_peer = false,
	.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC,
	.addr = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11},
};

static void ble_adv_evt_handler(struct ble_adv *adv, const struct ble_adv_evt *adv_evt)
{
	ble_adv_evt_type = adv_evt->evt_type;

	switch (adv_evt->evt_type) {
	case BLE_ADV_EVT_PEER_ADDR_REQUEST:
		ble_adv_peer_addr_reply(adv, &test_addr);
		break;
	default:
		break;
	}
}

void test_ble_adv_conn_cfg_tag_set(void)
{
	struct ble_adv ble_adv;
	uint8_t conn_cfg_tag = 1;
	uint32_t nrf_err;

	nrf_err = ble_adv_conn_cfg_tag_set(NULL, conn_cfg_tag);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_conn_cfg_tag_set(&ble_adv, conn_cfg_tag);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	TEST_ASSERT_EQUAL(conn_cfg_tag, ble_adv.conn_cfg_tag);
}

void test_ble_adv_init_error_null(void)
{
	struct ble_adv ble_adv;
	struct ble_adv_config config = {
		.conn_cfg_tag = 1,
		.evt_handler = ble_adv_evt_handler,
	};
	uint32_t nrf_err;

	nrf_err = ble_adv_init(NULL, &config);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
	nrf_err = ble_adv_init(&ble_adv, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
	config.evt_handler = NULL;
	nrf_err = ble_adv_init(&ble_adv, &config);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_adv_init_error_invalid_param(void)
{
	struct ble_adv ble_adv = {
		.adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET,
	};
	struct ble_adv_config config = {
		.conn_cfg_tag = 1,
		.evt_handler = ble_adv_evt_handler,
	};
	ble_gap_conn_sec_mode_t sec_mode = {0};
	uint32_t nrf_err;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

	/* Simulate an error in setting the device name */
	__cmock_sd_ble_gap_device_name_set_ExpectAndReturn(&sec_mode, CONFIG_BLE_ADV_NAME,
							   strlen(CONFIG_BLE_ADV_NAME),
							   NRF_ERROR_INVALID_ADDR);
	nrf_err = ble_adv_init(&ble_adv, &config);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	/* Simulate an error in setting the adv config */
	__cmock_sd_ble_gap_device_name_set_ExpectAndReturn(&sec_mode, CONFIG_BLE_ADV_NAME,
							   strlen(CONFIG_BLE_ADV_NAME),
							   NRF_SUCCESS);
	__cmock_sd_ble_gap_adv_set_configure_ExpectAndReturn(&ble_adv.adv_handle,
							     NULL, &ble_adv.adv_params,
							     NRF_ERROR_INVALID_ADDR);
	nrf_err = ble_adv_init(&ble_adv, &config);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_adv_init(void)
{
	uint8_t conn_cfg_tag = 1;
	uint32_t nrf_err;
	struct ble_adv ble_adv = {
		.adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET,
	};
	struct ble_adv_config config = {
		.conn_cfg_tag = conn_cfg_tag,
		.evt_handler = ble_adv_evt_handler,
	};

		/* Simulate an error in setting the adv config */
	__cmock_sd_ble_gap_device_name_set_IgnoreAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gap_adv_set_configure_IgnoreAndReturn(NRF_SUCCESS);

	nrf_err = ble_adv_init(&ble_adv, &config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(ble_adv.mode_current == BLE_ADV_MODE_IDLE);
	TEST_ASSERT_TRUE(ble_adv.conn_cfg_tag == conn_cfg_tag);
	TEST_ASSERT_TRUE(ble_adv.conn_handle == BLE_CONN_HANDLE_INVALID);
	TEST_ASSERT_TRUE(ble_adv.adv_handle == BLE_GAP_ADV_SET_HANDLE_NOT_SET);
	TEST_ASSERT_TRUE(ble_adv.evt_handler == ble_adv_evt_handler);
	TEST_ASSERT_TRUE(ble_adv.adv_params.properties.type ==
		BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED);
	TEST_ASSERT_TRUE(ble_adv.adv_params.duration == BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED);
	TEST_ASSERT_TRUE(ble_adv.adv_params.interval == BLE_GAP_ADV_INTERVAL_MAX);
	TEST_ASSERT_TRUE(ble_adv.adv_params.filter_policy == BLE_GAP_ADV_FP_ANY);
	TEST_ASSERT_TRUE(ble_adv.adv_params.primary_phy == BLE_GAP_PHY_AUTO);
	TEST_ASSERT_TRUE(ble_adv.is_initialized);
}

void test_ble_adv_peer_addr_reply(void)
{
	struct ble_adv ble_adv = {
		.peer_addr_reply_expected = true,
	};
	ble_gap_addr_t peer_addr = {0};
	uint32_t nrf_err;

	nrf_err = ble_adv_peer_addr_reply(NULL, &peer_addr);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_peer_addr_reply(&ble_adv, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_peer_addr_reply(&ble_adv, &peer_addr);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	peer_addr = (ble_gap_addr_t){
		.addr_id_peer = 0,
		.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC,
		.addr = {0x01, 0x02, 0x03, 0x00, 0x05, 0x06}
	};

	nrf_err = ble_adv_peer_addr_reply(&ble_adv, &peer_addr);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(ble_adv.peer_addr_reply_expected == false);
	TEST_ASSERT_TRUE(ble_adv.peer_address.addr_type == peer_addr.addr_type);
	TEST_ASSERT_TRUE(
		memcmp(ble_adv.peer_address.addr, peer_addr.addr, sizeof(peer_addr.addr)) == 0);
}


void test_ble_adv_allow_list_reply(void)
{
	struct ble_adv ble_adv_config = {0};
	uint32_t nrf_err;
	const ble_gap_addr_t addrs = {0};
	const ble_gap_irk_t irks = {0};

	nrf_err = ble_adv_allow_list_reply(NULL, &addrs, 0, &irks, 0);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_adv_allow_list_reply(&ble_adv_config, &addrs, 0, &irks, 0);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	ble_adv_config.allow_list_reply_expected = NULL;
	nrf_err = ble_adv_allow_list_reply(&ble_adv_config, NULL, 0, NULL, 0);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);

	ble_adv_config.allow_list_reply_expected = true;
	nrf_err = ble_adv_allow_list_reply(&ble_adv_config, &addrs, 0, &irks, 0);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(ble_adv_config.allow_list_reply_expected == false);
	TEST_ASSERT_TRUE(ble_adv_config.allow_list_in_use == false);

	ble_adv_config.allow_list_reply_expected = true;
	nrf_err = ble_adv_allow_list_reply(&ble_adv_config, &addrs, 1, &irks, 0);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_TRUE(ble_adv_config.allow_list_reply_expected == false);
	TEST_ASSERT_TRUE(ble_adv_config.allow_list_in_use == true);
}

void test_ble_adv_start(void)
{
	struct ble_adv ble_adv = {
		.is_initialized = true,
		.evt_handler = ble_adv_evt_handler,
		.allow_list_temporarily_disabled = false,
	};
	enum ble_adv_mode mode[MAX_ADV_MODES] = {BLE_ADV_MODE_DIRECTED_HIGH_DUTY,
		BLE_ADV_MODE_DIRECTED, BLE_ADV_MODE_FAST, BLE_ADV_MODE_SLOW, BLE_ADV_MODE_IDLE};
	int adv_prop_type[MAX_ADV_MODES] = {
		BLE_GAP_ADV_TYPE_CONNECTABLE_NONSCANNABLE_DIRECTED_HIGH_DUTY_CYCLE,
		BLE_GAP_ADV_TYPE_CONNECTABLE_NONSCANNABLE_DIRECTED,
		BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED,
		BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED, 0};
	int adv_param_prop_duration[MAX_ADV_MODES] = {
		BLE_GAP_ADV_TIMEOUT_HIGH_DUTY_MAX,
		CONFIG_BLE_ADV_DIRECTED_ADVERTISING_TIMEOUT,
		CONFIG_BLE_ADV_FAST_ADVERTISING_TIMEOUT,
		CONFIG_BLE_ADV_SLOW_ADVERTISING_TIMEOUT, 0};
	int adv_param_prop_interval[MAX_ADV_MODES] = {0,
		CONFIG_BLE_ADV_DIRECTED_ADVERTISING_INTERVAL,
		CONFIG_BLE_ADV_FAST_ADVERTISING_INTERVAL,
		CONFIG_BLE_ADV_SLOW_ADVERTISING_INTERVAL, 0};
	uint32_t nrf_err;

	/* Verifying the different adv modes in the ble_adv_mode array */
	for (int i = 0; i < MAX_ADV_MODES; i++) {
		if (i > 2) {
			__cmock_sd_ble_gap_adv_set_configure_ExpectAndReturn(
				&ble_adv.adv_handle, &ble_adv.adv_data,
				&ble_adv.adv_params, NRF_SUCCESS);
		}
		__cmock_sd_ble_gap_adv_set_configure_IgnoreAndReturn(NRF_SUCCESS);
		__cmock_sd_ble_gap_adv_start_IgnoreAndReturn(NRF_SUCCESS);

		nrf_err = ble_adv_start(&ble_adv, mode[i]);
		TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
		TEST_ASSERT_TRUE(ble_adv.mode_current == mode[i]);
		TEST_ASSERT_TRUE(ble_adv.allow_list_in_use == false);
		TEST_ASSERT_TRUE(ble_adv.adv_params.primary_phy == CONFIG_BLE_ADV_PRIMARY_PHY);
		TEST_ASSERT_TRUE(ble_adv.adv_params.secondary_phy == CONFIG_BLE_ADV_SECONDARY_PHY);
		TEST_ASSERT_TRUE(ble_adv.adv_params.filter_policy == BLE_GAP_ADV_FP_ANY);
		if (mode[i] != BLE_ADV_MODE_IDLE) {
			TEST_ASSERT_TRUE(ble_adv.adv_params.properties.type == adv_prop_type[i]);
			TEST_ASSERT_TRUE(ble_adv.adv_params.duration == adv_param_prop_duration[i]);
			TEST_ASSERT_TRUE(ble_adv.adv_params.interval == adv_param_prop_interval[i]);
		}
		if (mode[i] == BLE_ADV_MODE_IDLE) {
			TEST_ASSERT_TRUE(ble_adv_evt_type == BLE_ADV_EVT_IDLE);
		}
		if (mode[i] == BLE_ADV_MODE_DIRECTED_HIGH_DUTY) {
			TEST_ASSERT_TRUE(ble_adv.peer_addr_reply_expected == false);
			TEST_ASSERT_TRUE(ble_adv_evt_type == BLE_ADV_EVT_DIRECTED_HIGH_DUTY);
		}
		if (mode[i] == BLE_ADV_MODE_DIRECTED) {
			TEST_ASSERT_TRUE(ble_adv.peer_addr_reply_expected == false);
			TEST_ASSERT_TRUE(ble_adv_evt_type == BLE_ADV_EVT_DIRECTED);
		}
		if (mode[i] == BLE_ADV_MODE_FAST) {
			TEST_ASSERT_TRUE(ble_adv.allow_list_reply_expected == true);
			TEST_ASSERT_TRUE(ble_adv_evt_type == BLE_ADV_EVT_FAST);
		}
		if (mode[i] == BLE_ADV_MODE_SLOW) {
			TEST_ASSERT_TRUE(ble_adv.allow_list_reply_expected == true);
			TEST_ASSERT_TRUE(ble_adv_evt_type == BLE_ADV_EVT_SLOW);
		}
	}
}

void test_ble_adv_start_error_invalid_param(void)
{
	struct ble_adv ble_adv = {
		.is_initialized = true,
		.evt_handler = ble_adv_evt_handler,
		.allow_list_temporarily_disabled = false,
	};
	uint32_t nrf_err;

	__cmock_sd_ble_gap_adv_set_configure_IgnoreAndReturn(NRF_ERROR_INVALID_PARAM);
	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_SLOW);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	__cmock_sd_ble_gap_adv_set_configure_IgnoreAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gap_adv_start_IgnoreAndReturn(NRF_ERROR_INVALID_STATE);
	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_SLOW);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
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
