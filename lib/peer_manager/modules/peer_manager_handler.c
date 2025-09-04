/*
 * Copyright (c) 2018-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <nrf_error.h>
#include <nrf_strerror.h>
#include <ble_gap.h>
#include <ble_gattc.h>
#include <ble_conn_state.h>
#include <bluetooth/peer_manager/peer_manager.h>

#if CONFIG_PM_HANDLER_SEC_DELAY_MS > 0
#include <bm_timer.h>
#endif

#include <modules/peer_manager_handler.h>

LOG_MODULE_DECLARE(peer_manager, CONFIG_PEER_MANAGER_LOG_LEVEL);

#define APP_ERROR_CHECK(err)

static const char * const m_roles_str[] = {
	"Invalid Role",
	"Peripheral",
	"Central",
};

static const char * const m_sec_procedure_str[] = {
	"Encryption",
	"Bonding",
	"Pairing",
};

#define PM_EVT_STR(_name) [_name] = STRINGIFY(_name)

static const char * const m_event_str[] = {
	PM_EVT_STR(PM_EVT_BONDED_PEER_CONNECTED),
	PM_EVT_STR(PM_EVT_CONN_CONFIG_REQ),
	PM_EVT_STR(PM_EVT_CONN_SEC_START),
	PM_EVT_STR(PM_EVT_CONN_SEC_SUCCEEDED),
	PM_EVT_STR(PM_EVT_CONN_SEC_FAILED),
	PM_EVT_STR(PM_EVT_CONN_SEC_CONFIG_REQ),
	PM_EVT_STR(PM_EVT_CONN_SEC_PARAMS_REQ),
	PM_EVT_STR(PM_EVT_STORAGE_FULL),
	PM_EVT_STR(PM_EVT_ERROR_UNEXPECTED),
	PM_EVT_STR(PM_EVT_PEER_DATA_UPDATE_SUCCEEDED),
	PM_EVT_STR(PM_EVT_PEER_DATA_UPDATE_FAILED),
	PM_EVT_STR(PM_EVT_PEER_DELETE_SUCCEEDED),
	PM_EVT_STR(PM_EVT_PEER_DELETE_FAILED),
	PM_EVT_STR(PM_EVT_PEERS_DELETE_SUCCEEDED),
	PM_EVT_STR(PM_EVT_PEERS_DELETE_FAILED),
	PM_EVT_STR(PM_EVT_LOCAL_DB_CACHE_APPLIED),
	PM_EVT_STR(PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED),
	PM_EVT_STR(PM_EVT_SERVICE_CHANGED_IND_SENT),
	PM_EVT_STR(PM_EVT_SERVICE_CHANGED_IND_CONFIRMED),
	PM_EVT_STR(PM_EVT_SLAVE_SECURITY_REQ),
	PM_EVT_STR(PM_EVT_FLASH_GARBAGE_COLLECTED),
	PM_EVT_STR(PM_EVT_FLASH_GARBAGE_COLLECTION_FAILED),
};

static const char * const m_data_id_str[] = {
	"Outdated (0)",	    "Service changed pending flag",
	"Outdated (2)",	    "Outdated (3)",
	"Application data", "Remote database",
	"Peer rank",	    "Bonding data",
	"Local database",   "Central address resolution",
};

static const char * const m_data_action_str[] = {"Update", "Delete"};

#define PM_SEC_ERR_STR(_name)                                                                      \
	{                                                                                          \
		.error = _name, .error_str = #_name                                                \
	}

typedef struct {
	pm_sec_error_code_t error;
	const char *error_str;
} sec_err_str_t;

static const sec_err_str_t m_pm_sec_error_str[] = {
	PM_SEC_ERR_STR(PM_CONN_SEC_ERROR_PIN_OR_KEY_MISSING),
	PM_SEC_ERR_STR(PM_CONN_SEC_ERROR_MIC_FAILURE),
	PM_SEC_ERR_STR(PM_CONN_SEC_ERROR_DISCONNECT),
	PM_SEC_ERR_STR(PM_CONN_SEC_ERROR_SMP_TIMEOUT),
};

static const char *sec_err_string_get(pm_sec_error_code_t error)
{
	static char errstr[30];

	for (uint32_t i = 0; i < (sizeof(m_pm_sec_error_str) / sizeof(sec_err_str_t)); i++) {
		if (m_pm_sec_error_str[i].error == error) {
			return m_pm_sec_error_str[i].error_str;
		}
	}

	(void)snprintf(errstr, sizeof(errstr), "%s 0x%hx",
			   (error < PM_CONN_SEC_ERROR_BASE) ? "BLE_GAP_SEC_STATUS"
							    : "PM_CONN_SEC_ERROR",
			   error);
	return errstr;
}

static void _conn_secure(uint16_t conn_handle, bool force)
{
	uint32_t err_code;

	if (!force) {
		pm_conn_sec_status_t status;

		err_code = pm_conn_sec_status_get(conn_handle, &status);
		if (err_code != BLE_ERROR_INVALID_CONN_HANDLE) {
			APP_ERROR_CHECK(err_code);
		}

		/* If the link is already secured, don't initiate security procedure. */
		if (status.encrypted) {
			LOG_DBG("Already encrypted, skipping security.");
			return;
		}
	}

	err_code = pm_conn_secure(conn_handle, false);

	if ((err_code == NRF_SUCCESS) || (err_code == NRF_ERROR_BUSY)) {
		/* Success. */
	} else if (err_code == NRF_ERROR_TIMEOUT) {
		LOG_WRN("pm_conn_secure() failed because an SMP timeout is preventing security on "
			"the link. Disconnecting conn_handle %d.",
			conn_handle);

		err_code = sd_ble_gap_disconnect(conn_handle,
						 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (err_code != NRF_SUCCESS) {
			LOG_WRN("sd_ble_gap_disconnect() returned %s on conn_handle %d.",
				nrf_strerror_get(err_code), conn_handle);
		}
	} else if (err_code == NRF_ERROR_INVALID_DATA) {
		LOG_WRN("pm_conn_secure() failed because the stored data for conn_handle %d does "
			"not have a valid key.",
			conn_handle);
	} else if (err_code == BLE_ERROR_INVALID_CONN_HANDLE) {
		LOG_WRN("pm_conn_secure() failed because conn_handle %d is not a valid connection.",
			conn_handle);
	} else {
		LOG_ERR("Asserting. pm_conn_secure() returned %s on conn_handle %d.",
			nrf_strerror_get(err_code), conn_handle);
		APP_ERROR_CHECK(err_code);
	}
}

#if CONFIG_PM_HANDLER_SEC_DELAY_MS > 0
static struct bm_timer secure_delay_timer;

typedef union {
	struct {
		uint16_t conn_handle;
		bool force;
	} values;
	void *p_void;
} conn_secure_context_t;

STATIC_ASSERT(sizeof(conn_secure_context_t) <= sizeof(void *),
	      "conn_secure_context_t is too large.");

static void _conn_secure(uint16_t conn_handle, bool force);

static void delayed_conn_secure(void *context)
{
	conn_secure_context_t sec_context = {.p_void = context};

	_conn_secure(sec_context.values.conn_handle, sec_context.values.force);
}

static void conn_secure(uint16_t conn_handle, bool force)
{
	int err;
	static bool created;

	if (!created) {
		err = bm_timer_init(&secure_delay_timer, BM_TIMER_MODE_SINGLE_SHOT,
					delayed_conn_secure);
		APP_ERROR_CHECK(err);
		created = true;
	}

	conn_secure_context_t sec_context = {0};

	sec_context.values.conn_handle = conn_handle;
	sec_context.values.force = force;

	err = bm_timer_start(secure_delay_timer,
				   BM_TIMER_MS_TO_TICKS(CONFIG_PM_HANDLER_SEC_DELAY_MS),
				   sec_context.p_void);
	APP_ERROR_CHECK(err);
}

#else
static void conn_secure(uint16_t conn_handle, bool force)
{
	_conn_secure(conn_handle, force);
}
#endif

void pm_handler_on_pm_evt(pm_evt_t const *p_pm_evt)
{
	pm_handler_pm_evt_log(p_pm_evt);

	if (p_pm_evt->evt_id == PM_EVT_BONDED_PEER_CONNECTED) {
		conn_secure(p_pm_evt->conn_handle, false);
	} else if (p_pm_evt->evt_id == PM_EVT_ERROR_UNEXPECTED) {
		LOG_ERR("Asserting.");
		APP_ERROR_CHECK(p_pm_evt->params.error_unexpected.error);
	}
}

void pm_handler_flash_clean_on_return(void)
{
	/* Trigger the mechanism to make more room in flash. */
	pm_evt_t storage_full_evt = {.evt_id = PM_EVT_STORAGE_FULL};

	pm_handler_flash_clean(&storage_full_evt);
}

static void rank_highest(pm_peer_id_t peer_id)
{
	/* Trigger a pm_peer_rank_highest() with internal bookkeeping. */
	pm_evt_t connected_evt = {.evt_id = PM_EVT_BONDED_PEER_CONNECTED, .peer_id = peer_id};

	pm_handler_flash_clean(&connected_evt);
}

void pm_handler_flash_clean(pm_evt_t const *p_pm_evt)
{
	uint32_t err_code;
	/* Indicates whether garbage collection is currently being run. */
	static bool flash_cleaning;
	/* Indicates whether a successful write happened after the last garbage
	 * collection. If this is false when flash is full, it means just a
	 * garbage collection won't work, so some data should be deleted.
	 */
	static bool flash_write_after_gc = true;

/* Size of rank_queue. */
#define RANK_QUEUE_SIZE	  8
/* Initial value of rank_queue. */
#define RANK_QUEUE_INIT PM_PEER_ID_INVALID,

	/* Queue of rank_highest calls that failed because of full flash. */
	static pm_peer_id_t rank_queue[8] = {[0 ... RANK_QUEUE_SIZE - 1] = RANK_QUEUE_INIT};
	/* Write pointer for rank_queue. */
	static int rank_queue_wr;

	switch (p_pm_evt->evt_id) {
	case PM_EVT_BONDED_PEER_CONNECTED:
		err_code = pm_peer_rank_highest(p_pm_evt->peer_id);
		if ((err_code == NRF_ERROR_RESOURCES) || (err_code == NRF_ERROR_BUSY)) {
			/* Queue pm_peer_rank_highest() call and attempt to clean flash. */
			rank_queue[rank_queue_wr] = p_pm_evt->peer_id;
			rank_queue_wr = (rank_queue_wr + 1) % RANK_QUEUE_SIZE;
			pm_handler_flash_clean_on_return();
		} else if ((err_code != NRF_ERROR_NOT_SUPPORTED) &&
			   (err_code != NRF_ERROR_INVALID_PARAM) &&
			   (err_code != NRF_ERROR_DATA_SIZE)) {
			APP_ERROR_CHECK(err_code);
		} else {
			LOG_DBG("pm_peer_rank_highest() returned %s for peer id %d",
				nrf_strerror_get(err_code), p_pm_evt->peer_id);
		}
		break;

	case PM_EVT_CONN_SEC_START:
		break;

	case PM_EVT_CONN_SEC_SUCCEEDED:
		/* PM_CONN_SEC_PROCEDURE_ENCRYPTION in case peer was not recognized at connection
		 * time.
		 */
		if ((p_pm_evt->params.conn_sec_succeeded.procedure ==
		     PM_CONN_SEC_PROCEDURE_BONDING) ||
		    (p_pm_evt->params.conn_sec_succeeded.procedure ==
		     PM_CONN_SEC_PROCEDURE_ENCRYPTION)) {
			rank_highest(p_pm_evt->peer_id);
		}
		break;

	case PM_EVT_CONN_SEC_FAILED:
	case PM_EVT_CONN_SEC_CONFIG_REQ:
	case PM_EVT_CONN_SEC_PARAMS_REQ:
		break;

	case PM_EVT_STORAGE_FULL:
		if (!flash_cleaning) {
			err_code = NRF_SUCCESS;
			LOG_INF("Attempting to clean flash.");
			if (!flash_write_after_gc) {
				/* Check whether another user of FDS has deleted a record that can
				 * be GCed.
				 */
				fds_stat_t fds_stats;

				err_code = fds_stat(&fds_stats);
				APP_ERROR_CHECK(err_code);
				flash_write_after_gc = (fds_stats.dirty_records > 0);
			}
			if (!flash_write_after_gc) {
				pm_peer_id_t peer_id_to_delete;

				err_code = pm_peer_ranks_get(NULL, NULL, &peer_id_to_delete, NULL);
				if (err_code == NRF_SUCCESS) {
					LOG_INF("Deleting lowest ranked peer (peer_id: %d)",
						peer_id_to_delete);
					err_code = pm_peer_delete(peer_id_to_delete);
					APP_ERROR_CHECK(err_code);
					flash_write_after_gc = true;
				}
				if (err_code == NRF_ERROR_NOT_FOUND) {
					LOG_ERR("There are no peers to delete.");
				} else if (err_code == NRF_ERROR_NOT_SUPPORTED) {
					LOG_WRN("Peer ranks functionality is disabled, so "
						"no peers are deleted.");
				} else {
					APP_ERROR_CHECK(err_code);
				}
			}
			if (err_code == NRF_SUCCESS) {
				err_code = fds_gc();
				if (err_code == NRF_SUCCESS) {
					LOG_DBG("Running flash garbage collection.");
					flash_cleaning = true;
				} else if (err_code != FDS_ERR_NO_SPACE_IN_QUEUES) {
					APP_ERROR_CHECK(err_code);
				}
			}
		}
		break;

	case PM_EVT_ERROR_UNEXPECTED:
		break;

	case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
		flash_write_after_gc = true;
		break;

	case PM_EVT_PEER_DATA_UPDATE_FAILED:
		break;

	case PM_EVT_PEER_DELETE_SUCCEEDED:
		flash_write_after_gc = true;
		break;

	case PM_EVT_PEER_DELETE_FAILED:
	case PM_EVT_PEERS_DELETE_SUCCEEDED:
	case PM_EVT_PEERS_DELETE_FAILED:
	case PM_EVT_LOCAL_DB_CACHE_APPLIED:
	case PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED:
	case PM_EVT_SERVICE_CHANGED_IND_SENT:
	case PM_EVT_SERVICE_CHANGED_IND_CONFIRMED:
	case PM_EVT_SLAVE_SECURITY_REQ:
		break;

	case PM_EVT_FLASH_GARBAGE_COLLECTED:
		flash_cleaning = false;
		flash_write_after_gc = false;
		{
			/* Reattempt queued pm_peer_rank_highest() calls. */
			int rank_queue_rd = rank_queue_wr;

			for (int i = 0; i < RANK_QUEUE_SIZE; i++) {
				pm_peer_id_t peer_id =
					rank_queue[(i + rank_queue_rd) % RANK_QUEUE_SIZE];
				if (peer_id != PM_PEER_ID_INVALID) {
					rank_queue[(i + rank_queue_rd) % RANK_QUEUE_SIZE] =
						PM_PEER_ID_INVALID;
					rank_highest(peer_id);
				}
			}
		}
		break;

	case PM_EVT_FLASH_GARBAGE_COLLECTION_FAILED:
		flash_cleaning = false;

		if (p_pm_evt->params.garbage_collection_failed.error == FDS_ERR_BUSY ||
		    p_pm_evt->params.garbage_collection_failed.error == FDS_ERR_OPERATION_TIMEOUT) {
			/* Retry immediately if error is transient. */
			pm_handler_flash_clean_on_return();
		}
		break;

	default:
		break;
	}
}

void pm_handler_pm_evt_log(pm_evt_t const *p_pm_evt)
{
	LOG_DBG("Event %s", m_event_str[p_pm_evt->evt_id]);

	switch (p_pm_evt->evt_id) {
	case PM_EVT_BONDED_PEER_CONNECTED:
		LOG_DBG("Previously bonded peer connected: role: %s, conn_handle: %d, peer_id: %d",
			m_roles_str[ble_conn_state_role(p_pm_evt->conn_handle)],
			p_pm_evt->conn_handle, p_pm_evt->peer_id);
		break;

	case PM_EVT_CONN_CONFIG_REQ:
		LOG_DBG("Connection configuration request");
		break;

	case PM_EVT_CONN_SEC_START:
		LOG_DBG("Connection security procedure started: role: %s, conn_handle: %d, "
			"procedure: %s",
			m_roles_str[ble_conn_state_role(p_pm_evt->conn_handle)],
			p_pm_evt->conn_handle,
			m_sec_procedure_str[p_pm_evt->params.conn_sec_start.procedure]);
		break;

	case PM_EVT_CONN_SEC_SUCCEEDED:
		LOG_INF("Connection secured: role: %s, conn_handle: %d, procedure: %s",
			m_roles_str[ble_conn_state_role(p_pm_evt->conn_handle)],
			p_pm_evt->conn_handle,
			m_sec_procedure_str[p_pm_evt->params.conn_sec_start.procedure]);
		break;

	case PM_EVT_CONN_SEC_FAILED:
		LOG_INF("Connection security failed: role: %s, conn_handle: 0x%x, procedure: "
			"%s, error: %d",
			m_roles_str[ble_conn_state_role(p_pm_evt->conn_handle)],
			p_pm_evt->conn_handle,
			m_sec_procedure_str[p_pm_evt->params.conn_sec_start.procedure],
			p_pm_evt->params.conn_sec_failed.error);
		LOG_DBG("Error (decoded): %s",
			sec_err_string_get(p_pm_evt->params.conn_sec_failed.error));
		break;

	case PM_EVT_CONN_SEC_CONFIG_REQ:
		LOG_DBG("Security configuration request");
		break;

	case PM_EVT_CONN_SEC_PARAMS_REQ:
		LOG_DBG("Security parameter request");
		break;

	case PM_EVT_STORAGE_FULL:
		LOG_WRN("Flash storage is full");
		break;

	case PM_EVT_ERROR_UNEXPECTED:
		LOG_ERR("Unexpected fatal error occurred: error: %s",
			nrf_strerror_get(p_pm_evt->params.error_unexpected.error));
		break;

	case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
		LOG_INF("Peer data updated in flash: peer_id: %d, data_id: %s, action: %s%s",
			p_pm_evt->peer_id,
			m_data_id_str[p_pm_evt->params.peer_data_update_succeeded.data_id],
			m_data_action_str[p_pm_evt->params.peer_data_update_succeeded.action],
			p_pm_evt->params.peer_data_update_succeeded.flash_changed
				? ""
				: ", no change");
		break;

	case PM_EVT_PEER_DATA_UPDATE_FAILED:
		/* This can happen if the SoftDevice is too busy with BLE operations. */
		LOG_WRN("Peer data updated failed: peer_id: %d, data_id: %s, action: %s, error: %s",
			p_pm_evt->peer_id,
			m_data_id_str[p_pm_evt->params.peer_data_update_failed.data_id],
			m_data_action_str[p_pm_evt->params.peer_data_update_succeeded.action],
			nrf_strerror_get(p_pm_evt->params.peer_data_update_failed.error));
		break;

	case PM_EVT_PEER_DELETE_SUCCEEDED:
		LOG_ERR("Peer deleted successfully: peer_id: %d", p_pm_evt->peer_id);
		break;

	case PM_EVT_PEER_DELETE_FAILED:
		LOG_ERR("Peer deletion failed: peer_id: %d, error: %s", p_pm_evt->peer_id,
			nrf_strerror_get(p_pm_evt->params.peer_delete_failed.error));
		break;

	case PM_EVT_PEERS_DELETE_SUCCEEDED:
		LOG_INF("All peers deleted.");
		break;

	case PM_EVT_PEERS_DELETE_FAILED:
		LOG_ERR("All peer deletion failed: error: %s",
			nrf_strerror_get(p_pm_evt->params.peers_delete_failed_evt.error));
		break;

	case PM_EVT_LOCAL_DB_CACHE_APPLIED:
		LOG_DBG("Previously stored local DB applied: conn_handle: %d, peer_id: %d",
			p_pm_evt->conn_handle, p_pm_evt->peer_id);
		break;

	case PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED:
		/* This can happen when the local DB has changed. */
		LOG_WRN("Local DB could not be applied: conn_handle: %d, peer_id: %d",
			p_pm_evt->conn_handle, p_pm_evt->peer_id);
		break;

	case PM_EVT_SERVICE_CHANGED_IND_SENT:
		LOG_DBG("Sending Service Changed indication.");
		break;

	case PM_EVT_SERVICE_CHANGED_IND_CONFIRMED:
		LOG_DBG("Service Changed indication confirmed.");
		break;

	case PM_EVT_SLAVE_SECURITY_REQ:
		LOG_DBG("Security Request received from peer.");
		break;

	case PM_EVT_FLASH_GARBAGE_COLLECTED:
		LOG_DBG("Flash garbage collection complete.");
		break;

	case PM_EVT_FLASH_GARBAGE_COLLECTION_FAILED:
		LOG_WRN("Flash garbage collection failed with error %s.",
			nrf_strerror_get(p_pm_evt->params.garbage_collection_failed.error));
		break;

	default:
		LOG_WRN("Unexpected PM event ID: 0x%x.", p_pm_evt->evt_id);
		break;
	}
}

void pm_handler_disconnect_on_sec_failure(pm_evt_t const *p_pm_evt)
{
	uint32_t err_code;

	if (p_pm_evt->evt_id == PM_EVT_CONN_SEC_FAILED) {
		LOG_WRN("Disconnecting conn_handle %d.", p_pm_evt->conn_handle);
		err_code = sd_ble_gap_disconnect(p_pm_evt->conn_handle,
						 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if ((err_code != NRF_ERROR_INVALID_STATE) &&
		    (err_code != BLE_ERROR_INVALID_CONN_HANDLE)) {
			APP_ERROR_CHECK(err_code);
		}
	}
}

void pm_handler_disconnect_on_insufficient_sec(pm_evt_t const *p_pm_evt,
					       pm_conn_sec_status_t *p_min_conn_sec)
{
	if (p_pm_evt->evt_id == PM_EVT_CONN_SEC_SUCCEEDED) {
		if (!pm_sec_is_sufficient(p_pm_evt->conn_handle, p_min_conn_sec)) {
			LOG_WRN("Connection security is insufficient, disconnecting.");
			uint32_t err_code = sd_ble_gap_disconnect(
				p_pm_evt->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
			APP_ERROR_CHECK(err_code);
		}
	}
}

void pm_handler_secure_on_connection(ble_evt_t const *p_ble_evt)
{
	switch (p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_DBG("Connected, securing connection. conn_handle: %d",
			p_ble_evt->evt.gap_evt.conn_handle);
		conn_secure(p_ble_evt->evt.gap_evt.conn_handle, false);
		break;

#if CONFIG_PM_HANDLER_SEC_DELAY_MS > 0
	case BLE_GAP_EVT_DISCONNECTED: {
		int err = bm_timer_stop(&secure_delay_timer);

		APP_ERROR_CHECK(err);
	} break;
#endif

	default:
		break;
	}
}

void pm_handler_secure_on_error(ble_evt_t const *p_ble_evt)
{
	if ((p_ble_evt->header.evt_id >= BLE_GATTC_EVT_BASE) &&
	    (p_ble_evt->header.evt_id <= BLE_GATTC_EVT_LAST)) {
		if ((p_ble_evt->evt.gattc_evt.gatt_status ==
		     BLE_GATT_STATUS_ATTERR_INSUF_ENCRYPTION) ||
		    (p_ble_evt->evt.gattc_evt.gatt_status ==
		     BLE_GATT_STATUS_ATTERR_INSUF_AUTHENTICATION)) {
			LOG_INF("GATTC procedure (evt id 0x%x) failed because it needs "
				"encryption. Bonding: conn_handle=%d",
				p_ble_evt->header.evt_id,
				p_ble_evt->evt.gattc_evt.conn_handle);
			conn_secure(p_ble_evt->evt.gattc_evt.conn_handle, true);
		}
	}
}
