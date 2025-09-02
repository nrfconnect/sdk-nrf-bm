/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>

#include <bm/bluetooth/ble_radio_notification.h>

#include "cmock_nrf_soc.h"
#include "cmock_cmsis.h"

extern void radio_notification_isr(const void *arg);
bool radio_active;

static void ble_radio_notification_evt_handler(bool evt_radio_active)
{
	radio_active = evt_radio_active;
}

void test_ble_radio_notification_init_invalid_param(void)
{
	int ret;

	__cmock_NVIC_ClearPendingIRQ_Expect(RADIO_NOTIFICATION_IRQn);
	__cmock_NVIC_EnableIRQ_Expect(RADIO_NOTIFICATION_IRQn);
	__cmock_sd_radio_notification_cfg_set_ExpectAndReturn(
		NRF_RADIO_NOTIFICATION_TYPE_INT_ON_BOTH, 800, NRF_ERROR_INVALID_PARAM);
	ret = ble_radio_notification_init(800, ble_radio_notification_evt_handler);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_PARAM, ret);
}

void test_ble_radio_notification_init_null(void)
{
	int ret;

	ret = ble_radio_notification_init(800, NULL);
	TEST_ASSERT_EQUAL(NRF_ERROR_NULL, ret);
}

void test_ble_radio_notification_init(void)
{
	int ret;

	__cmock_NVIC_ClearPendingIRQ_Expect(RADIO_NOTIFICATION_IRQn);
	__cmock_NVIC_EnableIRQ_Expect(RADIO_NOTIFICATION_IRQn);
	__cmock_sd_radio_notification_cfg_set_ExpectAndReturn(
		NRF_RADIO_NOTIFICATION_TYPE_INT_ON_BOTH, 800, NRF_SUCCESS);
	ret = ble_radio_notification_init(800, ble_radio_notification_evt_handler);
	TEST_ASSERT_EQUAL(NRF_SUCCESS, ret);
}

void test_ble_radio_notification(void)
{
	TEST_ASSERT_FALSE(radio_active);
	radio_notification_isr(NULL);
	TEST_ASSERT_TRUE(radio_active);
	radio_notification_isr(NULL);
	TEST_ASSERT_FALSE(radio_active);
}

extern int unity_main(void);

int main(void)
{
	return unity_main();
}
