/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <nrf_strerror.h>
#include <stdint.h>
#include <string.h>
#include <ble.h>
#include <ble_gap.h>
#include <ble_err.h>
#include <bm/bluetooth/ble_conn_state.h>
#include <bm/bluetooth/peer_manager/peer_manager_types.h>

#include <modules/peer_data_storage.h>
#include <modules/peer_database.h>
#include <modules/id_manager.h>
#include <modules/security_dispatcher.h>
#if defined(CONFIG_PM_RA_PROTECTION)
#include <modules/auth_status_tracker.h>
#endif /* CONFIG_PM_RA_PROTECTION */

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_DECLARE(peer_manager, CONFIG_PEER_MANAGER_LOG_LEVEL);

/* The number of registered event handlers. */
#define SMD_EVENT_HANDLERS_CNT ARRAY_SIZE(evt_handlers)

/* Security Dispacher event handlers in Security Manager and GATT Cache Manager. */
extern void sm_smd_evt_handler(struct pm_evt *event);

/* Security Dispatcher events' handlers.
 * The number of elements in this array is SMD_EVENT_HANDLERS_CNT.
 */
static const pm_evt_handler_internal_t evt_handlers[] = {sm_smd_evt_handler};

static bool module_initialized;

static int flag_sec_proc = BLE_CONN_STATE_USER_FLAG_INVALID;
static int flag_sec_proc_pairing = BLE_CONN_STATE_USER_FLAG_INVALID;
static int flag_sec_proc_bonding = BLE_CONN_STATE_USER_FLAG_INVALID;
static int flag_allow_repairing = BLE_CONN_STATE_USER_FLAG_INVALID;

static ble_gap_lesc_p256_pk_t peer_pk;

static __INLINE bool sec_procedure(uint16_t conn_handle)
{
	return ble_conn_state_user_flag_get(conn_handle, flag_sec_proc);
}

static __INLINE bool pairing(uint16_t conn_handle)
{
	return ble_conn_state_user_flag_get(conn_handle, flag_sec_proc_pairing);
}

static __INLINE bool bonding(uint16_t conn_handle)
{
	return ble_conn_state_user_flag_get(conn_handle, flag_sec_proc_bonding);
}

static __INLINE bool allow_repairing(uint16_t conn_handle)
{
	return ble_conn_state_user_flag_get(conn_handle, flag_allow_repairing);
}

/**
 * @brief Function for sending an SMD event to all event handlers.
 *
 * @param[in]  event  The event to pass to all event handlers.
 */
static void evt_send(struct pm_evt *event)
{
	event->peer_id = im_peer_id_get_by_conn_handle(event->conn_handle);

	for (uint32_t i = 0; i < SMD_EVENT_HANDLERS_CNT; i++) {
		evt_handlers[i](event);
	}
}

/**
 * @brief Function for sending a PM_EVT_CONN_SEC_START event.
 *
 * @param[in] conn_handle The connection handle the event pertains to.
 * @param[in] procedure   The procedure that has started on the connection.
 */
static void sec_start_send(uint16_t conn_handle, enum pm_conn_sec_procedure procedure)
{
	struct pm_evt evt = {
		.evt_id = PM_EVT_CONN_SEC_START,
		.conn_handle = conn_handle,
		.params.conn_sec_start = {
			.procedure = procedure,
		},
	};

	evt_send(&evt);
}

/**
 * @brief Function for sending a PM_EVT_ERROR_UNEXPECTED event.
 *
 * @param[in] conn_handle The connection handle the event pertains to.
 * @param[in] nrf_err     The unexpected error that occurred.
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
 * @brief Function for sending a PM_EVT_STORAGE_FULL event.
 *
 * @param[in]  conn_handle  The connection handle the event pertains to.
 */
static void send_storage_full_evt(uint16_t conn_handle)
{
	struct pm_evt evt = {.evt_id = PM_EVT_STORAGE_FULL, .conn_handle = conn_handle};

	evt_send(&evt);
}

/**
 * @brief Function for cleaning up after a failed security procedure.
 *
 * @param[in]  conn_handle  The handle of the connection the security procedure happens on.
 * @param[in]  procedure    The procedure that failed.
 * @param[in]  error        The error the procedure failed with. See @ref PM_SEC_ERRORS.
 * @param[in]  error_src    The party that raised the error. See @ref BLE_GAP_SEC_STATUS_SOURCES.
 */
static void conn_sec_failure(uint16_t conn_handle, enum pm_conn_sec_procedure procedure,
			     uint16_t error, uint8_t error_src)
{
	struct pm_evt evt = {
		.evt_id = PM_EVT_CONN_SEC_FAILED,
		.conn_handle = conn_handle,
		.params.conn_sec_failed = {
			.procedure = procedure,
			.error = error,
			.error_src = error_src,
		},
	};

	ble_conn_state_user_flag_set(conn_handle, flag_sec_proc, false);

	evt_send(&evt);
}

/**
 * @brief Function for cleaning up after a failed pairing procedure.
 *
 * @param[in]  conn_handle  The handle of the connection the pairing procedure happens on.
 * @param[in]  error        The error the procedure failed with. See @ref PM_SEC_ERRORS.
 * @param[in]  error_src    The source of the error (local or remote). See @ref
 *                          BLE_GAP_SEC_STATUS_SOURCES.
 */
static void pairing_failure(uint16_t conn_handle, uint16_t error, uint8_t error_src)
{
	uint32_t nrf_err = NRF_SUCCESS;
	enum pm_conn_sec_procedure procedure = bonding(conn_handle) ? PM_CONN_SEC_PROCEDURE_BONDING
								    : PM_CONN_SEC_PROCEDURE_PAIRING;
	uint16_t temp_peer_id;

	nrf_err = pdb_temp_peer_id_get(conn_handle, &temp_peer_id);
	if (nrf_err == NRF_SUCCESS) {
		nrf_err = pdb_write_buf_release(temp_peer_id, PM_PEER_DATA_ID_BONDING);
	}

	if ((nrf_err != NRF_SUCCESS) &&
	    (nrf_err != NRF_ERROR_NOT_FOUND /* No buffer was allocated */)) {
		LOG_ERR("Could not clean up after failed bonding procedure. "
			"pdb_write_buf_release() returned %s. conn_handle: %d.",
			nrf_strerror_get(nrf_err), conn_handle);
		send_unexpected_error(conn_handle, nrf_err);
	}

	conn_sec_failure(conn_handle, procedure, error, error_src);
}

/**
 * @brief Function for cleaning up after a failed encryption procedure.
 *
 * @param[in]  conn_handle  The handle of the connection the encryption procedure happens on.
 * @param[in]  error        The error the procedure failed with. See @ref PM_SEC_ERRORS.
 * @param[in]  error_src    The party that raised the error. See @ref BLE_GAP_SEC_STATUS_SOURCES.
 */
static __INLINE void encryption_failure(uint16_t conn_handle, uint16_t error, uint8_t error_src)
{
	conn_sec_failure(conn_handle, PM_CONN_SEC_PROCEDURE_ENCRYPTION, error, error_src);
}

/**
 * @brief Function for possibly cleaning up after a failed pairing or encryption procedure.
 *
 * @param[in]  conn_handle  The handle of the connection the pairing procedure happens on.
 * @param[in]  error        The error the procedure failed with. See @ref PM_SEC_ERRORS.
 * @param[in]  error_src    The party that raised the error. See @ref BLE_GAP_SEC_STATUS_SOURCES.
 */
static void link_secure_failure(uint16_t conn_handle, uint16_t error, uint8_t error_src)
{
	if (sec_procedure(conn_handle)) {
		if (pairing(conn_handle)) {
			pairing_failure(conn_handle, error, error_src);
		} else {
			encryption_failure(conn_handle, error, error_src);
		}
	}
}

/**
 * @brief Function for administrative actions to be taken when a security process has started.
 *
 * @param[in]  conn_handle  The connection the security process was attempted on.
 * @param[in]  success      Whether the procedure was started successfully.
 * @param[in]  procedure    The procedure that was started.
 */
static void sec_proc_start(uint16_t conn_handle, bool success, enum pm_conn_sec_procedure procedure)
{
	ble_conn_state_user_flag_set(conn_handle, flag_sec_proc, success);
	if (success) {
		ble_conn_state_user_flag_set(conn_handle, flag_sec_proc_pairing,
					     (procedure != PM_CONN_SEC_PROCEDURE_ENCRYPTION));
		ble_conn_state_user_flag_set(conn_handle, flag_sec_proc_bonding,
					     (procedure == PM_CONN_SEC_PROCEDURE_BONDING));
		sec_start_send(conn_handle, procedure);
	}
}

/**
 * @brief Function for initiating pairing as a central, or all security as a periheral.
 *
 * See @ref smd_link_secure and @ref sd_ble_gap_authenticate for more information.
 */
static uint32_t link_secure_authenticate(uint16_t conn_handle, ble_gap_sec_params_t *sec_params)
{
	uint32_t nrf_err = sd_ble_gap_authenticate(conn_handle, sec_params);

	if (nrf_err == NRF_ERROR_NO_MEM) {
		/* sd_ble_gap_authenticate() returned NRF_ERROR_NO_MEM. Too many other sec
		 * procedures running.
		 */
		nrf_err = NRF_ERROR_BUSY;
	}

	return nrf_err;
}

#if defined(CONFIG_SOFTDEVICE_CENTRAL)
/**
 * @brief Function for initiating encryption as a central. See @ref smd_link_secure for more
 *        info.
 */
static uint32_t link_secure_central_encryption(uint16_t conn_handle, uint16_t peer_id)
{
	struct pm_peer_data peer_data;
	uint32_t nrf_err;
	const ble_gap_enc_key_t *existing_key = NULL;
	bool lesc = false;
	struct pm_peer_data_bonding bonding_data = { 0 };
	const uint32_t buf_size = sizeof(struct pm_peer_data_bonding);

	peer_data.bonding_data = &bonding_data;

	nrf_err = pds_peer_data_read(peer_id, PM_PEER_DATA_ID_BONDING, &peer_data, &buf_size);

	if (nrf_err == NRF_SUCCESS) {
		/* Use peer's key since they are peripheral. */
		existing_key = &(bonding_data.peer_ltk);

		lesc = bonding_data.own_ltk.enc_info.lesc;
		/* LESC was used during bonding. */
		if (lesc) {
			/* For LESC, always use own key. */
			existing_key = &(bonding_data.own_ltk);
		}
	}

	if ((nrf_err != NRF_SUCCESS) &&
	    (nrf_err != NRF_ERROR_NOT_FOUND)) {
		if (nrf_err != NRF_ERROR_BUSY) {
			LOG_ERR("Could not retrieve stored bond. pds_peer_data_read() "
				"returned %s. peer_id: %d", nrf_strerror_get(nrf_err), peer_id);
			nrf_err = NRF_ERROR_INTERNAL;
		}
	/* There is no bonding data stored. This means that a
	 * bonding procedure is in ongoing, or that the records
	 * in flash are in a bad state.
	 */
	} else if (existing_key == NULL) {
		nrf_err = NRF_ERROR_BUSY;
	} else if (!lesc && !im_master_id_is_valid(&(existing_key->master_id))) {
		/* No LTK to encrypt with. */
		nrf_err = NRF_ERROR_INVALID_DATA;
	} else {
		/* Encrypt with existing LTK. */
		nrf_err = sd_ble_gap_encrypt(conn_handle, &(existing_key->master_id),
					     &(existing_key->enc_info));
	}

	sec_proc_start(conn_handle, nrf_err == NRF_SUCCESS, PM_CONN_SEC_PROCEDURE_ENCRYPTION);

	return nrf_err;
}

/** @brief Function for intiating security as a central. See @ref smd_link_secure for more info. */
static uint32_t link_secure_central(uint16_t conn_handle, ble_gap_sec_params_t *sec_params,
				      bool force_repairing)
{
	uint32_t nrf_err;
	uint16_t peer_id;

	if (sec_params == NULL) {
		return link_secure_authenticate(conn_handle, NULL);
	}

	/* Set the default value for allowing repairing at the start of the sec proc.
	 * (for central)
	 */
	ble_conn_state_user_flag_set(conn_handle, flag_allow_repairing, force_repairing);

	peer_id = im_peer_id_get_by_conn_handle(conn_handle);

	if ((peer_id != PM_PEER_ID_INVALID) && !force_repairing) {
		/* There is already data in flash for this peer, and repairing has not been
		 * requested, so the link will be encrypted with the existing keys.
		 */
		nrf_err = link_secure_central_encryption(conn_handle, peer_id);
	} else {
		/* There are no existing keys, or repairing has been explicitly requested, so
		 * pairing (possibly including bonding) will be performed to encrypt the link.
		 */
		nrf_err = link_secure_authenticate(conn_handle, sec_params);
		enum pm_conn_sec_procedure procedure = (sec_params && sec_params->bond)
								? PM_CONN_SEC_PROCEDURE_BONDING
								: PM_CONN_SEC_PROCEDURE_PAIRING;
		sec_proc_start(conn_handle, nrf_err == NRF_SUCCESS, procedure);
	}

	return nrf_err;
}

/**
 * @brief Function for processing the @ref BLE_GAP_EVT_SEC_REQUEST event from the SoftDevice.
 *
 * @param[in]  gap_evt  The event from the SoftDevice.
 */
static void sec_request_process(const ble_gap_evt_t *gap_evt)
{
	if (sec_procedure(gap_evt->conn_handle)) {
		/* Ignore request as per spec. */
		return;
	}

	struct pm_evt evt = {
		.evt_id = PM_EVT_SLAVE_SECURITY_REQ,
		.conn_handle = gap_evt->conn_handle
	};

	memcpy(&evt.params.slave_security_req, &gap_evt->params.sec_request,
	       sizeof(ble_gap_evt_sec_request_t));

	evt_send(&evt);
}
#endif /* CONFIG_SOFTDEVICE_CENTRAL */

#if defined(CONFIG_SOFTDEVICE_PERIPHERAL)
/** @brief Function for asking the central to secure the link. See @ref smd_link_secure for more
 * info.
 */
static uint32_t link_secure_peripheral(uint16_t conn_handle, ble_gap_sec_params_t *sec_params)
{
	uint32_t nrf_err = NRF_SUCCESS;

	if (sec_params != NULL) {
		nrf_err = link_secure_authenticate(conn_handle, sec_params);
	}

	return nrf_err;
}

/** @brief Function for processing the @ref BLE_GAP_EVT_SEC_INFO_REQUEST event from the SoftDevice.
 *
 * @param[in]  gap_evt  The event from the SoftDevice.
 */
static void sec_info_request_process(const ble_gap_evt_t *gap_evt)
{
	uint32_t nrf_err;
	const ble_gap_enc_info_t *enc_info = NULL;
	struct pm_peer_data peer_data;
	uint16_t peer_id =
		im_peer_id_get_by_master_id(&gap_evt->params.sec_info_request.master_id);

	if (peer_id == PM_PEER_ID_INVALID) {
		peer_id = im_peer_id_get_by_conn_handle(gap_evt->conn_handle);
	} else {
		/* The peer might have been unrecognized until now (since connecting). E.g. if using
		 * a random non-resolvable advertising address. Report the discovered peer ID just
		 * in case.
		 */
		im_new_peer_id(gap_evt->conn_handle, peer_id);
	}

	sec_proc_start(gap_evt->conn_handle, true, PM_CONN_SEC_PROCEDURE_ENCRYPTION);

	struct pm_peer_data_bonding bonding_data = { 0 };
	uint32_t bonding_data_size = sizeof(struct pm_peer_data_bonding);

	if (peer_id != PM_PEER_ID_INVALID) {
		peer_data.all_data = &bonding_data;

		nrf_err = pds_peer_data_read(peer_id, PM_PEER_DATA_ID_BONDING, &peer_data,
					     &bonding_data_size);

		if (nrf_err == NRF_SUCCESS) {
			/* There is stored bonding data for this peer. */
			const ble_gap_enc_key_t *existing_key =
				&bonding_data.own_ltk;

			if (gap_evt->params.sec_info_request.enc_info &&
			    (existing_key->enc_info.lesc ||
			     im_master_ids_compare(
				     &existing_key->master_id,
				     &gap_evt->params.sec_info_request.master_id))) {
				enc_info = &existing_key->enc_info;
			}
		}
	}

	nrf_err = sd_ble_gap_sec_info_reply(gap_evt->conn_handle, enc_info, NULL, NULL);

	if (nrf_err == NRF_ERROR_INVALID_STATE) {
		/* Do nothing. If disconnecting, it will be caught later by the handling of the
		 * DISCONNECTED event. If there is no SEC_INFO_REQ pending, there is either a logic
		 * error, or the user is also calling sd_ble_gap_sec_info_reply(), but there is no
		 * way for the present code to detect which one is the case.
		 */
		LOG_WRN("sd_ble_gap_sec_info_reply() returned NRF_ERROR_INVALID_STATE, which is an "
			"error unless the link is disconnecting.");
	} else if (nrf_err) {
		LOG_ERR("Could not complete encryption procedure. sd_ble_gap_sec_info_reply() "
			"returned %s. conn_handle: %d, peer_id: %d.",
			nrf_strerror_get(nrf_err), gap_evt->conn_handle, peer_id);
		send_unexpected_error(gap_evt->conn_handle, nrf_err);
	} else if (gap_evt->params.sec_info_request.enc_info && (enc_info == NULL)) {
		encryption_failure(gap_evt->conn_handle, PM_CONN_SEC_ERROR_PIN_OR_KEY_MISSING,
				   BLE_GAP_SEC_STATUS_SOURCE_LOCAL);
	}
}
#endif /* CONFIG_SOFTDEVICE_PERIPHERAL */

/**
 * @brief Function for sending a PM_EVT_CONN_SEC_CONFIG_REQ event.
 *
 * @param[in]  conn_handle  The connection the sec parameters are needed for.
 */
static void send_config_req(uint16_t conn_handle)
{
	struct pm_evt evt;

	memset(&evt, 0, sizeof(evt));

	evt.evt_id = PM_EVT_CONN_SEC_CONFIG_REQ;
	evt.conn_handle = conn_handle;

	evt_send(&evt);
}

void smd_conn_sec_config_reply(uint16_t conn_handle, struct pm_conn_sec_config *conn_sec_config)
{
	__ASSERT_NO_MSG(module_initialized);
	__ASSERT_NO_MSG(conn_sec_config != NULL);

	ble_conn_state_user_flag_set(conn_handle, flag_allow_repairing,
				     conn_sec_config->allow_repairing);
}

/**
 * @brief Function for processing the @ref BLE_GAP_EVT_DISCONNECT event from the SoftDevice.
 *
 * @param[in]  gap_evt  The event from the SoftDevice.
 */
static void disconnect_process(const ble_gap_evt_t *gap_evt)
{
	uint16_t error = (gap_evt->params.disconnected.reason ==
			  BLE_HCI_CONN_TERMINATED_DUE_TO_MIC_FAILURE)
			  ? PM_CONN_SEC_ERROR_MIC_FAILURE : PM_CONN_SEC_ERROR_DISCONNECT;

	link_secure_failure(gap_evt->conn_handle, error, BLE_GAP_SEC_STATUS_SOURCE_LOCAL);
}

/**
 * @brief Function for sending a PARAMS_REQ event.
 *
 * @param[in]  conn_handle    The connection the security parameters are needed for.
 * @param[in]  peer_params  The security parameters from the peer. Can be NULL if the peer's
 * parameters are not yet available.
 */
static void send_params_req(uint16_t conn_handle, const ble_gap_sec_params_t *peer_params)
{
	struct pm_evt evt = {
		.evt_id = PM_EVT_CONN_SEC_PARAMS_REQ,
		.conn_handle = conn_handle,
		.params.conn_sec_params_req = {
			.peer_params = peer_params,
		},
	};

	evt_send(&evt);
}

/**
 * @brief Function for processing the @ref BLE_GAP_EVT_SEC_PARAMS_REQUEST event from the SoftDevice.
 *
 * @param[in]  gap_evt  The event from the SoftDevice.
 */
static void sec_params_request_process(const ble_gap_evt_t *gap_evt)
{
#if defined(CONFIG_SOFTDEVICE_PERIPHERAL)
	if (ble_conn_state_role(gap_evt->conn_handle) == BLE_GAP_ROLE_PERIPH) {
		sec_proc_start(gap_evt->conn_handle, true,
			       gap_evt->params.sec_params_request.peer_params.bond
				       ? PM_CONN_SEC_PROCEDURE_BONDING
				       : PM_CONN_SEC_PROCEDURE_PAIRING);
	}
#endif /* CONFIG_SOFTDEVICE_PERIPHERAL */

	send_params_req(gap_evt->conn_handle, &gap_evt->params.sec_params_request.peer_params);
}

/**
 * @brief Function for sending a Peer Manager event indicating that pairing has succeeded.
 *
 * @param[in]  gap_evt    The AUTH_STATUS event from the SoftDevice that triggered this.
 * @param[in]  data_stored  Whether bonding data was stored.
 */
static void pairing_success_evt_send(const ble_gap_evt_t *gap_evt, bool data_stored)
{
	struct pm_evt evt = {
		.evt_id = PM_EVT_CONN_SEC_SUCCEEDED,
		.conn_handle = gap_evt->conn_handle,
		.params.conn_sec_succeeded = {
			.procedure = gap_evt->params.auth_status.bonded
				? PM_CONN_SEC_PROCEDURE_BONDING : PM_CONN_SEC_PROCEDURE_PAIRING,
			.data_stored = data_stored,
		},
	};

	evt_send(&evt);
}

/**
 * @brief Function for processing the @ref BLE_GAP_EVT_AUTH_STATUS event from the SoftDevice, when
 *        the auth_status is success.
 *
 * @param[in]  gap_evt  The event from the SoftDevice.
 */
static void auth_status_success_process(const ble_gap_evt_t *gap_evt)
{
	uint32_t nrf_err;
	uint16_t conn_handle = gap_evt->conn_handle;
	uint16_t peer_id;
	uint16_t temp_peer_id;
	struct pm_peer_data peer_data;
	bool new_peer_id = false;

	ble_conn_state_user_flag_set(conn_handle, flag_sec_proc, false);

	if (!gap_evt->params.auth_status.bonded) {
		pairing_success_evt_send(gap_evt, false);
		return;
	}

	nrf_err = pdb_temp_peer_id_get(conn_handle, &temp_peer_id);
	if (nrf_err == NRF_SUCCESS) {
		nrf_err = pdb_write_buf_get(temp_peer_id, PM_PEER_DATA_ID_BONDING, 1, &peer_data);
	}

	if (nrf_err) {
		LOG_ERR("RAM buffer for new bond was unavailable. pdb_write_buf_get() "
			"returned %s. conn_handle: %d.",
			nrf_strerror_get(nrf_err), conn_handle);
		send_unexpected_error(conn_handle, nrf_err);
		pairing_success_evt_send(gap_evt, false);
		return;
	}

	peer_id = im_peer_id_get_by_conn_handle(conn_handle);

	if (peer_id == PM_PEER_ID_INVALID) {
		peer_id = im_find_duplicate_bonding_data(peer_data.bonding_data,
							 PM_PEER_ID_INVALID);

		if (peer_id != PM_PEER_ID_INVALID) {
			/* The peer has been identified as someone we have already bonded with. */
			im_new_peer_id(conn_handle, peer_id);

			/* If the flag is true, the configuration has been requested before. */
			if (!allow_repairing(conn_handle)) {
				send_config_req(conn_handle);
				if (!allow_repairing(conn_handle)) {
					pairing_success_evt_send(gap_evt, false);
					return;
				}
			}
		}
	}

	if (peer_id == PM_PEER_ID_INVALID) {
		peer_id = pds_peer_id_allocate();
		if (peer_id == PM_PEER_ID_INVALID) {
			LOG_ERR("Could not allocate new peer_id for incoming bond.");
			send_unexpected_error(conn_handle, NRF_ERROR_NO_MEM);
			pairing_success_evt_send(gap_evt, false);
			return;
		}
		im_new_peer_id(conn_handle, peer_id);
		new_peer_id = true;
	}

	nrf_err = pdb_write_buf_store(temp_peer_id, PM_PEER_DATA_ID_BONDING, peer_id);

	if (nrf_err == NRF_SUCCESS) {
		pairing_success_evt_send(gap_evt, true);
	} else if (nrf_err == NRF_ERROR_RESOURCES) {
		send_storage_full_evt(conn_handle);
		pairing_success_evt_send(gap_evt, true);
	} else {
		/* Unexpected error */
		LOG_ERR("Could not store bond. pdb_write_buf_store() returned %s. "
			"conn_handle: %d, peer_id: %d",
			nrf_strerror_get(nrf_err), conn_handle, peer_id);
		send_unexpected_error(conn_handle, nrf_err);
		pairing_success_evt_send(gap_evt, false);
		if (new_peer_id) {
			/* Unused return value. We are already in a bad state. */
			(void)im_peer_free(peer_id);
		}
	}
}

/**
 * @brief Function for processing the @ref BLE_GAP_EVT_AUTH_STATUS event from the SoftDevice, when
 *        the auth_status is failure.
 *
 * @param[in]  gap_evt  The event from the SoftDevice.
 */
static void auth_status_failure_process(const ble_gap_evt_t *gap_evt)
{
	link_secure_failure(gap_evt->conn_handle, gap_evt->params.auth_status.auth_status,
			    gap_evt->params.auth_status.error_src);
}

/**
 * @brief Function for processing the @ref BLE_GAP_EVT_AUTH_STATUS event from the SoftDevice.
 *
 * @param[in]  gap_evt  The event from the SoftDevice.
 */
static void auth_status_process(const ble_gap_evt_t *gap_evt)
{
	switch (gap_evt->params.auth_status.auth_status) {
	case BLE_GAP_SEC_STATUS_SUCCESS:
		auth_status_success_process(gap_evt);
		break;

	default:
		auth_status_failure_process(gap_evt);
#if defined(CONFIG_PM_RA_PROTECTION)
		ast_auth_error_notify(gap_evt->conn_handle);
#endif /* CONFIG_PM_RA_PROTECTION */
		break;
	}
}

/**
 * @brief Function for processing the @ref BLE_GAP_EVT_CONN_SEC_UPDATE event from the SoftDevice.
 *
 * @param[in]  gap_evt  The event from the SoftDevice.
 */
static void conn_sec_update_process(const ble_gap_evt_t *gap_evt)
{
	if (!pairing(gap_evt->conn_handle)) {
		/* This is an encryption procedure (not pairing), so this event marks the end of the
		 * procedure.
		 */
		if (!ble_conn_state_encrypted(gap_evt->conn_handle)) {
			encryption_failure(gap_evt->conn_handle,
					   PM_CONN_SEC_ERROR_PIN_OR_KEY_MISSING,
					   BLE_GAP_SEC_STATUS_SOURCE_REMOTE);
		} else {
			ble_conn_state_user_flag_set(gap_evt->conn_handle, flag_sec_proc,
						     false);

			struct pm_evt evt = {
				.evt_id = PM_EVT_CONN_SEC_SUCCEEDED,
				.conn_handle = gap_evt->conn_handle,
				.params.conn_sec_succeeded = {
					.procedure = PM_CONN_SEC_PROCEDURE_ENCRYPTION,
					.data_stored = false,
				},
			};

			evt_send(&evt);
		}
	}
}

/**
 * @brief Function for initializing a BLE Connection State user flag.
 *
 * @param[out] flag_id  The flag to initialize.
 */
static void flag_id_init(int *flag_id)
{
	if (*flag_id == BLE_CONN_STATE_USER_FLAG_INVALID) {
		*flag_id = ble_conn_state_user_flag_acquire();
	}
}

uint32_t smd_init(void)
{
	__ASSERT_NO_MSG(!module_initialized);

	flag_id_init(&flag_sec_proc);
	flag_id_init(&flag_sec_proc_pairing);
	flag_id_init(&flag_sec_proc_bonding);
	flag_id_init(&flag_allow_repairing);

	if ((flag_sec_proc == BLE_CONN_STATE_USER_FLAG_INVALID) ||
	    (flag_sec_proc_pairing == BLE_CONN_STATE_USER_FLAG_INVALID) ||
	    (flag_sec_proc_bonding == BLE_CONN_STATE_USER_FLAG_INVALID) ||
	    (flag_allow_repairing == BLE_CONN_STATE_USER_FLAG_INVALID)) {
		LOG_ERR("Could not acquire conn_state user flags. Increase "
			"BLE_CONN_STATE_USER_FLAG_COUNT in the ble_conn_state module.");
		return NRF_ERROR_INTERNAL;
	}

#if defined(CONFIG_PM_RA_PROTECTION)
	uint32_t nrf_err = ast_init();

	if (nrf_err) {
		return nrf_err;
	}
#endif /* CONFIG_PM_RA_PROTECTION */

	module_initialized = true;

	return NRF_SUCCESS;
}

/**
 * @brief Function for putting retrieving a buffer and putting pointers into a @ref
 * ble_gap_sec_keyset_t.
 *
 * @param[in]  conn_handle  The connection the security procedure is happening on.
 * @param[in]  role         Our role in the connection.
 * @param[in]  public_key   Pointer to a buffer holding the public key, or NULL.
 * @param[out] sec_keyset   Pointer to the keyset to be filled.
 *
 * @retval NRF_SUCCESS              Success.
 * @retval NRF_ERROR_BUSY           Could not process request at this time. Reattempt later.
 * @retval NRF_ERROR_INVALID_PARAM  Data ID or Peer ID was invalid or unallocated.
 * @retval NRF_ERROR_INVALID_STATE  The link is disconnected.
 * @retval NRF_ERROR_INTERNAL       Fatal error.
 */
static uint32_t sec_keyset_fill(uint16_t conn_handle, uint8_t role,
				ble_gap_lesc_p256_pk_t *public_key,
				ble_gap_sec_keyset_t *sec_keyset)
{
	uint32_t nrf_err;
	uint16_t temp_peer_id;
	struct pm_peer_data peer_data;

	if (sec_keyset == NULL) {
		LOG_ERR("Internal error: %s received NULL for sec_keyset.", __func__);
		return NRF_ERROR_INTERNAL;
	}

	nrf_err = pdb_temp_peer_id_get(conn_handle, &temp_peer_id);
	if (nrf_err == NRF_SUCCESS) {
		/* Acquire a memory buffer to receive bonding data into. */
		nrf_err = pdb_write_buf_get(temp_peer_id, PM_PEER_DATA_ID_BONDING, 1, &peer_data);
	}

	if (nrf_err == NRF_ERROR_BUSY) {
		/* No action. */
	} else if (nrf_err) {
		LOG_ERR("Could not retrieve RAM buffer for incoming bond. pdb_write_buf_get() "
			"returned %s. conn_handle: %d",
			nrf_strerror_get(nrf_err), conn_handle);
		nrf_err = NRF_ERROR_INTERNAL;
	} else {
		memset(peer_data.bonding_data, 0, sizeof(struct pm_peer_data_bonding));

		peer_data.bonding_data->own_role = role;

		sec_keyset->keys_own.p_enc_key = &peer_data.bonding_data->own_ltk;
		sec_keyset->keys_own.p_pk = public_key;
		sec_keyset->keys_peer.p_enc_key = &peer_data.bonding_data->peer_ltk;
		sec_keyset->keys_peer.p_id_key = &peer_data.bonding_data->peer_ble_id;
		sec_keyset->keys_peer.p_pk = &peer_pk;

		/* Retrieve the address the peer used during connection establishment.
		 * This address will be overwritten if ID is shared. Should not fail.
		 */
		nrf_err = im_ble_addr_get(conn_handle,
					  &peer_data.bonding_data->peer_ble_id.id_addr_info);
		if (nrf_err) {
			LOG_WRN("im_ble_addr_get() returned %s. conn_handle: %d. Link was "
				"likely disconnected.",
				nrf_strerror_get(nrf_err), conn_handle);
			return NRF_ERROR_INVALID_STATE;
		}
	}

	return nrf_err;
}

uint32_t smd_params_reply(uint16_t conn_handle, ble_gap_sec_params_t *sec_params,
			  ble_gap_lesc_p256_pk_t *public_key)
{
	__ASSERT_NO_MSG(module_initialized);

	uint8_t role = ble_conn_state_role(conn_handle);
	uint32_t nrf_err = NRF_SUCCESS;
	uint8_t sec_status = BLE_GAP_SEC_STATUS_SUCCESS;
	ble_gap_sec_keyset_t sec_keyset;

	memset(&sec_keyset, 0, sizeof(ble_gap_sec_keyset_t));
#if defined(CONFIG_SOFTDEVICE_PERIPHERAL)
	if (role == BLE_GAP_ROLE_PERIPH) {
		/* Set the default value for allowing repairing at the start of the sec proc. (for
		 * peripheral)
		 */
		ble_conn_state_user_flag_set(conn_handle, flag_allow_repairing, false);
	}
#endif /* CONFIG_SOFTDEVICE_PERIPHERAL */

	if (role == BLE_GAP_ROLE_INVALID) {
		return BLE_ERROR_INVALID_CONN_HANDLE;
	}

#if defined(CONFIG_PM_RA_PROTECTION)
	/* Check for repeated attempts here. */
	if (ast_peer_blacklisted(conn_handle)) {
		sec_status = BLE_GAP_SEC_STATUS_REPEATED_ATTEMPTS;
	} else
#endif /* CONFIG_PM_RA_PROTECTION */
		if (sec_params == NULL) {
			/* NULL params means reject pairing. */
			sec_status = BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP;
		} else {
#if defined(CONFIG_SOFTDEVICE_PERIPHERAL)
			if ((im_peer_id_get_by_conn_handle(conn_handle) != PM_PEER_ID_INVALID) &&
			    (role == BLE_GAP_ROLE_PERIPH) && !allow_repairing(conn_handle)) {
				/* Bond already exists. Reject the pairing request if the user
				 * doesn't intervene.
				 */
				send_config_req(conn_handle);
				if (!allow_repairing(conn_handle)) {
					/* Reject pairing. */
					sec_status = BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP;
				}
			}
#endif /* CONFIG_SOFTDEVICE_PERIPHERAL */

			if (!sec_params->bond) {
				/* Pairing, no bonding. */
				sec_keyset.keys_own.p_pk = public_key;
				sec_keyset.keys_peer.p_pk = &peer_pk;
			} else if (sec_status != BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP) {
				/* Bonding is to be performed, prepare to receive bonding data. */
				nrf_err = sec_keyset_fill(conn_handle, role, public_key,
							  &sec_keyset);
			}
		}

	if (nrf_err == NRF_SUCCESS) {
		/* Everything OK, reply to SoftDevice. If an error happened, the user is given an
		 * opportunity to change the parameters and retry the call.
		 */
		ble_gap_sec_params_t *aux_sec_params = NULL;
#if defined(CONFIG_SOFTDEVICE_PERIPHERAL)
		aux_sec_params = (role == BLE_GAP_ROLE_PERIPH) ? sec_params : NULL;
#endif /* CONFIG_SOFTDEVICE_PERIPHERAL */

		nrf_err = sd_ble_gap_sec_params_reply(conn_handle, sec_status, aux_sec_params,
						      &sec_keyset);
	}

	return nrf_err;
}

uint32_t smd_link_secure(uint16_t conn_handle, ble_gap_sec_params_t *sec_params,
			 bool force_repairing)
{
	__ASSERT_NO_MSG(module_initialized);

	uint8_t role = ble_conn_state_role(conn_handle);

	switch (role) {
#if defined(CONFIG_SOFTDEVICE_CENTRAL)
	case BLE_GAP_ROLE_CENTRAL:
		return link_secure_central(conn_handle, sec_params, force_repairing);
#endif /* CONFIG_SOFTDEVICE_CENTRAL */

#if defined(CONFIG_SOFTDEVICE_PERIPHERAL)
	case BLE_GAP_ROLE_PERIPH:
		return link_secure_peripheral(conn_handle, sec_params);
#endif /* CONFIG_SOFTDEVICE_PERIPHERAL */

	default:
		return BLE_ERROR_INVALID_CONN_HANDLE;
	}
}

void smd_ble_evt_handler(const ble_evt_t *ble_evt)
{
	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_DISCONNECTED:
		disconnect_process(&(ble_evt->evt.gap_evt));
		break;

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		sec_params_request_process(&(ble_evt->evt.gap_evt));
		break;

#if defined(CONFIG_SOFTDEVICE_PERIPHERAL)
	case BLE_GAP_EVT_SEC_INFO_REQUEST:
		sec_info_request_process(&(ble_evt->evt.gap_evt));
		break;
#endif /* CONFIG_SOFTDEVICE_PERIPHERAL */

#if defined(CONFIG_SOFTDEVICE_CENTRAL)
	case BLE_GAP_EVT_SEC_REQUEST:
		sec_request_process(&(ble_evt->evt.gap_evt));
		break;
#endif /* CONFIG_SOFTDEVICE_CENTRAL */

	case BLE_GAP_EVT_AUTH_STATUS:
		auth_status_process(&(ble_evt->evt.gap_evt));
		break;

	case BLE_GAP_EVT_CONN_SEC_UPDATE:
		conn_sec_update_process(&(ble_evt->evt.gap_evt));
		break;
	};
}
