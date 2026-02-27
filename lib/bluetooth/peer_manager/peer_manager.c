/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <nrf_strerror.h>
#include <string.h>

#include <ble_err.h>
#include <bm/bluetooth/peer_manager/peer_manager.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>

#include <modules/conn_state.h>
#include <modules/security_manager.h>
#include <modules/security_dispatcher.h>
#include <modules/gatt_cache_manager.h>
#include <modules/gatts_cache_manager.h>
#include <modules/peer_database.h>
#include <modules/peer_data_storage.h>
#include <modules/id_manager.h>
#include <modules/peer_manager_internal.h>

#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(peer_manager, CONFIG_PEER_MANAGER_LOG_LEVEL);

/** Whether or not @ref pm_init has been called successfully. */
static bool module_initialized;
/** Whether or not @ref rank_init has been called successfully. */
static bool peer_rank_initialized;
/** True from when @ref pm_peers_delete is called until all peers have been deleted. */
static bool deleting_all;
/** The store token of an ongoing peer rank update via a call to @ref pm_peer_rank_highest. If @ref
 * PM_STORE_TOKEN_INVALID, there is no ongoing update.
 */
static uint32_t peer_rank_token;
/** The current highest peer rank. Used by @ref pm_peer_rank_highest. */
static uint32_t current_highest_peer_rank;
/** The peer with the highest peer rank. Used by @ref pm_peer_rank_highest. */
static uint16_t highest_ranked_peer;
/** The subscribers to Peer Manager events, as registered through @ref pm_register. */
static pm_evt_handler_t evt_handlers[CONFIG_PM_MAX_REGISTRANTS];
/** The number of event handlers registered through @ref pm_register. */
static uint8_t n_registrants;

/** User flag indicating whether a connection is excluded from being handled by the Peer Manager. */
static int flag_conn_excluded = PM_CONN_STATE_USER_FLAG_INVALID;

/**
 * @brief Function for sending a Peer Manager event to all subscribers.
 *
 * @param[in]  pm_evt  The event to send.
 */
static void evt_send(const struct pm_evt *pm_evt)
{
	for (int i = 0; i < n_registrants; i++) {
		evt_handlers[i](pm_evt);
	}
}

#if defined(CONFIG_PM_PEER_RANKS)
/** @brief Function for initializing peer rank static variables. */
static void rank_vars_update(void)
{
	uint32_t nrf_err =
		pm_peer_ranks_get(&highest_ranked_peer, &current_highest_peer_rank, NULL, NULL);

	if (nrf_err == NRF_ERROR_NOT_FOUND) {
		highest_ranked_peer = PM_PEER_ID_INVALID;
		current_highest_peer_rank = 0;
	}

	peer_rank_initialized = ((nrf_err == NRF_SUCCESS) || (nrf_err == NRF_ERROR_NOT_FOUND));
}
#endif

/**
 * @brief Event handler for events from the Peer Database module.
 *        This handler is extern in the Peer Database module.
 *
 * @param[in]  pdb_evt  The incoming Peer Database event.
 */
void pm_pdb_evt_handler(struct pm_evt *pdb_evt)
{
	bool send_evt = true;

	pdb_evt->conn_handle = im_conn_handle_get(pdb_evt->peer_id);

	switch (pdb_evt->evt_id) {
#if defined(CONFIG_PM_PEER_RANKS)
	case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
		if (pdb_evt->params.peer_data_update_succeeded.action == PM_PEER_DATA_OP_UPDATE) {
			if ((peer_rank_token != PM_STORE_TOKEN_INVALID) &&
			    (peer_rank_token == pdb_evt->params.peer_data_update_succeeded.token)) {
				peer_rank_token = PM_STORE_TOKEN_INVALID;
				highest_ranked_peer = pdb_evt->peer_id;

				pdb_evt->params.peer_data_update_succeeded.token =
					PM_STORE_TOKEN_INVALID;
			} else if (peer_rank_initialized &&
				   (pdb_evt->peer_id == highest_ranked_peer) &&
				   (pdb_evt->params.peer_data_update_succeeded.data_id ==
				    PM_PEER_DATA_ID_PEER_RANK)) {
				/* Update peer rank variable if highest ranked peer has changed its
				 * rank.
				 */
				rank_vars_update();
			}
		} else if (pdb_evt->params.peer_data_update_succeeded.action ==
			   PM_PEER_DATA_OP_DELETE) {
			if (peer_rank_initialized && (pdb_evt->peer_id == highest_ranked_peer) &&
			    (pdb_evt->params.peer_data_update_succeeded.data_id ==
			     PM_PEER_DATA_ID_PEER_RANK)) {
				/* Update peer rank variable if highest ranked peer has deleted its
				 * rank.
				 */
				rank_vars_update();
			}
		}
		break;

	case PM_EVT_PEER_DATA_UPDATE_FAILED:
		if (pdb_evt->params.peer_data_update_succeeded.action == PM_PEER_DATA_OP_UPDATE) {
			if ((peer_rank_token != PM_STORE_TOKEN_INVALID) &&
			    (peer_rank_token == pdb_evt->params.peer_data_update_failed.token)) {
				peer_rank_token = PM_STORE_TOKEN_INVALID;
				current_highest_peer_rank -= 1;

				pdb_evt->params.peer_data_update_succeeded.token =
					PM_STORE_TOKEN_INVALID;
			}
		}
		break;
#endif

	case PM_EVT_PEER_DELETE_SUCCEEDED:
		/* Check that no peers marked for deletion are left. */
		if (deleting_all &&
		    (pds_next_peer_id_get(PM_PEER_ID_INVALID) == PM_PEER_ID_INVALID) &&
		    (pds_next_deleted_peer_id_get(PM_PEER_ID_INVALID) == PM_PEER_ID_INVALID)) {
			/* pm_peers_delete() has been called and this is the last peer to be
			 * deleted.
			 */
			deleting_all = false;

			struct pm_evt pm_delete_all_evt;

			memset(&pm_delete_all_evt, 0, sizeof(struct pm_evt));
			pm_delete_all_evt.evt_id = PM_EVT_PEERS_DELETE_SUCCEEDED;
			pm_delete_all_evt.peer_id = PM_PEER_ID_INVALID;
			pm_delete_all_evt.conn_handle = BLE_CONN_HANDLE_INVALID;

			send_evt = false;

			/* Forward the event to all registered Peer Manager event handlers.
			 * Ensure that PEER_DELETE_SUCCEEDED arrives before PEERS_DELETE_SUCCEEDED.
			 */
			evt_send(pdb_evt);
			evt_send(&pm_delete_all_evt);
		}

#if defined(CONFIG_PM_PEER_RANKS)
		if (peer_rank_initialized && (pdb_evt->peer_id == highest_ranked_peer)) {
			/* Update peer rank variable if highest ranked peer has been deleted. */
			rank_vars_update();
		}
#endif
		break;

	case PM_EVT_PEER_DELETE_FAILED:
		if (deleting_all) {
			/* pm_peers_delete() was called and has thus failed. */

			deleting_all = false;

			struct pm_evt pm_delete_all_evt;

			memset(&pm_delete_all_evt, 0, sizeof(struct pm_evt));
			pm_delete_all_evt.evt_id = PM_EVT_PEERS_DELETE_FAILED;
			pm_delete_all_evt.peer_id = PM_PEER_ID_INVALID;
			pm_delete_all_evt.conn_handle = BLE_CONN_HANDLE_INVALID;
			pm_delete_all_evt.params.peers_delete_failed_evt.error =
				pdb_evt->params.peer_delete_failed.error;

			send_evt = false;

			/* Forward the event to all registered Peer Manager event handlers.
			 * Ensure that PEER_DELETE_FAILED arrives before PEERS_DELETE_FAILED.
			 */
			evt_send(pdb_evt);
			evt_send(&pm_delete_all_evt);
		}
		break;

	default:
		/* Do nothing. */
		break;
	}

	if (send_evt) {
		/* Forward the event to all registered Peer Manager event handlers. */
		evt_send(pdb_evt);
	}
}

/**
 * @brief Event handler for events from the Security Manager module.
 *        This handler is extern in the Security Manager module.
 *
 * @param[in]  sm_evt  The incoming Security Manager event.
 */
void pm_sm_evt_handler(struct pm_evt *sm_evt)
{
	if (sm_evt == NULL) {
		return;
	}

	/* Forward the event to all registered Peer Manager event handlers. */
	evt_send(sm_evt);
}

/**
 * @brief Event handler for events from the GATT Cache Manager module.
 *        This handler is extern in GATT Cache Manager.
 *
 * @param[in]  gcm_evt  The incoming GATT Cache Manager event.
 */
void pm_gcm_evt_handler(struct pm_evt *gcm_evt)
{
	/* Forward the event to all registered Peer Manager event handlers. */
	evt_send(gcm_evt);
}

/**
 * @brief Event handler for events from the GATTS Cache Manager module.
 *        This handler is extern in GATTS Cache Manager.
 *
 * @param[in]  gscm_evt  The incoming GATTS Cache Manager event.
 */
void pm_gscm_evt_handler(struct pm_evt *gscm_evt)
{
	/* Forward the event to all registered Peer Manager event handlers. */
	evt_send(gscm_evt);
}

/**
 * @brief Event handler for events from the ID Manager module.
 *        This function is registered in the ID Manager.
 *
 * @param[in]  im_evt  The incoming ID Manager event.
 */
void pm_im_evt_handler(struct pm_evt *im_evt)
{
	/* Forward the event to all registered Peer Manager event handlers. */
	evt_send(im_evt);
}

static bool is_conn_handle_excluded(const ble_evt_t *ble_evt)
{
	uint16_t conn_handle = ble_evt->evt.gap_evt.conn_handle;

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED: {
		struct pm_evt pm_conn_config_req_evt;
		bool is_excluded = false;

		memset(&pm_conn_config_req_evt, 0, sizeof(struct pm_evt));
		pm_conn_config_req_evt.evt_id = PM_EVT_CONN_CONFIG_REQ;
		pm_conn_config_req_evt.peer_id = PM_PEER_ID_INVALID;
		pm_conn_config_req_evt.conn_handle = conn_handle;

		pm_conn_config_req_evt.params.conn_config_req.peer_params =
			&ble_evt->evt.gap_evt.params.connected;
		pm_conn_config_req_evt.params.conn_config_req.context = &is_excluded;

		evt_send(&pm_conn_config_req_evt);
		pm_conn_state_user_flag_set(conn_handle, flag_conn_excluded, is_excluded);

		return is_excluded;
	}

	default:
		return pm_conn_state_user_flag_get(conn_handle, flag_conn_excluded);
	}
}

/**
 * @brief Function for handling BLE events.
 *
 * @param[in]   ble_evt       Event received from the BLE stack.
 * @param[in]   context       Context.
 */
static void ble_evt_handler(const ble_evt_t *ble_evt, void *context)
{
	if (!module_initialized) {
		return;
	}

	if (is_conn_handle_excluded(ble_evt)) {
		LOG_DBG("Filtering BLE event with ID: 0x%04X targeting 0x%04X connection handle",
			ble_evt->header.evt_id, ble_evt->evt.gap_evt.conn_handle);
		return;
	}

	im_ble_evt_handler(ble_evt);
	sm_ble_evt_handler(ble_evt);
	gcm_ble_evt_handler(ble_evt);
}

NRF_SDH_BLE_OBSERVER(ble_evt_observer, ble_evt_handler, NULL, HIGH);

/** @brief Function for resetting the internal state of this module. */
static void internal_state_reset(void)
{
	highest_ranked_peer = PM_PEER_ID_INVALID;
	peer_rank_token = PM_STORE_TOKEN_INVALID;
}

uint32_t pm_init(void)
{
	uint32_t nrf_err;

	pm_conn_state_init();

	nrf_err = pds_init();
	if (nrf_err) {
		LOG_ERR("%s failed because pds_init() returned %s.", __func__,
			nrf_strerror_get(nrf_err));
		return NRF_ERROR_INTERNAL;
	}

	nrf_err = pdb_init();
	if (nrf_err) {
		LOG_ERR("%s failed because pdb_init() returned %s.", __func__,
			nrf_strerror_get(nrf_err));
		return NRF_ERROR_INTERNAL;
	}

	nrf_err = sm_init();
	if (nrf_err) {
		LOG_ERR("%s failed because sm_init() returned %s.", __func__,
			nrf_strerror_get(nrf_err));
		return NRF_ERROR_INTERNAL;
	}

	nrf_err = smd_init();
	if (nrf_err) {
		LOG_ERR("%s failed because smd_init() returned %s.", __func__,
			nrf_strerror_get(nrf_err));
		return NRF_ERROR_INTERNAL;
	}

	nrf_err = gcm_init();
	if (nrf_err) {
		LOG_ERR("%s failed because gcm_init() returned %s.", __func__,
			nrf_strerror_get(nrf_err));
		return NRF_ERROR_INTERNAL;
	}

	nrf_err = gscm_init();
	if (nrf_err) {
		LOG_ERR("%s failed because gscm_init() returned %s.", __func__,
			nrf_strerror_get(nrf_err));
		return NRF_ERROR_INTERNAL;
	}

	internal_state_reset();

	peer_rank_initialized = false;
	module_initialized = true;

	flag_conn_excluded = pm_conn_state_user_flag_acquire();

	/* If CONFIG_PM_PEER_RANKS is 0, these variables are unused. */
	UNUSED_VARIABLE(peer_rank_initialized);
	UNUSED_VARIABLE(peer_rank_token);
	UNUSED_VARIABLE(current_highest_peer_rank);
	UNUSED_VARIABLE(highest_ranked_peer);

	return NRF_SUCCESS;
}

uint32_t pm_register(pm_evt_handler_t event_handler)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (n_registrants >= CONFIG_PM_MAX_REGISTRANTS) {
		return NRF_ERROR_NO_MEM;
	}

	evt_handlers[n_registrants] = event_handler;
	n_registrants += 1;

	return NRF_SUCCESS;
}

uint32_t pm_sec_params_set(ble_gap_sec_params_t *sec_params)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	return sm_sec_params_set(sec_params);
}

uint32_t pm_conn_secure(uint16_t conn_handle, bool force_repairing)
{
	uint32_t nrf_err;

	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	nrf_err = sm_link_secure(conn_handle, force_repairing);

	if (nrf_err == NRF_ERROR_INVALID_STATE) {
		nrf_err = NRF_ERROR_BUSY;
	}

	return nrf_err;
}

uint32_t pm_conn_exclude(uint16_t conn_handle, const void *context)
{
	if (context == NULL) {
		return NRF_ERROR_NULL;
	}

	bool *is_conn_excluded = (bool *)context;

	*is_conn_excluded = true;

	return NRF_SUCCESS;
}

void pm_conn_sec_config_reply(uint16_t conn_handle, struct pm_conn_sec_config *conn_sec_config)
{
	if (conn_sec_config != NULL) {
		sm_conn_sec_config_reply(conn_handle, conn_sec_config);
	}
}

uint32_t pm_conn_sec_params_reply(uint16_t conn_handle, ble_gap_sec_params_t *sec_params,
				  const void *context)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	return sm_sec_params_reply(conn_handle, sec_params, context);
}

void pm_local_database_has_changed(void)
{
#if defined(CONFIG_PM_SERVICE_CHANGED)
	if (!module_initialized) {
		return;
	}

	gcm_local_database_has_changed();
#endif
}

uint32_t pm_id_addr_set(const ble_gap_addr_t *addr)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	return im_id_addr_set(addr);
}

uint32_t pm_id_addr_get(ble_gap_addr_t *addr)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (addr == NULL) {
		return NRF_ERROR_NULL;
	}

	return im_id_addr_get(addr);
}

uint32_t pm_privacy_set(const ble_gap_privacy_params_t *privacy_params)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (privacy_params == NULL) {
		return NRF_ERROR_NULL;
	}

	return im_privacy_set(privacy_params);
}

uint32_t pm_privacy_get(ble_gap_privacy_params_t *privacy_params)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (privacy_params == NULL || privacy_params->p_device_irk == NULL) {
		return NRF_ERROR_NULL;
	}

	return im_privacy_get(privacy_params);
}

bool pm_address_resolve(const ble_gap_addr_t *addr, const ble_gap_irk_t *irk)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	if ((addr == NULL) || (irk == NULL)) {
		return false;
	} else {
		return im_address_resolve(addr, irk);
	}
}

uint32_t pm_allow_list_set(const uint16_t *peers, uint32_t peer_cnt)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	return im_allow_list_set(peers, peer_cnt);
}

uint32_t pm_allow_list_get(ble_gap_addr_t *addrs, uint32_t *addr_cnt, ble_gap_irk_t *irks,
			   uint32_t *irk_cnt)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (((addrs == NULL) && (irks == NULL)) ||
	    ((addrs != NULL) && (addr_cnt == NULL)) ||
	    ((irks != NULL) && (irk_cnt == NULL))) {
		/* The buffers can't be both NULL, and if a buffer is provided its size must be
		 * specified.
		 */
		return NRF_ERROR_NULL;
	}

	return im_allow_list_get(addrs, addr_cnt, irks, irk_cnt);
}

uint32_t pm_device_identities_list_set(const uint16_t *peers, uint32_t peer_cnt)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	return im_device_identities_list_set(peers, peer_cnt);
}

uint32_t pm_conn_sec_status_get(uint16_t conn_handle, struct pm_conn_sec_status *conn_sec_status)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	return sm_conn_sec_status_get(conn_handle, conn_sec_status);
}

bool pm_sec_is_sufficient(uint16_t conn_handle, struct pm_conn_sec_status *sec_status_req)
{
	if (!module_initialized) {
		return false;
	}

	return sm_sec_is_sufficient(conn_handle, sec_status_req);
}

uint32_t pm_lesc_public_key_set(ble_gap_lesc_p256_pk_t *public_key)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	return sm_lesc_public_key_set(public_key);
}

uint32_t pm_conn_handle_get(uint16_t peer_id, uint16_t *conn_handle)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (conn_handle == NULL) {
		return NRF_ERROR_NULL;
	}

	*conn_handle = im_conn_handle_get(peer_id);
	return NRF_SUCCESS;
}

uint32_t pm_peer_id_get(uint16_t conn_handle, uint16_t *peer_id)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (peer_id == NULL) {
		return NRF_ERROR_NULL;
	}

	*peer_id = im_peer_id_get_by_conn_handle(conn_handle);
	return NRF_SUCCESS;
}

uint32_t pm_peer_count(void)
{
	if (!module_initialized) {
		return 0;
	}
	return pds_peer_count_get();
}

uint16_t pm_next_peer_id_get(uint16_t prev_peer_id)
{
	uint16_t next_peer_id = prev_peer_id;

	if (!module_initialized) {
		return PM_PEER_ID_INVALID;
	}

	do {
		next_peer_id = pds_next_peer_id_get(next_peer_id);
	} while (pds_peer_id_is_deleted(next_peer_id));

	return next_peer_id;
}

/**
 * @brief Function for checking if the peer has a valid Identity Resolving Key.
 *
 * @param[in] irk Pointer to the Identity Resolving Key.
 */
static bool peer_is_irk(const ble_gap_irk_t *const irk)
{
	for (uint32_t i = 0; i < ARRAY_SIZE(irk->irk); i++) {
		if (irk->irk[i] != 0) {
			return true;
		}
	}

	return false;
}

uint32_t pm_peer_id_list(uint16_t *peer_list, uint32_t *const list_size,
			 uint16_t first_peer_id, enum pm_peer_id_list_skip skip_id)
{
	uint32_t nrf_err;
	uint32_t size;
	uint32_t current_size = 0;
	struct pm_peer_data pm_car_data;
	struct pm_peer_data pm_bond_data;
	uint16_t current_peer_id = first_peer_id;
	const ble_gap_addr_t *gap_addr;
	bool skip_no_addr = skip_id & PM_PEER_ID_LIST_SKIP_NO_ID_ADDR;
	bool skip_no_irk = skip_id & PM_PEER_ID_LIST_SKIP_NO_IRK;
	bool skip_no_car = skip_id & PM_PEER_ID_LIST_SKIP_NO_CAR;

	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (peer_list == NULL || list_size == NULL) {
		return NRF_ERROR_NULL;
	}

	size = *list_size;

	if ((*list_size < 1) ||
	    (skip_id > (PM_PEER_ID_LIST_SKIP_NO_ID_ADDR | PM_PEER_ID_LIST_SKIP_ALL))) {
		return NRF_ERROR_INVALID_PARAM;
	}

	*list_size = 0;

	if (current_peer_id == PM_PEER_ID_INVALID) {
		current_peer_id = pm_next_peer_id_get(current_peer_id);

		if (current_peer_id == PM_PEER_ID_INVALID) {
			return NRF_SUCCESS;
		}
	}

	memset(&pm_car_data, 0, sizeof(struct pm_peer_data));
	memset(&pm_bond_data, 0, sizeof(struct pm_peer_data));

	while (current_peer_id != PM_PEER_ID_INVALID) {
		bool skip = false;

		if (skip_no_addr || skip_no_irk) {
			/* Get data */
			struct pm_peer_data_bonding bonding_data = { 0 };
			uint32_t bonding_data_size = sizeof(bonding_data);

			pm_bond_data.all_data = &bonding_data;

			nrf_err = pds_peer_data_read(current_peer_id, PM_PEER_DATA_ID_BONDING,
						     &pm_bond_data, &bonding_data_size);

			if (nrf_err == NRF_ERROR_NOT_FOUND) {
				skip = true;
			} else if (nrf_err) {
				return nrf_err;
			}

			/* Check data */
			if (skip_no_addr) {
				gap_addr = &bonding_data.peer_ble_id.id_addr_info;

				if ((gap_addr->addr_type != BLE_GAP_ADDR_TYPE_PUBLIC) &&
				    (gap_addr->addr_type != BLE_GAP_ADDR_TYPE_RANDOM_STATIC)) {
					skip = true;
				}
			}
			if (skip_no_irk) {
				if (!peer_is_irk(
					    &bonding_data.peer_ble_id.id_info)) {
					skip = true;
				}
			}
		}

		if (skip_no_car) {
			/* Get data */
			uint32_t central_addr_res = 0;
			uint32_t central_addr_res_size = sizeof(uint32_t);

			pm_car_data.all_data = &central_addr_res;

			nrf_err = pds_peer_data_read(current_peer_id,
						     PM_PEER_DATA_ID_CENTRAL_ADDR_RES,
						     &pm_car_data, &central_addr_res_size);

			if (nrf_err == NRF_ERROR_NOT_FOUND) {
				skip = true;
			} else if (nrf_err) {
				return nrf_err;
			}

			/* Check data */
			if (central_addr_res == 0) {
				skip = true;
			}
		}

		if (!skip) {
			peer_list[current_size++] = current_peer_id;

			if (current_size >= size) {
				break;
			}
		}

		current_peer_id = pm_next_peer_id_get(current_peer_id);
	}

	*list_size = current_size;

	return NRF_SUCCESS;
}

uint32_t pm_peer_data_load(uint16_t peer_id, enum pm_peer_data_id data_id, void *data,
			   uint32_t *length)
{
	struct pm_peer_data peer_data;

	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (data == NULL || length == NULL) {
		return NRF_ERROR_NULL;
	}

	memset(&peer_data, 0, sizeof(peer_data));
	peer_data.all_data = data;

	return pds_peer_data_read(peer_id, data_id, &peer_data, length);
}

uint32_t pm_peer_data_bonding_load(uint16_t peer_id, struct pm_peer_data_bonding *data)
{
	uint32_t length = sizeof(struct pm_peer_data_bonding);

	return pm_peer_data_load(peer_id, PM_PEER_DATA_ID_BONDING, data, &length);
}

uint32_t pm_peer_data_remote_db_load(uint16_t peer_id, struct ble_gatt_db_srv *data,
				     uint32_t *length)
{
	return pm_peer_data_load(peer_id, PM_PEER_DATA_ID_GATT_REMOTE, data, length);
}

uint32_t pm_peer_data_app_data_load(uint16_t peer_id, void *data, uint32_t *length)
{
	return pm_peer_data_load(peer_id, PM_PEER_DATA_ID_APPLICATION, data, length);
}

uint32_t pm_peer_data_store(uint16_t peer_id, enum pm_peer_data_id data_id, const void *data,
			      uint32_t length, uint32_t *token)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (data == NULL) {
		return NRF_ERROR_NULL;
	}

	if (!IS_ALIGNED(length, 4)) {
		return NRF_ERROR_INVALID_PARAM;
	}

	if (data_id == PM_PEER_DATA_ID_BONDING) {
		uint16_t dupl_peer_id;

		dupl_peer_id =
			im_find_duplicate_bonding_data((struct pm_peer_data_bonding *)data,
						       peer_id);

		if (dupl_peer_id != PM_PEER_ID_INVALID) {
			return NRF_ERROR_FORBIDDEN;
		}
	}

	struct pm_peer_data_const peer_data;

	memset(&peer_data, 0, sizeof(peer_data));
	peer_data.length_words = BYTES_TO_WORDS(length);
	peer_data.data_id = data_id;
	peer_data.all_data = data;

	return pds_peer_data_store(peer_id, &peer_data, token);
}

uint32_t pm_peer_data_bonding_store(uint16_t peer_id, const struct pm_peer_data_bonding *data,
				    uint32_t *token)
{
	return pm_peer_data_store(peer_id, PM_PEER_DATA_ID_BONDING, data,
				  ROUND_UP(sizeof(struct pm_peer_data_bonding), 4), token);
}

uint32_t pm_peer_data_remote_db_store(uint16_t peer_id, const struct ble_gatt_db_srv *data,
					uint32_t length, uint32_t *token)
{
	return pm_peer_data_store(peer_id, PM_PEER_DATA_ID_GATT_REMOTE, data, length, token);
}

uint32_t pm_peer_data_app_data_store(uint16_t peer_id, const void *data, uint32_t length,
				     uint32_t *token)
{
	return pm_peer_data_store(peer_id, PM_PEER_DATA_ID_APPLICATION, data, length, token);
}

uint32_t pm_peer_data_delete(uint16_t peer_id, enum pm_peer_data_id data_id)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (data_id == PM_PEER_DATA_ID_BONDING) {
		return NRF_ERROR_INVALID_PARAM;
	}

	return pds_peer_data_delete(peer_id, data_id);
}

uint32_t pm_peer_new(uint16_t *new_peer_id, struct pm_peer_data_bonding *bonding_data,
		     uint32_t *token)
{
	uint32_t nrf_err;
	uint16_t peer_id;
	uint16_t peer_id_iter;
	struct pm_peer_data_const peer_data;
	uint8_t peer_data_buffer[PM_PEER_DATA_MAX_SIZE] = { 0 };

	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (new_peer_id == NULL || bonding_data == NULL) {
		return NRF_ERROR_NULL;
	}

	memset(&peer_data, 0, sizeof(struct pm_peer_data_const));

	peer_data.all_data = peer_data_buffer;

	/* Search through existing bonds to look for a duplicate. */
	pds_peer_data_iterate_prepare(&peer_id_iter);

	while (pds_peer_data_iterate(PM_PEER_DATA_ID_BONDING, &peer_id, &peer_data,
		&peer_id_iter)) {
		if (im_is_duplicate_bonding_data(bonding_data, peer_data.bonding_data)) {
			*new_peer_id = peer_id;
			return NRF_SUCCESS;
		}
	}

	/* If no duplicate data is found, prepare to write a new bond to flash. */

	*new_peer_id = pds_peer_id_allocate();

	if (*new_peer_id == PM_PEER_ID_INVALID) {
		return NRF_ERROR_NO_MEM;
	}

	memset(&peer_data, 0, sizeof(struct pm_peer_data_const));

	peer_data.data_id = PM_PEER_DATA_ID_BONDING;
	peer_data.bonding_data = bonding_data;
	peer_data.length_words = BYTES_TO_WORDS(sizeof(struct pm_peer_data_bonding));

	nrf_err = pds_peer_data_store(*new_peer_id, &peer_data, token);

	if (nrf_err) {
		uint32_t nrf_err_free = im_peer_free(*new_peer_id);

		if (nrf_err_free) {
			LOG_ERR("Fatal error during cleanup of a failed call to %s. im_peer_free() "
				"returned %s. peer_id: %d",
				__func__, nrf_strerror_get(nrf_err_free), *new_peer_id);
			return NRF_ERROR_INTERNAL;
		}

		/* NRF_ERROR_RESOURCES, if no space in flash.
		 * NRF_ERROR_BUSY,         if flash filesystem was busy.
		 * NRF_ERROR_INVALID_ADDR, if bonding data is unaligned.
		 * NRF_ERROR_INTENRAL,     on internal error.
		 */
		return nrf_err;
	}

	return NRF_SUCCESS;
}

uint32_t pm_peer_delete(uint16_t peer_id)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	return im_peer_free(peer_id);
}

uint32_t pm_peers_delete(void)
{
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	deleting_all = true;

	uint16_t current_peer_id = pds_next_peer_id_get(PM_PEER_ID_INVALID);

	if (current_peer_id == PM_PEER_ID_INVALID) {
		/* No peers bonded. */
		deleting_all = false;

		struct pm_evt pm_delete_all_evt;

		memset(&pm_delete_all_evt, 0, sizeof(struct pm_evt));
		pm_delete_all_evt.evt_id = PM_EVT_PEERS_DELETE_SUCCEEDED;
		pm_delete_all_evt.peer_id = PM_PEER_ID_INVALID;
		pm_delete_all_evt.conn_handle = BLE_CONN_HANDLE_INVALID;

		evt_send(&pm_delete_all_evt);
	}

	while (current_peer_id != PM_PEER_ID_INVALID) {
		uint32_t nrf_err = pm_peer_delete(current_peer_id);

		if (nrf_err) {
			LOG_ERR("%s() failed because a call to pm_peer_delete() returned %s. "
				"peer_id: %d",
				__func__, nrf_strerror_get(nrf_err), current_peer_id);
			return NRF_ERROR_INTERNAL;
		}

		current_peer_id = pds_next_peer_id_get(current_peer_id);
	}

	return NRF_SUCCESS;
}

uint32_t pm_peer_ranks_get(uint16_t *highest_ranked_peer, uint32_t *highest_rank,
			   uint16_t *lowest_ranked_peer, uint32_t *lowest_rank)
{
#if !defined(CONFIG_PM_PEER_RANKS)
	return NRF_ERROR_NOT_SUPPORTED;
#else
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	uint16_t peer_id = pds_next_peer_id_get(PM_PEER_ID_INVALID);
	uint32_t peer_rank = 0;
	uint32_t length = sizeof(peer_rank);
	struct pm_peer_data peer_data = {.peer_rank = &peer_rank};
	uint32_t nrf_err =
		pds_peer_data_read(peer_id, PM_PEER_DATA_ID_PEER_RANK, &peer_data, &length);
	struct {
		uint32_t highest;
		uint32_t lowest;
		uint16_t highest_peer;
		uint16_t lowest_peer;
	} rank = {
		.highest = 0,
		.lowest = 0xFFFFFFFF,
		.highest_peer = PM_PEER_ID_INVALID,
		.lowest_peer = PM_PEER_ID_INVALID,
	};

	if (nrf_err == NRF_ERROR_INVALID_PARAM) {
		/* No peer IDs exist. */
		return NRF_ERROR_NOT_FOUND;
	}

	while ((nrf_err == NRF_SUCCESS) || (nrf_err == NRF_ERROR_NOT_FOUND)) {
		if (nrf_err == NRF_SUCCESS) {
			if (peer_rank >= rank.highest) {
				rank.highest = peer_rank;
				rank.highest_peer = peer_id;
			}
			if (peer_rank < rank.lowest) {
				rank.lowest = peer_rank;
				rank.lowest_peer = peer_id;
			}
		}
		peer_id = pds_next_peer_id_get(peer_id);
		nrf_err =
			pds_peer_data_read(peer_id, PM_PEER_DATA_ID_PEER_RANK, &peer_data, &length);
	}
	if (peer_id == PM_PEER_ID_INVALID) {
		if ((rank.highest_peer == PM_PEER_ID_INVALID) ||
		    (rank.lowest_peer == PM_PEER_ID_INVALID)) {
			nrf_err = NRF_ERROR_NOT_FOUND;
		} else {
			nrf_err = NRF_SUCCESS;
		}

		if (highest_ranked_peer != NULL) {
			*highest_ranked_peer = rank.highest_peer;
		}
		if (highest_rank != NULL) {
			*highest_rank = rank.highest;
		}
		if (lowest_ranked_peer != NULL) {
			*lowest_ranked_peer = rank.lowest_peer;
		}
		if (lowest_rank != NULL) {
			*lowest_rank = rank.lowest;
		}
	} else {
		LOG_ERR("Could not retrieve ranks. pdb_peer_data_load() returned %s. peer_id: %d",
			nrf_strerror_get(nrf_err), peer_id);
		nrf_err = NRF_ERROR_INTERNAL;
	}
	return nrf_err;
#endif
}

#if defined(CONFIG_PM_PEER_RANKS)
/** @brief Function for initializing peer rank functionality. */
static void rank_init(void)
{
	rank_vars_update();
}
#endif

uint32_t pm_peer_rank_highest(uint16_t peer_id)
{
#if !defined(CONFIG_PM_PEER_RANKS)
	return NRF_ERROR_NOT_SUPPORTED;
#else
	if (!module_initialized) {
		return NRF_ERROR_INVALID_STATE;
	}

	uint32_t nrf_err;
	struct pm_peer_data_const peer_data = {
		.length_words = BYTES_TO_WORDS(sizeof(current_highest_peer_rank)),
		.data_id = PM_PEER_DATA_ID_PEER_RANK,
		.peer_rank = &current_highest_peer_rank};

	if (!peer_rank_initialized) {
		rank_init();
	}

	if (!peer_rank_initialized || (peer_rank_token != PM_STORE_TOKEN_INVALID)) {
		nrf_err = NRF_ERROR_BUSY;
	} else {
		if ((peer_id == highest_ranked_peer) && (current_highest_peer_rank > 0)) {
			struct pm_evt pm_evt;

			/* The reported peer is already regarded as highest (provided it has an
			 * index at all)
			 */
			nrf_err = NRF_SUCCESS;

			memset(&pm_evt, 0, sizeof(struct pm_evt));
			pm_evt.evt_id = PM_EVT_PEER_DATA_UPDATE_SUCCEEDED;
			pm_evt.conn_handle = im_conn_handle_get(peer_id);
			pm_evt.peer_id = peer_id;
			pm_evt.params.peer_data_update_succeeded.data_id =
				PM_PEER_DATA_ID_PEER_RANK;
			pm_evt.params.peer_data_update_succeeded.action = PM_PEER_DATA_OP_UPDATE;
			pm_evt.params.peer_data_update_succeeded.token = PM_STORE_TOKEN_INVALID;
			pm_evt.params.peer_data_update_succeeded.flash_changed = false;

			evt_send(&pm_evt);
		} else {
			if (current_highest_peer_rank == UINT32_MAX) {
				nrf_err = NRF_ERROR_DATA_SIZE;
			} else {
				current_highest_peer_rank += 1;
				nrf_err = pds_peer_data_store(peer_id, &peer_data,
							      &peer_rank_token);
				if (nrf_err) {
					peer_rank_token = PM_STORE_TOKEN_INVALID;
					current_highest_peer_rank -= 1;
					/* Assume INVALID_PARAM
					 * refers to peer_id, not
					 * data_id.
					 */
					if ((nrf_err != NRF_ERROR_BUSY) &&
					    (nrf_err != NRF_ERROR_RESOURCES) &&
					    (nrf_err != NRF_ERROR_INVALID_PARAM)) {
						LOG_ERR("Could not update rank. "
							"pdb_raw_store() returned %s. "
							"peer_id: %d",
							nrf_strerror_get(nrf_err), peer_id);
						nrf_err = NRF_ERROR_INTERNAL;
					}
				}
			}
		}
	}
	return nrf_err;
#endif
}
