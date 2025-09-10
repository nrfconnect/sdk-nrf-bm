/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "gatts_cache_manager.c"

#include <string.h>
#include "unity.h"
#include "mock_peer_database.h"
#include "mock_peer_data_storage.h"
#include "mock_ble_gap.h"
#include "mock_ble_gatts.h"
#include "mock_id_manager.h"
#include "mock_ble_conn_state.h"
#include "peer_manager_types.h"

#define MAX_EVT_HANDLER_CALLS 20
#define SYS_ATTR_LEN_3_CCCDS  6 * 3 + 2

static uint16_t m_arbitrary_conn_handle = 63;
static pm_peer_id_t m_arbitrary_peer_id = 7;
static const uint16_t m_arbitrary_sys_attr_len = SYS_ATTR_LEN_3_CCCDS;
static uint8_t m_arbitrary_sys_attr_data[SYS_ATTR_LEN_3_CCCDS] = {
	7, 7, 96, 43, 3, 86, 8, 7, 58, 3, 8, 6, 74, 48, 7, 8}; // 3 CCCDs
static uint8_t m_arbitrary_array[50];
static uint8_t m_arbitrary_array2[50];
static pm_peer_data_local_gatt_db_t *m_arbitrary_local_gatt_data =
	(pm_peer_data_local_gatt_db_t *)m_arbitrary_array;
static pm_peer_data_local_gatt_db_t *m_arbitrary_local_gatt_data_ref =
	(pm_peer_data_local_gatt_db_t *)m_arbitrary_array2;

static uint32_t m_n_evt_handler_calls;
static pm_evt_t m_evt_handler_records[MAX_EVT_HANDLER_CALLS];

void evt_handler_call_record_clear(void)
{
	m_n_evt_handler_calls = 0;
}

void pm_gscm_evt_handler(pm_evt_t *event)
{
	m_evt_handler_records[m_n_evt_handler_calls++] = *event;
}

void tearDown(void)
{
	internal_state_reset();
}

void setUp(void)
{
	m_arbitrary_local_gatt_data->len = m_arbitrary_sys_attr_len;
	m_arbitrary_local_gatt_data->flags = SYS_ATTR_BOTH;
	memcpy(m_arbitrary_local_gatt_data->data, m_arbitrary_sys_attr_data,
	       m_arbitrary_sys_attr_len); // lint !e419

	m_arbitrary_local_gatt_data_ref->len = m_arbitrary_sys_attr_len;
	m_arbitrary_local_gatt_data_ref->flags = SYS_ATTR_BOTH;
	memcpy(m_arbitrary_local_gatt_data_ref->data, m_arbitrary_sys_attr_data,
	       m_arbitrary_sys_attr_len); // lint !e419
	(void)gscm_init();
}

void test_init(void)
{
	ret_code_t err_code;

	err_code = gscm_init();
	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);
}

void test_pdb_evt_handler(void)
{
	pm_evt_t pdb_evt = {.evt_id = PM_EVT_PEER_DATA_UPDATE_SUCCEEDED,
			    .peer_id = m_arbitrary_peer_id,
			    .params = {.peer_data_update_succeeded = {
					       .data_id = PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING,
					       .action = PM_PEER_DATA_OP_UPDATE}}};

	// Start local_db_changed()
	pds_next_peer_id_get_ExpectAndReturn(PM_PEER_ID_INVALID, m_arbitrary_peer_id);
	pds_peer_data_store_ExpectAndReturn(m_arbitrary_peer_id, NULL, NULL, NRF_ERROR_BUSY);
	pds_peer_data_store_IgnoreArg_p_peer_data();
	pds_peer_data_store_IgnoreArg_p_store_token();
	gscm_local_database_has_changed();

	// Continue local_db_changed.
	pds_peer_data_store_ExpectAndReturn(m_arbitrary_peer_id, NULL, NULL, NRF_SUCCESS);
	pds_peer_data_store_IgnoreArg_p_peer_data();
	pds_peer_data_store_IgnoreArg_p_store_token();
	pds_next_peer_id_get_ExpectAndReturn(m_arbitrary_peer_id, m_arbitrary_peer_id);
	pds_peer_data_store_ExpectAndReturn(m_arbitrary_peer_id, NULL, NULL, NRF_SUCCESS);
	pds_peer_data_store_IgnoreArg_p_peer_data();
	pds_peer_data_store_IgnoreArg_p_store_token();
	pds_next_peer_id_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_ID_INVALID);

	gscm_pdb_evt_handler(&pdb_evt);
}

void test_gscm_local_db_cache_update(void)
{
	ret_code_t err_code;
	pm_peer_data_t returned_peer_data;
	returned_peer_data.data_id = PM_PEER_DATA_ID_GATT_LOCAL;
	returned_peer_data.p_local_gatt_db = m_arbitrary_local_gatt_data;

	pm_peer_data_flash_t stored_peer_data;
	stored_peer_data.data_id = PM_PEER_DATA_ID_GATT_LOCAL;
	stored_peer_data.p_local_gatt_db = m_arbitrary_local_gatt_data_ref;

	// Invalid conn_handle.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, PM_PEER_ID_INVALID);

	err_code = gscm_local_db_cache_update(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(BLE_ERROR_INVALID_CONN_HANDLE, err_code);

	// pdb_write_buf_get error
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 1,
					  &returned_peer_data, NRF_ERROR_BUSY);
	pdb_write_buf_get_IgnoreArg_p_peer_data();

	err_code = gscm_local_db_cache_update(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_BUSY, err_code);

	// sd_ble_gatts_sys_attr_get error - no room.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 1,
					  &returned_peer_data, NRF_SUCCESS);
	pdb_write_buf_get_IgnoreArg_p_peer_data();
	pdb_write_buf_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_get_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, &m_arbitrary_local_gatt_data->data[0], 0,
		&m_arbitrary_local_gatt_data->len, 1,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_ERROR_DATA_SIZE);
	pdb_write_buf_release_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      NRF_SUCCESS);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 2,
					  &returned_peer_data, NRF_SUCCESS);
	pdb_write_buf_get_IgnoreArg_p_peer_data();
	pdb_write_buf_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_get_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, &m_arbitrary_local_gatt_data->data[0], 0,
		&m_arbitrary_local_gatt_data->len, 1,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_ERROR_DATA_SIZE);
	pdb_write_buf_release_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      NRF_SUCCESS);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 3,
					  &returned_peer_data, NRF_ERROR_BUSY);
	pdb_write_buf_get_IgnoreArg_p_peer_data();

	err_code = gscm_local_db_cache_update(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_BUSY, err_code);

	// sd_ble_gatts_sys_attr_get error - too large.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 1,
					  &returned_peer_data, NRF_SUCCESS);
	pdb_write_buf_get_IgnoreArg_p_peer_data();
	pdb_write_buf_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_get_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, &m_arbitrary_local_gatt_data->data[0], 0,
		&m_arbitrary_local_gatt_data->len, 1,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_ERROR_DATA_SIZE);
	pdb_write_buf_release_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      NRF_SUCCESS);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 2,
					  &returned_peer_data, NRF_ERROR_INVALID_PARAM);
	pdb_write_buf_get_IgnoreArg_p_peer_data();

	err_code = gscm_local_db_cache_update(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_DATA_SIZE, err_code);

	// pdb_write_buf_store error
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 1,
					  &returned_peer_data, NRF_SUCCESS);
	pdb_write_buf_get_IgnoreArg_p_peer_data();
	pdb_write_buf_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_get_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, &m_arbitrary_local_gatt_data->data[0], 0,
		&m_arbitrary_local_gatt_data->len, 1,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_SUCCESS);
	m_arbitrary_local_gatt_data->data[0]++; // to be different from reference
	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, NULL,
					      NRF_SUCCESS);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&stored_peer_data);
	pdb_write_buf_store_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					    m_arbitrary_peer_id, NRF_ERROR_STORAGE_FULL);

	err_code = gscm_local_db_cache_update(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_STORAGE_FULL, err_code);
	m_arbitrary_local_gatt_data->data[0]--; // revert

	// pdb_peer_data_ptr_get error
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 1,
					  &returned_peer_data, NRF_SUCCESS);
	pdb_write_buf_get_IgnoreArg_p_peer_data();
	pdb_write_buf_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_get_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, &m_arbitrary_local_gatt_data->data[0], 0,
		&m_arbitrary_local_gatt_data->len, 1,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_SUCCESS);
	sd_ble_gatts_sys_attr_get_ReturnArrayThruPtr_p_sys_attr_data(m_arbitrary_sys_attr_data,
								     m_arbitrary_sys_attr_len);
	sd_ble_gatts_sys_attr_get_ReturnThruPtr_p_len(((uint16_t *)&m_arbitrary_sys_attr_len));

	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, NULL,
					      NRF_ERROR_INVALID_PARAM);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&stored_peer_data);

	err_code = gscm_local_db_cache_update(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INTERNAL, err_code);

	// Success - no sys attributes.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 1,
					  &returned_peer_data, NRF_SUCCESS);
	pdb_write_buf_get_IgnoreArg_p_peer_data();
	pdb_write_buf_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_get_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, &m_arbitrary_local_gatt_data->data[0], 0,
		&m_arbitrary_local_gatt_data->len, 1,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_ERROR_NOT_FOUND);
	pdb_write_buf_release_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      NRF_SUCCESS);

	err_code = gscm_local_db_cache_update(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);

	// Success - no sys attributes - no previous data.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 1,
					  &returned_peer_data, NRF_SUCCESS);
	pdb_write_buf_get_IgnoreArg_p_peer_data();
	pdb_write_buf_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_get_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, &m_arbitrary_local_gatt_data->data[0], 0,
		&m_arbitrary_local_gatt_data->len, 1,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_ERROR_NOT_FOUND);
	pdb_write_buf_release_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      NRF_SUCCESS);

	err_code = gscm_local_db_cache_update(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);

	// Success - large sys attr.
	m_arbitrary_local_gatt_data->len++; // to be different from reference
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 1,
					  &returned_peer_data, NRF_SUCCESS);
	pdb_write_buf_get_IgnoreArg_p_peer_data();
	pdb_write_buf_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_get_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, &m_arbitrary_local_gatt_data->data[0], 0,
		&m_arbitrary_local_gatt_data->len, 1,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_ERROR_DATA_SIZE);

	pdb_write_buf_release_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      NRF_SUCCESS);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 2,
					  &returned_peer_data, NRF_SUCCESS);
	pdb_write_buf_get_IgnoreArg_p_peer_data();
	pdb_write_buf_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_get_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, &m_arbitrary_local_gatt_data->data[0], 0,
		&m_arbitrary_local_gatt_data->len, 1,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_ERROR_DATA_SIZE);

	pdb_write_buf_release_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      NRF_SUCCESS);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 3,
					  &returned_peer_data, NRF_SUCCESS);
	pdb_write_buf_get_IgnoreArg_p_peer_data();
	pdb_write_buf_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_get_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, &m_arbitrary_local_gatt_data->data[0], 0,
		&m_arbitrary_local_gatt_data->len, 1,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_SUCCESS);

	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, NULL,
					      NRF_SUCCESS);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&stored_peer_data);
	pdb_write_buf_store_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					    m_arbitrary_peer_id, NRF_SUCCESS);
	err_code = gscm_local_db_cache_update(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);
	m_arbitrary_local_gatt_data->len--; // revert

	// @TODO: Handle all stack errors (return NRF_ERROR_INTERNAL?)

	// Success
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 1,
					  &returned_peer_data, NRF_SUCCESS);
	pdb_write_buf_get_IgnoreArg_p_peer_data();
	pdb_write_buf_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_get_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, &m_arbitrary_local_gatt_data->data[0], 0,
		&m_arbitrary_local_gatt_data->len, 1,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_SUCCESS);
	sd_ble_gatts_sys_attr_get_ReturnArrayThruPtr_p_sys_attr_data(m_arbitrary_sys_attr_data,
								     m_arbitrary_sys_attr_len);
	sd_ble_gatts_sys_attr_get_ReturnThruPtr_p_len(((uint16_t *)&m_arbitrary_sys_attr_len));

	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, NULL,
					      NRF_ERROR_NOT_FOUND);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&stored_peer_data);
	pdb_write_buf_store_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					    m_arbitrary_peer_id, NRF_SUCCESS);

	err_code = gscm_local_db_cache_update(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);
	TEST_ASSERT_EQUAL_UINT(
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		m_arbitrary_local_gatt_data->flags);
	TEST_ASSERT_EQUAL_UINT(m_arbitrary_sys_attr_len, m_arbitrary_local_gatt_data->len);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(m_arbitrary_sys_attr_data, m_arbitrary_local_gatt_data->data,
				      m_arbitrary_sys_attr_len);

	// Success - no update
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_write_buf_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, 1,
					  &returned_peer_data, NRF_SUCCESS);
	pdb_write_buf_get_IgnoreArg_p_peer_data();
	pdb_write_buf_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_get_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, &m_arbitrary_local_gatt_data->data[0], 0,
		&m_arbitrary_local_gatt_data->len, 1,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_SUCCESS);
	sd_ble_gatts_sys_attr_get_ReturnArrayThruPtr_p_sys_attr_data(m_arbitrary_sys_attr_data,
								     m_arbitrary_sys_attr_len);
	sd_ble_gatts_sys_attr_get_ReturnThruPtr_p_len(((uint16_t *)&m_arbitrary_sys_attr_len));

	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL, NULL,
					      NRF_SUCCESS);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&stored_peer_data);
	pdb_write_buf_release_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      NRF_SUCCESS);

	err_code = gscm_local_db_cache_update(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INVALID_DATA, err_code);
	TEST_ASSERT_EQUAL_UINT(
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		m_arbitrary_local_gatt_data->flags);
	TEST_ASSERT_EQUAL_UINT(m_arbitrary_sys_attr_len, m_arbitrary_local_gatt_data->len);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(m_arbitrary_sys_attr_data, m_arbitrary_local_gatt_data->data,
				      m_arbitrary_sys_attr_len);
}

void test_gscm_local_db_cache_apply(void)
{
	ret_code_t err_code;
	pm_peer_data_flash_t returned_peer_data;
	pm_peer_data_local_gatt_db_t const *p_local_db_data;

	returned_peer_data.data_id = PM_PEER_DATA_ID_GATT_LOCAL;
	returned_peer_data.p_local_gatt_db = m_arbitrary_local_gatt_data;
	p_local_db_data = returned_peer_data.p_local_gatt_db;

	// Not bonded.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, PM_PEER_ID_INVALID);
	sd_ble_gatts_sys_attr_set_ExpectAndReturn(
		m_arbitrary_conn_handle, NULL, 0,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_SUCCESS);

	err_code = gscm_local_db_cache_apply(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);

	// data not found in cache.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      &returned_peer_data, NRF_ERROR_NOT_FOUND);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	sd_ble_gatts_sys_attr_set_ExpectAndReturn(
		m_arbitrary_conn_handle, NULL, 0,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_SUCCESS);

	err_code = gscm_local_db_cache_apply(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);

	// Invalid connection state
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      &returned_peer_data, NRF_SUCCESS);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_set_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, p_local_db_data->data, p_local_db_data->len,
		p_local_db_data->len, p_local_db_data->flags, NRF_ERROR_INVALID_STATE);

	err_code = gscm_local_db_cache_apply(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);

	// Stack busy
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      &returned_peer_data, NRF_SUCCESS);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_set_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, p_local_db_data->data, p_local_db_data->len,
		p_local_db_data->len, p_local_db_data->flags, NRF_ERROR_BUSY);

	err_code = gscm_local_db_cache_apply(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_BUSY, err_code);

	// Stack busy (NO_MEM)
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      &returned_peer_data, NRF_SUCCESS);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_set_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, p_local_db_data->data, p_local_db_data->len,
		p_local_db_data->len, p_local_db_data->flags, NRF_ERROR_NO_MEM);

	err_code = gscm_local_db_cache_apply(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_BUSY, err_code);

	// Invalid data -> DB has changed. Still invalid. Applied nothing.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      &returned_peer_data, NRF_SUCCESS);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_set_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, p_local_db_data->data, p_local_db_data->len,
		p_local_db_data->len, p_local_db_data->flags, NRF_ERROR_INVALID_DATA);
	sd_ble_gatts_sys_attr_set_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, p_local_db_data->data, p_local_db_data->len,
		p_local_db_data->len, BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS, NRF_ERROR_INVALID_DATA);
	sd_ble_gatts_sys_attr_set_ExpectAndReturn(
		m_arbitrary_conn_handle, NULL, 0,
		(BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS),
		NRF_SUCCESS);

	err_code = gscm_local_db_cache_apply(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INVALID_DATA, err_code);

	// Invalid data -> DB has changed. Applied system part.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      &returned_peer_data, NRF_SUCCESS);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_set_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, p_local_db_data->data, p_local_db_data->len,
		p_local_db_data->len, p_local_db_data->flags, NRF_ERROR_INVALID_DATA);
	sd_ble_gatts_sys_attr_set_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, p_local_db_data->data, p_local_db_data->len,
		p_local_db_data->len, BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS, NRF_SUCCESS);

	err_code = gscm_local_db_cache_apply(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INVALID_DATA, err_code);

	// @TODO: Handle all stack errors (return NRF_ERROR_INTERNAL?)

	// Success
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_GATT_LOCAL,
					      &returned_peer_data, NRF_SUCCESS);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	sd_ble_gatts_sys_attr_set_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, p_local_db_data->data, p_local_db_data->len,
		p_local_db_data->len, p_local_db_data->flags, NRF_SUCCESS);

	err_code = gscm_local_db_cache_apply(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);

	// No peer ID.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, PM_PEER_ID_INVALID);
	sd_ble_gatts_sys_attr_set_ExpectAndReturn(m_arbitrary_conn_handle, NULL, 0, SYS_ATTR_BOTH,
						  NRF_SUCCESS);

	err_code = gscm_local_db_cache_apply(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);
}

static bool m_expected_sc_store_state;
static int m_n_sc_store_calls;

ret_code_t pds_peer_data_store_stub(pm_peer_id_t peer_id, pm_peer_data_const_t const *p_peer_data,
				    pm_store_token_t *p_store_token, int numCalls)
{
	if ((numCalls + 1) < m_n_sc_store_calls) {
		TEST_ASSERT_EQUAL_UINT(m_arbitrary_peer_id, peer_id);
	} else if ((numCalls + 1) == m_n_sc_store_calls) {
		TEST_ASSERT_EQUAL_UINT(PM_PEER_ID_INVALID, peer_id);
	} else {
		TEST_ASSERT(false);
	}
	TEST_ASSERT_EQUAL_UINT(PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING, p_peer_data->data_id);
	TEST_ASSERT_EQUAL(m_expected_sc_store_state, *p_peer_data->p_service_changed_pending);
	TEST_ASSERT_NULL(p_store_token);

	return NRF_SUCCESS;
}

void test_gscm_local_database_has_changed1(void)
{
	m_expected_sc_store_state = true;

	// Busy
	pds_next_peer_id_get_ExpectAndReturn(PM_PEER_ID_INVALID, m_arbitrary_peer_id);
	pds_peer_data_store_ExpectAndReturn(m_arbitrary_peer_id, NULL, NULL, NRF_SUCCESS);
	pds_peer_data_store_IgnoreArg_p_peer_data();
	pds_peer_data_store_IgnoreArg_p_store_token();

	for (uint16_t i = 0; i < 3; i++) {
		pds_next_peer_id_get_ExpectAndReturn(m_arbitrary_peer_id, m_arbitrary_peer_id);
		pds_peer_data_store_ExpectAndReturn(m_arbitrary_peer_id, NULL, NULL, NRF_SUCCESS);
		pds_peer_data_store_IgnoreArg_p_peer_data();
		pds_peer_data_store_IgnoreArg_p_store_token();
	}

	pds_next_peer_id_get_ExpectAndReturn(m_arbitrary_peer_id, m_arbitrary_peer_id);
	pds_peer_data_store_ExpectAndReturn(m_arbitrary_peer_id, NULL, NULL, NRF_ERROR_BUSY);
	pds_peer_data_store_IgnoreArg_p_peer_data();
	pds_peer_data_store_IgnoreArg_p_store_token();

	gscm_local_database_has_changed();

	TEST_ASSERT_EQUAL_UINT(0, m_n_evt_handler_calls);

	// STORAGE_FULL
	pds_next_peer_id_get_ExpectAndReturn(PM_PEER_ID_INVALID, m_arbitrary_peer_id);
	pds_peer_data_store_ExpectAndReturn(m_arbitrary_peer_id, NULL, NULL,
					    NRF_ERROR_STORAGE_FULL);
	pds_peer_data_store_IgnoreArg_p_peer_data();
	pds_peer_data_store_IgnoreArg_p_store_token();
	im_conn_handle_get_ExpectAndReturn(m_arbitrary_peer_id, m_arbitrary_conn_handle);

	gscm_local_database_has_changed();

	TEST_ASSERT_EQUAL_UINT(1, m_n_evt_handler_calls);
	TEST_ASSERT_EQUAL_UINT(m_arbitrary_conn_handle, m_evt_handler_records[0].conn_handle);
	TEST_ASSERT_EQUAL_UINT(m_arbitrary_peer_id, m_evt_handler_records[0].peer_id);
	TEST_ASSERT_EQUAL_UINT(PM_EVT_STORAGE_FULL, m_evt_handler_records[0].evt_id);
	evt_handler_call_record_clear();

	// other error
	pds_next_peer_id_get_ExpectAndReturn(PM_PEER_ID_INVALID, m_arbitrary_peer_id);
	pds_peer_data_store_ExpectAndReturn(m_arbitrary_peer_id, NULL, NULL, NRF_ERROR_INTERNAL);
	pds_peer_data_store_IgnoreArg_p_peer_data();
	pds_peer_data_store_IgnoreArg_p_store_token();
	im_conn_handle_get_ExpectAndReturn(m_arbitrary_peer_id, m_arbitrary_conn_handle);

	gscm_local_database_has_changed();

	TEST_ASSERT_EQUAL_UINT(1, m_n_evt_handler_calls);
	TEST_ASSERT_EQUAL_UINT(m_arbitrary_conn_handle, m_evt_handler_records[0].conn_handle);
	TEST_ASSERT_EQUAL_UINT(m_arbitrary_peer_id, m_evt_handler_records[0].peer_id);
	TEST_ASSERT_EQUAL_UINT(PM_EVT_ERROR_UNEXPECTED, m_evt_handler_records[0].evt_id);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INTERNAL,
			       m_evt_handler_records[0].params.error_unexpected.error);
	evt_handler_call_record_clear();
}

void test_gscm_local_database_has_changed2(void)
{
	m_expected_sc_store_state = true;
	m_n_sc_store_calls = 5;

	// (start over and) Finish completely.
	pds_next_peer_id_get_ExpectAndReturn(PM_PEER_ID_INVALID, m_arbitrary_peer_id);
	pds_peer_data_store_StubWithCallback(pds_peer_data_store_stub);

	for (uint16_t i = 0; i < 3; i++) {
		pds_next_peer_id_get_ExpectAndReturn(m_arbitrary_peer_id, m_arbitrary_peer_id);
		pds_peer_data_store_StubWithCallback(pds_peer_data_store_stub);
	}

	pds_next_peer_id_get_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_ID_INVALID);
	pds_peer_data_store_StubWithCallback(pds_peer_data_store_stub);

	gscm_local_database_has_changed();
}

void test_gscm_service_changed_ind_needed(void)
{
	bool service_changed = true;
	// lint -save -e65 -e64
	pm_peer_data_flash_t returned_peer_data = {.data_id =
							   PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING,
						   .p_service_changed_pending = &service_changed};
	// lint -restore

	// No peer ID.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, PM_PEER_ID_INVALID);
	pdb_peer_data_ptr_get_ExpectAndReturn(PM_PEER_ID_INVALID,
					      PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING,
					      &returned_peer_data, NRF_ERROR_INVALID_PARAM);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();

	bool sc_needed = gscm_service_changed_ind_needed(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(false, sc_needed);

	// No data.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id,
					      PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING,
					      &returned_peer_data, NRF_ERROR_NOT_FOUND);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();

	sc_needed = gscm_service_changed_ind_needed(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(false, sc_needed);

	// Success true
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id,
					      PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING,
					      &returned_peer_data, NRF_SUCCESS);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&returned_peer_data);

	sc_needed = gscm_service_changed_ind_needed(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(true, sc_needed);

	// Success false
	service_changed = false;
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id,
					      PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING,
					      &returned_peer_data, NRF_SUCCESS);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&returned_peer_data);

	sc_needed = gscm_service_changed_ind_needed(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(false, sc_needed);

	(void)service_changed;
}

void test_gscm_service_changed_ind_send(void)
{
	ret_code_t err_code;

	// user_handle error.
	sd_ble_gatts_initial_user_handle_get_ExpectAndReturn(NULL, NRF_ERROR_INVALID_ADDR);
	sd_ble_gatts_initial_user_handle_get_IgnoreArg_p_handle();

	err_code = gscm_service_changed_ind_send(BLE_CONN_HANDLE_INVALID);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INTERNAL, err_code);

	// Invalid connection handle.
	sd_ble_gatts_initial_user_handle_get_ExpectAndReturn(NULL, NRF_SUCCESS);
	sd_ble_gatts_initial_user_handle_get_IgnoreArg_p_handle();
	sd_ble_gatts_service_changed_ExpectAndReturn(BLE_CONN_HANDLE_INVALID, 0x0000, 0xFFFF,
						     BLE_ERROR_INVALID_CONN_HANDLE);

	err_code = gscm_service_changed_ind_send(BLE_CONN_HANDLE_INVALID);
	TEST_ASSERT_EQUAL_UINT(BLE_ERROR_INVALID_CONN_HANDLE, err_code);

	// Invalid attribute handles. Will reattempt until valid. Will also look for SC CCCD handle.
	sd_ble_gatts_initial_user_handle_get_ExpectAndReturn(NULL, NRF_SUCCESS);
	sd_ble_gatts_initial_user_handle_get_IgnoreArg_p_handle();

	// Attempt 1.
	sd_ble_gatts_service_changed_ExpectAndReturn(m_arbitrary_conn_handle, 0x0000, 0xFFFF,
						     BLE_ERROR_INVALID_ATTR_HANDLE);

	// Attempt 2.
	sd_ble_gatts_service_changed_ExpectAndReturn(m_arbitrary_conn_handle, 0x0001, 0xFFFF,
						     BLE_ERROR_INVALID_ATTR_HANDLE);

	// Attempt 3.
	sd_ble_gatts_service_changed_ExpectAndReturn(m_arbitrary_conn_handle, 0x0002, 0xFFFF,
						     BLE_ERROR_INVALID_ATTR_HANDLE);

	// Attempt 4: Success.
	sd_ble_gatts_service_changed_ExpectAndReturn(m_arbitrary_conn_handle, 0x0003, 0xFFFF,
						     NRF_SUCCESS);

	err_code = gscm_service_changed_ind_send(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);

	// Busy
	sd_ble_gatts_initial_user_handle_get_ExpectAndReturn(NULL, NRF_SUCCESS);
	sd_ble_gatts_initial_user_handle_get_IgnoreArg_p_handle();
	sd_ble_gatts_service_changed_ExpectAndReturn(m_arbitrary_conn_handle, 0x0000, 0xFFFF,
						     NRF_ERROR_BUSY);
	sd_ble_gatts_service_changed_IgnoreArg_start_handle();

	err_code = gscm_service_changed_ind_send(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_BUSY, err_code);

	// System attributes missing.
	sd_ble_gatts_initial_user_handle_get_ExpectAndReturn(NULL, NRF_SUCCESS);
	sd_ble_gatts_initial_user_handle_get_IgnoreArg_p_handle();
	sd_ble_gatts_service_changed_ExpectAndReturn(m_arbitrary_conn_handle, 0x0000, 0xFFFF,
						     BLE_ERROR_GATTS_SYS_ATTR_MISSING);
	sd_ble_gatts_service_changed_IgnoreArg_start_handle();

	err_code = gscm_service_changed_ind_send(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(BLE_ERROR_GATTS_SYS_ATTR_MISSING, err_code);

	// CCCD not set.
	sd_ble_gatts_initial_user_handle_get_ExpectAndReturn(NULL, NRF_SUCCESS);
	sd_ble_gatts_initial_user_handle_get_IgnoreArg_p_handle();
	sd_ble_gatts_service_changed_ExpectAndReturn(m_arbitrary_conn_handle, 0x0000, 0xFFFF,
						     NRF_ERROR_INVALID_STATE);
	sd_ble_gatts_service_changed_IgnoreArg_start_handle();

	err_code = gscm_service_changed_ind_send(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INVALID_STATE, err_code);

	// Service changed characteristic not present
	sd_ble_gatts_initial_user_handle_get_ExpectAndReturn(NULL, NRF_SUCCESS);
	sd_ble_gatts_initial_user_handle_get_IgnoreArg_p_handle();
	sd_ble_gatts_service_changed_ExpectAndReturn(m_arbitrary_conn_handle, 0x0000, 0xFFFF,
						     NRF_ERROR_NOT_SUPPORTED);
	sd_ble_gatts_service_changed_IgnoreArg_start_handle();

	err_code = gscm_service_changed_ind_send(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_NOT_SUPPORTED, err_code);

	// Success
	sd_ble_gatts_initial_user_handle_get_ExpectAndReturn(NULL, NRF_SUCCESS);
	sd_ble_gatts_initial_user_handle_get_IgnoreArg_p_handle();
	sd_ble_gatts_service_changed_ExpectAndReturn(m_arbitrary_conn_handle, 0x0000, 0xFFFF,
						     NRF_SUCCESS);
	sd_ble_gatts_service_changed_IgnoreArg_start_handle();

	err_code = gscm_service_changed_ind_send(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);
}

void test_gscm_db_change_notification_done(void)
{
	m_expected_sc_store_state = false;
	m_n_sc_store_calls += 2; // Expect 1 more.

	pds_peer_data_store_StubWithCallback(pds_peer_data_store_stub);

	gscm_db_change_notification_done(m_arbitrary_peer_id);
}
