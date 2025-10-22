/*
 * Copyright (c) 2018-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <nrf_strerror.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ble_gap.h>
#include <ble_gattc.h>
#include <bm/bluetooth/ble_conn_state.h>
#include <bm/bluetooth/peer_manager/peer_manager.h>
#include <bm/bluetooth/peer_manager/peer_manager_handler.h>
#if CONFIG_PM_HANDLER_SEC_DELAY_MS > 0
#include <bm/bm_timer.h>
#endif

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(peer_manager, CONFIG_PEER_MANAGER_LOG_LEVEL);

#define APP_ERROR_CHECK(err) (void)(err)

static const char *const roles_str[] = {
	"Invalid Role",
	"Peripheral",
	"Central",
};

static const char *const sec_procedure_str[] = {
	"Encryption",
	"Bonding",
	"Pairing",
};

#define PM_EVT_STR(_name) [_name] = STRINGIFY(_name)

static const char *const event_str[] = {
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

static const char *const data_id_str[] = {
	"Outdated (0)",	    "Service changed pending flag",
	"Outdated (2)",	    "Outdated (3)",
	"Application data", "Remote database",
	"Peer rank",	    "Bonding data",
	"Local database",   "Central address resolution",
};

static const char *const data_action_str[] = {"Update", "Delete"};

#define PM_SEC_ERR_STR(_name)                                                                      \
	{                                                                                          \
		.error = _name, .error_str = #_name                                                \
	}

struct sec_err_str {
	uint16_t error;
	const char *error_str;
};

static const struct sec_err_str pm_sec_error_str[] = {
	PM_SEC_ERR_STR(PM_CONN_SEC_ERROR_PIN_OR_KEY_MISSING),
	PM_SEC_ERR_STR(PM_CONN_SEC_ERROR_MIC_FAILURE),
	PM_SEC_ERR_STR(PM_CONN_SEC_ERROR_DISCONNECT),
	PM_SEC_ERR_STR(PM_CONN_SEC_ERROR_SMP_TIMEOUT),
};

static const char *sec_err_string_get(uint16_t error)
{
	static char errstr[30];

	for (uint32_t i = 0; i < (sizeof(pm_sec_error_str) / sizeof(struct sec_err_str)); i++) {
		if (pm_sec_error_str[i].error == error) {
			return pm_sec_error_str[i].error_str;
		}
	}

	(void)snprintf(errstr, sizeof(errstr), "%s 0x%hx",
			   (error < PM_CONN_SEC_ERROR_BASE) ? "BLE_GAP_SEC_STATUS"
							    : "PM_CONN_SEC_ERROR",
			   error);
	return errstr;
}

static void conn_secure_impl(uint16_t conn_handle, bool force)
{
	uint32_t nrf_err;

	if (!force) {
		struct pm_conn_sec_status status;

		nrf_err = pm_conn_sec_status_get(conn_handle, &status);
		if (nrf_err != BLE_ERROR_INVALID_CONN_HANDLE) {
			APP_ERROR_CHECK(nrf_err);
		}

		/* If the link is already secured, don't initiate security procedure. */
		if (status.encrypted) {
			LOG_DBG("Already encrypted, skipping security.");
			return;
		}
	}

	nrf_err = pm_conn_secure(conn_handle, false);

	if ((nrf_err == NRF_SUCCESS) || (nrf_err == NRF_ERROR_BUSY)) {
		/* Success. */
	} else if (nrf_err == NRF_ERROR_TIMEOUT) {
		LOG_WRN("pm_conn_secure() failed because an SMP timeout is preventing security on "
			"the link. Disconnecting conn_handle %d.",
			conn_handle);

		nrf_err = sd_ble_gap_disconnect(conn_handle,
						BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (nrf_err) {
			LOG_WRN("sd_ble_gap_disconnect() returned %s on conn_handle %d.",
				nrf_strerror_get(nrf_err), conn_handle);
		}
	} else if (nrf_err == NRF_ERROR_INVALID_DATA) {
		LOG_WRN("pm_conn_secure() failed because the stored data for conn_handle %d does "
			"not have a valid key.",
			conn_handle);
	} else if (nrf_err == BLE_ERROR_INVALID_CONN_HANDLE) {
		LOG_WRN("pm_conn_secure() failed because conn_handle %d is not a valid connection.",
			conn_handle);
	} else {
		LOG_ERR("Asserting. pm_conn_secure() returned %s on conn_handle %d.",
			nrf_strerror_get(nrf_err), conn_handle);
		APP_ERROR_CHECK(nrf_err);
	}
}

#if CONFIG_PM_HANDLER_SEC_DELAY_MS > 0
static struct bm_timer secure_delay_timer;

union conn_secure_context {
	struct {
		uint16_t conn_handle;
		bool force;
	} values;
	void *ptr;
};

BUILD_ASSERT(sizeof(union conn_secure_context) <= sizeof(void *),
	     "The size of 'union conn_secure_context' must be smaller than the size of a pointer");

static void delayed_conn_secure(void *context)
{
	/* The context argument is data and not a valid address. Copy it. */
	union conn_secure_context sec_context = {.ptr = context};

	conn_secure_impl(sec_context.values.conn_handle, sec_context.values.force);
}
#endif

static void conn_secure(uint16_t conn_handle, bool force)
{
#if CONFIG_PM_HANDLER_SEC_DELAY_MS > 0
	int err;
	static bool created;
	union conn_secure_context sec_context = {
		.values = {
			.conn_handle = conn_handle,
			.force = force,
		},
	};

	if (!created) {
		err = bm_timer_init(&secure_delay_timer, BM_TIMER_MODE_SINGLE_SHOT,
				    delayed_conn_secure);
		APP_ERROR_CHECK(err);
		created = true;
	}

	/* The conn_secure_context is smaller than a pointer and is copied into the context
	 * argument itself. The passed context pointer is not a valid address, it is data.
	   It is fine for sec_context to go out of scope because the values are copied.
	 */
	err = bm_timer_start(&secure_delay_timer,
			     BM_TIMER_MS_TO_TICKS(CONFIG_PM_HANDLER_SEC_DELAY_MS),
			     sec_context.ptr);
	APP_ERROR_CHECK(err);
#else
	conn_secure_impl(conn_handle, force);
#endif
}

void pm_handler_on_pm_evt(const struct pm_evt *pm_evt)
{
	pm_handler_pm_evt_log(pm_evt);

	if (pm_evt->evt_id == PM_EVT_BONDED_PEER_CONNECTED) {
		conn_secure(pm_evt->conn_handle, false);
	} else if (pm_evt->evt_id == PM_EVT_ERROR_UNEXPECTED) {
		LOG_ERR("Asserting.");
		APP_ERROR_CHECK(pm_evt->params.error_unexpected.error);
	}
}

void pm_handler_flash_clean_on_return(void)
{
	/* Trigger the mechanism to make more room in flash. */
	struct pm_evt storage_full_evt = { .evt_id = PM_EVT_STORAGE_FULL };

	pm_handler_flash_clean(&storage_full_evt);
}

static void rank_highest(uint16_t peer_id)
{
	/* Trigger a pm_peer_rank_highest() with internal bookkeeping. */
	struct pm_evt connected_evt = {.evt_id = PM_EVT_BONDED_PEER_CONNECTED, .peer_id = peer_id};

	pm_handler_flash_clean(&connected_evt);
}

void pm_handler_flash_clean(const struct pm_evt *pm_evt)
{
	uint32_t nrf_err;
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
	static uint16_t rank_queue[8] = {[0 ... RANK_QUEUE_SIZE - 1] = RANK_QUEUE_INIT};
	/* Write pointer for rank_queue. */
	static int rank_queue_wr;

	switch (pm_evt->evt_id) {
	case PM_EVT_BONDED_PEER_CONNECTED:
		nrf_err = pm_peer_rank_highest(pm_evt->peer_id);
		if ((nrf_err == NRF_ERROR_RESOURCES) || (nrf_err == NRF_ERROR_BUSY)) {
			/* Queue pm_peer_rank_highest() call and attempt to clean flash. */
			rank_queue[rank_queue_wr] = pm_evt->peer_id;
			rank_queue_wr = (rank_queue_wr + 1) % RANK_QUEUE_SIZE;
			pm_handler_flash_clean_on_return();
		} else if ((nrf_err != NRF_ERROR_NOT_SUPPORTED) &&
			   (nrf_err != NRF_ERROR_INVALID_PARAM) &&
			   (nrf_err != NRF_ERROR_DATA_SIZE)) {
			APP_ERROR_CHECK(nrf_err);
		} else {
			LOG_DBG("pm_peer_rank_highest() returned %s for peer id %d",
				nrf_strerror_get(nrf_err), pm_evt->peer_id);
		}
		break;

	case PM_EVT_CONN_SEC_START:
		break;

	case PM_EVT_CONN_SEC_SUCCEEDED:
		/* PM_CONN_SEC_PROCEDURE_ENCRYPTION in case peer was not recognized at connection
		 * time.
		 */
		if ((pm_evt->params.conn_sec_succeeded.procedure ==
		     PM_CONN_SEC_PROCEDURE_BONDING) ||
		    (pm_evt->params.conn_sec_succeeded.procedure ==
		     PM_CONN_SEC_PROCEDURE_ENCRYPTION)) {
			rank_highest(pm_evt->peer_id);
		}
		break;

	case PM_EVT_CONN_SEC_FAILED:
	case PM_EVT_CONN_SEC_CONFIG_REQ:
	case PM_EVT_CONN_SEC_PARAMS_REQ:
		break;

	case PM_EVT_STORAGE_FULL:
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
	default:
		break;
	}
}

void pm_handler_pm_evt_log(const struct pm_evt *pm_evt)
{
	LOG_DBG("Event %s", event_str[pm_evt->evt_id]);

	switch (pm_evt->evt_id) {
	case PM_EVT_BONDED_PEER_CONNECTED:
		LOG_DBG("Previously bonded peer connected: role: %s, conn_handle: %d, peer_id: %d",
			roles_str[ble_conn_state_role(pm_evt->conn_handle)],
			pm_evt->conn_handle, pm_evt->peer_id);
		break;

	case PM_EVT_CONN_CONFIG_REQ:
		LOG_DBG("Connection configuration request");
		break;

	case PM_EVT_CONN_SEC_START:
		LOG_DBG("Connection security procedure started: role: %s, conn_handle: %d, "
			"procedure: %s",
			roles_str[ble_conn_state_role(pm_evt->conn_handle)],
			pm_evt->conn_handle,
			sec_procedure_str[pm_evt->params.conn_sec_start.procedure]);
		break;

	case PM_EVT_CONN_SEC_SUCCEEDED:
		LOG_INF("Connection secured: role: %s, conn_handle: %d, procedure: %s",
			roles_str[ble_conn_state_role(pm_evt->conn_handle)],
			pm_evt->conn_handle,
			sec_procedure_str[pm_evt->params.conn_sec_start.procedure]);
		break;

	case PM_EVT_CONN_SEC_FAILED:
		LOG_INF("Connection security failed: role: %s, conn_handle: 0x%x, procedure: "
			"%s, error: %d",
			roles_str[ble_conn_state_role(pm_evt->conn_handle)],
			pm_evt->conn_handle,
			sec_procedure_str[pm_evt->params.conn_sec_start.procedure],
			pm_evt->params.conn_sec_failed.error);
		LOG_DBG("Error (decoded): %s",
			sec_err_string_get(pm_evt->params.conn_sec_failed.error));
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
			nrf_strerror_get(pm_evt->params.error_unexpected.error));
		break;

	case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
		LOG_INF("Peer data updated in flash: peer_id: %d, data_id: %s, action: %s%s",
			pm_evt->peer_id,
			data_id_str[pm_evt->params.peer_data_update_succeeded.data_id],
			data_action_str[pm_evt->params.peer_data_update_succeeded.action],
			pm_evt->params.peer_data_update_succeeded.flash_changed
				? ""
				: ", no change");
		break;

	case PM_EVT_PEER_DATA_UPDATE_FAILED:
		/* This can happen if the SoftDevice is too busy with BLE operations. */
		LOG_WRN("Peer data updated failed: peer_id: %d, data_id: %s, action: %s, error: %s",
			pm_evt->peer_id,
			data_id_str[pm_evt->params.peer_data_update_failed.data_id],
			data_action_str[pm_evt->params.peer_data_update_succeeded.action],
			nrf_strerror_get(pm_evt->params.peer_data_update_failed.error));
		break;

	case PM_EVT_PEER_DELETE_SUCCEEDED:
		LOG_INF("Peer deleted successfully: peer_id: %d", pm_evt->peer_id);
		break;

	case PM_EVT_PEER_DELETE_FAILED:
		LOG_ERR("Peer deletion failed: peer_id: %d, error: %s", pm_evt->peer_id,
			nrf_strerror_get(pm_evt->params.peer_delete_failed.error));
		break;

	case PM_EVT_PEERS_DELETE_SUCCEEDED:
		LOG_INF("All peers deleted.");
		break;

	case PM_EVT_PEERS_DELETE_FAILED:
		LOG_ERR("All peer deletion failed: error: %s",
			nrf_strerror_get(pm_evt->params.peers_delete_failed_evt.error));
		break;

	case PM_EVT_LOCAL_DB_CACHE_APPLIED:
		LOG_DBG("Previously stored local DB applied: conn_handle: %d, peer_id: %d",
			pm_evt->conn_handle, pm_evt->peer_id);
		break;

	case PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED:
		/* This can happen when the local DB has changed. */
		LOG_WRN("Local DB could not be applied: conn_handle: %d, peer_id: %d",
			pm_evt->conn_handle, pm_evt->peer_id);
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
			nrf_strerror_get(pm_evt->params.garbage_collection_failed.error));
		break;

	default:
		LOG_WRN("Unexpected PM event ID: 0x%x.", pm_evt->evt_id);
		break;
	}
}

void pm_handler_disconnect_on_sec_failure(const struct pm_evt *pm_evt)
{
	uint32_t nrf_err;

	if (pm_evt->evt_id == PM_EVT_CONN_SEC_FAILED) {
		LOG_WRN("Disconnecting conn_handle %d.", pm_evt->conn_handle);
		nrf_err = sd_ble_gap_disconnect(pm_evt->conn_handle,
						BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if ((nrf_err != NRF_ERROR_INVALID_STATE) &&
		    (nrf_err != BLE_ERROR_INVALID_CONN_HANDLE)) {
			APP_ERROR_CHECK(nrf_err);
		}
	}
}

void pm_handler_disconnect_on_insufficient_sec(const struct pm_evt *pm_evt,
					       struct pm_conn_sec_status *min_conn_sec)
{
	if (pm_evt->evt_id == PM_EVT_CONN_SEC_SUCCEEDED) {
		if (!pm_sec_is_sufficient(pm_evt->conn_handle, min_conn_sec)) {
			LOG_WRN("Connection security is insufficient, disconnecting.");
			uint32_t nrf_err = sd_ble_gap_disconnect(
				pm_evt->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
			APP_ERROR_CHECK(nrf_err);
			LOG_ERR("sd_ble_gap_disconnect() error 0x%x", nrf_err);
		}
	}
}

void pm_handler_secure_on_connection(const ble_evt_t *ble_evt)
{
	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_DBG("Connected, securing connection. conn_handle: %d",
			ble_evt->evt.gap_evt.conn_handle);
		conn_secure(ble_evt->evt.gap_evt.conn_handle, false);
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

void pm_handler_secure_on_error(const ble_evt_t *ble_evt)
{
	if ((ble_evt->header.evt_id >= BLE_GATTC_EVT_BASE) &&
	    (ble_evt->header.evt_id <= BLE_GATTC_EVT_LAST)) {
		if ((ble_evt->evt.gattc_evt.gatt_status ==
		     BLE_GATT_STATUS_ATTERR_INSUF_ENCRYPTION) ||
		    (ble_evt->evt.gattc_evt.gatt_status ==
		     BLE_GATT_STATUS_ATTERR_INSUF_AUTHENTICATION)) {
			LOG_INF("GATTC procedure (evt id 0x%x) failed because it needs "
				"encryption. Bonding: conn_handle=%d",
				ble_evt->header.evt_id,
				ble_evt->evt.gattc_evt.conn_handle);
			conn_secure(ble_evt->evt.gattc_evt.conn_handle, true);
		}
	}
}
