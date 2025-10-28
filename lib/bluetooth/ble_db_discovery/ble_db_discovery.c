/*
 * Copyright (c) 2013 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm/bluetooth/ble_db_discovery.h>
#include <stdlib.h>
#include <inttypes.h>
#include <bm/bluetooth/ble_gq.h>
#include <zephyr/logging/log.h>
#include <bm/bluetooth/services/uuid.h>

LOG_MODULE_REGISTER(ble_db_disc, CONFIG_BLE_DB_DISCOVERY_LOG_LEVEL);

static ble_uuid_t registered_handlers[CONFIG_BLE_DB_DISCOVERY_MAX_SRV];
static ble_db_discovery_evt_handler evt_handler;
/* Pointer to BLE GATT Queue instance. */
static struct ble_gq *gatt_queue;

/* The number of handlers registered with the DB Discovery module. */
static uint32_t num_of_handlers_reg;

static ble_db_discovery_evt_handler registered_handler_get(ble_uuid_t const *srv_uuid)
{
	for (uint32_t i = 0; i < num_of_handlers_reg; i++) {
		if (BLE_UUID_EQ(&(registered_handlers[i]), srv_uuid)) {
			return evt_handler;
		}
	}

	return NULL;
}

static uint32_t registered_handler_set(ble_uuid_t const *srv_uuid,
				       ble_db_discovery_evt_handler evt_handler)
{
	if (registered_handler_get(srv_uuid) != NULL) {
		return NRF_SUCCESS;
	}

	if (num_of_handlers_reg < CONFIG_BLE_DB_DISCOVERY_MAX_SRV) {
		registered_handlers[num_of_handlers_reg] = *srv_uuid;
		num_of_handlers_reg++;

		return NRF_SUCCESS;
	} else {
		return NRF_ERROR_NO_MEM;
	}
}

static void pending_user_evts_send(struct ble_db_discovery *db_discovery)
{
	for (uint32_t i = 0; i < num_of_handlers_reg; i++) {
		/* Pass the event to the corresponding event handler. */
		db_discovery->pending_usr_evts[i].evt_handler(
			&(db_discovery->pending_usr_evts[i].evt));
	}

	db_discovery->pending_usr_evt_index = 0;
}

static void discovery_available_evt_trigger(struct ble_db_discovery *const db_discovery,
					    uint16_t const conn_handle)
{
	struct ble_db_discovery_evt evt = {0};

	evt.conn_handle = conn_handle;
	evt.evt_type = BLE_DB_DISCOVERY_AVAILABLE;
	evt.params.db_instance = (void *)db_discovery;

	if (evt_handler) {
		evt_handler(&evt);
	}
}

static void discovery_error_evt_trigger(struct ble_db_discovery *db_discovery, uint32_t err_code,
					uint16_t conn_handle)
{
	ble_db_discovery_evt_handler evt_handler;
	struct ble_gatt_db_srv *srv_being_discovered;

	srv_being_discovered = &(db_discovery->services[db_discovery->curr_srv_ind]);

	evt_handler = registered_handler_get(&(srv_being_discovered->srv_uuid));

	if (evt_handler != NULL) {
		struct ble_db_discovery_evt evt = {
			.conn_handle = conn_handle,
			.evt_type = BLE_DB_DISCOVERY_ERROR,
			.params.error.reason = err_code,
		};

		evt_handler(&evt);
	}
}

static void discovery_error_handler(uint16_t conn_handle, uint32_t nrf_error, void *ctx)
{
	struct ble_db_discovery *db_discovery = (struct ble_db_discovery *)ctx;

	db_discovery->discovery_in_progress = false;

	discovery_error_evt_trigger(db_discovery, nrf_error, conn_handle);
	discovery_available_evt_trigger(db_discovery, conn_handle);
}

static void discovery_complete_evt_trigger(struct ble_db_discovery *db_discovery, bool is_srv_found,
					   uint16_t conn_handle)
{
	ble_db_discovery_evt_handler evt_handler;
	struct ble_gatt_db_srv *srv_being_discovered;

	srv_being_discovered = &(db_discovery->services[db_discovery->curr_srv_ind]);

	evt_handler = registered_handler_get(&(srv_being_discovered->srv_uuid));

	if (evt_handler != NULL) {
		if (db_discovery->pending_usr_evt_index < CONFIG_BLE_DB_DISCOVERY_MAX_SRV) {
			/* Insert an event into the pending event list. */
			db_discovery->pending_usr_evts[db_discovery->pending_usr_evt_index]
				.evt.conn_handle = conn_handle;
			db_discovery->pending_usr_evts[db_discovery->pending_usr_evt_index]
				.evt.params.discovered_db = *srv_being_discovered;

			if (is_srv_found) {
				db_discovery->pending_usr_evts[db_discovery->pending_usr_evt_index]
					.evt.evt_type = BLE_DB_DISCOVERY_COMPLETE;
			} else {
				db_discovery->pending_usr_evts[db_discovery->pending_usr_evt_index]
					.evt.evt_type = BLE_DB_DISCOVERY_SRV_NOT_FOUND;
			}

			db_discovery->pending_usr_evts[db_discovery->pending_usr_evt_index]
				.evt_handler = evt_handler;
			db_discovery->pending_usr_evt_index++;

			if (db_discovery->pending_usr_evt_index == num_of_handlers_reg) {
				/* All registered modules have pending events. Send all pending
				 * events to the user modules.
				 */
				pending_user_evts_send(db_discovery);
			} else {
				/* Too many events pending. Do nothing. (Ideally this should not
				 * happen.)
				 */
			}
		}
	}
}

static void on_srv_disc_completion(struct ble_db_discovery *db_discovery, uint16_t conn_handle)
{
	struct ble_gq_req db_srv_disc_req = {0};

	db_discovery->discoveries_count++;

	/* Check if more services need to be discovered. */
	if (db_discovery->discoveries_count < num_of_handlers_reg) {
		/* Reset the current characteristic index since a new service discovery is about to
		 * start.
		 */
		db_discovery->curr_char_ind = 0;

		/* Initiate discovery of the next service. */
		db_discovery->curr_srv_ind++;

		struct ble_gatt_db_srv *srv_being_discovered;

		srv_being_discovered = &(db_discovery->services[db_discovery->curr_srv_ind]);

		srv_being_discovered->srv_uuid = registered_handlers[db_discovery->curr_srv_ind];

		/* Reset the characteristic count in the current service to zero since a new
		 * service discovery is about to start.
		 */
		srv_being_discovered->char_count = 0;

		LOG_DBG("Starting discovery of service with UUID 0x%x on connection handle 0x%x.",
			srv_being_discovered->srv_uuid.uuid, conn_handle);

		uint32_t err_code;

		db_srv_disc_req.type = BLE_GQ_REQ_SRV_DISCOVERY;
		db_srv_disc_req.gattc_srv_disc.start_handle = CONFIG_SRV_DISC_START_HANDLE;
		db_srv_disc_req.gattc_srv_disc.srvc_uuid = srv_being_discovered->srv_uuid;
		db_srv_disc_req.error_handler.ctx = db_discovery;
		db_srv_disc_req.error_handler.cb = discovery_error_handler;

		err_code = ble_gq_item_add(gatt_queue, &db_srv_disc_req, conn_handle);

		if (err_code != NRF_SUCCESS) {
			discovery_error_handler(conn_handle, err_code, db_discovery);

			return;
		}
	} else {
		/* No more service discovery is needed. */
		db_discovery->discovery_in_progress = false;

		discovery_available_evt_trigger(db_discovery, conn_handle);
	}
}

static bool is_char_discovery_reqd(struct ble_db_discovery *db_discovery,
				   ble_gattc_char_t *after_char)
{
	if (after_char->handle_value <
	    db_discovery->services[db_discovery->curr_srv_ind].handle_range.end_handle) {
		/* Handle value of the characteristic being discovered is less than the end handle
		 * of the service being discovered. There is a possibility of more characteristics
		 * being present. Hence a characteristic discovery is required.
		 */
		return true;
	}

	return false;
}

static bool is_desc_discovery_reqd(struct ble_db_discovery *db_discovery,
				   struct ble_gatt_db_char *curr_char,
				   struct ble_gatt_db_char *next_char,
				   ble_gattc_handle_range_t *handle_range)
{
	if (next_char == NULL) {
		/* Current characteristic is the last characteristic in the service. Check if the
		 * value handle of the current characteristic is equal to the service end handle.
		 */
		if (curr_char->characteristic.handle_value ==
		    db_discovery->services[db_discovery->curr_srv_ind].handle_range.end_handle) {
			/* No descriptors can be present for the current characteristic. curr_char
			 * is the last characteristic with no descriptors.
			 */
			return false;
		}

		handle_range->start_handle = curr_char->characteristic.handle_value + 1;

		/* Since the current characteristic is the last characteristic in the service, the
		 * end handle should be the end handle of the service.
		 */
		handle_range->end_handle =
			db_discovery->services[db_discovery->curr_srv_ind].handle_range.end_handle;

		return true;
	}

	/* next_char != NULL. Check for existence of descriptors between the current and the next
	 * characteristic.
	 */
	if ((curr_char->characteristic.handle_value + 1) == next_char->characteristic.handle_decl) {
		/* No descriptors can exist between the two characteristic. */
		return false;
	}

	handle_range->start_handle = curr_char->characteristic.handle_value + 1;
	handle_range->end_handle = next_char->characteristic.handle_decl - 1;

	return true;
}

static uint32_t characteristics_discover(struct ble_db_discovery *db_discovery,
					 uint16_t conn_handle)
{
	struct ble_gatt_db_srv *srv_being_discovered;
	ble_gattc_handle_range_t handle_range = {0};
	struct ble_gq_req db_char_disc_req = {0};

	srv_being_discovered = &(db_discovery->services[db_discovery->curr_srv_ind]);

	if (db_discovery->curr_char_ind != 0) {
		/* This is not the first characteristic being discovered. Hence the 'start handle'
		 * to be used must be computed using the handle_value of the previous
		 * characteristic.
		 */
		ble_gattc_char_t *prev_char;
		uint8_t prev_char_ind = db_discovery->curr_char_ind - 1;

		srv_being_discovered = &(db_discovery->services[db_discovery->curr_srv_ind]);

		prev_char = &(srv_being_discovered->charateristics[prev_char_ind].characteristic);

		handle_range.start_handle = prev_char->handle_value + 1;
	} else {
		/* This is the first characteristic of this service being discovered. */
		handle_range.start_handle = srv_being_discovered->handle_range.start_handle;
	}

	handle_range.end_handle = srv_being_discovered->handle_range.end_handle;

	db_char_disc_req.type = BLE_GQ_REQ_CHAR_DISCOVERY;
	db_char_disc_req.gattc_char_disc = handle_range;
	db_char_disc_req.error_handler.ctx = db_discovery;
	db_char_disc_req.error_handler.cb = discovery_error_handler;

	return ble_gq_item_add(gatt_queue, &db_char_disc_req, conn_handle);
}

static uint32_t descriptors_discover(struct ble_db_discovery *db_discovery,
				     bool *raise_discov_complete, uint16_t conn_handle)
{
	ble_gattc_handle_range_t handle_range;
	struct ble_gatt_db_char *curr_char_being_discovered;
	struct ble_gatt_db_srv *srv_being_discovered;
	struct ble_gq_req db_desc_disc_req = {0};
	bool is_discovery_reqd = false;

	srv_being_discovered = &(db_discovery->services[db_discovery->curr_srv_ind]);

	curr_char_being_discovered =
		&(srv_being_discovered->charateristics[db_discovery->curr_char_ind]);

	if ((db_discovery->curr_char_ind + 1) == srv_being_discovered->char_count) {
		/* This is the last characteristic of this service. */
		is_discovery_reqd = is_desc_discovery_reqd(db_discovery, curr_char_being_discovered,
							   NULL, &handle_range);
	} else {
		uint8_t i;
		struct ble_gatt_db_char *next_char;

		for (i = db_discovery->curr_char_ind; i < srv_being_discovered->char_count; i++) {
			if (i == (srv_being_discovered->char_count - 1)) {
				/* The current characteristic is the last characteristic in the
				 * service.
				 */
				next_char = NULL;
			} else {
				next_char = &(srv_being_discovered->charateristics[i + 1]);
			}

			/* Check if it is possible for the current characteristic to have a
			 * descriptor.
			 */
			if (is_desc_discovery_reqd(db_discovery, curr_char_being_discovered,
						   next_char, &handle_range)) {
				is_discovery_reqd = true;
				break;
			}
			/* No descriptors can exist. */
			curr_char_being_discovered = next_char;
			db_discovery->curr_char_ind++;
		}
	}

	if (!is_discovery_reqd) {
		/* No more descriptor discovery required. Discovery is complete.
		 * This informs the caller that a discovery complete event can be triggered.
		 */
		*raise_discov_complete = true;

		return NRF_SUCCESS;
	}

	*raise_discov_complete = false;

	db_desc_disc_req.type = BLE_GQ_REQ_DESC_DISCOVERY;
	db_desc_disc_req.gattc_desc_disc = handle_range;
	db_desc_disc_req.error_handler.ctx = db_discovery;
	db_desc_disc_req.error_handler.cb = discovery_error_handler;

	return ble_gq_item_add(gatt_queue, &db_desc_disc_req, conn_handle);
}

static void on_primary_srv_discovery_rsp(struct ble_db_discovery *db_discovery,
					 ble_gattc_evt_t const *ble_gattc_evt)
{
	struct ble_gatt_db_srv *srv_being_discovered;

	srv_being_discovered = &(db_discovery->services[db_discovery->curr_srv_ind]);

	if (ble_gattc_evt->conn_handle != db_discovery->conn_handle) {
		return;
	}

	if (ble_gattc_evt->gatt_status == BLE_GATT_STATUS_SUCCESS) {
		uint32_t err_code;
		ble_gattc_evt_prim_srvc_disc_rsp_t const *prim_srvc_disc_rsp_evt;

		LOG_DBG("Found service UUID 0x%x.", srv_being_discovered->srv_uuid.uuid);

		prim_srvc_disc_rsp_evt = &(ble_gattc_evt->params.prim_srvc_disc_rsp);

		srv_being_discovered->handle_range =
			prim_srvc_disc_rsp_evt->services[0].handle_range;

		/* Number of services, previously discovered. */
		uint8_t num_srv_previous_disc = db_discovery->srv_count;

		/* Number of services, currently discovered. */
		uint8_t current_srv_disc = prim_srvc_disc_rsp_evt->count;

		if ((num_srv_previous_disc + current_srv_disc) <= CONFIG_BLE_DB_DISCOVERY_MAX_SRV) {
			db_discovery->srv_count += current_srv_disc;
		} else {
			db_discovery->srv_count = CONFIG_BLE_DB_DISCOVERY_MAX_SRV;

			LOG_WRN("Not enough space for services.");
			LOG_WRN("Increase CONFIG_BLE_DB_DISCOVERY_MAX_SRV to be able to store more "
				"services!");
		}

		err_code = characteristics_discover(db_discovery, ble_gattc_evt->conn_handle);

		if (err_code != NRF_SUCCESS) {
			discovery_error_handler(ble_gattc_evt->conn_handle, err_code, db_discovery);
		}
	} else {
		LOG_DBG("Service UUID 0x%x not found.", srv_being_discovered->srv_uuid.uuid);
		/* Trigger Service Not Found event to the application. */
		discovery_complete_evt_trigger(db_discovery, false, ble_gattc_evt->conn_handle);
		on_srv_disc_completion(db_discovery, ble_gattc_evt->conn_handle);
	}
}

static void on_characteristic_discovery_rsp(struct ble_db_discovery *db_discovery,
					    ble_gattc_evt_t const *ble_gattc_evt)
{
	uint32_t err_code;
	struct ble_gatt_db_srv *srv_being_discovered;
	bool perform_desc_discov = false;

	if (ble_gattc_evt->conn_handle != db_discovery->conn_handle) {
		return;
	}

	srv_being_discovered = &(db_discovery->services[db_discovery->curr_srv_ind]);

	if (ble_gattc_evt->gatt_status == BLE_GATT_STATUS_SUCCESS) {
		ble_gattc_evt_char_disc_rsp_t const *char_disc_rsp_evt;

		char_disc_rsp_evt = &(ble_gattc_evt->params.char_disc_rsp);

		/* Find out the number of characteristics that were previously discovered (in
		 * earlier characteristic discovery responses, if any).
		 */
		uint8_t num_chars_prev_disc = srv_being_discovered->char_count;

		/* Find out the number of characteristics that are currently discovered (in the
		 * characteristic discovery response being handled).
		 */
		uint8_t num_chars_curr_disc = char_disc_rsp_evt->count;

		/* Check if the total number of discovered characteristics are supported by this
		 * module.
		 */
		if ((num_chars_prev_disc + num_chars_curr_disc) <= CONFIG_BLE_GATT_DB_MAX_CHARS) {
			/* Update the characteristics count. */
			srv_being_discovered->char_count += num_chars_curr_disc;
		} else {
			/* The number of characteristics discovered at the peer is more than the
			 * supported maximum. This module will store only the characteristics found
			 * up to this point.
			 */
			srv_being_discovered->char_count = CONFIG_BLE_GATT_DB_MAX_CHARS;
			LOG_WRN("Not enough space for characteristics associated with "
				"service 0x%04X !",
				srv_being_discovered->srv_uuid.uuid);
			LOG_WRN("Increase CONFIG_BLE_GATT_DB_MAX_CHARS to be able to store more "
				"characteristics for each service!");
		}

		uint32_t i;
		uint32_t j;

		for (i = num_chars_prev_disc, j = 0; i < srv_being_discovered->char_count;
		     i++, j++) {
			srv_being_discovered->charateristics[i].characteristic =
				char_disc_rsp_evt->chars[j];

			srv_being_discovered->charateristics[i].cccd_handle =
				BLE_GATT_HANDLE_INVALID;
			srv_being_discovered->charateristics[i].ext_prop_handle =
				BLE_GATT_HANDLE_INVALID;
			srv_being_discovered->charateristics[i].user_desc_handle =
				BLE_GATT_HANDLE_INVALID;
			srv_being_discovered->charateristics[i].report_ref_handle =
				BLE_GATT_HANDLE_INVALID;
		}

		ble_gattc_char_t *last_known_char;

		last_known_char = &(srv_being_discovered->charateristics[i - 1].characteristic);

		/* If no more characteristic discovery is required, or if the maximum number of
		 * supported characteristic per service has been reached, descriptor discovery will
		 * be performed.
		 */
		if (!is_char_discovery_reqd(db_discovery, last_known_char) ||
		    (srv_being_discovered->char_count == CONFIG_BLE_GATT_DB_MAX_CHARS)) {
			perform_desc_discov = true;
		} else {
			/* Update the current characteristic index. */
			db_discovery->curr_char_ind = srv_being_discovered->char_count;

			/* Perform another round of characteristic discovery. */
			err_code =
				characteristics_discover(db_discovery, ble_gattc_evt->conn_handle);

			if (err_code != NRF_SUCCESS) {
				discovery_error_handler(ble_gattc_evt->conn_handle, err_code,
							db_discovery);

				return;
			}
		}
	} else {
		/* The previous characteristic discovery resulted in no characteristics.
		 * descriptor discovery should be performed.
		 */
		perform_desc_discov = true;
	}

	if (perform_desc_discov) {
		bool raise_discov_complete;

		db_discovery->curr_char_ind = 0;

		err_code = descriptors_discover(db_discovery, &raise_discov_complete,
						ble_gattc_evt->conn_handle);

		if (err_code != NRF_SUCCESS) {
			discovery_error_handler(ble_gattc_evt->conn_handle, err_code, db_discovery);

			return;
		}
		if (raise_discov_complete) {
			/* No more characteristics and descriptors need to be discovered. Discovery
			 * is complete. Send a discovery complete event to the user application.
			 */
			LOG_DBG("Discovery of service with UUID 0x%x completed with success"
				" on connection handle 0x%x.",
				srv_being_discovered->srv_uuid.uuid, ble_gattc_evt->conn_handle);

			discovery_complete_evt_trigger(db_discovery, true,
						       ble_gattc_evt->conn_handle);
			on_srv_disc_completion(db_discovery, ble_gattc_evt->conn_handle);
		}
	}
}

static void on_descriptor_discovery_rsp(struct ble_db_discovery *const db_discovery,
					const ble_gattc_evt_t *const ble_gattc_evt)
{
	const ble_gattc_evt_desc_disc_rsp_t *desc_disc_rsp_evt;
	struct ble_gatt_db_srv *srv_being_discovered;

	if (ble_gattc_evt->conn_handle != db_discovery->conn_handle) {
		return;
	}

	srv_being_discovered = &(db_discovery->services[db_discovery->curr_srv_ind]);

	desc_disc_rsp_evt = &(ble_gattc_evt->params.desc_disc_rsp);

	struct ble_gatt_db_char *char_being_discovered =
		&(srv_being_discovered->charateristics[db_discovery->curr_char_ind]);

	if (ble_gattc_evt->gatt_status == BLE_GATT_STATUS_SUCCESS) {
		/* The descriptor was found at the peer.
		 * Iterate through and collect CCCD, Extended Properties,
		 * User Description & Report Reference descriptor handles.
		 */
		for (uint32_t i = 0; i < desc_disc_rsp_evt->count; i++) {
			switch (desc_disc_rsp_evt->descs[i].uuid.uuid) {
			case BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG:
				char_being_discovered->cccd_handle =
					desc_disc_rsp_evt->descs[i].handle;
				break;

			case BLE_UUID_DESCRIPTOR_CHAR_EXT_PROP:
				char_being_discovered->ext_prop_handle =
					desc_disc_rsp_evt->descs[i].handle;
				break;

			case BLE_UUID_DESCRIPTOR_CHAR_USER_DESC:
				char_being_discovered->user_desc_handle =
					desc_disc_rsp_evt->descs[i].handle;
				break;

			case BLE_UUID_REPORT_REF_DESCR:
				char_being_discovered->report_ref_handle =
					desc_disc_rsp_evt->descs[i].handle;
				break;
			}

			/* Break if we've found all the descriptors we are looking for. */
			if (char_being_discovered->cccd_handle != BLE_GATT_HANDLE_INVALID &&
			    char_being_discovered->ext_prop_handle != BLE_GATT_HANDLE_INVALID &&
			    char_being_discovered->user_desc_handle != BLE_GATT_HANDLE_INVALID &&
			    char_being_discovered->report_ref_handle != BLE_GATT_HANDLE_INVALID) {
				break;
			}
		}
	}

	bool raise_discov_complete = false;

	if ((db_discovery->curr_char_ind + 1) == srv_being_discovered->char_count) {
		/* No more characteristics and descriptors need to be discovered. Discovery is
		 * complete. Send a discovery complete event to the user application.
		 */
		raise_discov_complete = true;
	} else {
		/* Begin discovery of descriptors for the next characteristic.*/
		uint32_t err_code;

		db_discovery->curr_char_ind++;

		err_code = descriptors_discover(db_discovery, &raise_discov_complete,
						ble_gattc_evt->conn_handle);

		if (err_code != NRF_SUCCESS) {
			discovery_error_handler(ble_gattc_evt->conn_handle, err_code, db_discovery);

			return;
		}
	}

	if (raise_discov_complete) {
		LOG_DBG("Discovery of service with UUID 0x%x completed with success"
			" on connection handle 0x%x.",
			srv_being_discovered->srv_uuid.uuid, ble_gattc_evt->conn_handle);

		discovery_complete_evt_trigger(db_discovery, true, ble_gattc_evt->conn_handle);
		on_srv_disc_completion(db_discovery, ble_gattc_evt->conn_handle);
	}
}

static uint32_t discovery_start(struct ble_db_discovery *const db_discovery, uint16_t conn_handle)
{
	int err_code;
	struct ble_gatt_db_srv *srv_being_discovered;
	struct ble_gq_req db_srv_disc_req = {0};

	memset(db_discovery, 0x00, sizeof(struct ble_db_discovery));

	err_code = ble_gq_conn_handle_register(gatt_queue, conn_handle);
	if (err_code != NRF_SUCCESS) {
		return err_code;
	}

	db_discovery->conn_handle = conn_handle;

	srv_being_discovered = &(db_discovery->services[db_discovery->curr_srv_ind]);
	srv_being_discovered->srv_uuid = registered_handlers[db_discovery->curr_srv_ind];

	LOG_DBG("Starting discovery of service with UUID 0x%x on connection handle 0x%x.",
		srv_being_discovered->srv_uuid.uuid, conn_handle);

	db_srv_disc_req.type = BLE_GQ_REQ_SRV_DISCOVERY;
	db_srv_disc_req.gattc_srv_disc.start_handle = CONFIG_SRV_DISC_START_HANDLE;
	db_srv_disc_req.gattc_srv_disc.srvc_uuid = srv_being_discovered->srv_uuid;
	db_srv_disc_req.error_handler.ctx = db_discovery;
	db_srv_disc_req.error_handler.cb = discovery_error_handler;

	err_code = ble_gq_item_add(gatt_queue, &db_srv_disc_req, conn_handle);

	if (err_code == NRF_SUCCESS) {
		db_discovery->discovery_in_progress = true;
	}

	return err_code;
}

uint32_t ble_db_discovery_init(struct ble_db_discovery *dbd,
			       struct ble_db_discovery_config *db_config)
{
	if (!db_config || !(db_config->evt_handler) || !(db_config->gatt_queue)) {
		return NRF_ERROR_NULL;
	}

	if (!dbd) {
		return NRF_ERROR_NULL;
	}

	if (!gatt_queue) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (num_of_handlers_reg == 0) {
		/* No user modules were registered. There are no services to discover. */
		return NRF_ERROR_INVALID_STATE;
	}

	if (dbd->discovery_in_progress) {
		return NRF_ERROR_BUSY;
	}

	num_of_handlers_reg = 0;
	evt_handler = db_config->evt_handler;
	gatt_queue = db_config->gatt_queue;

	return discovery_start(dbd, db_config->conn_handle);
}

uint32_t ble_db_discovery_evt_register(ble_uuid_t const *uuid)
{
	if (!uuid) {
		return NRF_ERROR_NULL;
	}
	if (!gatt_queue) {
		return NRF_ERROR_INVALID_STATE;
	}

	return registered_handler_set(uuid, evt_handler);
}

static void on_disconnected(struct ble_db_discovery *db_discovery, ble_gap_evt_t const *evt)
{
	if (evt->conn_handle == db_discovery->conn_handle) {
		db_discovery->discovery_in_progress = false;
		db_discovery->conn_handle = BLE_CONN_HANDLE_INVALID;
	}
}

void ble_db_discovery_on_ble_evt(ble_evt_t const *ble_evt, void *context)
{
	if (!ble_evt || !context || !gatt_queue) {
		return;
	}

	struct ble_db_discovery *db_discovery = (struct ble_db_discovery *)context;

	switch (ble_evt->header.evt_id) {
	case BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP:
		on_primary_srv_discovery_rsp(db_discovery, &(ble_evt->evt.gattc_evt));
		break;

	case BLE_GATTC_EVT_CHAR_DISC_RSP:
		on_characteristic_discovery_rsp(db_discovery, &(ble_evt->evt.gattc_evt));
		break;

	case BLE_GATTC_EVT_DESC_DISC_RSP:
		on_descriptor_discovery_rsp(db_discovery, &(ble_evt->evt.gattc_evt));
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnected(db_discovery, &(ble_evt->evt.gap_evt));
		break;

	default:
		break;
	}
}
