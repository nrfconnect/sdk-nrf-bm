/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <nrf_strerror.h>
#include <stdint.h>
#include <ble_gap.h>
#include <ble_err.h>
#include <bm/bluetooth/peer_manager/peer_manager_types.h>

#include <modules/conn_state.h>
#include <modules/peer_manager_internal.h>
#include <modules/id_manager.h>
#include <modules/gatts_cache_manager.h>
#include <modules/peer_data_storage.h>
#include <modules/peer_database.h>
#include <modules/gatt_cache_manager.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_DECLARE(peer_manager, CONFIG_PEER_MANAGER_LOG_LEVEL);

#define MTX_LOCKED 1
#define MTX_UNLOCKED 0

/* The number of registered event handlers. */
#define GCM_EVENT_HANDLERS_CNT ARRAY_SIZE(evt_handlers)

/* GATT Cache Manager event handler in Peer Manager. */
extern void pm_gcm_evt_handler(struct pm_evt *gcm_evt);

/**
 * @brief GATT Cache Manager events' handlers.
 *        The number of elements in this array is GCM_EVENT_HANDLERS_CNT.
 */
static pm_evt_handler_internal_t evt_handlers[] = {pm_gcm_evt_handler};

static bool module_initialized;
/** @brief Mutex indicating whether a local DB write operation is ongoing. */
static atomic_t db_update_in_progress_mutex;
/**
 * @brief Flag ID for flag collection to keep track of which connections need a local DB update
 *        procedure.
 */
static int flag_local_db_update_pending;
/**
 * @brief Flag ID for flag collection to keep track of which connections need a local DB apply
 *        procedure.
 */
static int flag_local_db_apply_pending;
/**
 * @brief Flag ID for flag collection to keep track of which connections need to be sent a service
 *        changed indication.
 */
static int flag_service_changed_pending;
/**
 * @brief Flag ID for flag collection to keep track of which connections have been sent a service
 *        changed indication and are waiting for a handle value confirmation.
 */
static int flag_service_changed_sent;
/**
 * @brief Flag ID for flag collection to keep track of which connections need to have their Central
 *        Address Resolution value stored.
 */
static int flag_car_update_pending;
/**
 * @brief Flag ID for flag collection to keep track of which connections are pending Central
 *        Address Resolution handle reply.
 */
static int flag_car_handle_queried;
/**
 * @brief Flag ID for flag collection to keep track of which connections are pending Central
 *        Address Resolution value reply.
 */
static int flag_car_value_queried;

/**
 * @brief Function for resetting the module variable(s) of the GSCM module.
 *
 * @param[out]  The instance to reset.
 */
static void internal_state_reset(void)
{
	module_initialized = false;
}

static void evt_send(struct pm_evt *gcm_evt)
{
	gcm_evt->peer_id = im_peer_id_get_by_conn_handle(gcm_evt->conn_handle);

	for (uint32_t i = 0; i < GCM_EVENT_HANDLERS_CNT; i++) {
		evt_handlers[i](gcm_evt);
	}
}

/**
 * @brief Function for checking a write event for whether a CCCD was written during the write
 *        operation.
 *
 * @param[in]  write_evt  The parameters of the write event.
 *
 * @return  Whether the write was on a CCCD.
 */
static bool cccd_written(const ble_gatts_evt_write_t *write_evt)
{
	return ((write_evt->op == BLE_GATTS_OP_WRITE_REQ) &&
		(write_evt->uuid.type == BLE_UUID_TYPE_BLE) &&
		(write_evt->uuid.uuid == BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG));
}

/**
 * @brief Function for sending an PM_EVT_ERROR_UNEXPECTED event.
 *
 * @param[in]  conn_handle  The connection handle the event pertains to.
 * @param[in]  nrf_err      The unexpected error that occurred.
 */
static void send_unexpected_error(uint16_t conn_handle, uint32_t nrf_err)
{
	struct pm_evt error_evt = {
		.evt_id = PM_EVT_ERROR_UNEXPECTED,
		.conn_handle = conn_handle,
		.params.error_unexpected = {
			.error = nrf_err,
		},
	};

	evt_send(&error_evt);
}

/**
 * @brief Function for performing the local DB update procedure in an event context, where no return
 *        code can be given.
 *
 * @details This function will do the procedure, and check the result, set a flag if needed, and
 *          send an event if needed.
 *
 * @param[in]  conn_handle  The connection to perform the procedure on.
 */
static void local_db_apply_in_evt(uint16_t conn_handle)
{
	bool set_procedure_as_pending = false;
	uint32_t nrf_err;
	struct pm_evt event = {
		.conn_handle = conn_handle,
	};

	if (conn_handle == BLE_CONN_HANDLE_INVALID) {
		return;
	}

	nrf_err = gscm_local_db_cache_apply(conn_handle);

	switch (nrf_err) {
	case NRF_SUCCESS:
		event.evt_id = PM_EVT_LOCAL_DB_CACHE_APPLIED;

		evt_send(&event);
		break;

	case NRF_ERROR_BUSY:
		set_procedure_as_pending = true;
		break;

	case NRF_ERROR_INVALID_DATA:
		event.evt_id = PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED;

		LOG_WRN("The local database has changed, so some subscriptions to notifications "
			"and indications could not be restored for conn_handle %d",
			conn_handle);
		evt_send(&event);
		break;

	case BLE_ERROR_INVALID_CONN_HANDLE:
		/* Do nothing */
		break;

	default:
		LOG_ERR("gscm_local_db_cache_apply() returned %s which should not happen. "
			"conn_handle: %d",
			nrf_strerror_get(nrf_err), conn_handle);
		send_unexpected_error(conn_handle, nrf_err);
		break;
	}

	pm_conn_state_user_flag_set(conn_handle, flag_local_db_apply_pending,
				     set_procedure_as_pending);
}

/**
 * @brief Function for asynchronously starting a DB update procedure.
 *
 * @note This procedure can only be started asynchronously.
 *
 * @param[in]  conn_handle  The connection to perform the procedure on.
 * @param[in]  update       Whether to perform the procedure.
 */
static __INLINE void local_db_update(uint16_t conn_handle, bool update)
{
	pm_conn_state_user_flag_set(conn_handle, flag_local_db_update_pending, update);
}

/**
 * @brief Function for performing the local DB update procedure in an event context, where no return
 *        code can be given.
 *
 * @details This function will do the procedure, and check the result, set a flag if needed, and
 *          send an event if needed.
 *
 * @param[in]  conn_handle  The connection to perform the procedure on.
 */
static bool local_db_update_in_evt(uint16_t conn_handle)
{
	bool set_procedure_as_pending = false;
	bool success = false;
	uint32_t nrf_err = gscm_local_db_cache_update(conn_handle);

	switch (nrf_err) {
	case NRF_SUCCESS:
		success = true;
		break;

	case NRF_ERROR_INVALID_DATA:
		/* Fallthrough */
	case BLE_ERROR_INVALID_CONN_HANDLE:
		/* Do nothing */
		break;

	case NRF_ERROR_BUSY:
		set_procedure_as_pending = true;
		break;

	case NRF_ERROR_RESOURCES: {
		struct pm_evt event = {
			.evt_id = PM_EVT_STORAGE_FULL,
			.conn_handle = conn_handle,
		};

		LOG_WRN("Flash full. Could not store data for conn_handle: %d", conn_handle);
		evt_send(&event);
		break;
	}

	default:
		LOG_ERR("gscm_local_db_cache_update() returned %s for conn_handle: %d",
			nrf_strerror_get(nrf_err), conn_handle);
		send_unexpected_error(conn_handle, nrf_err);
		break;
	}

	local_db_update(conn_handle, set_procedure_as_pending);

	return success;
}

#if defined(CONFIG_PM_SERVICE_CHANGED)

/**
 * @brief Function for getting the value of the CCCD for the service changed characteristic.
 *
 * @details This function will search all system handles consecutively.
 *
 * @param[in]  conn_handle  The connection to check.
 * @param[out] cccd         The CCCD value of the service changed characteristic for this link.
 *
 * @return Any error from @ref sd_ble_gatts_value_get or @ref sd_ble_gatts_attr_get.
 */
static uint32_t service_changed_cccd(uint16_t conn_handle, uint16_t *cccd)
{
	bool sc_found = false;
	uint16_t end_handle;

	uint32_t nrf_err = sd_ble_gatts_initial_user_handle_get(&end_handle);

	__ASSERT_NO_MSG(nrf_err == NRF_SUCCESS);

	for (uint16_t handle = 1; handle < end_handle; handle++) {
		ble_uuid_t uuid;
		ble_gatts_value_t value = {.p_value = (uint8_t *)&uuid.uuid, .len = 2, .offset = 0};

		nrf_err = sd_ble_gatts_attr_get(handle, &uuid, NULL);
		if (nrf_err) {
			return nrf_err;
		} else if (!sc_found &&
			   (uuid.uuid == BLE_UUID_GATT_CHARACTERISTIC_SERVICE_CHANGED)) {
			sc_found = true;
		} else if (sc_found && (uuid.uuid == BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG)) {
			value.p_value = (uint8_t *)cccd;
			return sd_ble_gatts_value_get(conn_handle, handle, &value);
		}
	}
	return NRF_ERROR_NOT_FOUND;
}

/**
 * @brief Function for sending a service changed indication in an event context, where no return
 *        code can be given.
 *
 * @details This function will do the procedure, and check the result, set a flag if needed, and
 *          send an event if needed.
 *
 * @param[in]  conn_handle  The connection to perform the procedure on.
 */
static void service_changed_send_in_evt(uint16_t conn_handle)
{
	bool sc_pending_state = true;
	bool sc_sent_state = false;
	uint32_t nrf_err = gscm_service_changed_ind_send(conn_handle);

	switch (nrf_err) {
	case NRF_SUCCESS: {
		struct pm_evt event = {
			.evt_id = PM_EVT_SERVICE_CHANGED_IND_SENT,
			.conn_handle = conn_handle,
		};

		sc_sent_state = true;

		evt_send(&event);
		break;
	}

	case NRF_ERROR_BUSY:
		/* Do nothing. */
		break;

	case NRF_ERROR_INVALID_STATE: {
		uint16_t cccd;

		nrf_err = service_changed_cccd(conn_handle, &cccd);
		if ((nrf_err == NRF_SUCCESS) && cccd) {
			/* Possible ATT_MTU exchange ongoing. */
			/* Do nothing, treat as busy. */
			break;
		}
		if (nrf_err) {
			LOG_DBG("Unexpected error when looking for service changed "
				"CCCD: %s",
				nrf_strerror_get(nrf_err));
		}
		/* CCCDs not enabled or an error happened. Drop indication. */
		/* Fallthrough. */
	}
	/* Sometimes fallthrough. */
	case NRF_ERROR_NOT_SUPPORTED:
		/* Service changed not supported. Drop indication. */
		sc_pending_state = false;
		gscm_db_change_notification_done(im_peer_id_get_by_conn_handle(conn_handle));
		break;

	case BLE_ERROR_GATTS_SYS_ATTR_MISSING:
		local_db_apply_in_evt(conn_handle);
		break;

	case BLE_ERROR_INVALID_CONN_HANDLE:
		/* Do nothing. */
		break;

	default:
		LOG_ERR("gscm_service_changed_ind_send() returned %s for conn_handle: %d",
			nrf_strerror_get(nrf_err), conn_handle);
		send_unexpected_error(conn_handle, nrf_err);
		break;
	}

	pm_conn_state_user_flag_set(conn_handle, flag_service_changed_pending, sc_pending_state);
	pm_conn_state_user_flag_set(conn_handle, flag_service_changed_sent, sc_sent_state);
}
#endif

static void apply_pending_handle(uint16_t conn_handle, void *context)
{
	ARG_UNUSED(context);
	local_db_apply_in_evt(conn_handle);
}

static __INLINE void apply_pending_flags_check(void)
{
	(void)pm_conn_state_for_each_set_user_flag(flag_local_db_apply_pending,
						    apply_pending_handle, NULL);
}

static void db_update_pending_handle(uint16_t conn_handle, void *context)
{
	ARG_UNUSED(context);
	if (atomic_cas(&db_update_in_progress_mutex, MTX_UNLOCKED, MTX_LOCKED)) {
		if (local_db_update_in_evt(conn_handle)) {
			/* Successfully started writing to flash. */
			return;
		}

		atomic_val_t prev_state = atomic_set(&db_update_in_progress_mutex, MTX_UNLOCKED);

		__ASSERT(prev_state == MTX_LOCKED, "Incorrect mtx state when unlocking");
		ARG_UNUSED(prev_state);
	}
}

#if defined(CONFIG_PM_SERVICE_CHANGED)
static void sc_send_pending_handle(uint16_t conn_handle, void *context)
{
	ARG_UNUSED(context);
	if (!pm_conn_state_user_flag_get(conn_handle, flag_service_changed_sent)) {
		service_changed_send_in_evt(conn_handle);
	}
}

static __INLINE void service_changed_pending_flags_check(void)
{
	(void)(pm_conn_state_for_each_set_user_flag(flag_service_changed_pending,
						     sc_send_pending_handle, NULL));
}

static void service_changed_needed(uint16_t conn_handle)
{
	if (gscm_service_changed_ind_needed(conn_handle)) {
		pm_conn_state_user_flag_set(conn_handle, flag_service_changed_pending, true);
	}
}
#endif

static void car_update_pending_handle(uint16_t conn_handle, void *context)
{
	ARG_UNUSED(context);

	ble_uuid_t car_uuid;

	memset(&car_uuid, 0, sizeof(ble_uuid_t));
	car_uuid.uuid = BLE_UUID_GAP_CHARACTERISTIC_CAR;
	car_uuid.type = BLE_UUID_TYPE_BLE;

	const ble_gattc_handle_range_t car_handle_range = {1, 0xFFFF};

	uint32_t nrf_err =
		sd_ble_gattc_char_value_by_uuid_read(conn_handle, &car_uuid, &car_handle_range);

	if (nrf_err == NRF_SUCCESS) {
		pm_conn_state_user_flag_set(conn_handle, flag_car_handle_queried, true);
	}
}

static void car_update_needed(uint16_t conn_handle)
{
	uint32_t central_addr_res = 0;
	uint32_t central_addr_res_size = sizeof(uint32_t);
	struct pm_peer_data peer_data;

	peer_data.all_data = &central_addr_res;

	if (pds_peer_data_read(im_peer_id_get_by_conn_handle(conn_handle),
			       PM_PEER_DATA_ID_CENTRAL_ADDR_RES, &peer_data,
			       &central_addr_res_size) == NRF_ERROR_NOT_FOUND) {
		pm_conn_state_user_flag_set(conn_handle, flag_car_update_pending, true);
	}
}

static __INLINE void update_pending_flags_check(void)
{
	uint32_t count = pm_conn_state_for_each_set_user_flag(flag_local_db_update_pending,
							       db_update_pending_handle, NULL);
	if (count == 0) {
		(void)pm_conn_state_for_each_set_user_flag(flag_car_update_pending,
							    car_update_pending_handle, NULL);
	}
}

/**
 * @brief Callback function for events from the ID Manager module.
 *        This function is registered in the ID Manager module.
 *
 * @param[in]  event  The event from the ID Manager module.
 */
void gcm_im_evt_handler(struct pm_evt *event)
{
	switch (event->evt_id) {
	case PM_EVT_BONDED_PEER_CONNECTED:
		local_db_apply_in_evt(event->conn_handle);
#if defined(CONFIG_PM_SERVICE_CHANGED)
		service_changed_needed(event->conn_handle);
#endif
		car_update_needed(event->conn_handle);
		update_pending_flags_check();
		break;
	default:
		break;
	}
}

/**
 * @brief Callback function for events from the Peer Database module.
 *        This handler is extern in Peer Database.
 *
 * @param[in]  event  The event from the Security Dispatcher module.
 */
void gcm_pdb_evt_handler(struct pm_evt *event)
{
	if (event->evt_id == PM_EVT_PEER_DATA_UPDATE_SUCCEEDED &&
	    event->params.peer_data_update_succeeded.action == PM_PEER_DATA_OP_UPDATE) {
		switch (event->params.peer_data_update_succeeded.data_id) {
		case PM_PEER_DATA_ID_BONDING: {
			uint16_t conn_handle = im_conn_handle_get(event->peer_id);

			if (conn_handle != BLE_CONN_HANDLE_INVALID) {
				local_db_update(conn_handle, true);
				car_update_needed(conn_handle);
			}
			break;
		}

#if defined(CONFIG_PM_SERVICE_CHANGED)
		case PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING: {
			uint32_t nrf_err;
			bool service_changed_pending = false;
			uint32_t service_changed_pending_size = sizeof(bool);
			struct pm_peer_data peer_data;

			peer_data.all_data = &service_changed_pending;

			nrf_err = pds_peer_data_read(event->peer_id,
						     PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING,
						     &peer_data, &service_changed_pending_size);

			if (nrf_err == NRF_SUCCESS) {
				if (service_changed_pending) {
					uint16_t conn_handle = im_conn_handle_get(event->peer_id);

					if (conn_handle != BLE_CONN_HANDLE_INVALID) {
						pm_conn_state_user_flag_set(
							conn_handle, flag_service_changed_pending,
							true);
						service_changed_pending_flags_check();
					}
				}
			}
			break;
		}
#endif

		case PM_PEER_DATA_ID_GATT_LOCAL:
			(void)atomic_cas(&db_update_in_progress_mutex, MTX_LOCKED, MTX_UNLOCKED);

			/* Expecting a call to update_pending_flags_check() immediately. */
			break;

		default:
			/* No action */
			break;
		}
	}

	update_pending_flags_check();
}

uint32_t gcm_init(void)
{
	__ASSERT_NO_MSG(!module_initialized);

	internal_state_reset();

	flag_local_db_update_pending = pm_conn_state_user_flag_acquire();
	flag_local_db_apply_pending = pm_conn_state_user_flag_acquire();
	flag_service_changed_pending = pm_conn_state_user_flag_acquire();
	flag_service_changed_sent = pm_conn_state_user_flag_acquire();
	flag_car_update_pending = pm_conn_state_user_flag_acquire();
	flag_car_handle_queried = pm_conn_state_user_flag_acquire();
	flag_car_value_queried = pm_conn_state_user_flag_acquire();

	if ((flag_local_db_update_pending == PM_CONN_STATE_USER_FLAG_INVALID) ||
	    (flag_local_db_apply_pending == PM_CONN_STATE_USER_FLAG_INVALID) ||
	    (flag_service_changed_pending == PM_CONN_STATE_USER_FLAG_INVALID) ||
	    (flag_service_changed_sent == PM_CONN_STATE_USER_FLAG_INVALID) ||
	    (flag_car_update_pending == PM_CONN_STATE_USER_FLAG_INVALID) ||
	    (flag_car_handle_queried == PM_CONN_STATE_USER_FLAG_INVALID) ||
	    (flag_car_value_queried == PM_CONN_STATE_USER_FLAG_INVALID)) {
		LOG_ERR("Could not acquire conn_state user flags. Increase "
			"PM_CONN_STATE_USER_FLAG_COUNT in the pm_conn_state module.");
		return NRF_ERROR_INTERNAL;
	}

	(void)atomic_set(&db_update_in_progress_mutex, MTX_UNLOCKED);

	module_initialized = true;

	return NRF_SUCCESS;
}

void store_car_value(uint16_t conn_handle, bool car_value)
{
	/* Use a uint32_t to enforce 4-byte alignment. */
	static const uint32_t car_value_true = true;
	static const uint32_t car_value_false;

	struct pm_peer_data_const peer_data = {
		.data_id = PM_PEER_DATA_ID_CENTRAL_ADDR_RES,
		.length_words = 1,
	};

	pm_conn_state_user_flag_set(conn_handle, flag_car_update_pending, false);
	peer_data.central_addr_res = car_value ? &car_value_true : &car_value_false;
	uint32_t nrf_err =
		pds_peer_data_store(im_peer_id_get_by_conn_handle(conn_handle), &peer_data, NULL);
	if (nrf_err) {
		LOG_WRN("CAR char value couldn't be stored (error: %s). Reattempt will "
			"happen on the next connection.",
			nrf_strerror_get(nrf_err));
	}
}

/**
 * @brief Callback function for BLE events from the SoftDevice.
 *
 * @param[in]  ble_evt  The BLE event from the SoftDevice.
 */
void gcm_ble_evt_handler(const ble_evt_t *ble_evt)
{
	uint16_t conn_handle = ble_evt->evt.gatts_evt.conn_handle;

	switch (ble_evt->header.evt_id) {
	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		local_db_apply_in_evt(conn_handle);
		break;

#if defined(CONFIG_PM_SERVICE_CHANGED)
	case BLE_GATTS_EVT_SC_CONFIRM: {
		struct pm_evt event = {
			.evt_id = PM_EVT_SERVICE_CHANGED_IND_CONFIRMED,
			.peer_id = im_peer_id_get_by_conn_handle(conn_handle),
			.conn_handle = conn_handle,
		};

		gscm_db_change_notification_done(event.peer_id);

		pm_conn_state_user_flag_set(conn_handle, flag_service_changed_sent, false);
		pm_conn_state_user_flag_set(conn_handle, flag_service_changed_pending, false);
		evt_send(&event);
		break;
	}
#endif

	case BLE_GATTS_EVT_WRITE:
		if (cccd_written(&ble_evt->evt.gatts_evt.params.write)) {
			local_db_update(conn_handle, true);
			update_pending_flags_check();
		}
		break;

	case BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP: {
		bool handle_found = false;

		conn_handle = ble_evt->evt.gattc_evt.conn_handle;
		const ble_gattc_evt_char_val_by_uuid_read_rsp_t *val =
			&ble_evt->evt.gattc_evt.params.char_val_by_uuid_read_rsp;

		if (!pm_conn_state_user_flag_get(conn_handle, flag_car_handle_queried)) {
			break;
		}

		pm_conn_state_user_flag_set(conn_handle, flag_car_handle_queried, false);

		if (ble_evt->evt.gattc_evt.gatt_status ==
		    BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND) {
			/* Store 0. */
		} else if (ble_evt->evt.gattc_evt.gatt_status != BLE_GATT_STATUS_SUCCESS) {
			LOG_WRN("Unexpected GATT status while getting CAR char value: 0x%x",
				ble_evt->evt.gattc_evt.gatt_status);
			/* Store 0. */
		} else {
			if (val->count != 1) {
				LOG_WRN("Multiple (%d) CAR characteristics found, using the first.",
					val->count);
			}

			if (val->value_len != 1) {
				LOG_WRN("Unexpected CAR characteristic value length (%d), store 0.",
					val->value_len);
				/* Store 0. */
			} else {
				uint32_t nrf_err = sd_ble_gattc_read(
					conn_handle, *(uint16_t *)val->handle_value, 0);

				if (nrf_err == NRF_SUCCESS) {
					handle_found = true;
					pm_conn_state_user_flag_set(
						conn_handle, flag_car_value_queried, true);
				}
			}
		}

		if (!handle_found) {
			store_car_value(conn_handle, false);
		}
		break;
	}

	case BLE_GATTC_EVT_READ_RSP: {
		bool car_value = false;

		conn_handle = ble_evt->evt.gattc_evt.conn_handle;
		const ble_gattc_evt_read_rsp_t *val = &ble_evt->evt.gattc_evt.params.read_rsp;

		if (!pm_conn_state_user_flag_get(conn_handle, flag_car_value_queried)) {
			break;
		}

		pm_conn_state_user_flag_set(conn_handle, flag_car_value_queried, false);

		if (ble_evt->evt.gattc_evt.gatt_status != BLE_GATT_STATUS_SUCCESS) {
			LOG_WRN("Unexpected GATT status while getting CAR char value: 0x%x",
				ble_evt->evt.gattc_evt.gatt_status);
			/* Store 0. */
		} else {
			if (val->len != 1) {
				LOG_WRN("Unexpected CAR characteristic value length (%d), store 0.",
					val->len);
				/* Store 0. */
			} else {
				car_value = *val->data;
			}
		}

		store_car_value(conn_handle, car_value);
	}
	}

	apply_pending_flags_check();
#if defined(CONFIG_PM_SERVICE_CHANGED)
	service_changed_pending_flags_check();
#endif
}

uint32_t gcm_local_db_cache_update(uint16_t conn_handle)
{
	__ASSERT_NO_MSG(module_initialized);

	local_db_update(conn_handle, true);
	update_pending_flags_check();

	return NRF_SUCCESS;
}

#if defined(CONFIG_PM_SERVICE_CHANGED)
void gcm_local_database_has_changed(void)
{
	gscm_local_database_has_changed();

	struct pm_conn_state_conn_handle_list conn_handles = pm_conn_state_conn_handles();

	for (uint16_t i = 0; i < conn_handles.len; i++) {
		if (im_peer_id_get_by_conn_handle(conn_handles.conn_handles[i]) ==
		    PM_PEER_ID_INVALID) {
			pm_conn_state_user_flag_set(conn_handles.conn_handles[i],
						     flag_service_changed_pending, true);
		}
	}

	service_changed_pending_flags_check();
}
#endif
