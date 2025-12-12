/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <unity.h>
#include <stdint.h>
#include <stddef.h>
#include <bm/bluetooth/ble_scan.h>

#include <observers.h>

#include "cmock_ble_gap.h"
#include "cmock_ble_gatts.h"
#include "cmock_ble_gattc.h"
#include "cmock_nrf_sdh_ble.h"
#include "cmock_ble_adv_data.h"

#define CONN_HANDLE 1
#define DEVICE_NAME "my_device"

#define UUID 0x2312

BLE_SCAN_DEF(ble_scan);

static struct ble_scan_evt scan_event;
static struct ble_scan_evt scan_event_prev;

void scan_event_handler_func(struct ble_scan_evt const *scan_evt)
{
	scan_event_prev = scan_event;
	scan_event = *scan_evt;
}

void test_ble_scan_init_error_null(void)
{
	uint32_t nrf_err;

	struct ble_scan_config scan_cfg = {
		.evt_handler = scan_event_handler_func,
	};

	nrf_err = ble_scan_init(NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_scan_init(&ble_scan, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_scan_init(NULL, &scan_cfg);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_scan_init(void)
{
	uint32_t nrf_err;

	struct ble_scan_config scan_cfg = {
		.evt_handler = scan_event_handler_func,
	};

	struct ble_scan_config scan_cfg_no_handler = {
		.evt_handler = scan_event_handler_func,
	};

	struct ble_scan_config scan_cfg_with_params = {
		.scan_params = {
			.extended = 1,
			.report_incomplete_evts = 1,
			.active = 1,
			.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
			.scan_phys = BLE_GAP_PHY_1MBPS,
			.interval = CONFIG_BLE_SCAN_INTERVAL,
			.window = CONFIG_BLE_SCAN_WINDOW,
			.timeout = CONFIG_BLE_SCAN_DURATION,
			.channel_mask = {1, 1, 1, 1, 1},
		},
		.conn_params = {
			.min_conn_interval = 1,
			.max_conn_interval = 10,
			.slave_latency = CONFIG_BLE_SCAN_PERIPHERAL_LATENCY,
			.conn_sup_timeout = BLE_GAP_CP_CONN_SUP_TIMEOUT_MIN,
		},
		.evt_handler = scan_event_handler_func,
	};

	nrf_err = ble_scan_init(&ble_scan, &scan_cfg);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_scan_init(&ble_scan, &scan_cfg_no_handler);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_scan_init(&ble_scan, &scan_cfg_with_params);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_scan_start_error_null(void)
{
	uint32_t nrf_err;

	test_ble_scan_init();

	nrf_err = ble_scan_start(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_scan_start(void)
{
	uint32_t nrf_err;

	test_ble_scan_init();

	__cmock_sd_ble_gap_scan_stop_IgnoreAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(&ble_scan.scan_params,
						      &ble_scan.scan_buffer, NRF_SUCCESS);
	nrf_err = ble_scan_start(&ble_scan);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_scan_filters_enable_error_invalid_param(void)
{
	uint32_t nrf_err;

	test_ble_scan_init();

	nrf_err = ble_scan_filters_enable(&ble_scan, 0x20, true);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_scan_filters_enable_error_null(void)
{
	uint32_t nrf_err;

	test_ble_scan_init();

	nrf_err = ble_scan_filters_enable(NULL, BLE_SCAN_ADDR_FILTER, true);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_scan_filters_enable_all(void)
{
	uint32_t nrf_err;

	test_ble_scan_init();

	nrf_err = ble_scan_filters_enable(&ble_scan, (BLE_SCAN_NAME_FILTER |
						      BLE_SCAN_SHORT_NAME_FILTER |
						      BLE_SCAN_ADDR_FILTER |
						      BLE_SCAN_UUID_FILTER |
						      BLE_SCAN_APPEARANCE_FILTER), true);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_scan_filters_disable_error_null(void)
{
	uint32_t nrf_err;

	test_ble_scan_init();

	nrf_err = ble_scan_filters_disable(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_scan_filters_disable(void)
{
	uint32_t nrf_err;

	test_ble_scan_init();

	nrf_err = ble_scan_filters_disable(&ble_scan);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_scan_filter_add_get(void)
{
	uint32_t nrf_err;

	struct ble_scan_filters ble_scan_filter_data = {0};
	char *device_name = "generic_device";

	test_ble_scan_init();

	nrf_err = ble_scan_filter_get(&ble_scan, &ble_scan_filter_data);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, ble_scan_filter_data.name_filter.name_cnt);
	TEST_ASSERT_FALSE(ble_scan_filter_data.name_filter.name_filter_enabled);

	ble_scan_filters_enable(&ble_scan, (BLE_SCAN_NAME_FILTER |
						   BLE_SCAN_SHORT_NAME_FILTER |
						   BLE_SCAN_ADDR_FILTER |
						   BLE_SCAN_UUID_FILTER |
						   BLE_SCAN_APPEARANCE_FILTER), true);

	nrf_err = ble_scan_filter_get(&ble_scan, &ble_scan_filter_data);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, ble_scan_filter_data.name_filter.name_cnt);
	TEST_ASSERT_TRUE(ble_scan_filter_data.name_filter.name_filter_enabled);

	ble_scan_filter_add(&ble_scan, BLE_SCAN_NAME_FILTER, device_name);

	nrf_err = ble_scan_filter_get(&ble_scan, &ble_scan_filter_data);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, ble_scan_filter_data.name_filter.name_cnt);
	TEST_ASSERT_TRUE(ble_scan_filter_data.name_filter.name_filter_enabled);
	TEST_ASSERT_EQUAL_MEMORY(device_name, ble_scan_filter_data.name_filter.target_name[0],
				 sizeof(device_name));
}

void test_ble_scan_filter_add_error_null(void)
{
	uint32_t nrf_err;
	char *device_name = "generic_device";

	test_ble_scan_init();

	ble_scan_filters_enable(&ble_scan, (BLE_SCAN_NAME_FILTER), true);

	nrf_err = ble_scan_filter_add(NULL, (BLE_SCAN_NAME_FILTER), device_name);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_scan_filter_add(&ble_scan, (BLE_SCAN_NAME_FILTER), NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_scan_filter_add_error_invalid_param(void)
{
	uint32_t nrf_err;
	char *device_name = DEVICE_NAME;

	test_ble_scan_init();

	ble_scan_filters_enable(&ble_scan, BLE_SCAN_NAME_FILTER, true);

	nrf_err = ble_scan_filter_add(&ble_scan, 0, device_name);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_scan_filter_add_name(void)
{
	uint32_t nrf_err;
	char *device_name = DEVICE_NAME;

	test_ble_scan_init();

	ble_scan_filters_enable(&ble_scan, BLE_SCAN_NAME_FILTER, true);

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_NAME_FILTER, device_name);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_scan_filter_add_addr(void)
{
	uint32_t nrf_err;
	uint8_t addr[BLE_GAP_ADDR_LEN] = {0xa, 0xd, 0xd, 0x4, 0xe, 0x5};

	test_ble_scan_init();

	ble_scan_filters_enable(&ble_scan, BLE_SCAN_ADDR_FILTER, true);

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_ADDR_FILTER, addr);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_scan_filter_add_addr_enomem(void)
{
	uint32_t nrf_err;
	uint8_t addr[BLE_GAP_ADDR_LEN] = {0xa, 0xd, 0xd, 0x4, 0xe, 0x5};

	test_ble_scan_filter_add_addr();

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_ADDR_FILTER, addr);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_ble_scan_filter_add_uuid(void)
{
	uint32_t nrf_err;
	ble_uuid_t uuid = {
		.uuid = UUID,
		.type = BLE_UUID_TYPE_BLE,
	};

	test_ble_scan_init();

	ble_scan_filters_enable(&ble_scan, BLE_SCAN_UUID_FILTER, true);

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_UUID_FILTER, &uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_scan_filter_add_uuid_error_no_mem(void)
{
	uint32_t nrf_err;
	ble_uuid_t uuid;

	test_ble_scan_filter_add_uuid();

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_UUID_FILTER, &uuid);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_ble_scan_filter_add_appearance(void)
{
	uint32_t nrf_err;
	uint16_t appearance = 0xa44e;

	test_ble_scan_init();

	ble_scan_filters_enable(&ble_scan, BLE_SCAN_APPEARANCE_FILTER, true);

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_APPEARANCE_FILTER, &appearance);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_scan_filter_add_appearance_error_no_mem(void)
{
	uint32_t nrf_err;
	uint16_t appearance = 0xa44e;

	test_ble_scan_filter_add_appearance();

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_APPEARANCE_FILTER, &appearance);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_ble_scan_filter_add_short_name(void)
{
	uint32_t nrf_err;
	struct ble_scan_short_name short_name = {
		.short_name = "dev",
		.short_name_min_len = 2,
	};

	test_ble_scan_init();

	ble_scan_filters_enable(&ble_scan, BLE_SCAN_SHORT_NAME_FILTER, true);

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_SHORT_NAME_FILTER, &short_name);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_scan_filter_add_short_name_error_no_mem(void)
{
	uint32_t nrf_err;
	struct ble_scan_short_name short_name = {
		.short_name = "dev",
		.short_name_min_len = 2,
	};
	struct ble_scan_short_name short_name2 = {
		.short_name = "dev2",
		.short_name_min_len = 2,
	};

	test_ble_scan_filter_add_short_name();

	/* duplicate filter does not increase count, so the next will also succeed. */
	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_SHORT_NAME_FILTER, &short_name);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* We accept two short names so the second will succeed */
	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_SHORT_NAME_FILTER, &short_name2);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_SHORT_NAME_FILTER, &short_name2);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);

}

void test_is_allow_list_used(void)
{
	bool nrf_err;

	nrf_err = is_allow_list_used(&ble_scan);
	TEST_ASSERT_FALSE(nrf_err);

	ble_scan.scan_params.filter_policy = BLE_GAP_SCAN_FP_WHITELIST;

	nrf_err = is_allow_list_used(&ble_scan);
	TEST_ASSERT_TRUE(nrf_err);
}

void test_ble_scan_all_filter_remove(void)
{
	uint32_t nrf_err;

	nrf_err = ble_scan_all_filter_remove(&ble_scan);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_scan_params_set_error_null(void)
{
	uint32_t nrf_err;
	ble_gap_scan_params_t scan_params = BLE_SCAN_SCAN_PARAMS_DEFAULT;

	nrf_err = ble_scan_params_set(NULL, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_scan_params_set(&ble_scan, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_scan_params_set(NULL, &scan_params);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

}

void test_ble_scan_params_set(void)
{
	uint32_t nrf_err;
	ble_gap_scan_params_t scan_params = BLE_SCAN_SCAN_PARAMS_DEFAULT;

	__cmock_sd_ble_gap_scan_stop_IgnoreAndReturn(NRF_SUCCESS);
	nrf_err = ble_scan_params_set(&ble_scan, &scan_params);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_scan_copy_addr_to_sd_gap_addr_error_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_scan_copy_addr_to_sd_gap_addr(NULL, 0);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_scan_copy_addr_to_sd_gap_addr(void)
{
	uint32_t nrf_err;
	uint8_t address[BLE_GAP_ADDR_LEN] = {0};
	ble_gap_addr_t gap_address = {0};

	nrf_err = ble_scan_copy_addr_to_sd_gap_addr(&gap_address,
						    address);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_scan_on_ble_evt_adv_report_empty(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_ADV_REPORT,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.adv_report = {},
		},
	};

	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(
		NULL, &ble_scan.scan_buffer, NRF_SUCCESS);

	ble_evt_send(&ble_evt);

	test_ble_scan_init();
}

void test_ble_scan_on_ble_evt_adv_report_device_name_bad_data(void)
{
	char bad_data[] = "baddata";
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_ADV_REPORT,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.adv_report = {
				.data = {
					.p_data = bad_data,
					.len = sizeof(bad_data),
				},

			},
		},
	};

	test_ble_scan_init();
	test_ble_scan_filter_add_name();

	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(
		NULL, &ble_scan.scan_buffer, NRF_SUCCESS);

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_NOT_FOUND, scan_event.evt_type);

}

void test_ble_scan_on_ble_evt_adv_report_device_address_not_found(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_ADV_REPORT,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.adv_report = {
				.peer_addr = {
					.addr = {0xb, 0xa, 0xd, 0x4, 0xd, 0xd},
				},
			},
		},
	};

	test_ble_scan_init();
	test_ble_scan_filter_add_addr();

	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(
		NULL, &ble_scan.scan_buffer, NRF_SUCCESS);

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_NOT_FOUND, scan_event.evt_type);
}

void test_ble_scan_on_ble_evt_adv_report_device_address(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_ADV_REPORT,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.adv_report = {
				.peer_addr = {
					.addr = {0xa, 0xd, 0xd, 0x4, 0xe, 0x5},
				},
			},
		},
	};

	test_ble_scan_init();
	test_ble_scan_filter_add_addr();

	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(
		NULL, &ble_scan.scan_buffer, NRF_SUCCESS);

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_FILTER_MATCH, scan_event.evt_type);
	TEST_ASSERT_EQUAL(1, scan_event.params.filter_match.filter_match.address_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.name_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.short_name_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.appearance_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.uuid_filter_match);
	TEST_ASSERT_EQUAL_PTR(&ble_evt.evt.gap_evt.params.adv_report,
			      scan_event.params.filter_match.adv_report);
}

void test_ble_scan_on_ble_evt_adv_report_device_name_not_found(void)
{
	uint8_t device_name_data[] = {
		10, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
		'n', 'o', 't', '_', 'm', 'y', '_', 'd', 'e', 'v', 'i', 'c', 'e',
	};
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_ADV_REPORT,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.adv_report = {
				.data = {
					.p_data = device_name_data,
					.len = sizeof(device_name_data),
				},
			},
		},
	};

	test_ble_scan_init();
	test_ble_scan_filter_add_name();

	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(
		NULL, &ble_scan.scan_buffer, NRF_SUCCESS);

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_NOT_FOUND, scan_event.evt_type);
}

void test_ble_scan_on_ble_evt_adv_report_device_name(void)
{
	uint8_t device_name_data[] = {
		10, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
		'm', 'y', '_', 'd', 'e', 'v', 'i', 'c', 'e',
	};
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_ADV_REPORT,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.adv_report = {
				.data = {
					.p_data = device_name_data,
					.len = sizeof(device_name_data),
				},
			},
		},
	};

	test_ble_scan_init();
	test_ble_scan_filter_add_name();

	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(
		NULL, &ble_scan.scan_buffer, NRF_SUCCESS);

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_FILTER_MATCH, scan_event.evt_type);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.address_filter_match);
	TEST_ASSERT_EQUAL(1, scan_event.params.filter_match.filter_match.name_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.short_name_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.appearance_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.uuid_filter_match);
	TEST_ASSERT_EQUAL_PTR(&ble_evt.evt.gap_evt.params.adv_report,
			      scan_event.params.filter_match.adv_report);
}

void test_ble_scan_on_ble_evt_adv_report_device_short_name_not_found(void)
{
	static const char short_name_exp[] = "dev";
	const uint8_t min_len_exp = 2;
	uint8_t dummy_data[] = "hello";

	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_ADV_REPORT,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.adv_report = {
				.data = {
					.p_data = dummy_data,
					.len = sizeof(dummy_data),
				},
			},
		},
	};

	test_ble_scan_init();
	test_ble_scan_filter_add_short_name();

	__cmock_ble_adv_data_short_name_find_ExpectWithArrayAndReturn(
		ble_evt.evt.gap_evt.params.adv_report.data.p_data, 1,
		ble_evt.evt.gap_evt.params.adv_report.data.len,
		short_name_exp, min_len_exp, false);

	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(
		NULL, &ble_scan.scan_buffer, NRF_SUCCESS);

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_NOT_FOUND, scan_event.evt_type);
}

void test_ble_scan_on_ble_evt_adv_report_device_short_name(void)
{
	static const char short_name_exp[] = "dev";
	const uint8_t min_len_exp = 2;
	uint8_t dummy_data[] = "hello";

	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_ADV_REPORT,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.adv_report = {
				.data = {
					.p_data = dummy_data,
					.len = sizeof(dummy_data),
				},
			},
		},
	};

	test_ble_scan_init();
	test_ble_scan_filter_add_short_name();

	__cmock_ble_adv_data_short_name_find_ExpectWithArrayAndReturn(
		ble_evt.evt.gap_evt.params.adv_report.data.p_data, 1,
		ble_evt.evt.gap_evt.params.adv_report.data.len,
		short_name_exp, min_len_exp, true);

	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(
		NULL, &ble_scan.scan_buffer, NRF_SUCCESS);

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_FILTER_MATCH, scan_event.evt_type);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.address_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.name_filter_match);
	TEST_ASSERT_EQUAL(1, scan_event.params.filter_match.filter_match.short_name_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.appearance_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.uuid_filter_match);
	TEST_ASSERT_EQUAL_PTR(&ble_evt.evt.gap_evt.params.adv_report,
			      scan_event.params.filter_match.adv_report);
}

void test_ble_scan_on_ble_evt_adv_report_device_appearance_not_found(void)
{
	uint16_t appearance_exp = 0xa44e;
	uint8_t dummy_data[] = "hello";

	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_ADV_REPORT,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.adv_report = {
				.data = {
					.p_data = dummy_data,
					.len = sizeof(dummy_data),
				},
			},
		},
	};

	test_ble_scan_init();
	test_ble_scan_filter_add_appearance();

	__cmock_ble_adv_data_appearance_find_ExpectWithArrayAndReturn(
		ble_evt.evt.gap_evt.params.adv_report.data.p_data, 1,
		ble_evt.evt.gap_evt.params.adv_report.data.len,
		&appearance_exp, 1, false);

	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(NULL, &ble_scan.scan_buffer,
						      NRF_SUCCESS);

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_NOT_FOUND, scan_event.evt_type);
}

void test_ble_scan_on_ble_evt_adv_report_device_appearance(void)
{
	uint16_t appearance_exp = 0xa44e;
	uint8_t dummy_data[] = "hello";

	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_ADV_REPORT,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.adv_report = {
				.data = {
					.p_data = dummy_data,
					.len = sizeof(dummy_data),
				},
			},
		},
	};

	test_ble_scan_init();
	test_ble_scan_filter_add_appearance();

	__cmock_ble_adv_data_appearance_find_ExpectWithArrayAndReturn(
		ble_evt.evt.gap_evt.params.adv_report.data.p_data, 1,
		ble_evt.evt.gap_evt.params.adv_report.data.len,
		&appearance_exp, 1, true);

	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(NULL, &ble_scan.scan_buffer,
						      NRF_SUCCESS);

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_FILTER_MATCH, scan_event.evt_type);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.address_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.name_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.short_name_filter_match);
	TEST_ASSERT_EQUAL(1, scan_event.params.filter_match.filter_match.appearance_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.uuid_filter_match);
	TEST_ASSERT_EQUAL_PTR(&ble_evt.evt.gap_evt.params.adv_report,
			      scan_event.params.filter_match.adv_report);
}

void test_ble_scan_on_ble_evt_adv_report_device_uuid_not_found(void)
{
	const ble_uuid_t uuid_exp = {
		.uuid = UUID,
		.type = BLE_UUID_TYPE_BLE,
	};
	uint8_t dummy_data[] = "hello";
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_ADV_REPORT,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.adv_report = {
				.data = {
					.p_data = dummy_data,
					.len = sizeof(dummy_data),
				},
			},
		},
	};

	test_ble_scan_init();
	test_ble_scan_filter_add_uuid();

	__cmock_ble_adv_data_uuid_find_ExpectWithArrayAndReturn(
		ble_evt.evt.gap_evt.params.adv_report.data.p_data, 1,
		ble_evt.evt.gap_evt.params.adv_report.data.len,
		&uuid_exp, 1, false);
	/* Size of ble_uuid_t is 4, though only 3 is used, so last byte will fail to compare. */
	__cmock_ble_adv_data_uuid_find_IgnoreArg_uuid();

	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(NULL, &ble_scan.scan_buffer,
						      NRF_SUCCESS);

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_NOT_FOUND, scan_event.evt_type);
}

void test_ble_scan_on_ble_evt_adv_report_device_uuid(void)
{
	const ble_uuid_t uuid_exp = {
		.uuid = UUID,
		.type = BLE_UUID_TYPE_BLE,
	};
	uint8_t dummy_data[] = "hello";
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_ADV_REPORT,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.adv_report = {
				.data = {
					.p_data = dummy_data,
					.len = sizeof(dummy_data),
				},
			},
		},
	};

	test_ble_scan_init();
	test_ble_scan_filter_add_uuid();

	__cmock_ble_adv_data_uuid_find_ExpectWithArrayAndReturn(
		ble_evt.evt.gap_evt.params.adv_report.data.p_data, 1,
		ble_evt.evt.gap_evt.params.adv_report.data.len,
		&uuid_exp, 1, true);
	/* Size of ble_uuid_t is 4, though only 3 is used, so last byte will fail to compare. */
	__cmock_ble_adv_data_uuid_find_IgnoreArg_uuid();

	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(NULL, &ble_scan.scan_buffer,
						      NRF_SUCCESS);

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_FILTER_MATCH, scan_event.evt_type);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.address_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.name_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.short_name_filter_match);
	TEST_ASSERT_EQUAL(0, scan_event.params.filter_match.filter_match.appearance_filter_match);
	TEST_ASSERT_EQUAL(1, scan_event.params.filter_match.filter_match.uuid_filter_match);
	TEST_ASSERT_EQUAL_PTR(&ble_evt.evt.gap_evt.params.adv_report,
			      scan_event.params.filter_match.adv_report);
}

void test_ble_scan_on_ble_evt_adv_report_device_uuid_connect(void)
{
	uint32_t nrf_err;
	ble_uuid_t uuid = {
		.uuid = UUID,
		.type = BLE_UUID_TYPE_BLE,
	};
	const ble_uuid_t uuid_exp = {
		.uuid = UUID,
		.type = BLE_UUID_TYPE_BLE,
	};
	uint8_t dummy_data[] = "hello";
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_ADV_REPORT,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.adv_report = {
				.data = {
					.p_data = dummy_data,
					.len = sizeof(dummy_data),
				},
				.peer_addr = {
					.addr = {0xa, 0xd, 0xd, 0x4, 0xe, 0x5},
					.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE,
				}
			},
		},
	};

	struct ble_scan_config scan_cfg_with_params = {
		.scan_params = {
			.extended = 1,
			.report_incomplete_evts = 1,
			.active = 1,
			.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
			.scan_phys = BLE_GAP_PHY_1MBPS,
			.interval = CONFIG_BLE_SCAN_INTERVAL,
			.window = CONFIG_BLE_SCAN_WINDOW,
			.timeout = CONFIG_BLE_SCAN_DURATION,
			.channel_mask = {1, 1, 1, 1, 1},
		},
		.conn_params = {
			.min_conn_interval = 1,
			.max_conn_interval = 10,
			.slave_latency = CONFIG_BLE_SCAN_PERIPHERAL_LATENCY,
			.conn_sup_timeout = BLE_GAP_CP_CONN_SUP_TIMEOUT_MIN,
		},
		.evt_handler = scan_event_handler_func,
		.connect_if_match = true,
		.conn_cfg_tag = 5,
	};

	nrf_err = ble_scan_init(&ble_scan, &scan_cfg_with_params);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	ble_scan_filters_enable(&ble_scan, BLE_SCAN_UUID_FILTER, true);

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_UUID_FILTER, &uuid);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_ble_adv_data_uuid_find_ExpectWithArrayAndReturn(
		ble_evt.evt.gap_evt.params.adv_report.data.p_data, 1,
		ble_evt.evt.gap_evt.params.adv_report.data.len,
		&uuid_exp, 1, true);
	/* Size of ble_uuid_t is 4, though only 3 is used, so last byte will fail to compare. */
	__cmock_ble_adv_data_uuid_find_IgnoreArg_uuid();

	__cmock_sd_ble_gap_scan_stop_ExpectAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gap_connect_ExpectWithArrayAndReturn(
		&ble_evt.evt.gap_evt.params.adv_report.peer_addr, 1,
		&scan_cfg_with_params.scan_params, 1,
		&scan_cfg_with_params.conn_params, 1,
		scan_cfg_with_params.conn_cfg_tag, NRF_SUCCESS);

	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(NULL, &ble_scan.scan_buffer,
						      NRF_SUCCESS);

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_FILTER_MATCH, scan_event.evt_type);

	__cmock_ble_adv_data_uuid_find_ExpectWithArrayAndReturn(
		ble_evt.evt.gap_evt.params.adv_report.data.p_data, 1,
		ble_evt.evt.gap_evt.params.adv_report.data.len,
		&uuid_exp, 1, true);
	/* Size of ble_uuid_t is 4, though only 3 is used, so last byte will fail to compare. */
	__cmock_ble_adv_data_uuid_find_IgnoreArg_uuid();

	__cmock_sd_ble_gap_scan_stop_ExpectAndReturn(NRF_SUCCESS);
	__cmock_sd_ble_gap_connect_ExpectWithArrayAndReturn(
		&ble_evt.evt.gap_evt.params.adv_report.peer_addr, 1,
		&scan_cfg_with_params.scan_params, 1,
		&scan_cfg_with_params.conn_params, 1,
		scan_cfg_with_params.conn_cfg_tag, NRF_ERROR_BUSY);

	__cmock_sd_ble_gap_scan_start_ExpectAndReturn(NULL, &ble_scan.scan_buffer,
						      NRF_SUCCESS);

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_CONNECTING_ERROR, scan_event_prev.evt_type);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_FILTER_MATCH, scan_event.evt_type);
}

void test_ble_scan_on_ble_evt_timeout(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_TIMEOUT,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.timeout = {
				.src = BLE_GAP_TIMEOUT_SRC_SCAN,
			},
		},
	};

	test_ble_scan_init();

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_SCAN_TIMEOUT, scan_event.evt_type);
	TEST_ASSERT_EQUAL(BLE_GAP_TIMEOUT_SRC_SCAN, scan_event.params.timeout.src);
}

void test_ble_scan_on_ble_evt_connected(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_CONNECTED,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.connected = {
				.role = 1,
			},
		},
	};

	test_ble_scan_init();

	ble_evt_send(&ble_evt);
	TEST_ASSERT_EQUAL(BLE_SCAN_EVT_CONNECTED, scan_event.evt_type);
	TEST_ASSERT_EQUAL(1, scan_event.params.connected.connected->role);
	TEST_ASSERT_EQUAL(CONN_HANDLE, scan_event.params.connected.conn_handle);
}

void setUp(void)
{
	memset(&ble_scan, 0, sizeof(ble_scan));
}

void tearDown(void)
{
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
