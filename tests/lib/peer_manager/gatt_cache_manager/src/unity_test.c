
#define NRF_SDH_BLE_SERVICE_CHANGED 1
#include "unity.h"
#include "mock_nrf_mtx.h"
#include "gatt_cache_manager.c"

#include <string.h>
#include "mock_ble_gap.h"
#include "mock_ble_gattc.h"
#include "mock_ble_gatts.h"
#include "mock_id_manager.h"
#include "mock_security_dispatcher.h"
#include "mock_ble_conn_state.h"
#include "mock_gatts_cache_manager.h"
#include "mock_peer_database.h"
#include "mock_peer_data_storage.h"
#include "peer_manager_types.h"

#define MAX_EVT_HANDLER_CALLS 20

static uint16_t m_arbitrary_conn_handle = 63;
static uint16_t m_arbitrary_conn_handle2 = 92;
static pm_peer_id_t m_arbitrary_peer_id = 7;
static ble_conn_state_user_flag_id_t m_arbitrary_flag_id_update = BLE_CONN_STATE_USER_FLAG1;
static ble_conn_state_user_flag_id_t m_arbitrary_flag_id_apply = BLE_CONN_STATE_USER_FLAG2;
static ble_conn_state_user_flag_id_t m_arbitrary_flag_id_sc = BLE_CONN_STATE_USER_FLAG3;
static ble_conn_state_user_flag_id_t m_arbitrary_flag_id_sc_sent = BLE_CONN_STATE_USER_FLAG4;
static ble_conn_state_user_flag_id_t m_arbitrary_flag_id_car_upd = BLE_CONN_STATE_USER_FLAG5;
static ble_conn_state_user_flag_id_t m_arbitrary_flag_id_car_hdl = BLE_CONN_STATE_USER_FLAG6;
static ble_conn_state_user_flag_id_t m_arbitrary_flag_id_car_val = BLE_CONN_STATE_USER_FLAG7;
static uint16_t m_arbitrary_handle = 9;

static uint32_t m_n_evt_handler_calls;
static pm_evt_t m_evt_handler_records[MAX_EVT_HANDLER_CALLS];

void evt_handler_call_record_clear(void)
{
	m_n_evt_handler_calls = 0;
}

void pm_gcm_evt_handler(pm_evt_t *event)
{
	m_evt_handler_records[m_n_evt_handler_calls++] = *event;
}

void tearDown(void)
{
	internal_state_reset();
}

void setUp(void)
{
	evt_handler_call_record_clear();
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_update);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_apply);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_sc);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_sc_sent);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_car_upd);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_car_hdl);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_car_val);
	nrf_mtx_init_Expect(&m_db_update_in_progress_mutex);

	ret_code_t err_code = gcm_init();
	TEST_ASSERT_EQUAL(NRF_SUCCESS, err_code);
}

void ble_evt_init(ble_evt_t *p_ble_evt, uint8_t evt_id, uint16_t conn_handle)
{
	memset(p_ble_evt, 0, sizeof(ble_evt_t));
	p_ble_evt->header.evt_id = evt_id;
	p_ble_evt->evt.gatts_evt.conn_handle = conn_handle;
	p_ble_evt->header.evt_len = sizeof(ble_gatts_evt_t);
}

void sys_attr_missing_evt_constuct(ble_evt_t *p_ble_evt, uint16_t conn_handle)
{
	ble_evt_init(p_ble_evt, BLE_GATTS_EVT_SYS_ATTR_MISSING, conn_handle);
	p_ble_evt->evt.gatts_evt.params.sys_attr_missing.hint = 0;
}

void sc_confirm_evt_constuct(ble_evt_t *p_ble_evt, uint16_t conn_handle)
{
	ble_evt_init(p_ble_evt, BLE_GATTS_EVT_SC_CONFIRM, conn_handle);
}

void write_evt_constuct(ble_evt_t *p_ble_evt, uint16_t conn_handle)
{
	ble_evt_init(p_ble_evt, BLE_GATTS_EVT_WRITE, conn_handle);
	p_ble_evt->evt.gatts_evt.params.write.op = BLE_GATTS_OP_WRITE_REQ;
	p_ble_evt->evt.gatts_evt.params.write.offset = 0;
	p_ble_evt->evt.gatts_evt.params.write.uuid.type = BLE_UUID_TYPE_BLE;
	p_ble_evt->evt.gatts_evt.params.write.uuid.uuid = BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG;
	p_ble_evt->evt.gatts_evt.params.write.len = 2;
}

void read_by_uuid_evt_constuct(ble_evt_t *p_ble_evt, uint16_t conn_handle, uint16_t gatt_status)
{
	ble_evt_init(p_ble_evt, BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP, conn_handle);
	p_ble_evt->evt.gattc_evt.gatt_status = gatt_status;
	p_ble_evt->evt.gattc_evt.params.char_val_by_uuid_read_rsp.count = 1;
	p_ble_evt->evt.gattc_evt.params.char_val_by_uuid_read_rsp.value_len = 1;
	*(uint16_t *)&p_ble_evt->evt.gattc_evt.params.char_val_by_uuid_read_rsp.handle_value[0] =
		m_arbitrary_handle;
}

void read_evt_constuct(ble_evt_t *p_ble_evt, uint16_t conn_handle, uint16_t gatt_status,
		       uint8_t value)
{
	ble_evt_init(p_ble_evt, BLE_GATTC_EVT_READ_RSP, conn_handle);
	p_ble_evt->evt.gattc_evt.gatt_status = gatt_status;
	p_ble_evt->evt.gattc_evt.params.read_rsp.data[0] = value;
	p_ble_evt->evt.gattc_evt.params.read_rsp.handle = m_arbitrary_handle;
	p_ble_evt->evt.gattc_evt.params.read_rsp.len = 1;
	p_ble_evt->evt.gattc_evt.params.read_rsp.offset = 0;
}

void dummy_evt_constuct(ble_evt_t *p_ble_evt, uint16_t conn_handle)
{
	ble_evt_init(p_ble_evt, BLE_GATTC_EVT_HVX, m_arbitrary_conn_handle);
}

ble_evt_t *sys_attr_missing_evt(uint16_t conn_handle)
{
	static ble_evt_t ble_evt;
	sys_attr_missing_evt_constuct(&ble_evt, conn_handle);
	return &ble_evt;
}

ble_evt_t *sc_confirm_evt(uint16_t conn_handle)
{
	static ble_evt_t ble_evt;
	sc_confirm_evt_constuct(&ble_evt, conn_handle);
	return &ble_evt;
}

ble_evt_t *write_evt(uint16_t conn_handle)
{
	static ble_evt_t ble_evt;
	write_evt_constuct(&ble_evt, conn_handle);
	return &ble_evt;
}

ble_evt_t *read_by_uuid_evt(uint16_t conn_handle, uint16_t gatt_status)
{
	static ble_evt_t ble_evt;
	read_by_uuid_evt_constuct(&ble_evt, conn_handle, gatt_status);
	return &ble_evt;
}

ble_evt_t *read_evt(uint16_t conn_handle, uint16_t gatt_status, uint8_t value)
{
	static ble_evt_t ble_evt;
	read_evt_constuct(&ble_evt, conn_handle, gatt_status, value);
	return &ble_evt;
}

ble_evt_t *dummy_evt(uint16_t conn_handle)
{
	static ble_evt_t ble_evt;
	dummy_evt_constuct(&ble_evt, conn_handle);
	return &ble_evt;
}

uint32_t n_calls[4];

static uint32_t
ble_conn_state_for_each_set_user_flag_stub(ble_conn_state_user_flag_id_t flag_id,
					   ble_conn_state_user_function_t user_function,
					   void *p_context, int num_calls)
{
	TEST_ASSERT(num_calls < 3);
	TEST_ASSERT_NULL(p_context);
	for (uint16_t i = 0; i < n_calls[num_calls]; i++) {
		user_function(i, p_context);
	}
	return n_calls[num_calls];
}

/**@brief function for verifying that the update flags have been checked.
 *
 * @return The number of callbacks expected from this.
 */
uint16_t update_flags_check_test(uint16_t n_expected_calls, uint32_t n_call)
{
	uint16_t conn_handle;

	n_calls[n_call] = 8;
	ble_conn_state_for_each_set_user_flag_StubWithCallback(
		ble_conn_state_for_each_set_user_flag_stub);

	conn_handle = 0;
	nrf_mtx_trylock_ExpectAndReturn(&m_db_update_in_progress_mutex, false);

	conn_handle = 1;
	nrf_mtx_trylock_ExpectAndReturn(&m_db_update_in_progress_mutex, true);
	gscm_local_db_cache_update_ExpectAndReturn(conn_handle, NRF_ERROR_BUSY);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_update, true);
	nrf_mtx_unlock_Expect(&m_db_update_in_progress_mutex);

	conn_handle = 2;
	nrf_mtx_trylock_ExpectAndReturn(&m_db_update_in_progress_mutex, true);
	gscm_local_db_cache_update_ExpectAndReturn(conn_handle, BLE_ERROR_INVALID_CONN_HANDLE);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_update, false);
	nrf_mtx_unlock_Expect(&m_db_update_in_progress_mutex);

	conn_handle = 3;
	nrf_mtx_trylock_ExpectAndReturn(&m_db_update_in_progress_mutex, true);
	gscm_local_db_cache_update_ExpectAndReturn(conn_handle, NRF_ERROR_INVALID_DATA);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_update, false);
	nrf_mtx_unlock_Expect(&m_db_update_in_progress_mutex);

	conn_handle = 4;
	nrf_mtx_trylock_ExpectAndReturn(&m_db_update_in_progress_mutex, true);
	gscm_local_db_cache_update_ExpectAndReturn(conn_handle, NRF_ERROR_DATA_SIZE);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(conn_handle, m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_update, false);
	nrf_mtx_unlock_Expect(&m_db_update_in_progress_mutex);
	n_expected_calls++;

	conn_handle = 5;
	nrf_mtx_trylock_ExpectAndReturn(&m_db_update_in_progress_mutex, true);
	gscm_local_db_cache_update_ExpectAndReturn(conn_handle, NRF_ERROR_STORAGE_FULL);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(conn_handle, m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_update, false);
	nrf_mtx_unlock_Expect(&m_db_update_in_progress_mutex);
	n_expected_calls++;

	conn_handle = 6;
	nrf_mtx_trylock_ExpectAndReturn(&m_db_update_in_progress_mutex, true);
	gscm_local_db_cache_update_ExpectAndReturn(
		conn_handle, NRF_ERROR_INVALID_STATE); // arbitrary unexpected error.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(conn_handle, m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_update, false);
	nrf_mtx_unlock_Expect(&m_db_update_in_progress_mutex);
	n_expected_calls++;

	conn_handle = 7;
	nrf_mtx_trylock_ExpectAndReturn(&m_db_update_in_progress_mutex, true);
	gscm_local_db_cache_update_ExpectAndReturn(conn_handle, NRF_SUCCESS);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_update, false);

	return n_expected_calls;
}

ret_code_t sd_ble_gatts_value_get_success_stub(uint16_t conn_handle, uint16_t handle,
					       ble_gatts_value_t *p_value, int numCalls,
					       uint16_t cccd_value_in)
{
	TEST_ASSERT_NOT_NULL(p_value);
	TEST_ASSERT_NOT_NULL(p_value->p_value);
	TEST_ASSERT_EQUAL_UINT(p_value->len, 2);
	TEST_ASSERT_EQUAL_UINT(p_value->offset, 0);

	if (handle == (m_arbitrary_handle - 1)) {
		*(uint16_t *)p_value->p_value = cccd_value_in;
		return NRF_SUCCESS;
	} else {
		TEST_ASSERT_TRUE(false); // Shouldn't come here.
		return NRF_ERROR_INTERNAL;
	}
}

ret_code_t sd_ble_gatts_attr_get_success_stub(uint16_t handle, ble_uuid_t *p_uuid,
					      ble_gatts_attr_md_t *p_md, int numCalls)
{
	TEST_ASSERT_NOT_NULL(p_uuid);
	TEST_ASSERT_NULL(p_md);

	if (handle < (m_arbitrary_handle - 2)) {
		p_uuid->uuid = 1; // arbitrary
		return NRF_SUCCESS;
	} else if (handle == (m_arbitrary_handle - 2)) {
		p_uuid->uuid = BLE_UUID_GATT_CHARACTERISTIC_SERVICE_CHANGED;
		return NRF_SUCCESS;
	} else if (handle == (m_arbitrary_handle - 1)) {
		p_uuid->uuid = BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG;
		return NRF_SUCCESS;
	} else {
		TEST_ASSERT_TRUE(false); // Shouldn't come here.
		return NRF_ERROR_INTERNAL;
	}
}

ret_code_t sd_ble_gatts_value_get_success_stub_ind_on(uint16_t conn_handle, uint16_t handle,
						      ble_gatts_value_t *p_value, int numCalls)
{
	return sd_ble_gatts_value_get_success_stub(conn_handle, handle, p_value, numCalls, 2);
}

ret_code_t sd_ble_gatts_value_get_success_stub_ind_off(uint16_t conn_handle, uint16_t handle,
						       ble_gatts_value_t *p_value, int numCalls)
{
	return sd_ble_gatts_value_get_success_stub(conn_handle, handle, p_value, numCalls, 0);
}

/**@brief function for verifying that the service changed flags have been checked.
 *
 * @return The number of callbacks expected from this.
 */
uint16_t service_changed_flags_check_test(uint16_t n_expected_calls, uint32_t n_call)
{
	uint16_t conn_handle;

	n_calls[n_call] = 6;
	ble_conn_state_for_each_set_user_flag_StubWithCallback(
		ble_conn_state_for_each_set_user_flag_stub);

	// Success.
	conn_handle = 0;
	ble_conn_state_user_flag_get_ExpectAndReturn(conn_handle, m_arbitrary_flag_id_sc_sent,
						     false);
	gscm_service_changed_ind_send_ExpectAndReturn(conn_handle, NRF_SUCCESS);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(conn_handle, m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_sc, true);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_sc_sent, true);
	n_expected_calls++;

	// SD Busy
	conn_handle = 1;
	ble_conn_state_user_flag_get_ExpectAndReturn(conn_handle, m_arbitrary_flag_id_sc_sent,
						     false);
	gscm_service_changed_ind_send_ExpectAndReturn(conn_handle, NRF_ERROR_BUSY);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_sc, true);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_sc_sent, false);

	// Disconnect.
	conn_handle = 2;
	ble_conn_state_user_flag_get_ExpectAndReturn(conn_handle, m_arbitrary_flag_id_sc_sent,
						     false);
	gscm_service_changed_ind_send_ExpectAndReturn(conn_handle, BLE_ERROR_INVALID_CONN_HANDLE);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_sc, true);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_sc_sent, false);

	// Sys attributes missing
	conn_handle = 3;
	ble_conn_state_user_flag_get_ExpectAndReturn(conn_handle, m_arbitrary_flag_id_sc_sent,
						     false);
	gscm_service_changed_ind_send_ExpectAndReturn(conn_handle,
						      BLE_ERROR_GATTS_SYS_ATTR_MISSING);
	gscm_local_db_cache_apply_ExpectAndReturn(conn_handle, NRF_SUCCESS);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(conn_handle, m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_apply, false);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_sc, true);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_sc_sent, false);
	n_expected_calls++;

	// Unexpected error.
	conn_handle = 4;
	ble_conn_state_user_flag_get_ExpectAndReturn(conn_handle, m_arbitrary_flag_id_sc_sent,
						     false);
	gscm_service_changed_ind_send_ExpectAndReturn(
		conn_handle, NRF_ERROR_FORBIDDEN); // arbitrary unexpected error.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(conn_handle, m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_sc, true);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_sc_sent, false);
	n_expected_calls++;

	// CCCD not set.
	conn_handle = 5;
	ble_conn_state_user_flag_get_ExpectAndReturn(conn_handle, m_arbitrary_flag_id_sc_sent,
						     false);
	gscm_service_changed_ind_send_ExpectAndReturn(conn_handle, NRF_ERROR_INVALID_STATE);
	sd_ble_gatts_initial_user_handle_get_ExpectAndReturn(&m_arbitrary_handle, NRF_SUCCESS);
	sd_ble_gatts_initial_user_handle_get_IgnoreArg_p_handle();
	sd_ble_gatts_initial_user_handle_get_ReturnThruPtr_p_handle(&m_arbitrary_handle);
	sd_ble_gatts_value_get_StubWithCallback(sd_ble_gatts_value_get_success_stub_ind_off);
	sd_ble_gatts_attr_get_StubWithCallback(sd_ble_gatts_attr_get_success_stub);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(conn_handle, m_arbitrary_peer_id);
	gscm_db_change_notification_done_Expect(m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_sc, false);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_sc_sent, false);

	return n_expected_calls;
}

uint16_t service_changed_flags_check_test_2(uint16_t n_expected_calls, uint32_t n_call)
{
	n_calls[n_call] = 2;
	ble_conn_state_for_each_set_user_flag_StubWithCallback(
		ble_conn_state_for_each_set_user_flag_stub);

	// ATT_MTU exchange in progress. See retval docs for sd_*_service_changed()
	uint16_t conn_handle = 0;
	ble_conn_state_user_flag_get_ExpectAndReturn(conn_handle, m_arbitrary_flag_id_sc_sent,
						     false);
	gscm_service_changed_ind_send_ExpectAndReturn(conn_handle, NRF_ERROR_INVALID_STATE);
	sd_ble_gatts_initial_user_handle_get_ExpectAndReturn(&m_arbitrary_handle, NRF_SUCCESS);
	sd_ble_gatts_initial_user_handle_get_IgnoreArg_p_handle();
	sd_ble_gatts_initial_user_handle_get_ReturnThruPtr_p_handle(&m_arbitrary_handle);
	sd_ble_gatts_value_get_StubWithCallback(sd_ble_gatts_value_get_success_stub_ind_on);
	sd_ble_gatts_attr_get_StubWithCallback(sd_ble_gatts_attr_get_success_stub);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_sc, true);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_sc_sent, false);

	// Already sent.
	conn_handle = 1;
	ble_conn_state_user_flag_get_ExpectAndReturn(conn_handle, m_arbitrary_flag_id_sc_sent,
						     true);

	return n_expected_calls;
}

void test_service_changed_cccd(void)
{
	uint16_t cccd_value = 2;
	ble_uuid_t gatts_uuid_value = {.uuid = BLE_UUID_GATT_CHARACTERISTIC_SERVICE_CHANGED};
	ble_uuid_t gatts_uuid_cccd = {.uuid = BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG};
	ble_uuid_t gatts_uuid_value_wrong = {.uuid = BLE_UUID_GATT_CHARACTERISTIC_SERVICE_CHANGED +
						     1};

	sd_ble_gatts_initial_user_handle_get_ExpectAndReturn(&m_arbitrary_handle, NRF_SUCCESS);
	sd_ble_gatts_initial_user_handle_get_IgnoreArg_p_handle();
	sd_ble_gatts_initial_user_handle_get_ReturnThruPtr_p_handle(&m_arbitrary_handle);
	sd_ble_gatts_attr_get_ExpectAndReturn(1, NULL, NULL, NRF_ERROR_INVALID_PARAM);
	sd_ble_gatts_attr_get_IgnoreArg_p_uuid();

	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INVALID_PARAM,
			       service_changed_cccd(m_arbitrary_conn_handle, &cccd_value));

	sd_ble_gatts_initial_user_handle_get_ExpectAndReturn(&m_arbitrary_handle, NRF_SUCCESS);
	sd_ble_gatts_initial_user_handle_get_IgnoreArg_p_handle();
	sd_ble_gatts_initial_user_handle_get_ReturnThruPtr_p_handle(&m_arbitrary_handle);
	sd_ble_gatts_attr_get_ExpectAndReturn(1, NULL, NULL, NRF_SUCCESS);
	sd_ble_gatts_attr_get_IgnoreArg_p_uuid();
	sd_ble_gatts_attr_get_ReturnThruPtr_p_uuid(&gatts_uuid_value);
	sd_ble_gatts_attr_get_ExpectAndReturn(2, NULL, NULL, NRF_SUCCESS);
	sd_ble_gatts_attr_get_IgnoreArg_p_uuid();
	sd_ble_gatts_attr_get_ReturnThruPtr_p_uuid(&gatts_uuid_cccd);
	sd_ble_gatts_value_get_ExpectAndReturn(m_arbitrary_conn_handle, 2, NULL,
					       NRF_ERROR_INTERNAL);
	sd_ble_gatts_value_get_IgnoreArg_p_value();

	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INTERNAL,
			       service_changed_cccd(m_arbitrary_conn_handle, &cccd_value));

	sd_ble_gatts_initial_user_handle_get_ExpectAndReturn(&m_arbitrary_handle, NRF_SUCCESS);
	sd_ble_gatts_initial_user_handle_get_IgnoreArg_p_handle();
	sd_ble_gatts_initial_user_handle_get_ReturnThruPtr_p_handle(&m_arbitrary_handle);
	for (uint32_t i = 1; i < m_arbitrary_handle; i++) {
		sd_ble_gatts_attr_get_ExpectAndReturn(i, NULL, NULL, NRF_SUCCESS);
		sd_ble_gatts_attr_get_IgnoreArg_p_uuid();
		sd_ble_gatts_attr_get_ReturnThruPtr_p_uuid(&gatts_uuid_value_wrong);
	}

	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_NOT_FOUND,
			       service_changed_cccd(m_arbitrary_conn_handle, &cccd_value));
}

void test_service_changed_cccd_2(void)
{
	uint16_t cccd_value = 0;

	sd_ble_gatts_initial_user_handle_get_ExpectAndReturn(&m_arbitrary_handle, NRF_SUCCESS);
	sd_ble_gatts_initial_user_handle_get_IgnoreArg_p_handle();
	sd_ble_gatts_initial_user_handle_get_ReturnThruPtr_p_handle(&m_arbitrary_handle);
	sd_ble_gatts_value_get_StubWithCallback(sd_ble_gatts_value_get_success_stub_ind_on);
	sd_ble_gatts_attr_get_StubWithCallback(sd_ble_gatts_attr_get_success_stub);

	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS,
			       service_changed_cccd(m_arbitrary_conn_handle, &cccd_value));
	TEST_ASSERT_EQUAL_UINT(2, cccd_value);
}

void test_car_update_handle(void)
{
	ble_uuid_t car_uuid;
	memset(&car_uuid, 0, sizeof(ble_uuid_t));
	car_uuid.uuid = BLE_UUID_GAP_CHARACTERISTIC_CAR;
	car_uuid.type = BLE_UUID_TYPE_BLE;

	ble_gattc_handle_range_t const car_handle_range = {1, 0xFFFF};

	sd_ble_gattc_char_value_by_uuid_read_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, &car_uuid, 1, &car_handle_range, 1, NRF_SUCCESS);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_car_hdl,
					    true);

	car_update_pending_handle(m_arbitrary_conn_handle, NULL);

	sd_ble_gattc_char_value_by_uuid_read_ExpectWithArrayAndReturn(
		m_arbitrary_conn_handle, &car_uuid, 1, &car_handle_range, 1, NRF_ERROR_INTERNAL);

	car_update_pending_handle(m_arbitrary_conn_handle, NULL);
}

uint16_t gcm_local_database_has_changed_test(void)
{
	ble_conn_state_conn_handle_list_t conn_handles = {.len = 3};
	uint16_t n_expected_calls = 0;
	pm_peer_id_t peer_id = m_arbitrary_peer_id;

	gscm_local_database_has_changed_Expect();
	ble_conn_state_conn_handles_ExpectAndReturn(conn_handles);
	for (uint32_t i = 0; i < conn_handles.len; i++) {
		peer_id = (peer_id == PM_PEER_ID_INVALID)
				  ? m_arbitrary_peer_id
				  : PM_PEER_ID_INVALID; // Try both valid and invalid peer ids.
		im_peer_id_get_by_conn_handle_ExpectAndReturn(conn_handles.conn_handles[i],
							      peer_id);
		if (peer_id == PM_PEER_ID_INVALID) {
			ble_conn_state_user_flag_set_Expect(conn_handles.conn_handles[i],
							    m_arbitrary_flag_id_sc, true);
		}
	}

	n_expected_calls += service_changed_flags_check_test(n_expected_calls, 0);

	return n_expected_calls;
}

/*@brief function for verifying that the apply flags have been checked.
 */
uint16_t apply_flags_check_test(uint16_t n_expected_calls, uint32_t n_call)
{
	uint16_t conn_handle;

	n_calls[n_call] = 5;
	ble_conn_state_for_each_set_user_flag_StubWithCallback(
		ble_conn_state_for_each_set_user_flag_stub);

	conn_handle = 0;
	gscm_local_db_cache_apply_ExpectAndReturn(conn_handle, NRF_SUCCESS);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(conn_handle, m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_apply, false);
	n_expected_calls++;

	conn_handle = 1;
	gscm_local_db_cache_apply_ExpectAndReturn(conn_handle, BLE_ERROR_INVALID_CONN_HANDLE);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_apply, false);

	conn_handle = 2;
	gscm_local_db_cache_apply_ExpectAndReturn(conn_handle, NRF_ERROR_INVALID_DATA);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(conn_handle, m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_apply, false);
	n_expected_calls++;

	conn_handle = 3;
	gscm_local_db_cache_apply_ExpectAndReturn(conn_handle, NRF_ERROR_BUSY);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_apply, true);

	conn_handle = 4;
	gscm_local_db_cache_apply_ExpectAndReturn(
		conn_handle, NRF_ERROR_INVALID_STATE); // arbitrary unexpected error.
	im_peer_id_get_by_conn_handle_ExpectAndReturn(conn_handle, m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(conn_handle, m_arbitrary_flag_id_apply, false);
	n_expected_calls++;

	return n_expected_calls;
}

void test_init(void)
{
	ret_code_t err_code;

	// ble_conn_state_user_flag_acquire error.
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	err_code = gcm_init();
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INTERNAL, err_code);

	// ble_conn_state_user_flag_acquire error.
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_update);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	err_code = gcm_init();
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INTERNAL, err_code);

	// ble_conn_state_user_flag_acquire error.
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_update);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_apply);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	err_code = gcm_init();
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INTERNAL, err_code);

	// ble_conn_state_user_flag_acquire error.
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_update);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_apply);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_sc);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	err_code = gcm_init();
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INTERNAL, err_code);

	// ble_conn_state_user_flag_acquire error.
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_update);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_apply);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_sc);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_sc_sent);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	err_code = gcm_init();
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INTERNAL, err_code);

	// ble_conn_state_user_flag_acquire error.
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_update);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_apply);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_sc);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_sc_sent);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_car_upd);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	err_code = gcm_init();
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INTERNAL, err_code);

	// ble_conn_state_user_flag_acquire error.
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_update);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_apply);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_sc);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_sc_sent);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_car_upd);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_car_hdl);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(BLE_CONN_STATE_USER_FLAG_INVALID);
	err_code = gcm_init();
	TEST_ASSERT_EQUAL_UINT(NRF_ERROR_INTERNAL, err_code);

	// Success
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_update);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_apply);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_sc);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_sc_sent);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_car_upd);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_car_hdl);
	ble_conn_state_user_flag_acquire_ExpectAndReturn(m_arbitrary_flag_id_car_val);
	nrf_mtx_init_Expect(&m_db_update_in_progress_mutex);
	err_code = gcm_init();
	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);
}

void test_gcm_ble_evt_handler_BLE_GATTS_EVT_SYS_ATTR_MISSING(void)
{
	uint16_t n_expected_calls = 0;

	// Cache applied successfully.
	gscm_local_db_cache_apply_ExpectAndReturn(m_arbitrary_conn_handle, NRF_SUCCESS);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_apply,
					    false);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(sys_attr_missing_evt(m_arbitrary_conn_handle));
	TEST_ASSERT_EQUAL(1, m_n_evt_handler_calls);
	TEST_ASSERT_EQUAL(PM_EVT_LOCAL_DB_CACHE_APPLIED, m_evt_handler_records[0].evt_id);
	TEST_ASSERT_EQUAL(m_arbitrary_peer_id, m_evt_handler_records[0].peer_id);
	TEST_ASSERT_EQUAL(m_arbitrary_conn_handle, m_evt_handler_records[0].conn_handle);

	evt_handler_call_record_clear();

	// Cache application not needed.
	gscm_local_db_cache_apply_ExpectAndReturn(m_arbitrary_conn_handle,
						  BLE_ERROR_INVALID_CONN_HANDLE);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_apply,
					    false);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(sys_attr_missing_evt(m_arbitrary_conn_handle));

	// DB has changed.
	gscm_local_db_cache_apply_ExpectAndReturn(m_arbitrary_conn_handle, NRF_ERROR_INVALID_DATA);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_apply,
					    false);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(sys_attr_missing_evt(m_arbitrary_conn_handle));
	TEST_ASSERT_EQUAL(1 + n_expected_calls, m_n_evt_handler_calls);
	TEST_ASSERT_EQUAL(PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED, m_evt_handler_records[0].evt_id);
	TEST_ASSERT_EQUAL(m_arbitrary_peer_id, m_evt_handler_records[0].peer_id);
	TEST_ASSERT_EQUAL(m_arbitrary_conn_handle, m_evt_handler_records[0].conn_handle);

	evt_handler_call_record_clear();

	// SoftDevice busy.
	gscm_local_db_cache_apply_ExpectAndReturn(m_arbitrary_conn_handle, NRF_ERROR_BUSY);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_apply,
					    true);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(sys_attr_missing_evt(m_arbitrary_conn_handle));

	// Unexpected error.
	gscm_local_db_cache_apply_ExpectAndReturn(m_arbitrary_conn_handle, NRF_ERROR_INVALID_STATE);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_apply,
					    false);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(sys_attr_missing_evt(m_arbitrary_conn_handle));
	TEST_ASSERT_EQUAL(1, m_n_evt_handler_calls);
	TEST_ASSERT_EQUAL(PM_EVT_ERROR_UNEXPECTED, m_evt_handler_records[0].evt_id);
	TEST_ASSERT_EQUAL(m_arbitrary_peer_id, m_evt_handler_records[0].peer_id);
	TEST_ASSERT_EQUAL(m_arbitrary_conn_handle, m_evt_handler_records[0].conn_handle);
	TEST_ASSERT_EQUAL(NRF_ERROR_INVALID_STATE,
			  m_evt_handler_records[0].params.error_unexpected.error);

	evt_handler_call_record_clear();
}

void test_gcm_ble_evt_handler_BLE_GATTS_EVT_SC_CONFIRM(void)
{
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	gscm_db_change_notification_done_Expect(m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_sc_sent,
					    false);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_sc, false);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(sc_confirm_evt(m_arbitrary_conn_handle));

	TEST_ASSERT_EQUAL(1, m_n_evt_handler_calls);
	TEST_ASSERT_EQUAL(PM_EVT_SERVICE_CHANGED_IND_CONFIRMED, m_evt_handler_records[0].evt_id);
	TEST_ASSERT_EQUAL(m_arbitrary_peer_id, m_evt_handler_records[0].peer_id);
	TEST_ASSERT_EQUAL(m_arbitrary_conn_handle, m_evt_handler_records[0].conn_handle);
}

void test_gcm_ble_evt_handler_BLE_GATTS_EVT_WRITE(void)
{
	ble_evt_t *p_ble_evt = write_evt(m_arbitrary_conn_handle);

	// Success
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_update,
					    true);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_update,
							      db_update_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_car_upd,
							      car_update_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(p_ble_evt);

	// Descriptor Write - not CCCD.
	p_ble_evt = write_evt(m_arbitrary_conn_handle);
	p_ble_evt->evt.gatts_evt.params.write.uuid.uuid++;
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(p_ble_evt);
}

static uint8_t stored_car_value;

ret_code_t data_store_stub(pm_peer_id_t peer_id, pm_peer_data_const_t const *p_peer_data,
			   pm_store_token_t *p_store_token, int numCalls)
{
	TEST_ASSERT(numCalls <= 2);
	TEST_ASSERT_EQUAL(m_arbitrary_peer_id, peer_id);
	TEST_ASSERT_EQUAL(PM_PEER_DATA_ID_CENTRAL_ADDR_RES, p_peer_data->data_id);
	TEST_ASSERT_EQUAL(stored_car_value, *p_peer_data->p_central_addr_res);
	return NRF_SUCCESS;
}

void test_gcm_ble_evt_handler_BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP(void)
{
	stored_car_value = 0;

	// Not found -> 0
	ble_evt_t *p_ble_evt_err = read_by_uuid_evt(m_arbitrary_conn_handle,
						    BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND);
	ble_conn_state_user_flag_get_ExpectAndReturn(m_arbitrary_conn_handle,
						     m_arbitrary_flag_id_car_hdl, true);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_car_hdl,
					    false);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_car_upd,
					    false);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pds_peer_data_store_StubWithCallback(data_store_stub);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(p_ble_evt_err);

	// Success -> not pending
	ble_evt_t *p_ble_evt_success =
		read_by_uuid_evt(m_arbitrary_conn_handle2, BLE_GATT_STATUS_SUCCESS);
	ble_conn_state_user_flag_get_ExpectAndReturn(m_arbitrary_conn_handle2,
						     m_arbitrary_flag_id_car_hdl, false);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(p_ble_evt_success);

	// Success -> read error
	p_ble_evt_success = read_by_uuid_evt(m_arbitrary_conn_handle, BLE_GATT_STATUS_SUCCESS);
	ble_conn_state_user_flag_get_ExpectAndReturn(m_arbitrary_conn_handle,
						     m_arbitrary_flag_id_car_hdl, true);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_car_hdl,
					    false);
	sd_ble_gattc_read_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_handle, 0,
					  NRF_ERROR_INTERNAL);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_car_upd,
					    false);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pds_peer_data_store_StubWithCallback(data_store_stub);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(p_ble_evt_success);

	// Success
	p_ble_evt_success = read_by_uuid_evt(m_arbitrary_conn_handle2, BLE_GATT_STATUS_SUCCESS);
	ble_conn_state_user_flag_get_ExpectAndReturn(m_arbitrary_conn_handle2,
						     m_arbitrary_flag_id_car_hdl, true);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle2, m_arbitrary_flag_id_car_hdl,
					    false);
	sd_ble_gattc_read_ExpectAndReturn(m_arbitrary_conn_handle2, m_arbitrary_handle, 0,
					  NRF_SUCCESS);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle2, m_arbitrary_flag_id_car_val,
					    true);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(p_ble_evt_success);
}

void test_gcm_ble_evt_handler_BLE_GATTC_EVT_READ_RSP(void)
{
	// Success -> not pending
	ble_evt_t *p_ble_evt_success =
		read_evt(m_arbitrary_conn_handle2, BLE_GATT_STATUS_SUCCESS, 0);
	ble_conn_state_user_flag_get_ExpectAndReturn(m_arbitrary_conn_handle2,
						     m_arbitrary_flag_id_car_val, false);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(p_ble_evt_success);

	// Success (0)
	stored_car_value = 0;
	p_ble_evt_success = read_evt(m_arbitrary_conn_handle, BLE_GATT_STATUS_SUCCESS, 0);
	ble_conn_state_user_flag_get_ExpectAndReturn(m_arbitrary_conn_handle,
						     m_arbitrary_flag_id_car_val, true);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_car_val,
					    false);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_car_upd,
					    false);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pds_peer_data_store_StubWithCallback(data_store_stub);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(p_ble_evt_success);

	// Success (1)
	stored_car_value = 1;
	p_ble_evt_success = read_evt(m_arbitrary_conn_handle, BLE_GATT_STATUS_SUCCESS, 1);
	ble_conn_state_user_flag_get_ExpectAndReturn(m_arbitrary_conn_handle,
						     m_arbitrary_flag_id_car_val, true);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_car_val,
					    false);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_car_upd,
					    false);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pds_peer_data_store_StubWithCallback(data_store_stub);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(p_ble_evt_success);
}

void test_gcm_ble_evt_handler_checking_flags(void)
{
	ble_evt_t *p_ble_evt = dummy_evt(m_arbitrary_conn_handle);
	pm_evt_t pdb_evt = {.evt_id = PM_EVT_FLASH_GARBAGE_COLLECTED};
	uint16_t n_expected_calls = 0;

	// No flags set.
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_apply,
							      apply_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);

	gcm_ble_evt_handler(p_ble_evt);

	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_update,
							      db_update_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_car_upd,
							      car_update_pending_handle, NULL, 0);
	gcm_pdb_evt_handler(&pdb_evt);

	// Flags set.
	n_expected_calls = apply_flags_check_test(n_expected_calls, 0);
	n_expected_calls = service_changed_flags_check_test_2(n_expected_calls, 1);

	gcm_ble_evt_handler(p_ble_evt);

	n_expected_calls = update_flags_check_test(n_expected_calls, 2);
	gcm_pdb_evt_handler(&pdb_evt);

	TEST_ASSERT_EQUAL(n_expected_calls, m_n_evt_handler_calls);

	evt_handler_call_record_clear();
}

void test_im_evt_handler(void)
{
	pm_evt_t im_evt = {
		.evt_id = PM_EVT_BONDED_PEER_CONNECTED,
		.conn_handle = m_arbitrary_conn_handle,
	};

	// Newly connected bonded peer. Service Changed indication should be sent.
	gscm_local_db_cache_apply_ExpectAndReturn(m_arbitrary_conn_handle, NRF_SUCCESS);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_apply,
					    false);
	gscm_service_changed_ind_needed_ExpectAndReturn(m_arbitrary_conn_handle, true);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_sc, true);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pds_peer_data_read_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_CENTRAL_ADDR_RES,
					   NULL, NULL, NRF_SUCCESS);
	pds_peer_data_read_IgnoreArg_p_data();
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_update,
							      db_update_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_car_upd,
							      car_update_pending_handle, NULL, 0);

	gcm_im_evt_handler(&im_evt);
	TEST_ASSERT_EQUAL_UINT(1, m_n_evt_handler_calls);

	evt_handler_call_record_clear();

	// Newly connected bonded peer. Service Changed indication should not be sent.
	gscm_local_db_cache_apply_ExpectAndReturn(m_arbitrary_conn_handle, NRF_SUCCESS);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_apply,
					    false);
	gscm_service_changed_ind_needed_ExpectAndReturn(m_arbitrary_conn_handle, false);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pds_peer_data_read_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_CENTRAL_ADDR_RES,
					   NULL, NULL, NRF_SUCCESS);
	pds_peer_data_read_IgnoreArg_p_data();
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_update,
							      db_update_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_car_upd,
							      car_update_pending_handle, NULL, 0);

	gcm_im_evt_handler(&im_evt);
	TEST_ASSERT_EQUAL_UINT(1, m_n_evt_handler_calls);

	evt_handler_call_record_clear();
}

void test_pdb_evt_handler(void)
{

	pm_evt_t pdb_evt_local_db = {
		.evt_id = PM_EVT_PEER_DATA_UPDATE_SUCCEEDED,
		.peer_id = m_arbitrary_peer_id,
		.params = {.peer_data_update_succeeded = {.data_id = PM_PEER_DATA_ID_GATT_LOCAL,
							  .action = PM_PEER_DATA_OP_UPDATE}}};

	pm_evt_t pdb_evt_bonding = {
		.evt_id = PM_EVT_PEER_DATA_UPDATE_SUCCEEDED,
		.peer_id = m_arbitrary_peer_id,
		.params = {.peer_data_update_succeeded = {.data_id = PM_PEER_DATA_ID_BONDING,
							  .action = PM_PEER_DATA_OP_UPDATE}}};

	pm_evt_t pdb_evt_sc = {
		.evt_id = PM_EVT_PEER_DATA_UPDATE_SUCCEEDED,
		.peer_id = m_arbitrary_peer_id,
		.params = {.peer_data_update_succeeded = {
				   .data_id = PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING,
				   .action = PM_PEER_DATA_OP_UPDATE}}};

	pm_evt_t pdb_evt_other = {
		.evt_id = PM_EVT_PEER_DATA_UPDATE_SUCCEEDED,
		.peer_id = m_arbitrary_peer_id,
		.params = {.peer_data_update_succeeded = {.data_id = PM_PEER_DATA_ID_APPLICATION,
							  .action = PM_PEER_DATA_OP_UPDATE}}};

	bool service_changed = true;
	uint16_t n_expected_calls = 0;
	// lint -save -e65 -e64
	pm_peer_data_flash_t returned_peer_data = {.data_id =
							   PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING,
						   .p_service_changed_pending = &service_changed};
	// lint -restore

	// LOCAL DB
	// Local DB updated.
	m_db_update_in_progress_mutex = NRF_MTX_LOCKED;
	nrf_mtx_unlock_Expect(&m_db_update_in_progress_mutex);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_update,
							      db_update_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_car_upd,
							      car_update_pending_handle, NULL, 0);

	gcm_pdb_evt_handler(&pdb_evt_local_db);

	// SERVICE CHANGED

	// pdb_peer_data_ptr_get error.
	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id,
					      PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING,
					      &returned_peer_data, NRF_ERROR_NOT_FOUND);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_update,
							      db_update_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_car_upd,
							      car_update_pending_handle, NULL, 0);

	gcm_pdb_evt_handler(&pdb_evt_sc);

	TEST_ASSERT_EQUAL(0, m_n_evt_handler_calls);

	// Service Changed state stored - true
	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id,
					      PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING,
					      &returned_peer_data, NRF_SUCCESS);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	im_conn_handle_get_ExpectAndReturn(m_arbitrary_peer_id, m_arbitrary_conn_handle);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_sc, true);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_sc,
							      sc_send_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_update,
							      db_update_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_car_upd,
							      car_update_pending_handle, NULL, 0);

	gcm_pdb_evt_handler(&pdb_evt_sc);
	TEST_ASSERT_EQUAL(n_expected_calls, m_n_evt_handler_calls);
	evt_handler_call_record_clear();
	n_expected_calls = 0;

	// Service Changed state stored - false
	*(bool *)returned_peer_data.p_service_changed_pending = false;
	pdb_peer_data_ptr_get_ExpectAndReturn(m_arbitrary_peer_id,
					      PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING,
					      &returned_peer_data, NRF_SUCCESS);
	pdb_peer_data_ptr_get_IgnoreArg_p_peer_data();
	pdb_peer_data_ptr_get_ReturnThruPtr_p_peer_data(&returned_peer_data);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_update,
							      db_update_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_car_upd,
							      car_update_pending_handle, NULL, 0);

	gcm_pdb_evt_handler(&pdb_evt_sc);
	TEST_ASSERT_EQUAL(0, m_n_evt_handler_calls);
	evt_handler_call_record_clear();

	// BONDING DATA
	// Invalid conn_handle.
	im_conn_handle_get_ExpectAndReturn(m_arbitrary_peer_id, BLE_CONN_HANDLE_INVALID);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_update,
							      db_update_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_car_upd,
							      car_update_pending_handle, NULL, 0);

	gcm_pdb_evt_handler(&pdb_evt_bonding);

	// Success
	im_conn_handle_get_ExpectAndReturn(m_arbitrary_peer_id, m_arbitrary_conn_handle);
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_update,
					    true);
	im_peer_id_get_by_conn_handle_ExpectAndReturn(m_arbitrary_conn_handle, m_arbitrary_peer_id);
	pds_peer_data_read_ExpectAndReturn(m_arbitrary_peer_id, PM_PEER_DATA_ID_CENTRAL_ADDR_RES,
					   NULL, NULL, NRF_ERROR_NOT_FOUND);
	pds_peer_data_read_IgnoreArg_p_data();
	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_car_upd,
					    true);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_update,
							      db_update_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_car_upd,
							      car_update_pending_handle, NULL, 0);

	gcm_pdb_evt_handler(&pdb_evt_bonding);

	// OTHER
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_update,
							      db_update_pending_handle, NULL, 1);

	gcm_pdb_evt_handler(&pdb_evt_other);

	TEST_ASSERT_EQUAL(n_expected_calls, m_n_evt_handler_calls);
}

#if 0
// @note emdi: gccm was removed.
void test_gcm_remote_db_store(void)
{
    ret_code_t err_code;

    // gccm error.
    gccm_remote_db_store_ExpectAndReturn(m_arbitrary_peer_id, &m_arbitrary_remote_db, m_arbitrary_n_services, NRF_ERROR_NOT_FOUND);

    err_code = gcm_remote_db_store(m_arbitrary_peer_id, &m_arbitrary_remote_db, m_arbitrary_n_services);
    TEST_ASSERT_EQUAL_UINT(NRF_ERROR_NOT_FOUND, err_code);


    // Success
    gccm_remote_db_store_ExpectAndReturn(m_arbitrary_peer_id, &m_arbitrary_remote_db, m_arbitrary_n_services, NRF_SUCCESS);

    err_code = gcm_remote_db_store(m_arbitrary_peer_id, &m_arbitrary_remote_db, m_arbitrary_n_services);
    TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);


    // Success - NULL param.
    gccm_remote_db_store_ExpectAndReturn(m_arbitrary_peer_id, NULL, m_arbitrary_n_services, NRF_SUCCESS);

    err_code = gcm_remote_db_store(m_arbitrary_peer_id, NULL, m_arbitrary_n_services);
    TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);
}


void test_gcm_remote_db_retrieve(void)
{
    ret_code_t err_code;

    // NULL param.
    err_code = gcm_remote_db_retrieve(m_arbitrary_peer_id, NULL, &m_arbitrary_n_services);
    TEST_ASSERT_EQUAL_UINT(NRF_ERROR_NULL, err_code);


    // gccm error.
    gccm_remote_db_retrieve_ExpectAndReturn(m_arbitrary_peer_id, &m_arbitrary_remote_db, &m_arbitrary_n_services, NRF_ERROR_NOT_FOUND);

    err_code = gcm_remote_db_retrieve(m_arbitrary_peer_id, &m_arbitrary_remote_db, &m_arbitrary_n_services);
    TEST_ASSERT_EQUAL_UINT(NRF_ERROR_NOT_FOUND, err_code);


    // Success
    gccm_remote_db_retrieve_ExpectAndReturn(m_arbitrary_peer_id, &m_arbitrary_remote_db, &m_arbitrary_n_services, NRF_SUCCESS);

    err_code = gcm_remote_db_retrieve(m_arbitrary_peer_id, &m_arbitrary_remote_db, &m_arbitrary_n_services);
    TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);
}
#endif

void test_gcm_local_db_cache_update(void)
{
	ret_code_t err_code;

	ble_conn_state_user_flag_set_Expect(m_arbitrary_conn_handle, m_arbitrary_flag_id_update,
					    true);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_update,
							      db_update_pending_handle, NULL, 0);
	ble_conn_state_for_each_set_user_flag_ExpectAndReturn(m_arbitrary_flag_id_car_upd,
							      car_update_pending_handle, NULL, 0);

	err_code = gcm_local_db_cache_update(m_arbitrary_conn_handle);
	TEST_ASSERT_EQUAL_UINT(NRF_SUCCESS, err_code);
}

void test_gcm_local_database_has_changed(void)
{
	uint16_t n_expected_calls = gcm_local_database_has_changed_test();

	gcm_local_database_has_changed();

	TEST_ASSERT_EQUAL(n_expected_calls, m_n_evt_handler_calls);
}
