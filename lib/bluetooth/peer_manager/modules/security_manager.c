/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <nrf_strerror.h>
#include <string.h>
#include <ble_err.h>
#include <bm/bluetooth/ble_conn_state.h>
#if defined(CONFIG_PM_LESC)
#include <bm/bluetooth/peer_manager/nrf_ble_lesc.h>
#endif
#include <modules/id_manager.h>
#include <modules/security_dispatcher.h>
#include <modules/peer_database.h>
#include <modules/peer_data_storage.h>
#include <modules/security_manager.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_DECLARE(peer_manager, CONFIG_PEER_MANAGER_LOG_LEVEL);

/* The number of registered event handlers. */
#define SM_EVENT_HANDLERS_CNT ARRAY_SIZE(evt_handlers)

/* Security Manager event handler in Peer Manager. */
extern void pm_sm_evt_handler(struct pm_evt *sm_evt);

/* Security Manager events' handlers.
 * The number of elements in this array is SM_EVENT_HANDLERS_CNT.
 */
static const pm_evt_handler_internal_t evt_handlers[] = {pm_sm_evt_handler};

/* The context type that is used in PM_EVT_CONN_SEC_PARAMS_REQ events and in calls to
 * sm_sec_params_reply().
 */
struct sec_params_reply_context {
	/* The security parameters to use in the call to the security_dispatcher */
	ble_gap_sec_params_t *sec_params;
	/* The buffer for holding the security parameters. */
	ble_gap_sec_params_t sec_params_mem;
	/* Whether @ref sm_sec_params_reply has been called for this context instance. */
	bool params_reply_called;
};

/* Whether the Security Manager module has been initialized. */
static bool module_initialized;

/** The buffer for the default security parameters set by @ref sm_sec_params_set. */
static ble_gap_sec_params_t default_sec_params_buf;
/** The default security parameters set by @ref sm_sec_params_set. */
static ble_gap_sec_params_t *default_sec_params;
/** Whether @ref sm_sec_params_set has been called. */
static bool default_sec_params_set;

#if !defined(CONFIG_PM_LESC)
/* Pointer, provided by the user, to the public key to use for LESC procedures. */
static ble_gap_lesc_p256_pk_t *lesc_public_key;
#endif

/* User flag indicating whether a connection has a pending call to @ref sm_link_secure because it
 * returned @ref NRF_ERROR_BUSY.
 */
static int flag_link_secure_pending_busy = BLE_CONN_STATE_USER_FLAG_INVALID;
/* User flag indicating whether a pending call to @ref sm_link_secure should be called with true for
 * the force_repairing parameter.
 */
static int flag_link_secure_force_repairing = BLE_CONN_STATE_USER_FLAG_INVALID;
/* User flag indicating whether a pending call to @ref sm_link_secure should be called with NULL
 * security parameters.
 */
static int flag_link_secure_null_params = BLE_CONN_STATE_USER_FLAG_INVALID;
/* User flag indicating whether a connection has a pending call to @ref sm_sec_params_reply because
 * it returned @ref NRF_ERROR_BUSY.
 */
static int flag_params_reply_pending_busy = BLE_CONN_STATE_USER_FLAG_INVALID;

/**
 * @brief Function for sending an SM event to all registered event handlers.
 *
 * @param[in]  event  The event to send.
 */
static void evt_send(struct pm_evt *event)
{
	for (uint32_t i = 0; i < SM_EVENT_HANDLERS_CNT; i++) {
		evt_handlers[i](event);
	}
}

/**
 * @brief Function for setting or clearing user flags based on error codes returned from @ref
 *        smd_link_secure or @ref smd_params_reply.
 *
 * @param[in] conn_handle  The connection the call pertained to.
 * @param[in] nrf_err      The error code returned from @ref smd_link_secure or
 *                         @ref smd_params_reply.
 * @param[in] params_reply Whether the call was to @ref smd_params_reply.
 */
static void flags_set_from_err_code(uint16_t conn_handle, uint32_t nrf_err, bool params_reply)
{
	bool flag_value_busy = false;

	if (nrf_err == NRF_ERROR_BUSY) {
		flag_value_busy = true;
	} else {
		flag_value_busy = false;
	}

	if (params_reply) {
		ble_conn_state_user_flag_set(conn_handle, flag_params_reply_pending_busy,
					     flag_value_busy);
		ble_conn_state_user_flag_set(conn_handle, flag_link_secure_pending_busy, false);
	} else {
		ble_conn_state_user_flag_set(conn_handle, flag_link_secure_pending_busy,
					     flag_value_busy);
	}
}

static inline struct pm_evt new_evt(enum pm_evt_id evt_id, uint16_t conn_handle)
{
	struct pm_evt evt = {
		.evt_id = evt_id,
		.conn_handle = conn_handle,
		.peer_id = im_peer_id_get_by_conn_handle(conn_handle),
	};

	return evt;
}

/**
 * @brief Function for sending a PM_EVT_ERROR_UNEXPECTED event.
 *
 * @param[in] conn_handle The connection handle the event pertains to.
 * @param[in] nrf_err     The unexpected error that occurred.
 */
static void send_unexpected_error(uint16_t conn_handle, uint32_t nrf_err)
{
	struct pm_evt error_evt = new_evt(PM_EVT_ERROR_UNEXPECTED, conn_handle);

	error_evt.params.error_unexpected.error = nrf_err;
	evt_send(&error_evt);
}

/**
 * @brief Returns whether the LTK came from LESC bonding.
 *
 * @param[in] peer_id The peer to check.
 *
 * @return  Whether the key is LESC or not.
 */
static bool key_is_lesc(uint16_t peer_id)
{
	struct pm_peer_data_bonding bonding_data = { 0 };
	uint32_t bonding_data_size = sizeof(struct pm_peer_data_bonding);
	struct pm_peer_data peer_data;
	uint32_t nrf_err;

	peer_data.all_data = &bonding_data;

	nrf_err = pds_peer_data_read(peer_id, PM_PEER_DATA_ID_BONDING, &peer_data,
				     &bonding_data_size);

	return (nrf_err == NRF_SUCCESS) && (bonding_data.own_ltk.enc_info.lesc);
}

/**
 * @brief Function for sending an event based on error codes returned from @ref smd_link_secure or
 *        @ref smd_params_reply.
 *
 * @param[in]  conn_handle  The connection the event pertains to.
 * @param[in]  nrf_err      The error code returned from @ref smd_link_secure or
 *                          @ref smd_params_reply.
 * @param[in]  sec_params   The security parameters attempted to pass in the call to
 *                          @ref smd_link_secure or @ref smd_params_reply.
 */
static void events_send_from_err_code(uint16_t conn_handle, uint32_t nrf_err,
				      ble_gap_sec_params_t *sec_params)
{
	if ((nrf_err != NRF_SUCCESS) &&
	    (nrf_err != NRF_ERROR_BUSY) &&
	    (nrf_err != NRF_ERROR_INVALID_STATE)) {
		if (nrf_err == NRF_ERROR_TIMEOUT) {
			LOG_WRN("Cannot secure link because a previous security procedure "
				"ended in timeout. "
				"Disconnect and retry. smd_params_reply() or "
				"smd_link_secure() returned "
				"NRF_ERROR_TIMEOUT. conn_handle: %d",
				conn_handle);

			struct pm_evt evt = new_evt(PM_EVT_CONN_SEC_FAILED, conn_handle);

			evt.params.conn_sec_failed.procedure =
				((sec_params != NULL) && sec_params->bond)
					? PM_CONN_SEC_PROCEDURE_BONDING
					: PM_CONN_SEC_PROCEDURE_PAIRING;
			evt.params.conn_sec_failed.error_src = BLE_GAP_SEC_STATUS_SOURCE_LOCAL;
			evt.params.conn_sec_failed.error = PM_CONN_SEC_ERROR_SMP_TIMEOUT;
			evt_send(&evt);
		} else {
			LOG_ERR("Could not perform security procedure. smd_params_reply() or "
				"smd_link_secure() returned %s. conn_handle: %d",
				nrf_strerror_get(nrf_err), conn_handle);
			send_unexpected_error(conn_handle, nrf_err);
		}
	}
}

/**
 * @brief Function for sending an PM_EVT_CONN_SEC_PARAMS_REQ event.
 *
 * @param[in]  conn_handle  The connection the event pertains to.
 * @param[in]  peer_params  The peer's security parameters to include in the event. Can be NULL.
 * @param[in]  context      Pointer to a context that the user must include in the call to @ref
 *                          sm_sec_params_reply().
 */
static void params_req_send(uint16_t conn_handle, const ble_gap_sec_params_t *peer_params,
			    struct sec_params_reply_context *context)
{
	struct pm_evt evt = new_evt(PM_EVT_CONN_SEC_PARAMS_REQ, conn_handle);

	evt.params.conn_sec_params_req.peer_params = peer_params;
	evt.params.conn_sec_params_req.context = context;

	evt_send(&evt);
}

/**
 * @brief Function for creating a new @ref sec_params_reply_context with the correct initial
 * values.
 *
 * @return  The new context.
 */
static struct sec_params_reply_context new_context_get(void)
{
	struct sec_params_reply_context new_context = {.sec_params = default_sec_params,
						       .params_reply_called = false};
	return new_context;
}

/**
 * @brief Internal function corresponding to @ref sm_link_secure.
 *
 * @param[in]  conn_handle      The connection to secure.
 * @param[in]  null_params      Whether to pass NULL security parameters to the security_dispatcher.
 * @param[in]  force_repairing  Whether to force rebonding if peer exists.
 * @param[in]  send_events      Whether to send events based on the result of @ref smd_link_secure.
 *
 * @return  Same return codes as @ref sm_link_secure.
 */
static uint32_t link_secure(uint16_t conn_handle, bool null_params, bool force_repairing,
			    bool send_events)
{
	uint32_t nrf_err;
	uint32_t return_nrf_err;
	ble_gap_sec_params_t *sec_params;

	if (null_params) {
		sec_params = NULL;
	} else {
		struct sec_params_reply_context context = new_context_get();

		params_req_send(conn_handle, NULL, &context);
		sec_params = context.sec_params;

		if (!default_sec_params_set && !context.params_reply_called) {
			/* Security parameters have not been set. */
			return NRF_ERROR_NOT_FOUND;
		}
	}

	nrf_err = smd_link_secure(conn_handle, sec_params, force_repairing);

	flags_set_from_err_code(conn_handle, nrf_err, false);

	switch (nrf_err) {
	case NRF_ERROR_BUSY:
		ble_conn_state_user_flag_set(conn_handle, flag_link_secure_null_params,
					     null_params);
		ble_conn_state_user_flag_set(conn_handle, flag_link_secure_force_repairing,
					     force_repairing);
		return_nrf_err = NRF_SUCCESS;
		break;
	case NRF_SUCCESS:
	case NRF_ERROR_TIMEOUT:
	case BLE_ERROR_INVALID_CONN_HANDLE:
	case NRF_ERROR_INVALID_STATE:
	case NRF_ERROR_INVALID_DATA:
		return_nrf_err = nrf_err;
		break;
	default:
		LOG_ERR("Could not perform security procedure. smd_link_secure() returned %s. "
			"conn_handle: %d",
			nrf_strerror_get(nrf_err), conn_handle);
		return_nrf_err = NRF_ERROR_INTERNAL;
		break;
	}

	if (send_events) {
		events_send_from_err_code(conn_handle, nrf_err, sec_params);
	}

	return return_nrf_err;
}

/**
 * @brief Function for requesting security parameters from the user and passing them to the
 * security_dispatcher.
 *
 * @param[in]  conn_handle  The connection that needs security parameters.
 * @param[in]  peer_params  The peer's security parameters if present. Otherwise NULL.
 */
static void smd_params_reply_perform(uint16_t conn_handle,
				     const ble_gap_sec_params_t *peer_params)
{
	uint32_t nrf_err;
	ble_gap_lesc_p256_pk_t *public_key;
	struct sec_params_reply_context context = new_context_get();

	params_req_send(conn_handle, peer_params, &context);

#if defined(CONFIG_PM_LESC)
	public_key = nrf_ble_lesc_public_key_get();
#else
	public_key = lesc_public_key;
#endif /* CONFIG_PM_LESC */
	nrf_err = smd_params_reply(conn_handle, context.sec_params, public_key);

	flags_set_from_err_code(conn_handle, nrf_err, true);
	events_send_from_err_code(conn_handle, nrf_err, context.sec_params);
}

/**
 * @brief Function for handling @ref PM_EVT_CONN_SEC_PARAMS_REQ events.
 *
 * @param[in]  event  The @ref PM_EVT_CONN_SEC_PARAMS_REQ event.
 */
static __INLINE void params_req_process(const struct pm_evt *event)
{
	smd_params_reply_perform(event->conn_handle,
				 event->params.conn_sec_params_req.peer_params);
}

uint32_t sm_conn_sec_status_get(uint16_t conn_handle, struct pm_conn_sec_status *conn_sec_status)
{
	if (conn_sec_status == NULL) {
		return NRF_ERROR_NULL;
	}

	int status = ble_conn_state_status(conn_handle);

	if (status == BLE_CONN_STATUS_INVALID) {
		return BLE_ERROR_INVALID_CONN_HANDLE;
	}

	uint16_t peer_id = im_peer_id_get_by_conn_handle(conn_handle);

	conn_sec_status->connected = (status == BLE_CONN_STATUS_CONNECTED);
	conn_sec_status->bonded = (peer_id != PM_PEER_ID_INVALID);
	conn_sec_status->encrypted = ble_conn_state_encrypted(conn_handle);
	conn_sec_status->mitm_protected = ble_conn_state_mitm_protected(conn_handle);
	conn_sec_status->lesc = ble_conn_state_lesc(conn_handle) ||
				  (ble_conn_state_encrypted(conn_handle) && key_is_lesc(peer_id));
	return NRF_SUCCESS;
}

bool sm_sec_is_sufficient(uint16_t conn_handle, struct pm_conn_sec_status *sec_status_req)
{
	/* Set all bits in reserved to 1 so they are ignored in subsequent logic. */
	struct pm_conn_sec_status sec_status = {.reserved = ~0};
	uint32_t nrf_err = sm_conn_sec_status_get(conn_handle, &sec_status);

	__ASSERT_NO_MSG(sizeof(struct pm_conn_sec_status) == sizeof(uint8_t));

	uint8_t unmet_reqs = (~(*((uint8_t *)&sec_status)) & *((uint8_t *)sec_status_req));

	return (nrf_err == NRF_SUCCESS) && !unmet_reqs;
}

#if defined(CONFIG_SOFTDEVICE_CENTRAL)
/**
 * @brief Function for handling @ref PM_EVT_SLAVE_SECURITY_REQ events.
 *
 * @param[in]  event  The @ref PM_EVT_SLAVE_SECURITY_REQ event.
 */
static void sec_req_process(const struct pm_evt *event)
{
	bool null_params = false;
	bool force_repairing = false;

	if (default_sec_params == NULL) {
		null_params = true;
	} else if (ble_conn_state_encrypted(event->conn_handle)) {
		struct pm_conn_sec_status sec_status_req = {
			.bonded = event->params.slave_security_req.bond,
			.mitm_protected = event->params.slave_security_req.mitm,
			.lesc = event->params.slave_security_req.lesc,
		};

		force_repairing = !sm_sec_is_sufficient(event->conn_handle, &sec_status_req);
	}

	(void)link_secure(event->conn_handle, null_params, force_repairing, true);
	/* The error code has been properly handled inside link_secure(). */
}
#endif

/**
 * @brief Function for translating an SMD event to an SM event and passing it on to SM event
 * handlers.
 *
 * @param[in]  event  The event to forward.
 */
static void evt_forward(struct pm_evt *event)
{
	evt_send(event);
}

/**
 * @brief Event handler for events from the Security Dispatcher module.
 *        This handler is extern in Security Dispatcher.
 *
 * @param[in]  event   The event that has happened.
 */
void sm_smd_evt_handler(struct pm_evt *event)
{
	switch (event->evt_id) {
	case PM_EVT_CONN_SEC_PARAMS_REQ:
		params_req_process(event);
		break;
	case PM_EVT_SLAVE_SECURITY_REQ:
#if defined(CONFIG_SOFTDEVICE_CENTRAL)
		sec_req_process(event);
#endif
		/* fallthrough */
	default:
		/* Forward the event to all registered Security Manager event handlers. */
		evt_forward(event);
		break;
	}
}

/** @brief Function handling a pending params_reply. See @ref ble_conn_state_user_function_t. */
static void params_reply_pending_handle(uint16_t conn_handle, void *context)
{
	ARG_UNUSED(context);
	smd_params_reply_perform(conn_handle, NULL);
}

/** @brief Function handling a pending link_secure. See @ref ble_conn_state_user_function_t. */
static void link_secure_pending_handle(uint16_t conn_handle, void *context)
{
	ARG_UNUSED(context);

	bool force_repairing =
		ble_conn_state_user_flag_get(conn_handle, flag_link_secure_force_repairing);
	bool null_params =
		ble_conn_state_user_flag_get(conn_handle, flag_link_secure_null_params);

	/* If this fails, it will be automatically retried. */
	(void)link_secure(conn_handle, null_params, force_repairing, true);
}

/**
 * @brief Event handler for events from the Peer Database module.
 *        This handler is extern in Peer Database.
 *
 * @param[in]  event   The event that has happened.
 */
void sm_pdb_evt_handler(struct pm_evt *event)
{
	switch (event->evt_id) {
	case PM_EVT_FLASH_GARBAGE_COLLECTED:
	case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
	case PM_EVT_PEER_DATA_UPDATE_FAILED:
	case PM_EVT_PEER_DELETE_SUCCEEDED:
	case PM_EVT_PEER_DELETE_FAILED:
		(void)ble_conn_state_for_each_set_user_flag(flag_params_reply_pending_busy,
							    params_reply_pending_handle, NULL);
		(void)ble_conn_state_for_each_set_user_flag(flag_link_secure_pending_busy,
							    link_secure_pending_handle, NULL);
		break;
	default:
		/* Do nothing. */
		break;
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

uint32_t sm_init(void)
{
	__ASSERT_NO_MSG(!module_initialized);

#if defined(CONFIG_PM_LESC)
	uint32_t nrf_err = nrf_ble_lesc_init();

	if (nrf_err) {
		return nrf_err;
	}
#endif

	flag_id_init(&flag_link_secure_pending_busy);
	flag_id_init(&flag_link_secure_force_repairing);
	flag_id_init(&flag_link_secure_null_params);
	flag_id_init(&flag_params_reply_pending_busy);

	if (flag_params_reply_pending_busy == BLE_CONN_STATE_USER_FLAG_INVALID) {
		LOG_ERR("Could not acquire conn_state user flags. Increase "
			"BLE_CONN_STATE_USER_FLAG_COUNT in the ble_conn_state module.");
		return NRF_ERROR_INTERNAL;
	}

	module_initialized = true;

	return NRF_SUCCESS;
}

void sm_ble_evt_handler(const ble_evt_t *ble_evt)
{
	__ASSERT_NO_MSG(ble_evt != NULL);

	smd_ble_evt_handler(ble_evt);
#if defined(CONFIG_PM_LESC)
	nrf_ble_lesc_on_ble_evt(ble_evt);
#endif
	(void)ble_conn_state_for_each_set_user_flag(flag_params_reply_pending_busy,
						    params_reply_pending_handle, NULL);
	(void)ble_conn_state_for_each_set_user_flag(flag_link_secure_pending_busy,
						    link_secure_pending_handle, NULL);
}

/**
 * @brief Function for checking whether security parameters are valid.
 *
 * @param[out] sec_params  The security parameters to verify.
 *
 * @return  Whether the security parameters are valid.
 */
static bool sec_params_verify(ble_gap_sec_params_t *sec_params)
{
	/* NULL check. */
	if (sec_params == NULL) {
		return false;
	}

	/* OOB not allowed unless MITM. */
	if (!sec_params->mitm && sec_params->oob) {
		return false;
	}

	/* IO Capabilities must be one of the valid values from @ref BLE_GAP_IO_CAPS. */
	if (sec_params->io_caps > BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY) {
		return false;
	}

	/* Must have either IO capabilities or OOB if MITM. */
	if (sec_params->mitm && (sec_params->io_caps == BLE_GAP_IO_CAPS_NONE) &&
	    !sec_params->oob) {
		return false;
	}

	/* Minimum key size cannot be larger than maximum key size. */
	if (sec_params->min_key_size > sec_params->max_key_size) {
		return false;
	}

	/* Key size cannot be below 7 bytes. */
	if (sec_params->min_key_size < 7) {
		return false;
	}

	/* Key size cannot be above 16 bytes. */
	if (sec_params->max_key_size > 16) {
		return false;
	}

	/* If bonding is not enabled, no keys can be distributed. */
	if (!sec_params->bond && (sec_params->kdist_own.enc || sec_params->kdist_own.id ||
				  sec_params->kdist_peer.enc || sec_params->kdist_peer.id)) {
		return false;
	}

	/* If bonding is enabled, one or more keys must be distributed. */
	if (sec_params->bond && !sec_params->kdist_own.enc && !sec_params->kdist_own.id &&
	    !sec_params->kdist_peer.enc && !sec_params->kdist_peer.id) {
		return false;
	}

	return true;
}

uint32_t sm_sec_params_set(ble_gap_sec_params_t *sec_params)
{
	__ASSERT_NO_MSG(module_initialized);

	if (sec_params == NULL) {
		default_sec_params = NULL;
		default_sec_params_set = true;
		return NRF_SUCCESS;
	} else if (sec_params_verify(sec_params)) {
		default_sec_params_buf = *sec_params;
		default_sec_params = &default_sec_params_buf;
		default_sec_params_set = true;
		return NRF_SUCCESS;
	} else {
		return NRF_ERROR_INVALID_PARAM;
	}
}

void sm_conn_sec_config_reply(uint16_t conn_handle, struct pm_conn_sec_config *conn_sec_config)
{
	__ASSERT_NO_MSG(module_initialized);
	__ASSERT_NO_MSG(conn_sec_config != NULL);

	smd_conn_sec_config_reply(conn_handle, conn_sec_config);
}

uint32_t sm_sec_params_reply(uint16_t conn_handle, ble_gap_sec_params_t *sec_params,
			     const void *context)
{
	__ASSERT_NO_MSG(module_initialized);

	if (context == NULL) {
		return NRF_ERROR_NULL;
	}

	struct sec_params_reply_context *sec_params_reply_context =
		(struct sec_params_reply_context *)context;
	if (sec_params == NULL) {
		/* Set the store pointer to NULL, so that NULL is passed to the SoftDevice. */
		sec_params_reply_context->sec_params = NULL;
	} else if (sec_params_verify(sec_params)) {
		/* Copy the provided sec_params into the store. */
		sec_params_reply_context->sec_params_mem = *sec_params;
		sec_params_reply_context->sec_params =
			&sec_params_reply_context->sec_params_mem;
	} else {
		return NRF_ERROR_INVALID_PARAM;
	}
	sec_params_reply_context->params_reply_called = true;

	return NRF_SUCCESS;
}

uint32_t sm_lesc_public_key_set(ble_gap_lesc_p256_pk_t *public_key)
{
	__ASSERT_NO_MSG(module_initialized);

#if defined(CONFIG_PM_LESC)
	return NRF_ERROR_FORBIDDEN;
#else
	lesc_public_key = public_key;

	return NRF_SUCCESS;
#endif /* CONFIG_PM_LESC */
}

uint32_t sm_link_secure(uint16_t conn_handle, bool force_repairing)
{
	__ASSERT_NO_MSG(module_initialized);

	return link_secure(conn_handle, false, force_repairing, false);
}
