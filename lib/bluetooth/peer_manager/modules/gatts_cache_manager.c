/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */


#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/__assert.h>
#include <nrf_error.h>
#include <nrf_strerror.h>
#include <ble_gap.h>
#include <ble_err.h>
#include <bm/bluetooth/peer_manager/peer_manager_types.h>
#include <modules/peer_manager_internal.h>
#include <modules/peer_database.h>
#include <modules/peer_data_storage.h>
#include <modules/id_manager.h>
#include <modules/gatts_cache_manager.h>

LOG_MODULE_DECLARE(peer_manager, CONFIG_PEER_MANAGER_LOG_LEVEL);

#if defined(CONFIG_PM_SERVICE_CHANGED)

/* The number of registered event handlers. */
#define GSCM_EVENT_HANDLERS_CNT ARRAY_SIZE(evt_handlers)

/* GATTS Cache Manager event handler in Peer Manager. */
extern void pm_gscm_evt_handler(struct pm_evt *gcm_evt);

/* GATTS Cache Manager events' handlers.
 * The number of elements in this array is GSCM_EVENT_HANDLERS_CNT.
 */
static pm_evt_handler_internal_t evt_handlers[] = {pm_gscm_evt_handler};
#endif

/* Syntactic sugar, two spoons. */
#define SYS_ATTR_SYS  (BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS)
#define SYS_ATTR_USR  (BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS)
#define SYS_ATTR_BOTH (SYS_ATTR_SYS | SYS_ATTR_USR)

static bool module_initialized;
static uint16_t current_sc_store_peer_id;

/** @brief Function for resetting the module variable(s) of the GSCM module. */
static void internal_state_reset(void)
{
	module_initialized = false;
	current_sc_store_peer_id = PM_PEER_ID_INVALID;

	/* If CONFIG_PM_SERVICE_CHANGED is 0, this variable is unused. */
	UNUSED_VARIABLE(current_sc_store_peer_id);
}

#if defined(CONFIG_PM_SERVICE_CHANGED)
static void evt_send(struct pm_evt *gscm_evt)
{
	gscm_evt->conn_handle = im_conn_handle_get(gscm_evt->peer_id);

	for (uint32_t i = 0; i < GSCM_EVENT_HANDLERS_CNT; i++) {
		evt_handlers[i](gscm_evt);
	}
}

/**
 * @brief Function for storing service_changed_pending = true to flash for all peers, in sequence.
 *
 * This function aborts if it gets @ref NRF_ERROR_BUSY when trying to store. A subsequent call will
 * continue where the last call was aborted.
 */
static void service_changed_pending_set(void)
{
	__ASSERT_NO_MSG(module_initialized);

	uint32_t nrf_err;
	/* Use a uint32_t to enforce 4-byte alignment. */
	static const uint32_t service_changed_pending = true;

	struct pm_peer_data_const peer_data = {
		.data_id = PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING,
		.length_words = PM_SC_STATE_N_WORDS(),
		.service_changed_pending = (bool *)&service_changed_pending,
	};

	while (current_sc_store_peer_id != PM_PEER_ID_INVALID) {
		nrf_err = pds_peer_data_store(current_sc_store_peer_id, &peer_data, NULL);
		if (nrf_err) {
			struct pm_evt evt = {.peer_id = current_sc_store_peer_id};

			if (nrf_err == NRF_ERROR_BUSY) {
				/* Do nothing. */
			} else if (nrf_err == NRF_ERROR_RESOURCES) {
				evt.evt_id = PM_EVT_STORAGE_FULL;
				evt_send(&evt);
			} else {
				LOG_ERR("pds_peer_data_store() returned %s while storing "
					"service changed "
					"state for peer id %d.",
					nrf_strerror_get(nrf_err),
					current_sc_store_peer_id);
				evt.evt_id = PM_EVT_ERROR_UNEXPECTED;
				evt.params.error_unexpected.error = nrf_err;
				evt_send(&evt);
			}
			break;
		}

		current_sc_store_peer_id = pds_next_peer_id_get(current_sc_store_peer_id);
	}
}

/**
 * @brief Event handler for events from the Peer Database module.
 *        This function is extern in Peer Database.
 *
 * @param[in]  event The event that has happened with peer id and flags.
 */
void gscm_pdb_evt_handler(struct pm_evt *event)
{
	if (current_sc_store_peer_id != PM_PEER_ID_INVALID) {
		service_changed_pending_set();
	}
}
#endif

uint32_t gscm_init(void)
{
	__ASSERT_NO_MSG(!module_initialized);

	internal_state_reset();
	module_initialized = true;

	return NRF_SUCCESS;
}

uint32_t gscm_local_db_cache_update(uint16_t conn_handle)
{
	__ASSERT_NO_MSG(module_initialized);

	uint16_t peer_id = im_peer_id_get_by_conn_handle(conn_handle);
	uint32_t nrf_err;

	if (peer_id == PM_PEER_ID_INVALID) {
		return BLE_ERROR_INVALID_CONN_HANDLE;
	}

	struct pm_peer_data peer_data;
	uint16_t n_bufs = 1;
	bool retry_with_bigger_buffer = false;

	do {
		retry_with_bigger_buffer = false;

		nrf_err = pdb_write_buf_get(peer_id, PM_PEER_DATA_ID_GATT_LOCAL, n_bufs++,
					    &peer_data);
		if (nrf_err == NRF_SUCCESS) {
			struct pm_peer_data_local_gatt_db *local_gatt_db =
				peer_data.local_gatt_db;

			local_gatt_db->flags = SYS_ATTR_BOTH;

			nrf_err = sd_ble_gatts_sys_attr_get(
				conn_handle, &local_gatt_db->data[0],
				&local_gatt_db->len, local_gatt_db->flags);

			if (nrf_err == NRF_SUCCESS) {
				uint8_t local_gatt_db_buf[PM_PEER_DATA_LOCAL_GATT_DB_MAX_SIZE] = {
					0
				};
				uint32_t local_gatt_db_size = PM_PEER_DATA_LOCAL_GATT_DB_MAX_SIZE;
				struct pm_peer_data_local_gatt_db *curr_local_gatt_db =
					(struct pm_peer_data_local_gatt_db *)local_gatt_db_buf;
				struct pm_peer_data curr_peer_data;

				curr_peer_data.all_data = local_gatt_db_buf;

				nrf_err = pds_peer_data_read(peer_id,
							     PM_PEER_DATA_ID_GATT_LOCAL,
							     &curr_peer_data,
							     &local_gatt_db_size);

				if ((nrf_err != NRF_SUCCESS) &&
				    (nrf_err != NRF_ERROR_NOT_FOUND)) {
					LOG_ERR("pds_peer_data_read() returned %s "
						"for conn_handle: %d",
						nrf_strerror_get(nrf_err),
						conn_handle);
					return NRF_ERROR_INTERNAL;
				}

				if ((nrf_err == NRF_ERROR_NOT_FOUND) ||
				    (local_gatt_db->len != curr_local_gatt_db->len) ||
				    (memcmp(local_gatt_db->data, curr_local_gatt_db->data,
					    local_gatt_db->len) != 0)) {
					nrf_err = pdb_write_buf_store(peer_id,
								      PM_PEER_DATA_ID_GATT_LOCAL,
								      peer_id);
				} else {
					LOG_DBG("Local db is already up to date, "
						"skipping write.");
					uint32_t nrf_err_release = pdb_write_buf_release(
						peer_id, PM_PEER_DATA_ID_GATT_LOCAL);

					if (nrf_err_release == NRF_SUCCESS) {
						nrf_err = NRF_ERROR_INVALID_DATA;
					} else {
						LOG_ERR("Did another thread manipulate "
							"PM_PEER_DATA_ID_GATT_LOCAL for "
							"peer_id %d at the same time? "
							"pdb_write_buf_release() returned "
							"%s.",
							peer_id,
							nrf_strerror_get(nrf_err_release));
						nrf_err = NRF_ERROR_INTERNAL;
					}
				}
			} else {
				if (nrf_err == NRF_ERROR_DATA_SIZE) {
					/* The sys attributes are bigger than the requested
					 * write buffer.
					 */
					retry_with_bigger_buffer = true;
				} else if (nrf_err == NRF_ERROR_NOT_FOUND) {
					/* There are no sys attributes in the GATT db, so
					 * nothing needs to be stored.
					 */
					nrf_err = NRF_SUCCESS;
				}

				uint32_t nrf_err_release = pdb_write_buf_release(
					peer_id, PM_PEER_DATA_ID_GATT_LOCAL);

				if (nrf_err_release) {
					LOG_ERR("Did another thread manipulate "
						"PM_PEER_DATA_ID_GATT_LOCAL for "
						"peer_id %d at the same time? "
						"pdb_write_buf_release() returned %s.",
						peer_id,
						nrf_strerror_get(nrf_err_release));
					nrf_err = NRF_ERROR_INTERNAL;
				}
			}
		} else if (nrf_err == NRF_ERROR_INVALID_PARAM) {
			/* The sys attributes are bigger than the entire write buffer. */
			nrf_err = NRF_ERROR_DATA_SIZE;
		}
	} while (retry_with_bigger_buffer);

	return nrf_err;
}

uint32_t gscm_local_db_cache_apply(uint16_t conn_handle)
{
	__ASSERT_NO_MSG(module_initialized);

	uint16_t peer_id = im_peer_id_get_by_conn_handle(conn_handle);
	uint32_t nrf_err;
	struct pm_peer_data peer_data;
	const uint8_t *sys_attr_data = NULL;
	uint16_t sys_attr_len = 0;
	uint32_t sys_attr_flags = (SYS_ATTR_BOTH);
	bool all_attributes_applied = true;

	if (peer_id != PM_PEER_ID_INVALID) {
		uint8_t local_gatt_db_buf[PM_PEER_DATA_LOCAL_GATT_DB_MAX_SIZE] = { 0 };
		uint32_t local_gatt_db_size = PM_PEER_DATA_LOCAL_GATT_DB_MAX_SIZE;
		struct pm_peer_data_local_gatt_db *curr_local_gatt_db =
			(struct pm_peer_data_local_gatt_db *)local_gatt_db_buf;
		peer_data.all_data = &local_gatt_db_buf;
		nrf_err = pds_peer_data_read(peer_id, PM_PEER_DATA_ID_GATT_LOCAL, &peer_data,
					     &local_gatt_db_size);
		if (nrf_err == NRF_SUCCESS) {
			const struct pm_peer_data_local_gatt_db *local_gatt_db;

			local_gatt_db = curr_local_gatt_db;
			sys_attr_data = local_gatt_db->data;
			sys_attr_len = local_gatt_db->len;
			sys_attr_flags = local_gatt_db->flags;
		}
	}

	do {
		nrf_err = sd_ble_gatts_sys_attr_set(conn_handle, sys_attr_data, sys_attr_len,
						    sys_attr_flags);

		if (nrf_err == NRF_ERROR_NO_MEM) {
			nrf_err = NRF_ERROR_BUSY;
		} else if (nrf_err == NRF_ERROR_INVALID_STATE) {
			nrf_err = NRF_SUCCESS;
		} else if (nrf_err == NRF_ERROR_INVALID_DATA) {
			all_attributes_applied = false;

			if (sys_attr_flags & SYS_ATTR_USR) {
				/* Try setting only system attributes. */
				sys_attr_flags = SYS_ATTR_SYS;
			} else if (sys_attr_data || sys_attr_len) {
				/* Try reporting that none exist. */
				sys_attr_data = NULL;
				sys_attr_len = 0;
				sys_attr_flags = SYS_ATTR_BOTH;
			} else {
				LOG_ERR("sd_ble_gatts_sys_attr_set() returned "
					"NRF_ERROR_INVALID_DATA for NULL "
					"pointer which should never happen. conn_handle: %d",
					conn_handle);
				nrf_err = NRF_ERROR_INTERNAL;
			}
		}
	} while (nrf_err == NRF_ERROR_INVALID_DATA);

	if (!all_attributes_applied) {
		nrf_err = NRF_ERROR_INVALID_DATA;
	}

	return nrf_err;
}

#if defined(CONFIG_PM_SERVICE_CHANGED)
void gscm_local_database_has_changed(void)
{
	__ASSERT_NO_MSG(module_initialized);
	current_sc_store_peer_id = pds_next_peer_id_get(PM_PEER_ID_INVALID);
	service_changed_pending_set();
}

bool gscm_service_changed_ind_needed(uint16_t conn_handle)
{
	uint32_t nrf_err;
	bool service_changed_state;
	uint32_t service_changed_state_size = sizeof(bool);
	struct pm_peer_data peer_data;

	peer_data.all_data = &service_changed_state;
	uint16_t peer_id = im_peer_id_get_by_conn_handle(conn_handle);

	nrf_err = pds_peer_data_read(peer_id, PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING, &peer_data,
				     &service_changed_state_size);

	if (nrf_err) {
		return false;
	}

	return service_changed_state;
}

uint32_t gscm_service_changed_ind_send(uint16_t conn_handle)
{
	static uint16_t start_handle;
	const uint16_t end_handle = 0xFFFF;
	uint32_t nrf_err;

	nrf_err = sd_ble_gatts_initial_user_handle_get(&start_handle);

	if (nrf_err) {
		LOG_ERR("sd_ble_gatts_initial_user_handle_get() returned %s which should not "
			"happen.",
			nrf_strerror_get(nrf_err));
		return NRF_ERROR_INTERNAL;
	}

	do {
		nrf_err = sd_ble_gatts_service_changed(conn_handle, start_handle, end_handle);
		if (nrf_err == BLE_ERROR_INVALID_ATTR_HANDLE) {
			start_handle += 1;
		}
	} while (nrf_err == BLE_ERROR_INVALID_ATTR_HANDLE);

	return nrf_err;
}

void gscm_db_change_notification_done(uint16_t peer_id)
{
	__ASSERT_NO_MSG(module_initialized);

	/* Use a uint32_t to enforce 4-byte alignment. */
	static const uint32_t service_changed_pending;

	struct pm_peer_data_const peer_data = {
		.data_id = PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING,
		.length_words = PM_SC_STATE_N_WORDS(),
		.service_changed_pending = (bool *)&service_changed_pending,
	};

	/* Don't need to check return code, because all error conditions can be ignored. */
	(void)pds_peer_data_store(peer_id, &peer_data, NULL);
}
#endif
