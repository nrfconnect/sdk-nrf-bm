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
#include <bm/bluetooth/ble_conn_state.h>
#include <zephyr/sys/util.h>

static uint16_t conn_handle1;
static uint16_t conn_handle2 = 1;
static uint16_t conn_handle3 = 2;
static uint16_t conn_handle4 = 3;
static uint16_t conn_handle5 = 4;
static uint16_t conn_handle6 = 5;
static uint16_t conn_handle7 = 10;
static uint16_t conn_handle8 = 19;

static uint16_t conn_handles_registered[CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT] = {
	[0 ... CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT - 1] = BLE_CONN_HANDLE_INVALID,
};

void conn_handle_register(uint16_t conn_handle)
{
	for (int i = 0; i < ARRAY_SIZE(conn_handles_registered) ; i++) {
		if (conn_handles_registered[i] == BLE_CONN_HANDLE_INVALID) {
			conn_handles_registered[i] = conn_handle;
			return;
		}
	}
}

void conn_handle_deregister(uint16_t conn_handle)
{
	for (int i = 0; i < ARRAY_SIZE(conn_handles_registered) ; i++) {
		if (conn_handles_registered[i] == conn_handle) {
			conn_handles_registered[i] = BLE_CONN_HANDLE_INVALID;
			return;
		}
	}
}

int nrf_sdh_ble_idx_get(uint16_t conn_handle)
{
	for (int i = 0; i < ARRAY_SIZE(conn_handles_registered) ; i++) {
		if (conn_handles_registered[i] == conn_handle) {
			return i;
		}
	}

	return -1;
}

uint16_t nrf_sdh_ble_conn_handle_get(int idx)
{
	return conn_handles_registered[idx];
}

extern void ble_evt_handler(const ble_evt_t *ble_evt, void *ctx);

static uint32_t arbitrary_context;
/* Conn handle that will cause flag operations to overflow into next flag if not checked. */
static uint16_t conn_handle_overflow = 32;

static uint32_t calls;
static uint16_t conn_handles[10] = {BLE_CONN_HANDLE_INVALID};
static void *contexts[10] = {NULL};

void ble_evt_init(ble_evt_t *ble_evt, uint8_t evt_id, uint16_t conn_handle)
{
	memset(ble_evt, 0, sizeof(ble_evt_t));
	ble_evt->header.evt_id                     = evt_id;
	ble_evt->evt.gap_evt.conn_handle           = conn_handle;
	ble_evt->header.evt_len                    = sizeof(ble_gap_evt_t);
}

void connected_evt_constuct(ble_evt_t *ble_evt, uint16_t conn_handle, uint8_t role)
{
	ble_evt_init(ble_evt, BLE_GAP_EVT_CONNECTED, conn_handle);
	ble_evt->evt.gap_evt.params.connected.role = role;
}

void disconnected_evt_constuct(ble_evt_t *ble_evt, uint16_t conn_handle)
{
	ble_evt_init(ble_evt, BLE_GAP_EVT_DISCONNECTED, conn_handle);
}

void conn_sec_update_evt_constuct(ble_evt_t *ble_evt, uint16_t conn_handle, uint8_t level)
{
	ble_evt_init(ble_evt, BLE_GAP_EVT_CONN_SEC_UPDATE, conn_handle);
	ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.lv = level;
}

void auth_status_evt_constuct(ble_evt_t *ble_evt, uint16_t conn_handle, bool lesc,
			      uint8_t auth_status)
{
	ble_evt_init(ble_evt, BLE_GAP_EVT_AUTH_STATUS, conn_handle);
	ble_evt->evt.gap_evt.params.auth_status.auth_status = auth_status;
	ble_evt->evt.gap_evt.params.auth_status.lesc = lesc;
}

ble_evt_t *connected_evt(uint16_t conn_handle, uint8_t role)
{
	static ble_evt_t ble_evt;

	connected_evt_constuct(&ble_evt, conn_handle, role);
	return &ble_evt;
}

ble_evt_t *disconnected_evt(uint16_t conn_handle)
{
	static ble_evt_t ble_evt;

	disconnected_evt_constuct(&ble_evt, conn_handle);
	return &ble_evt;
}

ble_evt_t *conn_sec_update_evt(uint16_t conn_handle, uint8_t level)
{
	static ble_evt_t ble_evt;

	conn_sec_update_evt_constuct(&ble_evt, conn_handle, level);
	return &ble_evt;
}

ble_evt_t *auth_status_evt(uint16_t conn_handle, bool lesc, uint8_t auth_status)
{
	static ble_evt_t ble_evt;

	auth_status_evt_constuct(&ble_evt, conn_handle, lesc, auth_status);
	return &ble_evt;
}

void test_ble_conn_state_init(void)
{
	uint8_t  dummy_role   = BLE_GAP_ROLE_PERIPH;
	uint32_t valid_conn_handles;

	conn_handle1 = 0;
	conn_handle2 = 1;
	conn_handle3 = 2;
	conn_handle4 = 3;
	conn_handle5 = 4;
	conn_handle6 = 5;
	conn_handle7 = 10;
	conn_handle8 = 19;

	conn_handle_register(conn_handle1);
	conn_handle_register(conn_handle2);
	conn_handle_register(conn_handle3);
	conn_handle_register(conn_handle4);
	conn_handle_register(conn_handle5);
	conn_handle_register(conn_handle6);
	conn_handle_register(conn_handle7);
	conn_handle_register(conn_handle8);

	ble_evt_handler(connected_evt(conn_handle1, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle2, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle3, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle4, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle5, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle6, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle7, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle8, dummy_role), NULL);

	valid_conn_handles = ble_conn_state_conn_count();
	TEST_ASSERT(valid_conn_handles > 0);

	ble_conn_state_init();

	valid_conn_handles = ble_conn_state_conn_count();
	TEST_ASSERT_EQUAL(0, valid_conn_handles);
}

void test_ble_conn_state_valid(void)
{
	bool valid;

	conn_handle1 = 0;
	conn_handle2 = 1;
	conn_handle3 = 2;
	conn_handle4 = 3;
	conn_handle5 = 4;
	conn_handle6 = 5;
	conn_handle7 = 10;
	conn_handle8 = BLE_CONN_STATE_MAX_CONNECTIONS-1;
	uint8_t  dummy_role   = BLE_GAP_ROLE_PERIPH;
	char msg[] = "conn_handle = 65535";

	valid = ble_conn_state_valid(BLE_CONN_HANDLE_INVALID);
	TEST_ASSERT_FALSE(valid);

	/* Testing that all conn. handles are invalid at first. */
	for (uint16_t conn_handle = 0; conn_handle < (65535); conn_handle++) {
		valid = ble_conn_state_valid(conn_handle);
		TEST_ASSERT_FALSE(valid);
	}

	conn_handle_register(conn_handle1);
	conn_handle_register(conn_handle2);
	conn_handle_register(conn_handle3);
	conn_handle_register(conn_handle4);
	conn_handle_register(conn_handle5);
	conn_handle_register(conn_handle6);
	conn_handle_register(conn_handle7);
	conn_handle_register(conn_handle8);

	/* Activate some conn. handles and check that those are reported as valid. */
	ble_evt_handler(connected_evt(conn_handle1, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle2, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle3, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle4, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle5, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle6, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle7, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle8, dummy_role), NULL);

	for (uint16_t conn_handle = 0; conn_handle < (65535); conn_handle++) {
		valid = ble_conn_state_valid(conn_handle);
		if ((conn_handle == conn_handle1) || (conn_handle == conn_handle2) ||
		    (conn_handle == conn_handle3) || (conn_handle == conn_handle4) ||
		    (conn_handle == conn_handle5) || (conn_handle == conn_handle6) ||
		    (conn_handle == conn_handle7) || (conn_handle == conn_handle8)) {
			TEST_ASSERT(valid);
		} else {
			TEST_ASSERT_FALSE(valid);
		}
	}

	/* Deactivate some conn handles and check that they are still valid
	 * Handles which are disconnected should still be valid until a connect event occurs
	 */
	ble_evt_handler(disconnected_evt(conn_handle2), NULL);
	ble_evt_handler(disconnected_evt(conn_handle3), NULL);
	ble_evt_handler(disconnected_evt(conn_handle7), NULL);

	for (uint16_t conn_handle = 0; conn_handle < (65535); conn_handle++) {
		valid = ble_conn_state_valid(conn_handle);
		if ((conn_handle == conn_handle1) || (conn_handle == conn_handle2) ||
		    (conn_handle == conn_handle3) || (conn_handle == conn_handle4) ||
		    (conn_handle == conn_handle5) || (conn_handle == conn_handle6) ||
		    (conn_handle == conn_handle7) || (conn_handle == conn_handle8)) {
			/* Handles which are disconnected are still valid until a connect
			 * event occurs
			 */
			TEST_ASSERT(valid);
		} else {
			TEST_ASSERT_FALSE_MESSAGE(valid, msg);
		}
	}

	/* Reactivating a connection handle and checking that the disconnected handles are now
	 * invalid
	 */
	conn_handle_register(conn_handle3);
	ble_evt_handler(connected_evt(conn_handle3, dummy_role), NULL);

	for (uint16_t conn_handle = 0; conn_handle < (65535); conn_handle++) {
		sprintf(msg, "conn_handle = %d", conn_handle);
		valid = ble_conn_state_valid(conn_handle);
		if ((conn_handle == conn_handle1) || (conn_handle == conn_handle3) ||
		    (conn_handle == conn_handle4) || (conn_handle == conn_handle5) ||
		    (conn_handle == conn_handle6) || (conn_handle == conn_handle8)) {
			TEST_ASSERT(valid);
		} else {
			TEST_ASSERT_FALSE_MESSAGE(valid, msg);
		}
	}
}

void test_ble_conn_state_role(void)
{
	uint8_t role;

	uint16_t conn_handle1 = 15;
	uint16_t conn_handle2 = 16;

	conn_handle_register(conn_handle1);
	conn_handle_register(conn_handle2);

	/* Testing that invalid handle has an invalid role. */
	role = ble_conn_state_role(BLE_CONN_HANDLE_INVALID);
	TEST_ASSERT_EQUAL_UINT(BLE_GAP_ROLE_INVALID, role);

	/* Testing that invalid handle (not yet recorded) has an invalid role. */
	role = ble_conn_state_role(conn_handle1);
	TEST_ASSERT_EQUAL_UINT(BLE_GAP_ROLE_INVALID, role);

#if defined(BLE_GAP_ROLE_CENTRAL)
	/* Activating a connection with CENTRAL role. */
	conn_handle_register(conn_handle1);
	ble_evt_handler(connected_evt(conn_handle1, BLE_GAP_ROLE_CENTRAL), NULL);

	/* Test that the role is properly returned. */
	role = ble_conn_state_role(conn_handle1);
	TEST_ASSERT_EQUAL_UINT(BLE_GAP_ROLE_CENTRAL, role);

	/* The role should still be invalid for this other handle. */
	role = ble_conn_state_role(conn_handle2);
	TEST_ASSERT_EQUAL_UINT(BLE_GAP_ROLE_INVALID, role);

	/* Disconnect a handle and test that it still has a valid role,
	 * until a new connection occurs.
	 */
	ble_evt_handler(disconnected_evt(conn_handle1), NULL);
	role = ble_conn_state_role(conn_handle1);
	TEST_ASSERT_EQUAL_UINT(BLE_GAP_ROLE_CENTRAL, role);

	/* Test that a disconnected handle is invalidated after a connection has occurred. */
	conn_handle_register(conn_handle2);
	ble_evt_handler(connected_evt(conn_handle2, BLE_GAP_ROLE_CENTRAL), NULL);
	role = ble_conn_state_role(conn_handle1);
	TEST_ASSERT_EQUAL_UINT(BLE_GAP_ROLE_INVALID, role);
#endif
	/* (Re)activate both connections */
	ble_evt_handler(disconnected_evt(conn_handle1), NULL);
	ble_evt_handler(disconnected_evt(conn_handle2), NULL);
#if defined(BLE_GAP_ROLE_CENTRAL)
	conn_handle_register(conn_handle1);
	ble_evt_handler(connected_evt(conn_handle1, BLE_GAP_ROLE_CENTRAL), NULL);
#endif

	conn_handle_register(conn_handle2);
	ble_evt_handler(connected_evt(conn_handle2, BLE_GAP_ROLE_PERIPH), NULL);

#if defined(BLE_GAP_ROLE_CENTRAL)
	role = ble_conn_state_role(conn_handle1);
	TEST_ASSERT_EQUAL_UINT(BLE_GAP_ROLE_CENTRAL, role);
#endif

	role = ble_conn_state_role(conn_handle2);
	TEST_ASSERT_EQUAL_UINT(BLE_GAP_ROLE_PERIPH,  role);

	/* Testing overflow of conn_handle. */
	ble_conn_state_init();
	role = ble_conn_state_role(conn_handle_overflow);
	TEST_ASSERT_EQUAL_UINT(BLE_GAP_ROLE_INVALID, role);
	role = ble_conn_state_role(BLE_CONN_HANDLE_INVALID);
	TEST_ASSERT_EQUAL_UINT(BLE_GAP_ROLE_INVALID, role);
}

void test_ble_conn_state_encrypted(void)
{
	bool encrypted, mitm_protected, lesc;

	conn_handle1 = 12;
	conn_handle2 = 17; /* dummy conn handle */

	conn_handle_register(conn_handle1);
	conn_handle_register(conn_handle2);

	/* Testing that an invalid handle returns unencrypted. */
	encrypted      = ble_conn_state_encrypted(BLE_CONN_HANDLE_INVALID);
	mitm_protected = ble_conn_state_mitm_protected(BLE_CONN_HANDLE_INVALID);
	lesc           = ble_conn_state_lesc(BLE_CONN_HANDLE_INVALID);
	TEST_ASSERT_FALSE(encrypted);
	TEST_ASSERT_FALSE(mitm_protected);
	TEST_ASSERT_FALSE(lesc);

	/* Testing that an inactive handle returns unencrypted. */
	encrypted      = ble_conn_state_encrypted(conn_handle1);
	mitm_protected = ble_conn_state_mitm_protected(conn_handle1);
	lesc           = ble_conn_state_lesc(conn_handle1);
	TEST_ASSERT_FALSE(encrypted);
	TEST_ASSERT_FALSE(mitm_protected);
	TEST_ASSERT_FALSE(lesc);

	/* Testing that an active, unencrypted handle returns unencrypted. */
	ble_evt_handler(connected_evt(conn_handle1, BLE_GAP_ROLE_PERIPH), NULL);
	encrypted      = ble_conn_state_encrypted(conn_handle1);
	mitm_protected = ble_conn_state_mitm_protected(conn_handle1);
	lesc           = ble_conn_state_lesc(conn_handle1);
	TEST_ASSERT_FALSE(encrypted);
	TEST_ASSERT_FALSE(mitm_protected);
	TEST_ASSERT_FALSE(lesc);

	/* Testing that a security level of 2 or greater returns encrypted */
	ble_evt_handler(conn_sec_update_evt(conn_handle1, 2), NULL);
	encrypted      = ble_conn_state_encrypted(conn_handle1);
	mitm_protected = ble_conn_state_mitm_protected(conn_handle1);
	lesc           = ble_conn_state_lesc(conn_handle1);
	TEST_ASSERT(encrypted);
	TEST_ASSERT_FALSE(mitm_protected);
	TEST_ASSERT_FALSE(lesc);

	/* Testing that a successful auth_status with LESC returns LESC. */
	ble_evt_handler(auth_status_evt(conn_handle1, false, BLE_GAP_SEC_STATUS_SUCCESS), NULL);
	lesc           = ble_conn_state_lesc(conn_handle1);
	TEST_ASSERT_FALSE(lesc);
	ble_evt_handler(auth_status_evt(conn_handle1, true, BLE_GAP_SEC_STATUS_UNSPECIFIED), NULL);
	lesc           = ble_conn_state_lesc(conn_handle1);
	TEST_ASSERT_FALSE(lesc);
	ble_evt_handler(auth_status_evt(conn_handle1, true, BLE_GAP_SEC_STATUS_SUCCESS), NULL);
	lesc           = ble_conn_state_lesc(conn_handle1);
	TEST_ASSERT(lesc);

	/* level 3 returns MITM protected. */
	ble_evt_handler(conn_sec_update_evt(conn_handle1, 3), NULL);
	encrypted      = ble_conn_state_encrypted(conn_handle1);
	mitm_protected = ble_conn_state_mitm_protected(conn_handle1);
	lesc           = ble_conn_state_lesc(conn_handle1);
	TEST_ASSERT(encrypted);
	TEST_ASSERT(mitm_protected);
	TEST_ASSERT_FALSE(lesc);

	/* level 4 returns LESC. */
	ble_evt_handler(conn_sec_update_evt(conn_handle1, 4), NULL);
	encrypted      = ble_conn_state_encrypted(conn_handle1);
	mitm_protected = ble_conn_state_mitm_protected(conn_handle1);
	lesc           = ble_conn_state_lesc(conn_handle1);
	TEST_ASSERT(encrypted);
	TEST_ASSERT(mitm_protected);
	TEST_ASSERT(lesc);

	/* Testing that a security level of less than 2 returns unencrypted. */
	ble_evt_handler(conn_sec_update_evt(conn_handle1, 0), NULL);
	encrypted      = ble_conn_state_encrypted(conn_handle1);
	mitm_protected = ble_conn_state_mitm_protected(conn_handle1);
	lesc           = ble_conn_state_lesc(conn_handle1);
	TEST_ASSERT_FALSE(encrypted);
	TEST_ASSERT_FALSE(mitm_protected);
	TEST_ASSERT_FALSE(lesc);

	ble_evt_handler(conn_sec_update_evt(conn_handle1, 1), NULL);
	encrypted      = ble_conn_state_encrypted(conn_handle1);
	mitm_protected = ble_conn_state_mitm_protected(conn_handle1);
	lesc           = ble_conn_state_lesc(conn_handle1);
	TEST_ASSERT_FALSE(encrypted);
	TEST_ASSERT_FALSE(mitm_protected);
	TEST_ASSERT_FALSE(lesc);

	/* Adding a second connection. */
	ble_evt_handler(connected_evt(conn_handle2, BLE_GAP_ROLE_PERIPH), NULL);
	ble_evt_handler(conn_sec_update_evt(conn_handle2, 4), NULL);
	encrypted      = ble_conn_state_encrypted(conn_handle2);
	mitm_protected = ble_conn_state_mitm_protected(conn_handle2);
	lesc           = ble_conn_state_lesc(conn_handle2);
	TEST_ASSERT(encrypted);
	TEST_ASSERT(mitm_protected);
	TEST_ASSERT(lesc);

	/* Testing overflow of conn_handle. */
	ble_conn_state_init();

	/* Make sure this doesn't read from next flag (mitm_protected) */
	encrypted      = ble_conn_state_encrypted(conn_handle_overflow);
	/* Make sure this doesn't read from next flag (user flag 0) */
	mitm_protected = ble_conn_state_mitm_protected(conn_handle_overflow);
	/* Make sure this doesn't read from next flag (user flag 0) */
	lesc           = ble_conn_state_lesc(conn_handle_overflow);
	TEST_ASSERT_FALSE(encrypted);
	TEST_ASSERT_FALSE(mitm_protected);
	TEST_ASSERT_FALSE(lesc);
	encrypted      = ble_conn_state_encrypted(BLE_CONN_HANDLE_INVALID);
	mitm_protected = ble_conn_state_mitm_protected(BLE_CONN_HANDLE_INVALID);
	lesc           = ble_conn_state_lesc(BLE_CONN_HANDLE_INVALID);
	TEST_ASSERT_FALSE(encrypted);
	TEST_ASSERT_FALSE(mitm_protected);
	TEST_ASSERT_FALSE(lesc);
}

void test_ble_conn_state_status(void)
{
	bool valid;

	conn_handle1 = 0;
	conn_handle_register(conn_handle1);
#if defined(BLE_GAP_ROLE_CENTRAL)
	conn_handle2 = 12;
	conn_handle_register(conn_handle2);
#endif
	conn_handle3 = 19;
	conn_handle_register(conn_handle3);
	uint8_t  dummy_role;

#if defined(BLE_GAP_ROLE_CENTRAL)
	dummy_role = BLE_GAP_ROLE_CENTRAL;

	/* Test that invalid connections have BLE_CONN_STATUS_INVALID as their connection status */
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(8172), BLE_CONN_STATUS_INVALID);
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(BLE_CONN_HANDLE_INVALID),
			       BLE_CONN_STATUS_INVALID);

	/* Activating some conn handles. */
	ble_evt_handler(connected_evt(conn_handle1, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle2, dummy_role), NULL);
	ble_evt_handler(connected_evt(conn_handle3, dummy_role), NULL);

	/* Let's test they are connected */
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle1), BLE_CONN_STATUS_CONNECTED);
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle2), BLE_CONN_STATUS_CONNECTED);
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle3), BLE_CONN_STATUS_CONNECTED);

	/* Disconnect one handle */
	ble_evt_handler(disconnected_evt(conn_handle2), NULL);
	/* Its status should be DISCONNECTED now */
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle1), BLE_CONN_STATUS_CONNECTED);
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle2), BLE_CONN_STATUS_DISCONNECTED);
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle3), BLE_CONN_STATUS_CONNECTED);

	/* Disconnect another handle */
	ble_evt_handler(disconnected_evt(conn_handle3), NULL);
	/* There should be two connections whose status is DISCONNECTED */
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle1), BLE_CONN_STATUS_CONNECTED);
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle2), BLE_CONN_STATUS_DISCONNECTED);
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle3), BLE_CONN_STATUS_DISCONNECTED);

	/* Handles of connection whose status is DISCONNECTED should still be valid */
	valid = ble_conn_state_valid(conn_handle1);
	valid = valid && ble_conn_state_valid(conn_handle2);
	valid = valid && ble_conn_state_valid(conn_handle3);

	TEST_ASSERT(valid);

	/* Reactivate a connection handle */
	ble_evt_handler(connected_evt(conn_handle3, dummy_role), NULL);

	/* After a connection event is received, disconnected connections are purged */
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle1), BLE_CONN_STATUS_CONNECTED);
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle2), BLE_CONN_STATUS_INVALID);
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle3), BLE_CONN_STATUS_CONNECTED);

	valid = ble_conn_state_status(conn_handle2);
	TEST_ASSERT_FALSE(valid);

	/* Let's disconnect another handle */
	ble_evt_handler(disconnected_evt(conn_handle1), NULL);

	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle1), BLE_CONN_STATUS_DISCONNECTED);
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle2), BLE_CONN_STATUS_INVALID);
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle3), BLE_CONN_STATUS_CONNECTED);

	valid = ble_conn_state_status(conn_handle1);
	TEST_ASSERT(valid);
	valid = ble_conn_state_status(conn_handle2);
	TEST_ASSERT_FALSE(valid);

	ble_evt_handler(disconnected_evt(conn_handle3), NULL);
#endif

	dummy_role = BLE_GAP_ROLE_PERIPH;

	ble_evt_handler(connected_evt(conn_handle1, dummy_role), NULL);
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle1), BLE_CONN_STATUS_CONNECTED);

	ble_evt_handler(disconnected_evt(conn_handle1), NULL);
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle1), BLE_CONN_STATUS_DISCONNECTED);

	ble_evt_handler(connected_evt(conn_handle3, dummy_role), NULL);
	TEST_ASSERT_EQUAL_UINT(ble_conn_state_status(conn_handle1), BLE_CONN_STATUS_INVALID);

	/* Testing overflow of conn_handle. */
	ble_conn_state_init();
	valid = ble_conn_state_status(conn_handle_overflow);
	TEST_ASSERT_FALSE(valid);
	valid = ble_conn_state_status(BLE_CONN_HANDLE_INVALID);
	TEST_ASSERT_FALSE(valid);
}

void test_ble_conn_state_connections_and_list(void)
{
	uint32_t connections;
	struct ble_conn_state_conn_handle_list conn_handle_list;
	uint16_t conn_handles[8] = {0, 1, 2, 3, 10, 12, 18, 19};

	/* Testing that n is initially 0 and list is empty. */
	connections = ble_conn_state_conn_count();
	conn_handle_list = ble_conn_state_conn_handles();
	TEST_ASSERT_EQUAL_UINT(0, connections);
	TEST_ASSERT_EQUAL_UINT(0, conn_handle_list.len);

	/* Activating all connections. Testing that n is updated. */
	for (int i = 0; i < 8; i++) {
		conn_handle_register(conn_handles[i]);
		ble_evt_handler(connected_evt(conn_handles[i], BLE_GAP_ROLE_PERIPH), NULL);
	}

	connections = ble_conn_state_conn_count();
	conn_handle_list = ble_conn_state_conn_handles();
	TEST_ASSERT_EQUAL_UINT(8, connections);
	TEST_ASSERT_EQUAL_UINT(8, conn_handle_list.len);
	TEST_ASSERT_EQUAL_UINT16_ARRAY(conn_handles, conn_handle_list.conn_handles,
				       conn_handle_list.len);

	/* Deactivating all but one connection. Testing that n is updated */
	for (int i = 1; i < 8; i++) {
		ble_evt_handler(disconnected_evt(conn_handles[i]), NULL);
	}

	connections = ble_conn_state_conn_count();
	TEST_ASSERT_EQUAL_UINT(1, connections);

	/* The connections should still be valid after being disconnected,
	 * until a new connection event is received
	 */
	conn_handle_list = ble_conn_state_conn_handles();
	TEST_ASSERT_EQUAL_UINT(8, conn_handle_list.len);
	TEST_ASSERT_EQUAL_UINT16_ARRAY(conn_handles, conn_handle_list.conn_handles,
				       conn_handle_list.len);

	/* Activate one connection. Testing that n is updated (should now be one). */
	ble_evt_handler(connected_evt(conn_handles[0], BLE_GAP_ROLE_PERIPH), NULL);

	connections = ble_conn_state_conn_count();
	TEST_ASSERT_EQUAL_UINT(1, connections);

	conn_handle_list = ble_conn_state_conn_handles();
	TEST_ASSERT_EQUAL_UINT(1, conn_handle_list.len);

	/* Activating all connections. Testing that n is updated (should now be 8). */
	for (int i = 0; i < 8; i++) {
		ble_evt_handler(connected_evt(conn_handles[i], BLE_GAP_ROLE_PERIPH), NULL);
	}

	connections = ble_conn_state_conn_count();
	TEST_ASSERT_EQUAL_UINT(8, connections);

	conn_handle_list = ble_conn_state_conn_handles();
	TEST_ASSERT_EQUAL_UINT(8, conn_handle_list.len);
	TEST_ASSERT_EQUAL_UINT16_ARRAY(conn_handles, conn_handle_list.conn_handles,
				       conn_handle_list.len);
}

void test_ble_conn_state_centrals_and_list(void)
{
#if defined(BLE_GAP_ROLE_CENTRAL)
	bool valid
	uint32_t n_centrals;
	uint32_t connections;
	uint16_t conn_handles[8] = {0, 1, 2, 3, 10, 12, 17, 18};
	struct ble_conn_state_conn_handle_list conn_handle_list;

	/* Testing that n is initially 0 and list is empty. */
	connections = ble_conn_state_conn_count();
	TEST_ASSERT_EQUAL_UINT(0, connections);

	n_centrals = ble_conn_state_central_conn_count();
	TEST_ASSERT_EQUAL_UINT(0, n_centrals);

	conn_handle_list = ble_conn_state_conn_handles();
	TEST_ASSERT_EQUAL_UINT(0, conn_handle_list.len);

	/* Activating all connections. Testing that n is updated. */
	for (int i = 0; i < 8; i++) {
		ble_evt_handler(connected_evt(conn_handles[i], BLE_GAP_ROLE_CENTRAL), NULL);
	}

	n_centrals = ble_conn_state_central_conn_count();
	TEST_ASSERT_EQUAL_UINT(8, n_centrals);

	conn_handle_list = ble_conn_state_conn_handles();
	TEST_ASSERT_EQUAL_UINT(8, conn_handle_list.len);
	TEST_ASSERT_EQUAL_UINT16_ARRAY(conn_handles, conn_handle_list.conn_handles,
				       conn_handle_list.len);

	/* Deactivating all but one connection. Testing that n is unchanged */
	for (int i = 1; i < 8; i++) {
		ble_evt_handler(disconnected_evt(conn_handles[i]), NULL);
		/* Should still be valid */
		valid = ble_conn_state_valid(conn_handles[i]);
		TEST_ASSERT(valid);
	}

	n_centrals = ble_conn_state_central_conn_count();
	TEST_ASSERT_EQUAL_UINT(1, n_centrals);

	/* The connections should still be valid after being disconnected,
	 * until a new connection event is received
	 */
	conn_handle_list = ble_conn_state_conn_handles();
	TEST_ASSERT_EQUAL_UINT(8, conn_handle_list.len);
	TEST_ASSERT_EQUAL_UINT16_ARRAY(conn_handles, conn_handle_list.conn_handles,
				       conn_handle_list.len);

	/* Activate one connection. Testing that n is updated. */
	ble_evt_handler(connected_evt(conn_handles[0], BLE_GAP_ROLE_CENTRAL), NULL);

	n_centrals = ble_conn_state_central_conn_count();
	TEST_ASSERT_EQUAL_UINT(1, n_centrals);

	conn_handle_list = ble_conn_state_conn_handles();
	TEST_ASSERT_EQUAL_UINT(1, conn_handle_list.len);

	/* Activating all connections. Testing that n is updated. */
	for (int i = 0; i < 8; i++) {
		ble_evt_handler(connected_evt(conn_handles[i], BLE_GAP_ROLE_CENTRAL), NULL);
	}

	n_centrals = ble_conn_state_central_conn_count();
	TEST_ASSERT_EQUAL_UINT(8, n_centrals);

	conn_handle_list = ble_conn_state_conn_handles();
	TEST_ASSERT_EQUAL_UINT(8, conn_handle_list.len);
	TEST_ASSERT_EQUAL_UINT16_ARRAY(conn_handles, conn_handle_list.conn_handles,
				       conn_handle_list.len);
#endif
}

void test_ble_conn_status_n_peripherals_and_handle(void)
{
	bool valid;

	/* No peripherals should be connected */
	TEST_ASSERT_EQUAL(0, ble_conn_state_peripheral_conn_count());

	/* Connect one device as peripheral, and check that the number of peripherals is
	 * correctly updated
	 */
	conn_handle_register(BLE_CONN_STATE_MAX_CONNECTIONS-1);
	ble_evt_handler(connected_evt(BLE_CONN_STATE_MAX_CONNECTIONS-1, BLE_GAP_ROLE_PERIPH), NULL);
	TEST_ASSERT_EQUAL(1, ble_conn_state_peripheral_conn_count());

	/* Disconnect the peripheral and check that the number is updated */
	ble_evt_handler(disconnected_evt(BLE_CONN_STATE_MAX_CONNECTIONS-1), NULL);
	TEST_ASSERT_EQUAL(0, ble_conn_state_peripheral_conn_count());

	/* The handle should not be in the list,
	 * but should be valid until a new connection occurs
	 */
	TEST_ASSERT_EQUAL(0, ble_conn_state_periph_handles().len);
	valid = ble_conn_state_valid(BLE_CONN_STATE_MAX_CONNECTIONS-1);
	TEST_ASSERT(valid);

	/* app_error_handler_bare_Expect(NRF_ERROR_NO_MEM); */
	ble_evt_handler(connected_evt(BLE_CONN_STATE_MAX_CONNECTIONS, BLE_GAP_ROLE_PERIPH), NULL);

	/* app_error_handler_bare_Expect(NRF_ERROR_NO_MEM); */
	conn_handle_register(1000);
	ble_evt_handler(connected_evt(1000, BLE_GAP_ROLE_PERIPH), NULL);

	/* Connect some handles */
	for (int i = 0; i < BLE_CONN_STATE_MAX_CONNECTIONS-1; i++) {
		conn_handle_register(i);
		ble_evt_handler(connected_evt(i, BLE_GAP_ROLE_PERIPH), NULL);
	}
	/* Should report all peripherals */
	TEST_ASSERT_EQUAL(BLE_CONN_STATE_MAX_CONNECTIONS-1, ble_conn_state_peripheral_conn_count());

	/* This handle should have been invalidated by now */
	valid = ble_conn_state_valid(BLE_CONN_STATE_MAX_CONNECTIONS-1);
	TEST_ASSERT_FALSE(valid);
}

void test_ble_conn_state_user_flag_acquire(void)
{
	uint32_t acquired_ids[CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT];

	char msg1[] = "i = 00";
	char msg2[] = "i = 00, j = 00";

	for (int i = 0; i < CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT; i++) {
		acquired_ids[i] = ble_conn_state_user_flag_acquire();
	}

	for (uint32_t i = 0; i < CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT; i++) {
		sprintf(msg1, "i = %d", i);

		TEST_ASSERT_MESSAGE(acquired_ids[i] != CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT, msg1);
		for (uint32_t j = i + 1; j < CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT; j++) {
			sprintf(msg2, "i = %d, j = %d", i, j);

			TEST_ASSERT_MESSAGE(acquired_ids[i] != acquired_ids[j], msg2);
		}
	}

	TEST_ASSERT_EQUAL(BLE_CONN_STATE_USER_FLAG_INVALID, ble_conn_state_user_flag_acquire());
	TEST_ASSERT_EQUAL(BLE_CONN_STATE_USER_FLAG_INVALID, ble_conn_state_user_flag_acquire());
}

void test_ble_conn_state_user_flag_set_get(void)
{
	bool flag_state;
	uint32_t acquired_ids[CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT] = {0};

	uint16_t conn_handles[BLE_CONN_STATE_MAX_CONNECTIONS];
	uint16_t invalid_conn_handle = 35354;

	for (uint32_t i = 0; i < BLE_CONN_STATE_MAX_CONNECTIONS; i++) {
		conn_handles[i] = i;
	}

	char out_str[] = "flag_id index: 00, conn_handle index: 00.";

	for (int i = 0; i < BLE_CONN_STATE_MAX_CONNECTIONS; i++) {
		conn_handle_register(conn_handles[i]);
		ble_evt_handler(connected_evt(conn_handles[i], BLE_GAP_ROLE_PERIPH), NULL);
	}

	for (int i = 0; i < CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT; i++) {
		for (int j = 0; j < BLE_CONN_STATE_MAX_CONNECTIONS; j++) {
			/* Testing that setting and getting has no effect for invalid
			 * connection handles.
			 */
			sprintf(out_str, "flag_id index: %d, conn_handle index: %d.", i, j);

			flag_state = ble_conn_state_user_flag_get(conn_handles[j], acquired_ids[i]);
			TEST_ASSERT_FALSE(flag_state);

			ble_conn_state_user_flag_set(conn_handles[j], acquired_ids[i], true);
			flag_state = ble_conn_state_user_flag_get(conn_handles[j], acquired_ids[i]);
			TEST_ASSERT_FALSE(flag_state);
		}
	}

	for (int i = 0; i < CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT; i++) {
		acquired_ids[i] = ble_conn_state_user_flag_acquire();
	}

	for (int i = 0; i < CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT; i++) {
		for (int j = 0; j < BLE_CONN_STATE_MAX_CONNECTIONS; j++) {
			/* Testing that setting and getting works. */
			sprintf(out_str, "flag_id index: %d, conn_handle index: %d.", i, j);
			flag_state = ble_conn_state_user_flag_get(conn_handles[j], acquired_ids[i]);
			TEST_ASSERT_FALSE_MESSAGE(flag_state, out_str);

			ble_conn_state_user_flag_set(conn_handles[j], acquired_ids[i], true);
			flag_state = ble_conn_state_user_flag_get(conn_handles[j], acquired_ids[i]);
			TEST_ASSERT_MESSAGE(flag_state, out_str);

			ble_conn_state_user_flag_set(conn_handles[j], acquired_ids[i], false);
			flag_state = ble_conn_state_user_flag_get(conn_handles[j], acquired_ids[i]);
			TEST_ASSERT_FALSE_MESSAGE(flag_state, out_str);

			ble_conn_state_user_flag_set(conn_handles[j], acquired_ids[i], true);
			flag_state = ble_conn_state_user_flag_get(conn_handles[j], acquired_ids[i]);
			TEST_ASSERT_MESSAGE(flag_state, out_str);
		}

		/* Testing that setting and getting has no effect for invalid connection handles,
		 * even after others are connected.
		 */
		sprintf(out_str, "flag_id index: %d.", i);

		ble_conn_state_user_flag_set(BLE_CONN_STATE_MAX_CONNECTIONS, acquired_ids[i], true);
		flag_state = ble_conn_state_user_flag_get(BLE_CONN_STATE_MAX_CONNECTIONS,
							  acquired_ids[i]);
		TEST_ASSERT_FALSE_MESSAGE(flag_state, out_str);

		ble_conn_state_user_flag_set(BLE_CONN_HANDLE_INVALID, acquired_ids[i], true);
		flag_state = ble_conn_state_user_flag_get(BLE_CONN_HANDLE_INVALID, acquired_ids[i]);
		TEST_ASSERT_FALSE_MESSAGE(flag_state, out_str);

		ble_conn_state_user_flag_set(invalid_conn_handle, acquired_ids[i], true);
		flag_state = ble_conn_state_user_flag_get(invalid_conn_handle, acquired_ids[i]);
		TEST_ASSERT_FALSE_MESSAGE(flag_state, out_str);

		ble_conn_state_user_flag_set(invalid_conn_handle, acquired_ids[i], false);
		flag_state = ble_conn_state_user_flag_get(invalid_conn_handle, acquired_ids[i]);
		TEST_ASSERT_FALSE_MESSAGE(flag_state, out_str);
	}

	/* The rest of the test will be on two arbitrary indices. */
	int arbitrary_index1 = 3;
	int arbitrary_index2 = 5;

	for (int i = 0; i < CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT; i++) {
		/* The flags should still be 1. */
		sprintf(out_str, "flag_id index: %d.", i);
		flag_state = ble_conn_state_user_flag_get(conn_handles[0], acquired_ids[i]);
		TEST_ASSERT_MESSAGE(flag_state, out_str);
		flag_state = ble_conn_state_user_flag_get(conn_handles[arbitrary_index1],
							  acquired_ids[i]);
		TEST_ASSERT_MESSAGE(flag_state, out_str);
		flag_state = ble_conn_state_user_flag_get(conn_handles[arbitrary_index2],
							  acquired_ids[i]);
		TEST_ASSERT_MESSAGE(flag_state, out_str);
		/* Test that it doesn't overflow into next flags. */
		ble_conn_state_user_flag_set(conn_handle_overflow, acquired_ids[i], false);
	}

	/* Invalidate the connection handles by disconnecting them and reconnecting. */
	ble_evt_handler(disconnected_evt(conn_handles[arbitrary_index1]), NULL);
	ble_evt_handler(disconnected_evt(conn_handles[arbitrary_index2]), NULL);
	ble_evt_handler(connected_evt(conn_handles[arbitrary_index1], BLE_GAP_ROLE_PERIPH), NULL);
	ble_evt_handler(connected_evt(conn_handles[arbitrary_index2], BLE_GAP_ROLE_PERIPH), NULL);

	for (int i = 0; i < CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT; i++) {
		/* Test that the flags are now 0 because of being invalidated. */
		sprintf(out_str, "flag_id index: %d.", i);
		flag_state = ble_conn_state_user_flag_get(conn_handles[arbitrary_index1],
							  acquired_ids[i]);
		TEST_ASSERT_FALSE_MESSAGE(flag_state, out_str);
		flag_state = ble_conn_state_user_flag_get(conn_handles[arbitrary_index2],
							  acquired_ids[i]);
		TEST_ASSERT_FALSE_MESSAGE(flag_state, out_str);
	}
}

void test_ble_conn_state_conn_idx(void)
{
#if defined(BLE_GAP_ROLE_CENTRAL)
	uint16_t conn_handle_err = BLE_CONN_STATE_MAX_CONNECTIONS+1;
	uint16_t conn_handle_last = BLE_CONN_STATE_MAX_CONNECTIONS-1;

	ble_evt_handler(connected_evt(0, BLE_GAP_ROLE_CENTRAL), NULL);
	ble_evt_handler(connected_evt(1, BLE_GAP_ROLE_PERIPH), NULL);
	ble_evt_handler(connected_evt(5, BLE_GAP_ROLE_CENTRAL), NULL);
	ble_evt_handler(connected_evt(conn_handle_last, BLE_GAP_ROLE_CENTRAL), NULL);

	TEST_ASSERT_EQUAL(0, ble_conn_state_conn_idx(0));
	TEST_ASSERT_EQUAL(1, ble_conn_state_conn_idx(1));
	TEST_ASSERT_EQUAL(5, ble_conn_state_conn_idx(5));
	TEST_ASSERT_EQUAL(conn_handle_last, ble_conn_state_conn_idx(conn_handle_last));
	TEST_ASSERT_EQUAL(BLE_CONN_STATE_MAX_CONNECTIONS, ble_conn_state_conn_idx(conn_handle_err));
	TEST_ASSERT_EQUAL(BLE_CONN_STATE_MAX_CONNECTIONS, ble_conn_state_conn_idx(2));
	TEST_ASSERT_EQUAL(BLE_CONN_STATE_MAX_CONNECTIONS, ble_conn_state_conn_idx(3));
	TEST_ASSERT_EQUAL(BLE_CONN_STATE_MAX_CONNECTIONS, ble_conn_state_conn_idx(4));
	for (uint16_t i = 6; i < conn_handle_last; i++) {
		TEST_ASSERT_EQUAL(BLE_CONN_STATE_MAX_CONNECTIONS, ble_conn_state_conn_idx(i));
	}
#endif
}

void user_flag_function(uint16_t conn_handle, void *context)
{
	TEST_ASSERT(calls < 10);
	TEST_ASSERT_EQUAL_UINT(conn_handles[calls], conn_handle);
	TEST_ASSERT_EQUAL_PTR(contexts[calls], context);
	calls++;
}

void expect_user_function_user_flag(uint16_t conn_handle,
				    uint32_t flag_id,
				    void *context,
				    uint32_t call)
{
	conn_handles[call] = conn_handle;
	contexts[call] = context;
}

void expect_user_function_connected(uint16_t conn_handle, void *context, uint32_t call)
{
	conn_handles[call] = conn_handle;
	contexts[call] = context;
}

void test_ble_conn_state_for_each_set_user_flag(void)
{
	uint32_t flag_id1 = ble_conn_state_user_flag_acquire();
	uint32_t flag_id2 = ble_conn_state_user_flag_acquire();
	uint32_t calls_ret;
	uint16_t conn_handles[10];

	for (uint32_t i = 0; i < 10; i++) {
		conn_handles[i] = i;
	}

	for (int i = 0; i < 10; i++) {
		conn_handle_register(conn_handles[i]);
		ble_evt_handler(connected_evt(conn_handles[i], BLE_GAP_ROLE_PERIPH), NULL);
	}

	/* No set flags. */
	calls_ret = ble_conn_state_for_each_set_user_flag(flag_id1, user_flag_function, NULL);
	TEST_ASSERT_EQUAL(0, calls_ret);
	TEST_ASSERT_EQUAL(0, calls);

	/* One set flag */
	ble_conn_state_user_flag_set(conn_handles[0], flag_id1, 1);
	expect_user_function_user_flag(conn_handles[0], flag_id1, NULL, 0);
	calls_ret = ble_conn_state_for_each_set_user_flag(flag_id1, user_flag_function, NULL);
	TEST_ASSERT_EQUAL(1, calls_ret);
	TEST_ASSERT_EQUAL(1, calls);
	calls = 0;

	/* One set flag, other flag id. */
	ble_conn_state_user_flag_set(conn_handles[1], flag_id2, 1);
	expect_user_function_user_flag(conn_handles[1], flag_id2, NULL, 0);
	calls_ret = ble_conn_state_for_each_set_user_flag(flag_id2, user_flag_function, NULL);
	TEST_ASSERT_EQUAL(1, calls_ret);
	TEST_ASSERT_EQUAL(1, calls);
	calls = 0;

	/* Two set flags, context */
	ble_conn_state_user_flag_set(conn_handles[3], flag_id1, 1);
	expect_user_function_user_flag(conn_handles[0], flag_id1, &arbitrary_context, 0);
	expect_user_function_user_flag(conn_handles[3], flag_id1, &arbitrary_context, 1);
	calls_ret = ble_conn_state_for_each_set_user_flag(flag_id1, user_flag_function,
							  &arbitrary_context);
	TEST_ASSERT_EQUAL(2, calls_ret);
	TEST_ASSERT_EQUAL(2, calls);
	calls = 0;

	/* One set flag */
	ble_conn_state_user_flag_set(conn_handles[0], flag_id1, 0);
	expect_user_function_user_flag(conn_handles[3], flag_id1, NULL, 0);
	calls_ret = ble_conn_state_for_each_set_user_flag(flag_id1, user_flag_function, NULL);
	TEST_ASSERT_EQUAL(1, calls_ret);
	TEST_ASSERT_EQUAL(1, calls);
	calls = 0;

	/* All set flags */
	for (int i = 0; i < 10; i++) {
		ble_conn_state_user_flag_set(conn_handles[i], flag_id2, 1);
		expect_user_function_user_flag(conn_handles[i], flag_id2, NULL, i);
	}
	calls_ret = ble_conn_state_for_each_set_user_flag(flag_id2, user_flag_function, NULL);
	TEST_ASSERT_EQUAL(10, calls_ret);
	TEST_ASSERT_EQUAL(10, calls);
	calls = 0;

	/* No set flags. */
	ble_conn_state_user_flag_set(conn_handles[3], flag_id1, 0);
	calls_ret = ble_conn_state_for_each_set_user_flag(flag_id1, user_flag_function, NULL);
	TEST_ASSERT_EQUAL(0, calls_ret);
	TEST_ASSERT_EQUAL(0, calls);

	/* No set flags. */
	for (int i = 0; i < 10; i++) {
		ble_evt_handler(disconnected_evt(conn_handles[i]), NULL);
		ble_evt_handler(connected_evt(conn_handles[i], BLE_GAP_ROLE_PERIPH), NULL);
	}
	ble_conn_state_user_flag_set(conn_handles[3], flag_id2, 0);
	calls_ret = ble_conn_state_for_each_set_user_flag(flag_id1, user_flag_function, NULL);
	TEST_ASSERT_EQUAL(0, calls_ret);
	TEST_ASSERT_EQUAL(0, calls);
}

void test_ble_conn_state_for_each_connected(void)
{
#if defined(BLE_GAP_ROLE_CENTRAL)
	uint32_t calls_ret;

	calls_ret = ble_conn_state_for_each_connected(user_flag_function, NULL);
	TEST_ASSERT_EQUAL(0, calls_ret);

	ble_evt_handler(connected_evt(1, BLE_GAP_ROLE_CENTRAL), NULL);
	ble_evt_handler(connected_evt(2, BLE_GAP_ROLE_CENTRAL), NULL);
	ble_evt_handler(connected_evt(8, BLE_GAP_ROLE_PERIPH), NULL);

	expect_user_function_connected(1, NULL, 0);
	expect_user_function_connected(2, NULL, 1);
	expect_user_function_connected(8, NULL, 2);

	calls_ret = ble_conn_state_for_each_connected(user_flag_function, NULL);
	TEST_ASSERT_EQUAL(3, calls_ret);
	TEST_ASSERT_EQUAL(3, calls);
	calls = 0;

	ble_evt_handler(disconnected_evt(1), NULL);
	ble_evt_handler(connected_evt(5, BLE_GAP_ROLE_PERIPH), NULL);
	ble_evt_handler(disconnected_evt(8), NULL);

	expect_user_function_connected(2, NULL, 0);
	expect_user_function_connected(5, NULL, 1);

	calls_ret = ble_conn_state_for_each_connected(user_flag_function, NULL);
	TEST_ASSERT_EQUAL(2, calls_ret);
	TEST_ASSERT_EQUAL(2, calls);
	calls = 0;
#endif
}

void setUp(void)
{
	ble_conn_state_init();
}
void tearDown(void)
{
	/* Maximum connection handle allowed is 100*/
	for (int i = 0; i < 100; i++) {
		conn_handle_deregister(i);
	}
}


extern int unity_main(void);

int main(void)
{
	return unity_main();
}
