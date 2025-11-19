/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <unity.h>
#include <stdint.h>
#include <stddef.h>
#include <bm/bluetooth/ble_scan.h>

#include "cmock_ble.h"
#include "cmock_ble_gap.h"
#include "cmock_ble_gatts.h"
#include "cmock_ble_gattc.h"

void scan_event_handler_func(struct scan_evt const *scan_evt)
{
}

void test_ble_scan_init(void)
{
	int err;

	err = ble_scan_init(NULL, NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	ble_scan_evt_handler_t scan_event = scan_event_handler_func;

	struct ble_scan ble_scan_module = {0};

	struct ble_scan_init scan_init = {.scan_param = NULL, .conn_param = NULL};

	err = ble_scan_init(&ble_scan_module, NULL, scan_event);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	err = ble_scan_init(&ble_scan_module, &scan_init, scan_event);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);

	ble_gap_scan_params_t dummy_scan_params = {.extended = 1,
						   .report_incomplete_evts = 1,
						   .active = 1,
						   .filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
						   .scan_phys = BLE_GAP_PHY_1MBPS,
						   .interval = CONFIG_BLE_SCAN_SCAN_INTERVAL,
						   .window = CONFIG_BLE_SCAN_SCAN_WINDOW,
						   .timeout = CONFIG_BLE_SCAN_SCAN_DURATION,
						   .channel_mask = {1, 1, 1, 1, 1}};

	ble_gap_conn_params_t dummy_conn_params = {.min_conn_interval = 1,
						   .max_conn_interval = 10,
						   .slave_latency = CONFIG_BLE_SCAN_SLAVE_LATENCY,
						   .conn_sup_timeout =
							   BLE_GAP_CP_CONN_SUP_TIMEOUT_MIN};

	scan_init.scan_param = &dummy_scan_params;
	scan_init.conn_param = &dummy_conn_params;
	err = ble_scan_init(&ble_scan_module, &scan_init, scan_event);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_ble_scan_start(void)
{
	int err;

	ble_scan_evt_handler_t scan_event = scan_event_handler_func;

	ble_gap_scan_params_t dummy_scan_params = {.extended = 1,
						   .report_incomplete_evts = 1,
						   .active = 1,
						   .filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
						   .scan_phys = BLE_GAP_PHY_1MBPS,
						   .interval = CONFIG_BLE_SCAN_SCAN_INTERVAL,
						   .window = CONFIG_BLE_SCAN_SCAN_WINDOW,
						   .timeout = CONFIG_BLE_SCAN_SCAN_DURATION,
						   .channel_mask = {1, 1, 1, 1, 1}};

	ble_gap_conn_params_t dummy_conn_params = {.min_conn_interval = 1,
						   .max_conn_interval = 10,
						   .slave_latency = CONFIG_BLE_SCAN_SLAVE_LATENCY,
						   .conn_sup_timeout =
							   BLE_GAP_CP_CONN_SUP_TIMEOUT_MIN};

	struct ble_scan ble_scan_module = {0};

	struct ble_scan_init scan_init = {.scan_param = &dummy_scan_params,
					  .conn_param = &dummy_conn_params};

	err = ble_scan_init(&ble_scan_module, &scan_init, scan_event);

	err = ble_scan_start(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	__cmock_sd_ble_gap_scan_stop_IgnoreAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(&ble_scan_module.scan_params,
						      &ble_scan_module.scan_buffer, NRF_SUCCESS);
	err = ble_scan_start(&ble_scan_module);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_ble_scan_filters_enable(void)
{
	int err;

	err = ble_scan_filters_enable(NULL, BLE_SCAN_ADDR_FILTER, true);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	ble_scan_evt_handler_t scan_event = scan_event_handler_func;

	ble_gap_scan_params_t dummy_scan_params = {.extended = 1,
						   .report_incomplete_evts = 1,
						   .active = 1,
						   .filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
						   .scan_phys = BLE_GAP_PHY_1MBPS,
						   .interval = CONFIG_BLE_SCAN_SCAN_INTERVAL,
						   .window = CONFIG_BLE_SCAN_SCAN_WINDOW,
						   .timeout = CONFIG_BLE_SCAN_SCAN_DURATION,
						   .channel_mask = {1, 1, 1, 1, 1}};

	ble_gap_conn_params_t dummy_conn_params = {.min_conn_interval = 1,
						   .max_conn_interval = 10,
						   .slave_latency = CONFIG_BLE_SCAN_SLAVE_LATENCY,
						   .conn_sup_timeout =
							   BLE_GAP_CP_CONN_SUP_TIMEOUT_MIN};

	struct ble_scan ble_scan_module = {0};

	struct ble_scan_init scan_init = {.scan_param = &dummy_scan_params,
					  .conn_param = &dummy_conn_params};

	err = ble_scan_init(&ble_scan_module, &scan_init, scan_event);

	err = ble_scan_filters_enable(&ble_scan_module, 0x20, true);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, err);

	err = ble_scan_filters_enable(&ble_scan_module, BLE_SCAN_ALL_FILTER, true);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_ble_scan_filters_disable(void)
{
	int err;

	err = ble_scan_filters_disable(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	ble_scan_evt_handler_t scan_event = scan_event_handler_func;

	ble_gap_scan_params_t dummy_scan_params = {.extended = 1,
						   .report_incomplete_evts = 1,
						   .active = 1,
						   .filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
						   .scan_phys = BLE_GAP_PHY_1MBPS,
						   .interval = CONFIG_BLE_SCAN_SCAN_INTERVAL,
						   .window = CONFIG_BLE_SCAN_SCAN_WINDOW,
						   .timeout = CONFIG_BLE_SCAN_SCAN_DURATION,
						   .channel_mask = {1, 1, 1, 1, 1}};

	ble_gap_conn_params_t dummy_conn_params = {.min_conn_interval = 1,
						   .max_conn_interval = 10,
						   .slave_latency = CONFIG_BLE_SCAN_SLAVE_LATENCY,
						   .conn_sup_timeout =
							   BLE_GAP_CP_CONN_SUP_TIMEOUT_MIN};

	struct ble_scan ble_scan_module = {0};

	struct ble_scan_init scan_init = {.scan_param = &dummy_scan_params,
					  .conn_param = &dummy_conn_params};

	err = ble_scan_init(&ble_scan_module, &scan_init, scan_event);

	err = ble_scan_filters_disable(&ble_scan_module);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_ble_scan_filter_get(void)
{
	int err;

	struct ble_scan ble_scan_module = {0};

	struct ble_scan_filters ble_scan_filter_data = {0};

	ble_scan_evt_handler_t scan_event = scan_event_handler_func;

	ble_gap_scan_params_t dummy_scan_params = {.extended = 1,
						   .report_incomplete_evts = 1,
						   .active = 1,
						   .filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
						   .scan_phys = BLE_GAP_PHY_1MBPS,
						   .interval = CONFIG_BLE_SCAN_SCAN_INTERVAL,
						   .window = CONFIG_BLE_SCAN_SCAN_WINDOW,
						   .timeout = CONFIG_BLE_SCAN_SCAN_DURATION,
						   .channel_mask = {1, 1, 1, 1, 1}};

	ble_gap_conn_params_t dummy_conn_params = {.min_conn_interval = 1,
						   .max_conn_interval = 10,
						   .slave_latency = CONFIG_BLE_SCAN_SLAVE_LATENCY,
						   .conn_sup_timeout =
							   BLE_GAP_CP_CONN_SUP_TIMEOUT_MIN};

	struct ble_scan_init scan_init = {.scan_param = &dummy_scan_params,
					  .conn_param = &dummy_conn_params};

	ble_scan_init(&ble_scan_module, &scan_init, scan_event);

	ble_scan_filters_enable(&ble_scan_module, BLE_SCAN_ALL_FILTER, true);

	char *device_name = "generic_device";

	ble_scan_filter_set(&ble_scan_module, BLE_SCAN_ALL_FILTER, device_name);

	err = ble_scan_filter_get(&ble_scan_module, &ble_scan_filter_data);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_ble_scan_filter_set(void)
{
	int err;

	struct ble_scan ble_scan_module = {0};

	ble_scan_evt_handler_t scan_event = scan_event_handler_func;

	ble_gap_scan_params_t dummy_scan_params = {.extended = 1,
						   .report_incomplete_evts = 1,
						   .active = 1,
						   .filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
						   .scan_phys = BLE_GAP_PHY_1MBPS,
						   .interval = CONFIG_BLE_SCAN_SCAN_INTERVAL,
						   .window = CONFIG_BLE_SCAN_SCAN_WINDOW,
						   .timeout = CONFIG_BLE_SCAN_SCAN_DURATION,
						   .channel_mask = {1, 1, 1, 1, 1}};

	ble_gap_conn_params_t dummy_conn_params = {.min_conn_interval = 1,
						   .max_conn_interval = 10,
						   .slave_latency = CONFIG_BLE_SCAN_SLAVE_LATENCY,
						   .conn_sup_timeout =
							   BLE_GAP_CP_CONN_SUP_TIMEOUT_MIN};

	struct ble_scan_init scan_init = {.scan_param = &dummy_scan_params,
					  .conn_param = &dummy_conn_params};

	ble_scan_init(&ble_scan_module, &scan_init, scan_event);

	ble_scan_filters_enable(&ble_scan_module, BLE_SCAN_ALL_FILTER, true);

	char *device_name = "generic_device";

	err = ble_scan_filter_set(NULL, BLE_SCAN_ALL_FILTER, device_name);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	err = ble_scan_filter_set(&ble_scan_module, BLE_SCAN_ALL_FILTER, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);
}

void test_is_whitelist_used(void)
{
	bool err;

	struct ble_scan ble_scan_module = {0};

	err = is_whitelist_used(&ble_scan_module);
	TEST_ASSERT_FALSE(err);

	ble_scan_module.scan_params.filter_policy = BLE_GAP_SCAN_FP_WHITELIST;

	err = is_whitelist_used(&ble_scan_module);
	TEST_ASSERT_TRUE(err);
}

void test_ble_scan_all_filter_remove(void)
{
	int err;

	struct ble_scan ble_scan_module = {0};

	err = ble_scan_all_filter_remove(&ble_scan_module);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_ble_scan_params_set(void)
{
	int err;

	struct ble_scan ble_scan_module = {0};

	err = ble_scan_params_set(NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	__cmock_sd_ble_gap_scan_stop_IgnoreAndReturn(NRF_SUCCESS);
	err = ble_scan_params_set(&ble_scan_module, NULL);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_ble_scan_copy_addr_to_sd_gap_addr(void)
{
	int err;

	err = ble_scan_copy_addr_to_sd_gap_addr(NULL, 0);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, err);

	uint8_t address[BLE_GAP_ADDR_LEN];
	ble_gap_addr_t gap_address = {0};
	err = ble_scan_copy_addr_to_sd_gap_addr(&gap_address, address);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err);
}

void test_ble_scan_on_ble_evt(void)
{
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
