/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <zephyr/logging/log.h>
#include <nrf_error.h>
#include <nrf_sdh_ble.h>
#include <ble_err.h>
#include <ble_conn_state.h>
#include <bluetooth/peer_manager/peer_manager.h>
#include <sdk_macros.h>
#include <modules/security_manager.h>
#include <modules/security_dispatcher.h>
#include <modules/gatt_cache_manager.h>
#include <modules/gatts_cache_manager.h>
#include <modules/peer_database.h>
#include <modules/peer_data_storage.h>
#include <modules/id_manager.h>
#include <modules/peer_manager_internal.h>

LOG_MODULE_DECLARE(peer_manager, CONFIG_PEER_MANAGER_LOG_LEVEL);

/** Macro indicating whether the module has been initialized properly. */
#define MODULE_INITIALIZED (m_module_initialized)
#define VERIFY_MODULE_INITIALIZED() VERIFY_TRUE((MODULE_INITIALIZED), NRF_ERROR_INVALID_STATE)

/** Whether or not @ref pm_init has been called successfully. */
static bool m_module_initialized;
/** Whether or not @ref rank_init has been called successfully. */
static bool m_peer_rank_initialized;
/** True from when @ref pm_peers_delete is called until all peers have been deleted. */
static bool m_deleting_all;
/** The store token of an ongoing peer rank update via a call to @ref pm_peer_rank_highest. If @ref
 * PM_STORE_TOKEN_INVALID, there is no ongoing update.
 */
static pm_store_token_t m_peer_rank_token;
/** The current highest peer rank. Used by @ref pm_peer_rank_highest. */
static uint32_t m_current_highest_peer_rank;
/** The peer with the highest peer rank. Used by @ref pm_peer_rank_highest. */
static pm_peer_id_t m_highest_ranked_peer;
/** The subscribers to Peer Manager events, as registered through @ref pm_register. */
static pm_evt_handler_t m_evt_handlers[CONFIG_PM_MAX_REGISTRANTS];
/** The number of event handlers registered through @ref pm_register. */
static uint8_t m_n_registrants;

/** User flag indicating whether a connection is excluded from being handled by the Peer Manager. */
static int m_flag_conn_excluded = CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT;

/**
 * @brief Function for sending a Peer Manager event to all subscribers.
 *
 * @param[in]  p_pm_evt  The event to send.
 */
static void evt_send(pm_evt_t const *p_pm_evt)
{
	for (int i = 0; i < m_n_registrants; i++) {
		m_evt_handlers[i](p_pm_evt);
	}
}

#if CONFIG_PM_PEER_RANKS_ENABLED == 1
/** @brief Function for initializing peer rank static variables. */
static void rank_vars_update(void)
{
	uint32_t err_code =
		pm_peer_ranks_get(&m_highest_ranked_peer, &m_current_highest_peer_rank, NULL, NULL);

	if (err_code == NRF_ERROR_NOT_FOUND) {
		m_highest_ranked_peer = PM_PEER_ID_INVALID;
		m_current_highest_peer_rank = 0;
	}

	m_peer_rank_initialized = ((err_code == NRF_SUCCESS) || (err_code == NRF_ERROR_NOT_FOUND));
}
#endif

/**
 * @brief Event handler for events from the Peer Database module.
 *        This handler is extern in the Peer Database module.
 *
 * @param[in]  p_pdb_evt  The incoming Peer Database event.
 */
void pm_pdb_evt_handler(pm_evt_t *p_pdb_evt)
{
	bool send_evt = true;

	p_pdb_evt->conn_handle = im_conn_handle_get(p_pdb_evt->peer_id);

	switch (p_pdb_evt->evt_id) {
#if CONFIG_PM_PEER_RANKS_ENABLED == 1
	case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
		if (p_pdb_evt->params.peer_data_update_succeeded.action == PM_PEER_DATA_OP_UPDATE) {
			if ((m_peer_rank_token != PM_STORE_TOKEN_INVALID) &&
			    (m_peer_rank_token ==
			     p_pdb_evt->params.peer_data_update_succeeded.token)) {
				m_peer_rank_token = PM_STORE_TOKEN_INVALID;
				m_highest_ranked_peer = p_pdb_evt->peer_id;

				p_pdb_evt->params.peer_data_update_succeeded.token =
					PM_STORE_TOKEN_INVALID;
			} else if (m_peer_rank_initialized &&
				   (p_pdb_evt->peer_id == m_highest_ranked_peer) &&
				   (p_pdb_evt->params.peer_data_update_succeeded.data_id ==
				    PM_PEER_DATA_ID_PEER_RANK)) {
				/* Update peer rank variable if highest ranked peer has changed its
				 * rank.
				 */
				rank_vars_update();
			}
		} else if (p_pdb_evt->params.peer_data_update_succeeded.action ==
			   PM_PEER_DATA_OP_DELETE) {
			if (m_peer_rank_initialized &&
			    (p_pdb_evt->peer_id == m_highest_ranked_peer) &&
			    (p_pdb_evt->params.peer_data_update_succeeded.data_id ==
			     PM_PEER_DATA_ID_PEER_RANK)) {
				/* Update peer rank variable if highest ranked peer has deleted its
				 * rank.
				 */
				rank_vars_update();
			}
		}
		break;

	case PM_EVT_PEER_DATA_UPDATE_FAILED:
		if (p_pdb_evt->params.peer_data_update_succeeded.action == PM_PEER_DATA_OP_UPDATE) {
			if ((m_peer_rank_token != PM_STORE_TOKEN_INVALID) &&
			    (m_peer_rank_token ==
			     p_pdb_evt->params.peer_data_update_failed.token)) {
				m_peer_rank_token = PM_STORE_TOKEN_INVALID;
				m_current_highest_peer_rank -= 1;

				p_pdb_evt->params.peer_data_update_succeeded.token =
					PM_STORE_TOKEN_INVALID;
			}
		}
		break;
#endif

	case PM_EVT_PEER_DELETE_SUCCEEDED:
		/* Check that no peers marked for deletion are left. */
		if (m_deleting_all &&
		    (pds_next_peer_id_get(PM_PEER_ID_INVALID) == PM_PEER_ID_INVALID) &&
		    (pds_next_deleted_peer_id_get(PM_PEER_ID_INVALID) == PM_PEER_ID_INVALID)) {
			/* pm_peers_delete() has been called and this is the last peer to be
			 * deleted.
			 */
			m_deleting_all = false;

			pm_evt_t pm_delete_all_evt;

			memset(&pm_delete_all_evt, 0, sizeof(pm_evt_t));
			pm_delete_all_evt.evt_id = PM_EVT_PEERS_DELETE_SUCCEEDED;
			pm_delete_all_evt.peer_id = PM_PEER_ID_INVALID;
			pm_delete_all_evt.conn_handle = BLE_CONN_HANDLE_INVALID;

			send_evt = false;

			/* Forward the event to all registered Peer Manager event handlers.
			 * Ensure that PEER_DELETE_SUCCEEDED arrives before PEERS_DELETE_SUCCEEDED.
			 */
			evt_send(p_pdb_evt);
			evt_send(&pm_delete_all_evt);
		}

#if CONFIG_PM_PEER_RANKS_ENABLED == 1
		if (m_peer_rank_initialized && (p_pdb_evt->peer_id == m_highest_ranked_peer)) {
			/* Update peer rank variable if highest ranked peer has been deleted. */
			rank_vars_update();
		}
#endif
		break;

	case PM_EVT_PEER_DELETE_FAILED:
		if (m_deleting_all) {
			/* pm_peers_delete() was called and has thus failed. */

			m_deleting_all = false;

			pm_evt_t pm_delete_all_evt;

			memset(&pm_delete_all_evt, 0, sizeof(pm_evt_t));
			pm_delete_all_evt.evt_id = PM_EVT_PEERS_DELETE_FAILED;
			pm_delete_all_evt.peer_id = PM_PEER_ID_INVALID;
			pm_delete_all_evt.conn_handle = BLE_CONN_HANDLE_INVALID;
			pm_delete_all_evt.params.peers_delete_failed_evt.error =
				p_pdb_evt->params.peer_delete_failed.error;

			send_evt = false;

			/* Forward the event to all registered Peer Manager event handlers.
			 * Ensure that PEER_DELETE_FAILED arrives before PEERS_DELETE_FAILED.
			 */
			evt_send(p_pdb_evt);
			evt_send(&pm_delete_all_evt);
		}
		break;

	default:
		/* Do nothing. */
		break;
	}

	if (send_evt) {
		/* Forward the event to all registered Peer Manager event handlers. */
		evt_send(p_pdb_evt);
	}
}

/**
 * @brief Event handler for events from the Security Manager module.
 *        This handler is extern in the Security Manager module.
 *
 * @param[in]  p_sm_evt  The incoming Security Manager event.
 */
void pm_sm_evt_handler(pm_evt_t *p_sm_evt)
{
	VERIFY_PARAM_NOT_NULL_VOID(p_sm_evt);

	/* Forward the event to all registered Peer Manager event handlers. */
	evt_send(p_sm_evt);
}

/**
 * @brief Event handler for events from the GATT Cache Manager module.
 *        This handler is extern in GATT Cache Manager.
 *
 * @param[in]  p_gcm_evt  The incoming GATT Cache Manager event.
 */
void pm_gcm_evt_handler(pm_evt_t *p_gcm_evt)
{
	/* Forward the event to all registered Peer Manager event handlers. */
	evt_send(p_gcm_evt);
}

/**
 * @brief Event handler for events from the GATTS Cache Manager module.
 *        This handler is extern in GATTS Cache Manager.
 *
 * @param[in]  p_gscm_evt  The incoming GATTS Cache Manager event.
 */
void pm_gscm_evt_handler(pm_evt_t *p_gscm_evt)
{
	/* Forward the event to all registered Peer Manager event handlers. */
	evt_send(p_gscm_evt);
}

/**
 * @brief Event handler for events from the ID Manager module.
 *        This function is registered in the ID Manager.
 *
 * @param[in]  p_im_evt  The incoming ID Manager event.
 */
void pm_im_evt_handler(pm_evt_t *p_im_evt)
{
	/* Forward the event to all registered Peer Manager event handlers. */
	evt_send(p_im_evt);
}

static bool is_conn_handle_excluded(ble_evt_t const *p_ble_evt)
{
	uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

	switch (p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED: {
		pm_evt_t pm_conn_config_req_evt;
		bool is_excluded = false;

		memset(&pm_conn_config_req_evt, 0, sizeof(pm_evt_t));
		pm_conn_config_req_evt.evt_id = PM_EVT_CONN_CONFIG_REQ;
		pm_conn_config_req_evt.peer_id = PM_PEER_ID_INVALID;
		pm_conn_config_req_evt.conn_handle = conn_handle;

		pm_conn_config_req_evt.params.conn_config_req.p_peer_params =
			&p_ble_evt->evt.gap_evt.params.connected;
		pm_conn_config_req_evt.params.conn_config_req.p_context = &is_excluded;

		evt_send(&pm_conn_config_req_evt);
		ble_conn_state_user_flag_set(conn_handle, m_flag_conn_excluded, is_excluded);

		return is_excluded;
	}

	default:
		return ble_conn_state_user_flag_get(conn_handle, m_flag_conn_excluded);
	}
}

/**
 * @brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt       Event received from the BLE stack.
 * @param[in]   p_context       Context.
 */
static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
	VERIFY_MODULE_INITIALIZED_VOID();

	if (is_conn_handle_excluded(p_ble_evt)) {
		LOG_DBG("Filtering BLE event with ID: 0x%04X targeting 0x%04X connection handle",
			p_ble_evt->header.evt_id, p_ble_evt->evt.gap_evt.conn_handle);
		return;
	}

	im_ble_evt_handler(p_ble_evt);
	sm_ble_evt_handler(p_ble_evt);
	gcm_ble_evt_handler(p_ble_evt);
}

NRF_SDH_BLE_OBSERVER(m_ble_evt_observer, ble_evt_handler, NULL, CONFIG_PM_BLE_OBSERVER_PRIO);

/** @brief Function for resetting the internal state of this module. */
static void internal_state_reset(void)
{
	m_highest_ranked_peer = PM_PEER_ID_INVALID;
	m_peer_rank_token = PM_STORE_TOKEN_INVALID;
}

uint32_t pm_init(void)
{
	uint32_t err_code;

	err_code = pds_init();
	if (err_code != NRF_SUCCESS) {
		LOG_ERR("%s failed because pds_init() returned %s.", __func__,
			nrf_strerror_get(err_code));
		return NRF_ERROR_INTERNAL;
	}

	err_code = pdb_init();
	if (err_code != NRF_SUCCESS) {
		LOG_ERR("%s failed because pdb_init() returned %s.", __func__,
			nrf_strerror_get(err_code));
		return NRF_ERROR_INTERNAL;
	}

	err_code = sm_init();
	if (err_code != NRF_SUCCESS) {
		LOG_ERR("%s failed because sm_init() returned %s.", __func__,
			nrf_strerror_get(err_code));
		return NRF_ERROR_INTERNAL;
	}

	err_code = smd_init();
	if (err_code != NRF_SUCCESS) {
		LOG_ERR("%s failed because smd_init() returned %s.", __func__,
			nrf_strerror_get(err_code));
		return NRF_ERROR_INTERNAL;
	}

	err_code = gcm_init();
	if (err_code != NRF_SUCCESS) {
		LOG_ERR("%s failed because gcm_init() returned %s.", __func__,
			nrf_strerror_get(err_code));
		return NRF_ERROR_INTERNAL;
	}

	err_code = gscm_init();
	if (err_code != NRF_SUCCESS) {
		LOG_ERR("%s failed because gscm_init() returned %s.", __func__,
			nrf_strerror_get(err_code));
		return NRF_ERROR_INTERNAL;
	}

	internal_state_reset();

	m_peer_rank_initialized = false;
	m_module_initialized = true;

	m_flag_conn_excluded = ble_conn_state_user_flag_acquire();

	/* If CONFIG_PM_PEER_RANKS_ENABLED is 0, these variables are unused. */
	UNUSED_VARIABLE(m_peer_rank_initialized);
	UNUSED_VARIABLE(m_peer_rank_token);
	UNUSED_VARIABLE(m_current_highest_peer_rank);
	UNUSED_VARIABLE(m_highest_ranked_peer);

	return NRF_SUCCESS;
}

uint32_t pm_register(pm_evt_handler_t event_handler)
{
	VERIFY_MODULE_INITIALIZED();

	if (m_n_registrants >= CONFIG_PM_MAX_REGISTRANTS) {
		return NRF_ERROR_NO_MEM;
	}

	m_evt_handlers[m_n_registrants] = event_handler;
	m_n_registrants += 1;

	return NRF_SUCCESS;
}

uint32_t pm_sec_params_set(ble_gap_sec_params_t *p_sec_params)
{
	VERIFY_MODULE_INITIALIZED();

	uint32_t err_code;

	err_code = sm_sec_params_set(p_sec_params);

	/* NRF_ERROR_INVALID_PARAM if parameters are invalid,
	 * NRF_SUCCESS             otherwise.
	 */
	return err_code;
}

uint32_t pm_conn_secure(uint16_t conn_handle, bool force_repairing)
{
	VERIFY_MODULE_INITIALIZED();

	uint32_t err_code;

	err_code = sm_link_secure(conn_handle, force_repairing);

	if (err_code == NRF_ERROR_INVALID_STATE) {
		err_code = NRF_ERROR_BUSY;
	}

	return err_code;
}

uint32_t pm_conn_exclude(uint16_t conn_handle, void const *p_context)
{
	VERIFY_PARAM_NOT_NULL(p_context);

	bool *p_is_conn_excluded = (bool *)p_context;

	*p_is_conn_excluded = true;

	return NRF_SUCCESS;
}

void pm_conn_sec_config_reply(uint16_t conn_handle, pm_conn_sec_config_t *p_conn_sec_config)
{
	if (p_conn_sec_config != NULL) {
		sm_conn_sec_config_reply(conn_handle, p_conn_sec_config);
	}
}

uint32_t pm_conn_sec_params_reply(uint16_t conn_handle, ble_gap_sec_params_t *p_sec_params,
				    void const *p_context)
{
	VERIFY_MODULE_INITIALIZED();

	return sm_sec_params_reply(conn_handle, p_sec_params, p_context);
}

void pm_local_database_has_changed(void)
{
#if !defined(CONFIG_PM_SERVICE_CHANGED_ENABLED) || (CONFIG_PM_SERVICE_CHANGED_ENABLED == 1)
	VERIFY_MODULE_INITIALIZED_VOID();

	gcm_local_database_has_changed();
#endif
}

uint32_t pm_id_addr_set(ble_gap_addr_t const *p_addr)
{
	VERIFY_MODULE_INITIALIZED();
	return im_id_addr_set(p_addr);
}

uint32_t pm_id_addr_get(ble_gap_addr_t *p_addr)
{
	VERIFY_MODULE_INITIALIZED();
	VERIFY_PARAM_NOT_NULL(p_addr);
	return im_id_addr_get(p_addr);
}

uint32_t pm_privacy_set(pm_privacy_params_t const *p_privacy_params)
{
	VERIFY_MODULE_INITIALIZED();
	VERIFY_PARAM_NOT_NULL(p_privacy_params);
	return im_privacy_set(p_privacy_params);
}

uint32_t pm_privacy_get(pm_privacy_params_t *p_privacy_params)
{
	VERIFY_MODULE_INITIALIZED();
	VERIFY_PARAM_NOT_NULL(p_privacy_params);
	VERIFY_PARAM_NOT_NULL(p_privacy_params->p_device_irk);
	return im_privacy_get(p_privacy_params);
}

bool pm_address_resolve(ble_gap_addr_t const *p_addr, ble_gap_irk_t const *p_irk)
{
	VERIFY_MODULE_INITIALIZED();

	if ((p_addr == NULL) || (p_irk == NULL)) {
		return false;
	} else {
		return im_address_resolve(p_addr, p_irk);
	}
}

uint32_t pm_whitelist_set(pm_peer_id_t const *p_peers, uint32_t peer_cnt)
{
	VERIFY_MODULE_INITIALIZED();
	return im_whitelist_set(p_peers, peer_cnt);
}

uint32_t pm_whitelist_get(ble_gap_addr_t *p_addrs, uint32_t *p_addr_cnt, ble_gap_irk_t *p_irks,
			    uint32_t *p_irk_cnt)
{
	VERIFY_MODULE_INITIALIZED();

	if (((p_addrs == NULL) && (p_irks == NULL)) ||
	    ((p_addrs != NULL) && (p_addr_cnt == NULL)) ||
	    ((p_irks != NULL) && (p_irk_cnt == NULL))) {
		/* The buffers can't be both NULL, and if a buffer is provided its size must be
		 * specified.
		 */
		return NRF_ERROR_NULL;
	}

	return im_whitelist_get(p_addrs, p_addr_cnt, p_irks, p_irk_cnt);
}

uint32_t pm_device_identities_list_set(pm_peer_id_t const *p_peers, uint32_t peer_cnt)
{
	VERIFY_MODULE_INITIALIZED();
	return im_device_identities_list_set(p_peers, peer_cnt);
}

uint32_t pm_conn_sec_status_get(uint16_t conn_handle, pm_conn_sec_status_t *p_conn_sec_status)
{
	VERIFY_MODULE_INITIALIZED();
	return sm_conn_sec_status_get(conn_handle, p_conn_sec_status);
}

bool pm_sec_is_sufficient(uint16_t conn_handle, pm_conn_sec_status_t *p_sec_status_req)
{
	VERIFY_MODULE_INITIALIZED_BOOL();
	return sm_sec_is_sufficient(conn_handle, p_sec_status_req);
}

uint32_t pm_lesc_public_key_set(ble_gap_lesc_p256_pk_t *p_public_key)
{
	VERIFY_MODULE_INITIALIZED();
	return sm_lesc_public_key_set(p_public_key);
}

uint32_t pm_conn_handle_get(pm_peer_id_t peer_id, uint16_t *p_conn_handle)
{
	VERIFY_MODULE_INITIALIZED();
	VERIFY_PARAM_NOT_NULL(p_conn_handle);
	*p_conn_handle = im_conn_handle_get(peer_id);
	return NRF_SUCCESS;
}

uint32_t pm_peer_id_get(uint16_t conn_handle, pm_peer_id_t *p_peer_id)
{
	VERIFY_MODULE_INITIALIZED();
	VERIFY_PARAM_NOT_NULL(p_peer_id);
	*p_peer_id = im_peer_id_get_by_conn_handle(conn_handle);
	return NRF_SUCCESS;
}

uint32_t pm_peer_count(void)
{
	if (!MODULE_INITIALIZED) {
		return 0;
	}
	return pds_peer_count_get();
}

pm_peer_id_t pm_next_peer_id_get(pm_peer_id_t prev_peer_id)
{
	pm_peer_id_t next_peer_id = prev_peer_id;

	if (!MODULE_INITIALIZED) {
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
 * @param[in] p_irk Pointer to the Identity Resolving Key.
 */
static bool peer_is_irk(ble_gap_irk_t const *const p_irk)
{
	for (uint32_t i = 0; i < ARRAY_SIZE(p_irk->irk); i++) {
		if (p_irk->irk[i] != 0) {
			return true;
		}
	}

	return false;
}

uint32_t pm_peer_id_list(pm_peer_id_t *p_peer_list, uint32_t *const p_list_size,
			   pm_peer_id_t first_peer_id, pm_peer_id_list_skip_t skip_id)
{
	VERIFY_MODULE_INITIALIZED();
	VERIFY_PARAM_NOT_NULL(p_list_size);
	VERIFY_PARAM_NOT_NULL(p_peer_list);

	uint32_t err_code;
	uint32_t size = *p_list_size;
	uint32_t current_size = 0;
	pm_peer_data_t pm_car_data;
	pm_peer_data_t pm_bond_data;
	pm_peer_id_t current_peer_id = first_peer_id;
	ble_gap_addr_t const *p_gap_addr;
	bool skip_no_addr = skip_id & PM_PEER_ID_LIST_SKIP_NO_ID_ADDR;
	bool skip_no_irk = skip_id & PM_PEER_ID_LIST_SKIP_NO_IRK;
	bool skip_no_car = skip_id & PM_PEER_ID_LIST_SKIP_NO_CAR;

	if ((*p_list_size < 1) ||
	    (skip_id > (PM_PEER_ID_LIST_SKIP_NO_ID_ADDR | PM_PEER_ID_LIST_SKIP_ALL))) {
		return NRF_ERROR_INVALID_PARAM;
	}

	*p_list_size = 0;

	if (current_peer_id == PM_PEER_ID_INVALID) {
		current_peer_id = pm_next_peer_id_get(current_peer_id);

		if (current_peer_id == PM_PEER_ID_INVALID) {
			return NRF_SUCCESS;
		}
	}

	memset(&pm_car_data, 0, sizeof(pm_peer_data_t));
	memset(&pm_bond_data, 0, sizeof(pm_peer_data_t));

	while (current_peer_id != PM_PEER_ID_INVALID) {
		bool skip = false;

		if (skip_no_addr || skip_no_irk) {
			/* Get data */
			pm_bond_data.p_bonding_data = NULL;

			err_code = pds_peer_data_read(current_peer_id, PM_PEER_DATA_ID_BONDING,
						      &pm_bond_data, NULL);

			if (err_code == NRF_ERROR_NOT_FOUND) {
				skip = true;
			} else {
				VERIFY_SUCCESS(err_code);
			}

			/* Check data */
			if (skip_no_addr) {
				p_gap_addr = &pm_bond_data.p_bonding_data->peer_ble_id.id_addr_info;

				if ((p_gap_addr->addr_type != BLE_GAP_ADDR_TYPE_PUBLIC) &&
				    (p_gap_addr->addr_type != BLE_GAP_ADDR_TYPE_RANDOM_STATIC)) {
					skip = true;
				}
			}
			if (skip_no_irk) {
				if (!peer_is_irk(
					    &pm_bond_data.p_bonding_data->peer_ble_id.id_info)) {
					skip = true;
				}
			}
		}

		if (skip_no_car) {
			/* Get data */
			pm_car_data.p_central_addr_res = NULL;

			err_code = pds_peer_data_read(current_peer_id,
						      PM_PEER_DATA_ID_CENTRAL_ADDR_RES,
						      &pm_car_data, NULL);

			if (err_code == NRF_ERROR_NOT_FOUND) {
				skip = true;
			} else {
				VERIFY_SUCCESS(err_code);
			}

			/* Check data */
			if (*pm_car_data.p_central_addr_res == 0) {
				skip = true;
			}
		}

		if (!skip) {
			p_peer_list[current_size++] = current_peer_id;

			if (current_size >= size) {
				break;
			}
		}

		current_peer_id = pm_next_peer_id_get(current_peer_id);
	}

	*p_list_size = current_size;

	return NRF_SUCCESS;
}

uint32_t pm_peer_data_load(pm_peer_id_t peer_id, pm_peer_data_id_t data_id, void *p_data,
			     uint32_t *p_length)
{
	VERIFY_MODULE_INITIALIZED();
	VERIFY_PARAM_NOT_NULL(p_data);
	VERIFY_PARAM_NOT_NULL(p_length);

	pm_peer_data_t peer_data;

	memset(&peer_data, 0, sizeof(peer_data));
	peer_data.p_all_data = p_data;

	return pds_peer_data_read(peer_id, data_id, &peer_data, p_length);
}

uint32_t pm_peer_data_bonding_load(pm_peer_id_t peer_id, pm_peer_data_bonding_t *p_data)
{
	uint32_t length = sizeof(pm_peer_data_bonding_t);

	return pm_peer_data_load(peer_id, PM_PEER_DATA_ID_BONDING, p_data, &length);
}

uint32_t pm_peer_data_remote_db_load(pm_peer_id_t peer_id, ble_gatt_db_srv_t *p_data,
				       uint32_t *p_length)
{
	return pm_peer_data_load(peer_id, PM_PEER_DATA_ID_GATT_REMOTE, p_data, p_length);
}

uint32_t pm_peer_data_app_data_load(pm_peer_id_t peer_id, void *p_data, uint32_t *p_length)
{
	return pm_peer_data_load(peer_id, PM_PEER_DATA_ID_APPLICATION, p_data, p_length);
}

uint32_t pm_peer_data_store(pm_peer_id_t peer_id, pm_peer_data_id_t data_id, void const *p_data,
			      uint32_t length, pm_store_token_t *p_token)
{
	VERIFY_MODULE_INITIALIZED();
	VERIFY_PARAM_NOT_NULL(p_data);
	if (ALIGN_NUM(4, length) != length) {
		return NRF_ERROR_INVALID_PARAM;
	}

	if (data_id == PM_PEER_DATA_ID_BONDING) {
		pm_peer_id_t dupl_peer_id;

		dupl_peer_id =
			im_find_duplicate_bonding_data((pm_peer_data_bonding_t *)p_data, peer_id);

		if (dupl_peer_id != PM_PEER_ID_INVALID) {
			return NRF_ERROR_FORBIDDEN;
		}
	}

	pm_peer_data_flash_t peer_data;

	memset(&peer_data, 0, sizeof(peer_data));
	peer_data.length_words = BYTES_TO_WORDS(length);
	peer_data.data_id = data_id;
	peer_data.p_all_data = p_data;

	return pds_peer_data_store(peer_id, &peer_data, p_token);
}

uint32_t pm_peer_data_bonding_store(pm_peer_id_t peer_id, pm_peer_data_bonding_t const *p_data,
				      pm_store_token_t *p_token)
{
	return pm_peer_data_store(peer_id, PM_PEER_DATA_ID_BONDING, p_data,
				  ALIGN_NUM(4, sizeof(pm_peer_data_bonding_t)), p_token);
}

uint32_t pm_peer_data_remote_db_store(pm_peer_id_t peer_id, ble_gatt_db_srv_t const *p_data,
					uint32_t length, pm_store_token_t *p_token)
{
	return pm_peer_data_store(peer_id, PM_PEER_DATA_ID_GATT_REMOTE, p_data, length, p_token);
}

uint32_t pm_peer_data_app_data_store(pm_peer_id_t peer_id, void const *p_data, uint32_t length,
				       pm_store_token_t *p_token)
{
	return pm_peer_data_store(peer_id, PM_PEER_DATA_ID_APPLICATION, p_data, length, p_token);
}

uint32_t pm_peer_data_delete(pm_peer_id_t peer_id, pm_peer_data_id_t data_id)
{
	VERIFY_MODULE_INITIALIZED();

	if (data_id == PM_PEER_DATA_ID_BONDING) {
		return NRF_ERROR_INVALID_PARAM;
	}

	return pds_peer_data_delete(peer_id, data_id);
}

uint32_t pm_peer_new(pm_peer_id_t *p_new_peer_id, pm_peer_data_bonding_t *p_bonding_data,
		       pm_store_token_t *p_token)
{
	uint32_t err_code;
	pm_peer_id_t peer_id;
	pm_peer_data_flash_t peer_data;

	VERIFY_MODULE_INITIALIZED();
	VERIFY_PARAM_NOT_NULL(p_bonding_data);
	VERIFY_PARAM_NOT_NULL(p_new_peer_id);

	memset(&peer_data, 0, sizeof(pm_peer_data_flash_t));

	/* Search through existing bonds to look for a duplicate. */
	pds_peer_data_iterate_prepare();

	/* @note This check is not thread safe since data is not copied while iterating. */
	while (pds_peer_data_iterate(PM_PEER_DATA_ID_BONDING, &peer_id, &peer_data)) {
		if (im_is_duplicate_bonding_data(p_bonding_data, peer_data.p_bonding_data)) {
			*p_new_peer_id = peer_id;
			return NRF_SUCCESS;
		}
	}

	/* If no duplicate data is found, prepare to write a new bond to flash. */

	*p_new_peer_id = pds_peer_id_allocate();

	if (*p_new_peer_id == PM_PEER_ID_INVALID) {
		return NRF_ERROR_NO_MEM;
	}

	memset(&peer_data, 0, sizeof(pm_peer_data_flash_t));

	peer_data.data_id = PM_PEER_DATA_ID_BONDING;
	peer_data.p_bonding_data = p_bonding_data;
	peer_data.length_words = BYTES_TO_WORDS(sizeof(pm_peer_data_bonding_t));

	err_code = pds_peer_data_store(*p_new_peer_id, &peer_data, p_token);

	if (err_code != NRF_SUCCESS) {
		uint32_t err_code_free = im_peer_free(*p_new_peer_id);

		if (err_code_free != NRF_SUCCESS) {
			LOG_ERR("Fatal error during cleanup of a failed call to %s. im_peer_free() "
				"returned %s. peer_id: %d",
				__func__, nrf_strerror_get(err_code_free), *p_new_peer_id);
			return NRF_ERROR_INTERNAL;
		}

		/* NRF_ERROR_RESOURCES, if no space in flash.
		 * NRF_ERROR_BUSY,         if flash filesystem was busy.
		 * NRF_ERROR_INVALID_ADDR, if bonding data is unaligned.
		 * NRF_ERROR_INTENRAL,     on internal error.
		 */
		return err_code;
	}

	return NRF_SUCCESS;
}

uint32_t pm_peer_delete(pm_peer_id_t peer_id)
{
	VERIFY_MODULE_INITIALIZED();

	return im_peer_free(peer_id);
}

uint32_t pm_peers_delete(void)
{
	VERIFY_MODULE_INITIALIZED();

	m_deleting_all = true;

	pm_peer_id_t current_peer_id = pds_next_peer_id_get(PM_PEER_ID_INVALID);

	if (current_peer_id == PM_PEER_ID_INVALID) {
		/* No peers bonded. */
		m_deleting_all = false;

		pm_evt_t pm_delete_all_evt;

		memset(&pm_delete_all_evt, 0, sizeof(pm_evt_t));
		pm_delete_all_evt.evt_id = PM_EVT_PEERS_DELETE_SUCCEEDED;
		pm_delete_all_evt.peer_id = PM_PEER_ID_INVALID;
		pm_delete_all_evt.conn_handle = BLE_CONN_HANDLE_INVALID;

		evt_send(&pm_delete_all_evt);
	}

	while (current_peer_id != PM_PEER_ID_INVALID) {
		uint32_t err_code = pm_peer_delete(current_peer_id);

		if (err_code != NRF_SUCCESS) {
			LOG_ERR("%s() failed because a call to pm_peer_delete() returned %s. "
				"peer_id: %d",
				__func__, nrf_strerror_get(err_code), current_peer_id);
			return NRF_ERROR_INTERNAL;
		}

		current_peer_id = pds_next_peer_id_get(current_peer_id);
	}

	return NRF_SUCCESS;
}

uint32_t pm_peer_ranks_get(pm_peer_id_t *p_highest_ranked_peer, uint32_t *p_highest_rank,
			     pm_peer_id_t *p_lowest_ranked_peer, uint32_t *p_lowest_rank)
{
#if CONFIG_PM_PEER_RANKS_ENABLED == 0
	return NRF_ERROR_NOT_SUPPORTED;
#else
	VERIFY_MODULE_INITIALIZED();

	pm_peer_id_t peer_id = pds_next_peer_id_get(PM_PEER_ID_INVALID);
	uint32_t peer_rank = 0;
	uint32_t length = sizeof(peer_rank);
	pm_peer_data_t peer_data = {.p_peer_rank = &peer_rank};
	uint32_t err_code =
		pds_peer_data_read(peer_id, PM_PEER_DATA_ID_PEER_RANK, &peer_data, &length);
	uint32_t highest_rank = 0;
	uint32_t lowest_rank = 0xFFFFFFFF;
	pm_peer_id_t highest_ranked_peer = PM_PEER_ID_INVALID;
	pm_peer_id_t lowest_ranked_peer = PM_PEER_ID_INVALID;

	if (err_code == NRF_ERROR_INVALID_PARAM) {
		/* No peer IDs exist. */
		return NRF_ERROR_NOT_FOUND;
	}

	while ((err_code == NRF_SUCCESS) || (err_code == NRF_ERROR_NOT_FOUND)) {
		if (err_code == NRF_SUCCESS) {
			if (peer_rank >= highest_rank) {
				highest_rank = peer_rank;
				highest_ranked_peer = peer_id;
			}
			if (peer_rank < lowest_rank) {
				lowest_rank = peer_rank;
				lowest_ranked_peer = peer_id;
			}
		}
		peer_id = pds_next_peer_id_get(peer_id);
		err_code =
			pds_peer_data_read(peer_id, PM_PEER_DATA_ID_PEER_RANK, &peer_data, &length);
	}
	if (peer_id == PM_PEER_ID_INVALID) {
		if ((highest_ranked_peer == PM_PEER_ID_INVALID) ||
		    (lowest_ranked_peer == PM_PEER_ID_INVALID)) {
			err_code = NRF_ERROR_NOT_FOUND;
		} else {
			err_code = NRF_SUCCESS;
		}

		if (p_highest_ranked_peer != NULL) {
			*p_highest_ranked_peer = highest_ranked_peer;
		}
		if (p_highest_rank != NULL) {
			*p_highest_rank = highest_rank;
		}
		if (p_lowest_ranked_peer != NULL) {
			*p_lowest_ranked_peer = lowest_ranked_peer;
		}
		if (p_lowest_rank != NULL) {
			*p_lowest_rank = lowest_rank;
		}
	} else {
		LOG_ERR("Could not retrieve ranks. pdb_peer_data_load() returned %s. peer_id: %d",
			nrf_strerror_get(err_code), peer_id);
		err_code = NRF_ERROR_INTERNAL;
	}
	return err_code;
#endif
}

#if CONFIG_PM_PEER_RANKS_ENABLED == 1
/** @brief Function for initializing peer rank functionality. */
static void rank_init(void)
{
	rank_vars_update();
}
#endif

uint32_t pm_peer_rank_highest(pm_peer_id_t peer_id)
{
#if CONFIG_PM_PEER_RANKS_ENABLED == 0
	return NRF_ERROR_NOT_SUPPORTED;
#else
	VERIFY_MODULE_INITIALIZED();

	uint32_t err_code;
	pm_peer_data_flash_t peer_data = {
		.length_words = BYTES_TO_WORDS(sizeof(m_current_highest_peer_rank)),
		.data_id = PM_PEER_DATA_ID_PEER_RANK,
		.p_peer_rank = &m_current_highest_peer_rank};

	if (!m_peer_rank_initialized) {
		rank_init();
	}

	if (!m_peer_rank_initialized || (m_peer_rank_token != PM_STORE_TOKEN_INVALID)) {
		err_code = NRF_ERROR_BUSY;
	} else {
		if ((peer_id == m_highest_ranked_peer) && (m_current_highest_peer_rank > 0)) {
			pm_evt_t pm_evt;

			/* The reported peer is already regarded as highest (provided it has an
			 * index at all)
			 */
			err_code = NRF_SUCCESS;

			memset(&pm_evt, 0, sizeof(pm_evt));
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
			if (m_current_highest_peer_rank == UINT32_MAX) {
				err_code = NRF_ERROR_DATA_SIZE;
			} else {
				m_current_highest_peer_rank += 1;
				err_code = pds_peer_data_store(peer_id, &peer_data,
							       &m_peer_rank_token);
				if (err_code != NRF_SUCCESS) {
					m_peer_rank_token = PM_STORE_TOKEN_INVALID;
					m_current_highest_peer_rank -= 1;
					/* Assume INVALID_PARAM
					 * refers to peer_id, not
					 * data_id.
					 */
					if ((err_code != NRF_ERROR_BUSY) &&
					    (err_code != NRF_ERROR_RESOURCES) &&
					    (err_code != NRF_ERROR_INVALID_PARAM)) {
						LOG_ERR("Could not update rank. "
							"pdb_raw_store() returned %s. "
							"peer_id: %d",
							nrf_strerror_get(err_code), peer_id);
						err_code = NRF_ERROR_INTERNAL;
					}
				}
			}
		}
	}
	return err_code;
#endif
}
