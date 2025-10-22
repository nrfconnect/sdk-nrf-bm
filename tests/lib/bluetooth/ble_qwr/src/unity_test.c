/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <unity.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <bm/bluetooth/ble_qwr.h>

#include "cmock_ble.h"
#include "cmock_ble_gatts.h"

static uint16_t ble_qwr_evt_handler(struct ble_qwr *qwr, const struct ble_qwr_evt *evt)
{
	return 0;
}

void test_ble_qwr_init_error_null(void)
{
	uint32_t nrf_err;
	struct ble_qwr qwr;
	struct ble_qwr_config qwr_config = {0};

	nrf_err = ble_qwr_init(&qwr, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_qwr_init(NULL, &qwr_config);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_qwr_init_error_invalid_state(void)
{
	uint32_t nrf_err;
	struct ble_qwr qwr;
	struct ble_qwr_config qwr_config = {0};

	nrf_err = ble_qwr_init(&qwr, &qwr_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	/* Second attempt should fail */
	nrf_err = ble_qwr_init(&qwr, &qwr_config);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_qwr_init(void)
{
	uint32_t nrf_err;
	uint8_t mem[10];
	struct ble_qwr qwr;
	struct ble_qwr_config qwr_config = {
		.mem_buffer = {
			.p_mem = mem,
			.len = sizeof(mem),
		},
		.evt_handler = ble_qwr_evt_handler,
	};

	nrf_err = ble_qwr_init(&qwr, &qwr_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	TEST_ASSERT_EQUAL(BLE_CONN_HANDLE_INVALID, qwr.conn_handle);
	TEST_ASSERT_EQUAL(0, qwr.nb_registered_attr);
	TEST_ASSERT_EQUAL(0, qwr.nb_written_handles);
	TEST_ASSERT_FALSE(qwr.is_user_mem_reply_pending);

	TEST_ASSERT_EQUAL_PTR(qwr_config.mem_buffer.p_mem, qwr.mem_buffer.p_mem);
	TEST_ASSERT_EQUAL(qwr_config.mem_buffer.len, qwr.mem_buffer.len);

	TEST_ASSERT_EQUAL_PTR(ble_qwr_evt_handler, qwr.evt_handler);
}

void test_ble_qwr_attr_register_error_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_qwr_attr_register(NULL, 1);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_qwr_attr_register_error_invalid_state(void)
{
	uint32_t nrf_err;
	struct ble_qwr qwr = {0};

	nrf_err = ble_qwr_attr_register(&qwr, 1);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_qwr_attr_register_error_invalid_param(void)
{
	uint32_t nrf_err;
	uint8_t mem[10];
	struct ble_qwr qwr;
	struct ble_qwr_config qwr_config = {
		.mem_buffer = {
			.p_mem = mem,
			.len = sizeof(mem),
		},
		.evt_handler = ble_qwr_evt_handler,
	};

	nrf_err = ble_qwr_init(&qwr, &qwr_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_qwr_attr_register(&qwr, BLE_GATT_HANDLE_INVALID);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, nrf_err);
}

void test_ble_qwr_attr_register_error_no_mem(void)
{
	uint32_t nrf_err;
	uint8_t mem[10];
	struct ble_qwr qwr;
	struct ble_qwr_config qwr_config = {
		.mem_buffer = {
			.p_mem = mem,
			.len = sizeof(mem),
		},
		.evt_handler = ble_qwr_evt_handler,
	};

	qwr_config.mem_buffer.p_mem = NULL;
	qwr_config.mem_buffer.len = sizeof(mem);

	nrf_err = ble_qwr_init(&qwr, &qwr_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_qwr_attr_register(&qwr, 1);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);

	/* Reset qwr so it can be initialized again */
	qwr.initialized = 0;

	qwr_config.mem_buffer.p_mem = mem;
	qwr_config.mem_buffer.len = 0;

	nrf_err = ble_qwr_init(&qwr, &qwr_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_qwr_attr_register(&qwr, 1);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);

	/* Reset qwr so it can be initialized again */
	qwr.initialized = 0;

	qwr_config.mem_buffer.p_mem = mem;
	qwr_config.mem_buffer.len = sizeof(mem);

	nrf_err = ble_qwr_init(&qwr, &qwr_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_qwr_attr_register(&qwr, 1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_qwr_attr_register(&qwr, 2);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_qwr_attr_register(&qwr, 3);
	TEST_ASSERT_EQUAL(NRF_ERROR_NO_MEM, nrf_err);
}

void test_ble_qwr_attr_register(void)
{
	uint32_t nrf_err;
	uint8_t mem[10];
	struct ble_qwr qwr;
	struct ble_qwr_config qwr_config = {
		.mem_buffer = {
			.p_mem = mem,
			.len = sizeof(mem),
		},
		.evt_handler = ble_qwr_evt_handler,
	};

	nrf_err = ble_qwr_init(&qwr, &qwr_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_qwr_attr_register(&qwr, 0xa1);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(1, qwr.nb_registered_attr);
	TEST_ASSERT_EQUAL(0xa1, qwr.attr_handles[0]);

	nrf_err = ble_qwr_attr_register(&qwr, 0xa2);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(2, qwr.nb_registered_attr);
	TEST_ASSERT_EQUAL(0xa2, qwr.attr_handles[1]);
}

void test_ble_qwr_value_get_error_null(void)
{
	uint32_t nrf_err;
	struct ble_qwr qwr;
	uint8_t mem[1];
	uint16_t len = sizeof(mem);

	nrf_err = ble_qwr_value_get(NULL, 1, mem, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_qwr_value_get(&qwr, 1, NULL, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);

	nrf_err = ble_qwr_value_get(&qwr, 1, mem, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_qwr_value_get_error_invalid_state(void)
{
	uint32_t nrf_err;
	struct ble_qwr qwr = {0};
	uint8_t mem[1];
	uint16_t len = sizeof(mem);

	nrf_err = ble_qwr_value_get(&qwr, 1, mem, &len);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_qwr_value_get(void)
{
	uint32_t nrf_err;
	struct ble_qwr qwr = {0};
	/* mem is filled by softdevice, we do it here */
	uint8_t mem[] = {
		0xa1, 0x00, 0x00, 0x00, /* attr_handle (little endian), val_offset */
		0x06, 0x00, 0x01, 0x02, /* val_len, val */
		0x03, 0x04, 0x05, 0x06, /* val */
		0xa2, 0x00, 0x00, 0x00, /* attr_handle, val_offset */
		0x06, 0x00, 0x11, 0x12, /* val_len, val */
		0x13, 0x14, 0x15, 0x16, /* val */
		0xa1, 0x00, 0x06, 0x00, /* attr_handle, val_offset */
		0x06, 0x00, 0x07, 0x08, /* val_len, val */
		0x09, 0x0A, 0x0B, 0x0C, /* val */
	};
	struct ble_qwr_config qwr_config = {
		.mem_buffer = {
			.p_mem = mem,
			.len = sizeof(mem),
		},
		.evt_handler = ble_qwr_evt_handler,
	};

	uint8_t buf[16];
	uint16_t buf_len = sizeof(buf);

	uint8_t attr1_expected_val[] = {
		0x01, 0x02, 0x03, 0x04,
		0x05, 0x06, 0x07, 0x08,
		0x09, 0x0A, 0x0B, 0x0C,
	};
	uint8_t attr2_expected_val[] = {
		0x11, 0x12, 0x13, 0x14,
		0x15, 0x16,
	};

	nrf_err = ble_qwr_init(&qwr, &qwr_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_qwr_value_get(&qwr, 0xa1, buf, &buf_len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(12, buf_len);
	TEST_ASSERT_EQUAL_MEMORY(attr1_expected_val, buf, sizeof(attr1_expected_val));

	nrf_err = ble_qwr_value_get(&qwr, 0xa2, buf, &buf_len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(6, buf_len);
	TEST_ASSERT_EQUAL_MEMORY(attr2_expected_val, buf, sizeof(attr2_expected_val));

	nrf_err = ble_qwr_value_get(&qwr, 0xa3, buf, &buf_len);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);
	TEST_ASSERT_EQUAL(0, buf_len);
}

void test_ble_qwr_conn_handle_assign_error_null(void)
{
	uint32_t nrf_err;

	nrf_err = ble_qwr_conn_handle_assign(NULL, 1);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, nrf_err);
}

void test_ble_qwr_conn_handle_assign_error_invalid_state(void)
{
	uint32_t nrf_err;
	struct ble_qwr qwr = {0};

	nrf_err = ble_qwr_conn_handle_assign(&qwr, 1);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE, nrf_err);
}

void test_ble_qwr_conn_handle_assign(void)
{
	uint32_t nrf_err;
	struct ble_qwr qwr = {0};
	uint8_t mem[1];
	struct ble_qwr_config qwr_config = {
		.mem_buffer = {
			.p_mem = mem,
			.len = sizeof(mem),
		},
		.evt_handler = ble_qwr_evt_handler,
	};

	nrf_err = ble_qwr_init(&qwr, &qwr_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_qwr_conn_handle_assign(&qwr, 0xC044);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	TEST_ASSERT_EQUAL(0xC044, qwr.conn_handle);
}

void test_ble_qwr_on_ble_evt_do_nothing(void)
{
	ble_evt_t const ble_evt = {0};
	struct ble_qwr qwr = {0};

	/* We expect these to return immediately */
	ble_qwr_on_ble_evt(&ble_evt, NULL);
	ble_qwr_on_ble_evt(NULL, &qwr);
	ble_qwr_on_ble_evt(&ble_evt, &qwr);
}

void test_ble_qwr_on_ble_evt_mem_req_sd_busy(void)
{
	uint32_t nrf_err;
	struct ble_qwr qwr = {0};
	uint8_t mem[16];
	struct ble_qwr_config qwr_config = {
		.mem_buffer = {
			.p_mem = mem,
			.len = sizeof(mem),
		},
		.evt_handler = ble_qwr_evt_handler,
	};

	ble_evt_t const ble_evt_mem_req = {
		.header = {
			.evt_id = BLE_EVT_USER_MEM_REQUEST,
			.evt_len = 7,
		},
		.evt = {
			.common_evt = {
				.conn_handle = 0xC044,
				.params = {
					.user_mem_request.type =
						BLE_USER_MEM_TYPE_GATTS_QUEUED_WRITES,
				}
			},
		},
	};

	ble_evt_t const ble_evt_common_evt = {
		.evt = {
			.common_evt = {
				.conn_handle = 0xC044,
			},
		},
	};

	/* Initialize qwr */
	nrf_err = ble_qwr_init(&qwr, &qwr_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_qwr_conn_handle_assign(&qwr, 0xC044);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_user_mem_reply_ExpectAndReturn(0xC044, &qwr.mem_buffer, NRF_ERROR_BUSY);
	ble_qwr_on_ble_evt(&ble_evt_mem_req, &qwr);

	/* Last call was busy, expect sd to be called again on common event */
	__cmock_sd_ble_user_mem_reply_ExpectAndReturn(0xC044, &qwr.mem_buffer, NRF_SUCCESS);
	ble_qwr_on_ble_evt(&ble_evt_common_evt, &qwr);
}

void test_ble_qwr_on_ble_evt_mem_req(void)
{
	uint32_t nrf_err;
	struct ble_qwr qwr = {0};
	uint8_t mem[16];
	struct ble_qwr_config qwr_config = {
		.mem_buffer = {
			.p_mem = mem,
			.len = sizeof(mem),
		},
		.evt_handler = ble_qwr_evt_handler,
	};

	ble_evt_t const ble_evt_mem_req = {
		.header = {
			.evt_id = BLE_EVT_USER_MEM_REQUEST,
			.evt_len = 7,
		},
		.evt = {
			.common_evt = {
				.conn_handle = 0xC044,
				.params = {
					.user_mem_request.type =
						BLE_USER_MEM_TYPE_GATTS_QUEUED_WRITES,
				},
			},
		},
	};

	ble_evt_t const ble_evt_common_evt = {
		.evt = {
			.common_evt = {
				.conn_handle = 0xC044,
			},
		},
	};

	/* Initialize qwr */
	nrf_err = ble_qwr_init(&qwr, &qwr_config);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	nrf_err = ble_qwr_conn_handle_assign(&qwr, 0xC044);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, nrf_err);

	__cmock_sd_ble_user_mem_reply_ExpectAndReturn(0xC044, &qwr.mem_buffer, NRF_SUCCESS);
	ble_qwr_on_ble_evt(&ble_evt_mem_req, &qwr);

	/* Last call succeeded, do not expect sd to be called again on common event */
	ble_qwr_on_ble_evt(&ble_evt_common_evt, &qwr);

	/* New mem request, new call to sd. */
	__cmock_sd_ble_user_mem_reply_ExpectAndReturn(0xC044, &qwr.mem_buffer, NRF_SUCCESS);
	ble_qwr_on_ble_evt(&ble_evt_mem_req, &qwr);
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
