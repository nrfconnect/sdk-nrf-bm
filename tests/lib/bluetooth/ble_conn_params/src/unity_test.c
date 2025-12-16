/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <bm/bluetooth/ble_conn_params.h>

#include <observers.h>

#include "cmock_ble_gap.h"
#include "cmock_ble_gattc.h"
#include "cmock_ble_gatts.h"
#include "cmock_nrf_sdh_ble.h"

#define BLE_GAP_DATA_LENGTH_DEFAULT 27

#define CONN_HANDLE 1
#define ATT_MTU_VALID BLE_GATT_ATT_MTU_DEFAULT
#define ATT_MTU_INVALID (BLE_GATT_ATT_MTU_DEFAULT - 1)

struct ble_conn_params_evt app_evt;

/* Event handler for conn params */
void conn_params_evt_handler(const struct ble_conn_params_evt *evt)
{
	app_evt = *evt;
}

void test_ble_conn_params_evt_handler_set_error_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_conn_params_evt_handler_set(NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_evt_no_handler(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTC_EVT_EXCHANGE_MTU_RSP,
		.evt.gattc_evt = {
			.conn_handle = CONN_HANDLE,
			.params.exchange_mtu_rsp.server_rx_mtu = ATT_MTU_VALID,
		},

	};

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	ble_evt_send(&ble_evt);
}

void test_ble_conn_params_evt_handler_set(void)
{
	uint32_t nrf_err;

	nrf_err = ble_conn_params_evt_handler_set(conn_params_evt_handler);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_conn_params_override_error_null(void)
{
	uint32_t nrf_err;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_override(CONN_HANDLE, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_conn_params_override_error_invalid_conn_handle(void)
{
	uint32_t nrf_err;
	const ble_gap_conn_params_t conn_params = {
		.conn_sup_timeout = 10,
		.min_conn_interval = 20,
		.max_conn_interval = 30,
		.slave_latency = 40,
	};

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	nrf_err = ble_conn_params_override(CONN_HANDLE, &conn_params);
	TEST_ASSERT_EQUAL(BLE_ERROR_INVALID_CONN_HANDLE, nrf_err);
}

void test_ble_conn_params_override_error_busy(void)
{
	uint32_t nrf_err;
	const ble_gap_conn_params_t conn_params = {
		.conn_sup_timeout = 10,
		.min_conn_interval = 20,
		.max_conn_interval = 30,
		.slave_latency = 40,
	};

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_sd_ble_gap_conn_param_update_ExpectAndReturn(CONN_HANDLE, &conn_params,
							     NRF_ERROR_BUSY);

	nrf_err = ble_conn_params_override(CONN_HANDLE, &conn_params);
	TEST_ASSERT_EQUAL(NRF_ERROR_BUSY, nrf_err);
}

void test_ble_conn_params_override(void)
{
	uint32_t nrf_err;
	const ble_gap_conn_params_t conn_params = {
		.conn_sup_timeout = 10,
		.min_conn_interval = 20,
		.max_conn_interval = 30,
		.slave_latency = 40,
	};

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_sd_ble_gap_conn_param_update_ExpectAndReturn(CONN_HANDLE, &conn_params,
							     NRF_SUCCESS);

	nrf_err = ble_conn_params_override(CONN_HANDLE, &conn_params);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_conn_params_att_mtu_set_error_invalid_param(void)
{
	uint32_t nrf_err;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_att_mtu_set(CONN_HANDLE, ATT_MTU_INVALID);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	nrf_err = ble_conn_params_att_mtu_set(CONN_HANDLE, ATT_MTU_VALID);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_conn_params_att_mtu_set(void)
{
	uint32_t nrf_err;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_sd_ble_gattc_exchange_mtu_request_ExpectAndReturn(CONN_HANDLE, ATT_MTU_VALID,
								  NRF_SUCCESS);

	nrf_err = ble_conn_params_att_mtu_set(CONN_HANDLE, ATT_MTU_VALID);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_sd_ble_gattc_exchange_mtu_request_ExpectAndReturn(CONN_HANDLE, ATT_MTU_VALID,
								  NRF_ERROR_INVALID_STATE);

	nrf_err = ble_conn_params_att_mtu_set(CONN_HANDLE, ATT_MTU_VALID);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_conn_params_att_mtu_set_retry_after_busy(void)
{
	uint32_t nrf_err;

	ble_evt_t ble_evt = {
		.evt.common_evt.conn_handle = CONN_HANDLE,
	};

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_sd_ble_gattc_exchange_mtu_request_ExpectAndReturn(CONN_HANDLE, ATT_MTU_VALID,
								  NRF_ERROR_BUSY);

	nrf_err = ble_conn_params_att_mtu_set(CONN_HANDLE, ATT_MTU_VALID);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Inject SoftDevice event to retrigger the request */

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	__cmock_sd_ble_gattc_exchange_mtu_request_ExpectAndReturn(CONN_HANDLE, ATT_MTU_VALID,
								  NRF_SUCCESS);

	ble_evt_send(&ble_evt);

	/* Event is processed, not called again */

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	ble_evt_send(&ble_evt);
}

void test_ble_conn_params_att_mtu_get_error_null(void)
{
	uint32_t nrf_err;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_att_mtu_get(CONN_HANDLE, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_conn_params_att_mtu_get_error_invalid_param(void)
{
	uint32_t nrf_err;
	uint16_t att_mtu;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	nrf_err = ble_conn_params_att_mtu_get(CONN_HANDLE, &att_mtu);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_conn_params_att_mtu_get(void)
{
	uint32_t nrf_err;
	uint16_t att_mtu;

	test_ble_conn_params_att_mtu_set();

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_att_mtu_get(CONN_HANDLE, &att_mtu);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(ATT_MTU_VALID, att_mtu);
}

void test_ble_conn_params_data_length_set_invalid_param(void)
{
	uint32_t nrf_err;
	struct ble_conn_params_data_length dl = {
		.rx = BLE_GAP_DATA_LENGTH_DEFAULT - 1,
		.tx = BLE_GAP_DATA_LENGTH_DEFAULT,
	};

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	nrf_err = ble_conn_params_data_length_set(CONN_HANDLE, dl);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_data_length_set(CONN_HANDLE, dl);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	dl.rx = CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_TX + 1;
	dl.tx = CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_RX;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_data_length_set(CONN_HANDLE, dl);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);

	dl.rx = CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_TX;
	dl.tx = CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_RX + 1;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_data_length_set(CONN_HANDLE, dl);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_conn_params_data_length_set(void)
{
	uint32_t nrf_err;
	struct ble_conn_params_data_length dl = {
		.rx = BLE_GAP_DATA_LENGTH_DEFAULT,
		.tx = BLE_GAP_DATA_LENGTH_DEFAULT,
	};

	ble_gap_data_length_params_t dlp_expected = {
		.max_tx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_rx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
		.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
	};
	ble_gap_data_length_limitation_t dll_expected = {0};

	__cmock_sd_ble_gap_data_length_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &dlp_expected, 1, &dll_expected, 1, NRF_SUCCESS);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_data_length_set(CONN_HANDLE, dl);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_conn_params_data_length_set_busy(void)
{
	uint32_t nrf_err;
	struct ble_conn_params_data_length dl = {
		.rx = BLE_GAP_DATA_LENGTH_DEFAULT,
		.tx = BLE_GAP_DATA_LENGTH_DEFAULT,
	};

	ble_gap_data_length_params_t dlp_expected = {
		.max_tx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_rx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
		.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
	};
	ble_gap_data_length_limitation_t dll_expected = {0};

	ble_evt_t ble_evt = {
		.evt.common_evt.conn_handle = CONN_HANDLE,
	};

	__cmock_sd_ble_gap_data_length_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &dlp_expected, 1, &dll_expected, 1, NRF_ERROR_BUSY);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_data_length_set(CONN_HANDLE, dl);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Inject SoftDevice event to retrigger the request */

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	__cmock_sd_ble_gap_data_length_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &dlp_expected, 1, &dll_expected, 1, NRF_SUCCESS);

	ble_evt_send(&ble_evt);

	/* Event is processed, not called again */

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	ble_evt_send(&ble_evt);
}

void test_ble_conn_params_data_length_set_resources(void)
{
	uint32_t nrf_err;
	struct ble_conn_params_data_length dl = {
		.rx = BLE_GAP_DATA_LENGTH_DEFAULT,
		.tx = BLE_GAP_DATA_LENGTH_DEFAULT,
	};
	ble_gap_data_length_limitation_t dll_expected = {0};
	ble_gap_data_length_limitation_t dll_return = {
		.rx_payload_limited_octets = 10,
		.tx_payload_limited_octets = 9,
		.tx_rx_time_limited_us = 100,
	};

	ble_gap_data_length_params_t dlp_expected1 = {
		.max_tx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_rx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
		.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
	};
	ble_gap_data_length_params_t dlp_expected2 = {
		.max_tx_octets = BLE_GAP_DATA_LENGTH_DEFAULT - dll_return.tx_payload_limited_octets,
		.max_rx_octets = BLE_GAP_DATA_LENGTH_DEFAULT - dll_return.rx_payload_limited_octets,
		.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
		.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
	};

	__cmock_sd_ble_gap_data_length_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &dlp_expected1, 1, &dll_expected, 1, NRF_ERROR_RESOURCES);
	__cmock_sd_ble_gap_data_length_update_ReturnThruPtr_p_dl_limitation(&dll_return);

	__cmock_sd_ble_gap_data_length_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &dlp_expected2, 1, &dll_return, 1, NRF_SUCCESS);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_data_length_set(CONN_HANDLE, dl);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_conn_params_data_length_set_resources_rx(void)
{
	uint32_t nrf_err;
	struct ble_conn_params_data_length dl = {
		.rx = BLE_GAP_DATA_LENGTH_DEFAULT,
		.tx = BLE_GAP_DATA_LENGTH_DEFAULT,
	};
	ble_gap_data_length_limitation_t dll_expected = {0};
	ble_gap_data_length_limitation_t dll_return = {
		.rx_payload_limited_octets = 10,
		.tx_payload_limited_octets = 0,
		.tx_rx_time_limited_us = 0,
	};

	ble_gap_data_length_params_t dlp_expected1 = {
		.max_tx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_rx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
		.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
	};
	ble_gap_data_length_params_t dlp_expected2 = {
		.max_tx_octets = BLE_GAP_DATA_LENGTH_DEFAULT - dll_return.tx_payload_limited_octets,
		.max_rx_octets = BLE_GAP_DATA_LENGTH_DEFAULT - dll_return.rx_payload_limited_octets,
		.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
		.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
	};

	__cmock_sd_ble_gap_data_length_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &dlp_expected1, 1, &dll_expected, 1, NRF_ERROR_RESOURCES);
	__cmock_sd_ble_gap_data_length_update_ReturnThruPtr_p_dl_limitation(&dll_return);

	__cmock_sd_ble_gap_data_length_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &dlp_expected2, 1, &dll_return, 1, NRF_SUCCESS);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_data_length_set(CONN_HANDLE, dl);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_conn_params_data_length_set_resources_tx(void)
{
	uint32_t nrf_err;
	struct ble_conn_params_data_length dl = {
		.rx = BLE_GAP_DATA_LENGTH_DEFAULT,
		.tx = BLE_GAP_DATA_LENGTH_DEFAULT,
	};
	ble_gap_data_length_limitation_t dll_expected = {0};
	ble_gap_data_length_limitation_t dll_return = {
		.rx_payload_limited_octets = 0,
		.tx_payload_limited_octets = 10,
		.tx_rx_time_limited_us = 0,
	};

	ble_gap_data_length_params_t dlp_expected1 = {
		.max_tx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_rx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
		.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
	};
	ble_gap_data_length_params_t dlp_expected2 = {
		.max_tx_octets = BLE_GAP_DATA_LENGTH_DEFAULT - dll_return.tx_payload_limited_octets,
		.max_rx_octets = BLE_GAP_DATA_LENGTH_DEFAULT - dll_return.rx_payload_limited_octets,
		.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
		.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
	};

	__cmock_sd_ble_gap_data_length_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &dlp_expected1, 1, &dll_expected, 1, NRF_ERROR_RESOURCES);
	__cmock_sd_ble_gap_data_length_update_ReturnThruPtr_p_dl_limitation(&dll_return);

	__cmock_sd_ble_gap_data_length_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &dlp_expected2, 1, &dll_return, 1, NRF_SUCCESS);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_data_length_set(CONN_HANDLE, dl);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_conn_params_data_length_set_other(void)
{
	uint32_t nrf_err;
	struct ble_conn_params_data_length dl = {
		.rx = BLE_GAP_DATA_LENGTH_DEFAULT,
		.tx = BLE_GAP_DATA_LENGTH_DEFAULT,
	};
	ble_gap_data_length_limitation_t dll_expected = {0};

	ble_gap_data_length_params_t dlp_expected = {
		.max_tx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_rx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
		.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
	};

	__cmock_sd_ble_gap_data_length_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &dlp_expected, 1, &dll_expected, 1, NRF_ERROR_INVALID_STATE);

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_data_length_set(CONN_HANDLE, dl);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_conn_params_data_length_get_error_null(void)
{
	uint32_t nrf_err;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_data_length_get(CONN_HANDLE, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_conn_params_data_length_get_error_invalid_param(void)
{
	uint32_t nrf_err;
	struct ble_conn_params_data_length dl;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	nrf_err = ble_conn_params_data_length_get(CONN_HANDLE, &dl);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_conn_params_data_length_get(void)
{
	uint32_t nrf_err;
	struct ble_conn_params_data_length dl;

	test_ble_conn_params_data_length_set();

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_data_length_get(CONN_HANDLE, &dl);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(BLE_GAP_DATA_LENGTH_DEFAULT, dl.rx);
	TEST_ASSERT_EQUAL(BLE_GAP_DATA_LENGTH_DEFAULT, dl.tx);
}

void test_ble_conn_params_phy_radio_mode_set_error_invalid_param(void)
{
	uint32_t nrf_err;
	ble_gap_phys_t phy_pref;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	nrf_err = ble_conn_params_phy_radio_mode_set(CONN_HANDLE, phy_pref);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_conn_params_phy_radio_mode_set(void)
{
	uint32_t nrf_err;
	ble_gap_phys_t phy_1mbps = {
		.rx_phys = BLE_GAP_PHY_1MBPS,
		.tx_phys = BLE_GAP_PHY_1MBPS,
	};
	ble_gap_phys_t phy_all = {
		.rx_phys = BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_CODED,
		.tx_phys = BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_CODED,
	};

	ble_gap_phys_t phy_supported = {
		.rx_phys = BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_2MBPS,
		.tx_phys = BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_2MBPS,
	};

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_sd_ble_gap_phy_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &phy_1mbps, 1, NRF_SUCCESS);

	nrf_err = ble_conn_params_phy_radio_mode_set(CONN_HANDLE, phy_1mbps);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Check that we filter out PHYs we do not support. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_sd_ble_gap_phy_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &phy_supported, 1, NRF_SUCCESS);

	nrf_err = ble_conn_params_phy_radio_mode_set(CONN_HANDLE, phy_all);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_conn_params_phy_radio_mode_set_error_resources(void)
{
	uint32_t nrf_err;
	ble_gap_phys_t phy_config = {
		.rx_phys = CONFIG_BLE_CONN_PARAMS_PHY,
		.tx_phys = CONFIG_BLE_CONN_PARAMS_PHY,
	};
	ble_gap_phys_t phy_all = {
		.rx_phys = BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_CODED,
		.tx_phys = BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_CODED,
	};
	ble_gap_phys_t phy_supported = {
		.rx_phys = BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_2MBPS,
		.tx_phys = BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_2MBPS,
	};

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_sd_ble_gap_phy_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &phy_supported, 1, NRF_ERROR_RESOURCES);

	/* Operation is retried wirth default parameters. */
	__cmock_sd_ble_gap_phy_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &phy_config, 1, NRF_SUCCESS);

	nrf_err = ble_conn_params_phy_radio_mode_set(CONN_HANDLE, phy_all);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
}

void test_ble_conn_params_phy_radio_mode_set_busy(void)
{
	uint32_t nrf_err;
	ble_gap_phys_t phy_1mbps = {
		.rx_phys = BLE_GAP_PHY_1MBPS,
		.tx_phys = BLE_GAP_PHY_1MBPS,
	};
	ble_evt_t ble_evt = {
		.evt.common_evt.conn_handle = CONN_HANDLE,
	};

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_sd_ble_gap_phy_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &phy_1mbps, 1, NRF_ERROR_BUSY);

	nrf_err = ble_conn_params_phy_radio_mode_set(CONN_HANDLE, phy_1mbps);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Inject SoftDevice event to retrigger the request */

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	__cmock_sd_ble_gap_phy_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &phy_1mbps, 1, NRF_SUCCESS);

	ble_evt_send(&ble_evt);

	/* Event is processed, not called again */

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	ble_evt_send(&ble_evt);
}

void test_ble_conn_params_phy_radio_mode_get_error_null(void)
{
	uint32_t nrf_err;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_phy_radio_mode_get(CONN_HANDLE, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_conn_params_phy_radio_mode_get_error_invalid_param(void)
{
	uint32_t nrf_err;
	ble_gap_phys_t phy_pref;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, -1);

	nrf_err = ble_conn_params_phy_radio_mode_get(CONN_HANDLE, &phy_pref);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_conn_params_phy_radio_mode_get(void)
{
	uint32_t nrf_err;
	ble_gap_phys_t phy_pref;

	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	nrf_err = ble_conn_params_phy_radio_mode_get(CONN_HANDLE, &phy_pref);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(BLE_GAP_PHY_1MBPS, phy_pref.rx_phys);
	TEST_ASSERT_EQUAL(BLE_GAP_PHY_1MBPS, phy_pref.tx_phys);
}

void test_ble_evt_connected(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_CONNECTED,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.connected = {
				.role = BLE_GAP_ROLE_PERIPH,
			},
		}
	};

	ble_gap_data_length_params_t dlp_expected = {
		.max_tx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_rx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
		.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
	};
	ble_gap_data_length_limitation_t dll_expected = {0};
	ble_gap_phys_t phy_supported = {
		.rx_phys = BLE_GAP_PHY_1MBPS,
		.tx_phys = BLE_GAP_PHY_1MBPS,
	};
	const ble_gap_conn_params_t conn_params_expected = {
		.conn_sup_timeout = CONFIG_BLE_CONN_PARAMS_SUP_TIMEOUT,
		.min_conn_interval = CONFIG_BLE_CONN_PARAMS_MIN_CONN_INTERVAL,
		.max_conn_interval = CONFIG_BLE_CONN_PARAMS_MAX_CONN_INTERVAL,
		.slave_latency = CONFIG_BLE_CONN_PARAMS_PERIPHERAL_LATENCY,
	};

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	__cmock_sd_ble_gattc_exchange_mtu_request_ExpectAndReturn(CONN_HANDLE, ATT_MTU_VALID,
								  NRF_SUCCESS);
	__cmock_sd_ble_gap_data_length_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &dlp_expected, 1, &dll_expected, 1, NRF_SUCCESS);
	__cmock_sd_ble_gap_phy_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &phy_supported, 1, NRF_SUCCESS);

	__cmock_sd_ble_gap_conn_param_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &conn_params_expected, 1, NRF_ERROR_BUSY);

	ble_evt_send(&ble_evt);
}

void test_ble_evt_connected_conn_params(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_CONNECTED,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.connected = {
				.role = BLE_GAP_ROLE_PERIPH,
				.conn_params = {
					.conn_sup_timeout = CONFIG_BLE_CONN_PARAMS_SUP_TIMEOUT,
					.min_conn_interval =
						CONFIG_BLE_CONN_PARAMS_MIN_CONN_INTERVAL + 2,
					.max_conn_interval = 20,
					.slave_latency = CONFIG_BLE_CONN_PARAMS_PERIPHERAL_LATENCY,
				},
			},
		}
	};

	ble_gap_data_length_params_t dlp_expected = {
		.max_tx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_rx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
		.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
	};
	ble_gap_data_length_limitation_t dll_expected = {0};
	ble_gap_phys_t phy_supported = {
		.rx_phys = BLE_GAP_PHY_1MBPS,
		.tx_phys = BLE_GAP_PHY_1MBPS,
	};

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	__cmock_sd_ble_gattc_exchange_mtu_request_ExpectAndReturn(CONN_HANDLE, ATT_MTU_VALID,
								  NRF_SUCCESS);
	__cmock_sd_ble_gap_data_length_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &dlp_expected, 1, &dll_expected, 1, NRF_SUCCESS);
	__cmock_sd_ble_gap_phy_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &phy_supported, 1, NRF_SUCCESS);

	ble_evt_send(&ble_evt);
}

void test_ble_evt_disconnected(void)
{
	ble_evt_t ble_evt = {
		.evt.common_evt.conn_handle = CONN_HANDLE,
		.header.evt_id = BLE_GAP_EVT_DISCONNECTED,
	};

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	ble_evt_send(&ble_evt);
}

void test_ble_evt_exchange_mtu_rsp(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTC_EVT_EXCHANGE_MTU_RSP,
		.evt.gattc_evt = {
			.conn_handle = CONN_HANDLE,
			.params.exchange_mtu_rsp.server_rx_mtu = ATT_MTU_VALID,
		},

	};

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	ble_evt_send(&ble_evt);

	TEST_ASSERT_EQUAL(BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED, app_evt.evt_type);
	TEST_ASSERT_EQUAL(CONN_HANDLE, app_evt.conn_handle);
	TEST_ASSERT_EQUAL(ATT_MTU_VALID, app_evt.att_mtu);
}

void test_ble_evt_exchange_mtu_request(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST,
		.evt.gatts_evt = {
			.conn_handle = CONN_HANDLE,
			.params.exchange_mtu_request = {
				.client_rx_mtu = ATT_MTU_VALID,
			},
		},
	};

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	__cmock_sd_ble_gatts_exchange_mtu_reply_ExpectAndReturn(
		CONN_HANDLE, ATT_MTU_VALID, NRF_SUCCESS);

	ble_evt_send(&ble_evt);

	TEST_ASSERT_EQUAL(BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED, app_evt.evt_type);
	TEST_ASSERT_EQUAL(CONN_HANDLE, app_evt.conn_handle);
	TEST_ASSERT_EQUAL(ATT_MTU_VALID, app_evt.att_mtu);
}

void test_ble_evt_exchange_mtu_request_error(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST,
		.evt.gatts_evt = {
			.conn_handle = CONN_HANDLE,
			.params.exchange_mtu_request = {
				.client_rx_mtu = ATT_MTU_VALID,
			},
		},
	};

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	__cmock_sd_ble_gatts_exchange_mtu_reply_ExpectAndReturn(
		CONN_HANDLE, ATT_MTU_VALID, NRF_ERROR_BUSY);

	ble_evt_send(&ble_evt);
}

void test_ble_evt_conn_param_update_accepted(void)
{
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_CONN_PARAM_UPDATE,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.conn_param_update.conn_params = {
				.conn_sup_timeout = CONFIG_BLE_CONN_PARAMS_SUP_TIMEOUT,
				.min_conn_interval = CONFIG_BLE_CONN_PARAMS_MIN_CONN_INTERVAL,
				.max_conn_interval = CONFIG_BLE_CONN_PARAMS_MAX_CONN_INTERVAL,
				.slave_latency = CONFIG_BLE_CONN_PARAMS_PERIPHERAL_LATENCY,
			},
		},
	};

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	ble_evt_send(&ble_evt);

	TEST_ASSERT_EQUAL(BLE_CONN_PARAMS_EVT_UPDATED, app_evt.evt_type);
	TEST_ASSERT_EQUAL(CONN_HANDLE, app_evt.conn_handle);
	TEST_ASSERT_EQUAL(CONFIG_BLE_CONN_PARAMS_SUP_TIMEOUT,
			  app_evt.conn_params.conn_sup_timeout);
	TEST_ASSERT_EQUAL(CONFIG_BLE_CONN_PARAMS_MIN_CONN_INTERVAL,
			  app_evt.conn_params.min_conn_interval);
	TEST_ASSERT_EQUAL(CONFIG_BLE_CONN_PARAMS_MAX_CONN_INTERVAL,
			  app_evt.conn_params.max_conn_interval);
	TEST_ASSERT_EQUAL(CONFIG_BLE_CONN_PARAMS_PERIPHERAL_LATENCY,
			  app_evt.conn_params.slave_latency);
}

void test_ble_evt_conn_param_update_negotiate(void)
{
	const ble_gap_conn_params_t conn_params_expected = {
		.conn_sup_timeout = CONFIG_BLE_CONN_PARAMS_SUP_TIMEOUT,
		.min_conn_interval = CONFIG_BLE_CONN_PARAMS_MIN_CONN_INTERVAL,
		.max_conn_interval = CONFIG_BLE_CONN_PARAMS_MAX_CONN_INTERVAL,
		.slave_latency = CONFIG_BLE_CONN_PARAMS_PERIPHERAL_LATENCY,
	};
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_CONN_PARAM_UPDATE,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.conn_param_update.conn_params = {
				.conn_sup_timeout = 110,
				.min_conn_interval = 21,
				.max_conn_interval = 31,
				.slave_latency = 41,
			},
		},
	};

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	__cmock_sd_ble_gap_conn_param_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &conn_params_expected, 1, NRF_SUCCESS);

	ble_evt_send(&ble_evt);
}

void test_ble_evt_conn_param_update_busy(void)
{
	const ble_gap_conn_params_t conn_params_expected = {
		.conn_sup_timeout = CONFIG_BLE_CONN_PARAMS_SUP_TIMEOUT,
		.min_conn_interval = CONFIG_BLE_CONN_PARAMS_MIN_CONN_INTERVAL,
		.max_conn_interval = CONFIG_BLE_CONN_PARAMS_MAX_CONN_INTERVAL,
		.slave_latency = CONFIG_BLE_CONN_PARAMS_PERIPHERAL_LATENCY,
	};
	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_CONN_PARAM_UPDATE,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.conn_param_update.conn_params = {
				.conn_sup_timeout = 110,
				.min_conn_interval = 21,
				.max_conn_interval = 0,
				.slave_latency = 41,
			},
		},
	};

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	__cmock_sd_ble_gap_conn_param_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &conn_params_expected, 1, NRF_ERROR_BUSY);

	ble_evt_send(&ble_evt);

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	__cmock_sd_ble_gap_disconnect_ExpectAndReturn(
		CONN_HANDLE, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE, NRF_SUCCESS);

	ble_evt_send(&ble_evt);

	TEST_ASSERT_EQUAL(BLE_CONN_PARAMS_EVT_REJECTED, app_evt.evt_type);
	TEST_ASSERT_EQUAL(CONN_HANDLE, app_evt.conn_handle);
}

void test_ble_evt_data_length_update(void)
{

	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_DATA_LENGTH_UPDATE,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.data_length_update.effective_params = {
				.max_rx_octets = 1,
				.max_rx_time_us = 2,
				.max_tx_octets = 3,
				.max_tx_time_us = 4,
			},
		},
	};

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	ble_evt_send(&ble_evt);

	TEST_ASSERT_EQUAL(BLE_CONN_PARAMS_EVT_DATA_LENGTH_UPDATED, app_evt.evt_type);
	TEST_ASSERT_EQUAL(CONN_HANDLE, app_evt.conn_handle);
	TEST_ASSERT_EQUAL(1, app_evt.data_length.rx);
	TEST_ASSERT_EQUAL(3, app_evt.data_length.tx);
}

void test_ble_evt_data_length_update_request(void)
{

	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.data_length_update_request.peer_params = {
				.max_tx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
				.max_rx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
				.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
				.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
			},
		},
	};

	ble_gap_data_length_params_t dlp_expected = {
		.max_tx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_rx_octets = BLE_GAP_DATA_LENGTH_DEFAULT,
		.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
		.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO,
	};
	ble_gap_data_length_limitation_t dll_expected = {0};

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	__cmock_sd_ble_gap_data_length_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &dlp_expected, 1, &dll_expected, 1, NRF_SUCCESS);

	ble_evt_send(&ble_evt);
}

void test_ble_evt_phy_update(void)
{

	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_PHY_UPDATE,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.phy_update = {
				.rx_phy = BLE_GAP_PHY_2MBPS,
				.tx_phy = BLE_GAP_PHY_1MBPS,
				.status = BLE_HCI_STATUS_CODE_SUCCESS,
			},
		},
	};

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	ble_evt_send(&ble_evt);

	TEST_ASSERT_EQUAL(BLE_CONN_PARAMS_EVT_RADIO_PHY_MODE_UPDATED, app_evt.evt_type);
	TEST_ASSERT_EQUAL(CONN_HANDLE, app_evt.conn_handle);
	TEST_ASSERT_EQUAL(BLE_GAP_PHY_2MBPS, app_evt.phy_update_evt.rx_phy);
	TEST_ASSERT_EQUAL(BLE_GAP_PHY_1MBPS, app_evt.phy_update_evt.tx_phy);
	TEST_ASSERT_EQUAL(BLE_HCI_STATUS_CODE_SUCCESS, app_evt.phy_update_evt.status);
}

void test_ble_evt_phy_update_request(void)
{

	ble_evt_t ble_evt = {
		.header.evt_id = BLE_GAP_EVT_PHY_UPDATE_REQUEST,
		.evt.gap_evt = {
			.conn_handle = CONN_HANDLE,
			.params.phy_update_request.peer_preferred_phys = {
				.rx_phys = BLE_GAP_PHY_2MBPS,
				.tx_phys = BLE_GAP_PHY_2MBPS,
			},
		},
	};
	ble_gap_phys_t phy_2mbps = {
		.rx_phys = BLE_GAP_PHY_2MBPS,
		.tx_phys = BLE_GAP_PHY_2MBPS,
	};

	/* Calls from BLE observers. */
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);
	__cmock_nrf_sdh_ble_idx_get_ExpectAndReturn(CONN_HANDLE, 0);

	__cmock_sd_ble_gap_phy_update_ExpectWithArrayAndReturn(
		CONN_HANDLE, &phy_2mbps, 1, NRF_SUCCESS);

	ble_evt_send(&ble_evt);
}

void test_sdh_state_evt(void)
{
	static const ble_gap_conn_params_t ppcp = {
		.min_conn_interval = CONFIG_BLE_CONN_PARAMS_MIN_CONN_INTERVAL,
		.max_conn_interval = CONFIG_BLE_CONN_PARAMS_MAX_CONN_INTERVAL,
		.slave_latency = CONFIG_BLE_CONN_PARAMS_PERIPHERAL_LATENCY,
		.conn_sup_timeout = CONFIG_BLE_CONN_PARAMS_SUP_TIMEOUT,
	};

	state_evt_send(NRF_SDH_STATE_EVT_DISABLED);

	__cmock_sd_ble_gap_ppcp_set_ExpectWithArrayAndReturn(&ppcp, 1, NRF_ERROR_BUSY);

	state_evt_send(NRF_SDH_STATE_EVT_BLE_ENABLED);

	__cmock_sd_ble_gap_ppcp_set_ExpectWithArrayAndReturn(&ppcp, 1, NRF_SUCCESS);

	state_evt_send(NRF_SDH_STATE_EVT_BLE_ENABLED);
}

void setUp(void)
{
	memset(&app_evt, 0, sizeof(app_evt));
}
void tearDown(void)
{

}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
