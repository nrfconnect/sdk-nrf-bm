/*
 * Copyright (c) 2013 - 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <stdint.h>
#include <ble_gap.h>
#include <ble_gattc.h>
#include <ble_types.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/ble_gq.h>
#include <bm/bluetooth/services/uuid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_db_disc, CONFIG_BLE_DB_DISCOVERY_LOG_LEVEL);

static inline struct ble_gatt_db_srv *curr_discovered_srv_get(struct ble_db_discovery *db_discovery)
{
	return &(db_discovery->services[db_discovery->curr_srv_idx]);
}

static bool is_uuid_registered(struct ble_db_discovery *db_discovery, const ble_uuid_t *uuid)
{
	for (uint32_t i = 0; i < db_discovery->num_registered_uuids; i++) {
		if (BLE_UUID_EQ(&(db_discovery->registered_uuids[i]), uuid)) {
			return true;
		}
	}

	return false;
}

static uint32_t uuid_register(struct ble_db_discovery *db_discovery, const ble_uuid_t *srv_uuid)
{
	if (is_uuid_registered(db_discovery, srv_uuid)) {
		return NRF_SUCCESS;
	}

	if (db_discovery->num_registered_uuids < CONFIG_BLE_DB_DISCOVERY_MAX_SRV) {
		db_discovery->registered_uuids[db_discovery->num_registered_uuids] = *srv_uuid;
		db_discovery->num_registered_uuids++;

		return NRF_SUCCESS;
	} else {
		return NRF_ERROR_NO_MEM;
	}
}

static void pending_user_events_send(struct ble_db_discovery *db_discovery)
{
	for (uint32_t i = 0; i < db_discovery->pending_usr_evt_idx; i++) {
		db_discovery->evt_handler(db_discovery, &(db_discovery->pending_usr_evts[i]));
	}

	db_discovery->pending_usr_evt_idx = 0;
}

static void discovery_available_evt_trigger(struct ble_db_discovery *const db_discovery,
					    const uint16_t conn_handle)
{
	struct ble_db_discovery_evt evt = {
		.evt_type = BLE_DB_DISCOVERY_AVAILABLE,
		.conn_handle = conn_handle,
	};

	if (db_discovery->evt_handler) {
		db_discovery->evt_handler(db_discovery, &evt);
	}
}

static void discovery_error_evt_trigger(struct ble_db_discovery *db_discovery, uint32_t nrf_err,
					uint16_t conn_handle)
{
	struct ble_gatt_db_srv *const srv_being_discovered = curr_discovered_srv_get(db_discovery);

	if (is_uuid_registered(db_discovery, &(srv_being_discovered->srv_uuid))) {
		struct ble_db_discovery_evt evt = {
			.evt_type = BLE_DB_DISCOVERY_ERROR,
			.conn_handle = conn_handle,
			.params.error.reason = nrf_err,
		};

		db_discovery->evt_handler(db_discovery, &evt);
	}
}

static void discovery_error_handler(uint16_t conn_handle, uint32_t nrf_err, void *ctx)
{
	struct ble_db_discovery *db_discovery = (struct ble_db_discovery *)ctx;

	db_discovery->discovery_in_progress = false;

	discovery_error_evt_trigger(db_discovery, nrf_err, conn_handle);
	discovery_available_evt_trigger(db_discovery, conn_handle);
}

static void discovery_gq_event_handler(const struct ble_gq_req *req, struct ble_gq_evt *evt)
{
	if (evt->evt_type != BLE_GQ_EVT_ERROR) {
		return;
	}

	discovery_error_handler(evt->conn_handle, evt->error.reason, req->ctx);
}

static void discovery_complete_evt_trigger(struct ble_db_discovery *db_discovery, bool is_srv_found,
					   uint16_t conn_handle)
{
	struct ble_gatt_db_srv *const srv_being_discovered = curr_discovered_srv_get(db_discovery);

	if (is_uuid_registered(db_discovery, &(srv_being_discovered->srv_uuid))) {
		if (db_discovery->pending_usr_evt_idx < CONFIG_BLE_DB_DISCOVERY_MAX_SRV) {
			/* Insert an event into the pending event list. */
			db_discovery->pending_usr_evts[db_discovery->pending_usr_evt_idx]
				.conn_handle = conn_handle;
			db_discovery->pending_usr_evts[db_discovery->pending_usr_evt_idx]
				.params.discovered_db = *srv_being_discovered;

			if (is_srv_found) {
				db_discovery->pending_usr_evts[db_discovery->pending_usr_evt_idx]
					.evt_type = BLE_DB_DISCOVERY_COMPLETE;
			} else {
				db_discovery->pending_usr_evts[db_discovery->pending_usr_evt_idx]
					.evt_type = BLE_DB_DISCOVERY_SRV_NOT_FOUND;
			}

			db_discovery->pending_usr_evt_idx++;

			if (db_discovery->pending_usr_evt_idx ==
			    db_discovery->num_registered_uuids) {
				/* All registered modules have pending events. Send all pending
				 * events to the user modules.
				 */
				pending_user_events_send(db_discovery);
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
	uint32_t nrf_err;
	struct ble_gatt_db_srv *srv_being_discovered;

	db_discovery->discoveries_count++;

	/* Check if more services need to be discovered. */
	if (db_discovery->discoveries_count < db_discovery->num_registered_uuids) {
		/* Reset the current characteristic index since a new service discovery is about to
		 * start.
		 */
		db_discovery->curr_char_idx = 0;

		/* Initiate discovery of the next service. */
		db_discovery->curr_srv_idx++;

		srv_being_discovered = curr_discovered_srv_get(db_discovery);

		srv_being_discovered->srv_uuid =
			db_discovery->registered_uuids[db_discovery->curr_srv_idx];

		/* Reset the characteristic count in the current service to zero since a new
		 * service discovery is about to start.
		 */
		srv_being_discovered->char_count = 0;

		LOG_DBG("Starting discovery of service with UUID %#x on connection handle %#x",
			srv_being_discovered->srv_uuid.uuid, conn_handle);

		struct ble_gq_req req = {
			.type = BLE_GQ_REQ_SRV_DISCOVERY,
			.evt_handler = discovery_gq_event_handler,
			.ctx = db_discovery,
			.gattc_srv_disc = {
				.srvc_uuid = srv_being_discovered->srv_uuid,
				.start_handle = CONFIG_BLE_DB_DISCOVERY_SRV_DISC_START_HANDLE,
			},
		};

		nrf_err = ble_gq_item_add(db_discovery->gatt_queue, &req, conn_handle);
		if (nrf_err) {
			discovery_error_handler(conn_handle, nrf_err, db_discovery);

			return;
		}
	} else {
		/* No more service discovery is needed. */
		db_discovery->discovery_in_progress = false;

		discovery_available_evt_trigger(db_discovery, conn_handle);
	}
}

static bool is_char_discovery_required(struct ble_db_discovery *db_discovery,
				       ble_gattc_char_t *after_char)
{
	if (after_char->handle_value <
	    db_discovery->services[db_discovery->curr_srv_idx].handle_range.end_handle) {
		/* Handle value of the characteristic being discovered is less than the end handle
		 * of the service being discovered. There is a possibility of more characteristics
		 * being present. Hence a characteristic discovery is required.
		 */
		return true;
	}

	return false;
}

static bool is_desc_discovery_required(struct ble_db_discovery *db_discovery,
				       struct ble_gatt_db_char *curr_char,
				       struct ble_gatt_db_char *next_char,
				       ble_gattc_handle_range_t *handle_range)
{
	if (next_char == NULL) {
		/* Current characteristic is the last characteristic in the service. Check if the
		 * value handle of the current characteristic is equal to the service end handle.
		 */
		if (curr_char->characteristic.handle_value ==
		    db_discovery->services[db_discovery->curr_srv_idx].handle_range.end_handle) {
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
			db_discovery->services[db_discovery->curr_srv_idx].handle_range.end_handle;

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
	struct ble_gatt_db_srv *const srv_being_discovered = curr_discovered_srv_get(db_discovery);
	ble_gattc_handle_range_t handle_range = {0};

	if (db_discovery->curr_char_idx != 0) {
		/* This is not the first characteristic being discovered. Hence the 'start handle'
		 * to be used must be computed using the handle_value of the previous
		 * characteristic.
		 */
		ble_gattc_char_t *prev_char;
		uint8_t prev_char_ind = db_discovery->curr_char_idx - 1;

		prev_char = &(srv_being_discovered->characteristics[prev_char_ind].characteristic);

		handle_range.start_handle = prev_char->handle_value + 1;
	} else {
		/* This is the first characteristic of this service being discovered. */
		handle_range.start_handle = srv_being_discovered->handle_range.start_handle;
	}

	handle_range.end_handle = srv_being_discovered->handle_range.end_handle;

	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_CHAR_DISCOVERY,
		.evt_handler = discovery_gq_event_handler,
		.ctx = db_discovery,
		.gattc_char_disc = handle_range,
	};

	return ble_gq_item_add(db_discovery->gatt_queue, &req, conn_handle);
}

static uint32_t descriptors_discover(struct ble_db_discovery *db_discovery,
				     bool *raise_discovery_complete, uint16_t conn_handle)
{
	ble_gattc_handle_range_t handle_range;
	struct ble_gatt_db_char *curr_char_being_discovered;
	struct ble_gatt_db_srv *const srv_being_discovered = curr_discovered_srv_get(db_discovery);
	bool is_descriptor_discovery_required = false;

	curr_char_being_discovered =
		&(srv_being_discovered->characteristics[db_discovery->curr_char_idx]);

	if ((db_discovery->curr_char_idx + 1) == srv_being_discovered->char_count) {
		/* This is the last characteristic of this service. */
		is_descriptor_discovery_required = is_desc_discovery_required(
			db_discovery, curr_char_being_discovered, NULL, &handle_range);
	} else {
		uint8_t i;
		struct ble_gatt_db_char *next_char;

		for (i = db_discovery->curr_char_idx; i < srv_being_discovered->char_count; i++) {
			if (i == (srv_being_discovered->char_count - 1)) {
				/* The current characteristic is the last characteristic in the
				 * service.
				 */
				next_char = NULL;
			} else {
				next_char = &(srv_being_discovered->characteristics[i + 1]);
			}

			/* Check if it is possible for the current characteristic to have a
			 * descriptor.
			 */
			if (is_desc_discovery_required(db_discovery, curr_char_being_discovered,
						       next_char, &handle_range)) {
				is_descriptor_discovery_required = true;
				break;
			}
			/* No descriptors can exist. */
			curr_char_being_discovered = next_char;
			db_discovery->curr_char_idx++;
		}
	}

	if (!is_descriptor_discovery_required) {
		/* No more descriptor discovery required. Discovery is complete.
		 * This informs the caller that a discovery complete event can be triggered.
		 */
		*raise_discovery_complete = true;

		return NRF_SUCCESS;
	}

	*raise_discovery_complete = false;

	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_DESC_DISCOVERY,
		.evt_handler = discovery_gq_event_handler,
		.ctx = db_discovery,
		.gattc_desc_disc = handle_range,
	};

	return ble_gq_item_add(db_discovery->gatt_queue, &req, conn_handle);
}

static void on_primary_srv_discovery_rsp(struct ble_db_discovery *db_discovery,
					 const ble_gattc_evt_t *gattc_evt)
{
	struct ble_gatt_db_srv *const srv_being_discovered = curr_discovered_srv_get(db_discovery);

	if (gattc_evt->conn_handle != db_discovery->conn_handle) {
		return;
	}

	if (gattc_evt->gatt_status == BLE_GATT_STATUS_SUCCESS) {
		uint32_t nrf_err;
		const ble_gattc_evt_prim_srvc_disc_rsp_t *prim_srvc_disc_rsp_evt;

		LOG_DBG("Found service UUID %#x", srv_being_discovered->srv_uuid.uuid);

		prim_srvc_disc_rsp_evt = &(gattc_evt->params.prim_srvc_disc_rsp);

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

			LOG_WRN("Not enough space for services");
			LOG_WRN("Increase CONFIG_BLE_DB_DISCOVERY_MAX_SRV to be able to store more "
				"services!");
		}

		nrf_err = characteristics_discover(db_discovery, gattc_evt->conn_handle);

		if (nrf_err) {
			discovery_error_handler(gattc_evt->conn_handle, nrf_err, db_discovery);
		}
	} else {
		LOG_DBG("Service UUID %#x not found", srv_being_discovered->srv_uuid.uuid);
		/* Trigger Service Not Found event to the application. */
		discovery_complete_evt_trigger(db_discovery, false, gattc_evt->conn_handle);
		on_srv_disc_completion(db_discovery, gattc_evt->conn_handle);
	}
}

static void on_characteristic_discovery_rsp(struct ble_db_discovery *db_discovery,
					    const ble_gattc_evt_t *gattc_evt)
{
	uint32_t nrf_err;
	struct ble_gatt_db_srv *const srv_being_discovered = curr_discovered_srv_get(db_discovery);
	bool perform_descriptor_discovery = false;

	if (gattc_evt->conn_handle != db_discovery->conn_handle) {
		return;
	}

	if (gattc_evt->gatt_status == BLE_GATT_STATUS_SUCCESS) {
		const ble_gattc_evt_char_disc_rsp_t *char_disc_rsp_evt;

		char_disc_rsp_evt = &(gattc_evt->params.char_disc_rsp);

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
				"service %#x",
				srv_being_discovered->srv_uuid.uuid);
			LOG_WRN("Increase CONFIG_BLE_GATT_DB_MAX_CHARS to be able to store more "
				"characteristics for each service!");
		}

		uint32_t i;
		uint32_t j;

		for (i = num_chars_prev_disc, j = 0; i < srv_being_discovered->char_count;
		     i++, j++) {
			srv_being_discovered->characteristics[i].characteristic =
				char_disc_rsp_evt->chars[j];

			srv_being_discovered->characteristics[i].cccd_handle =
				BLE_GATT_HANDLE_INVALID;
			srv_being_discovered->characteristics[i].ext_prop_handle =
				BLE_GATT_HANDLE_INVALID;
			srv_being_discovered->characteristics[i].user_desc_handle =
				BLE_GATT_HANDLE_INVALID;
			srv_being_discovered->characteristics[i].report_ref_handle =
				BLE_GATT_HANDLE_INVALID;
		}

		ble_gattc_char_t *last_known_char;

		last_known_char = &(srv_being_discovered->characteristics[i - 1].characteristic);

		/* If no more characteristic discovery is required, or if the maximum number of
		 * supported characteristic per service has been reached, descriptor discovery will
		 * be performed.
		 */
		if (!is_char_discovery_required(db_discovery, last_known_char) ||
		    (srv_being_discovered->char_count == CONFIG_BLE_GATT_DB_MAX_CHARS)) {
			perform_descriptor_discovery = true;
		} else {
			/* Update the current characteristic index. */
			db_discovery->curr_char_idx = srv_being_discovered->char_count;

			/* Perform another round of characteristic discovery. */
			nrf_err = characteristics_discover(db_discovery, gattc_evt->conn_handle);
			if (nrf_err) {
				discovery_error_handler(gattc_evt->conn_handle, nrf_err,
							db_discovery);

				return;
			}
		}
	} else {
		/* The previous characteristic discovery resulted in no characteristics.
		 * descriptor discovery should be performed.
		 */
		perform_descriptor_discovery = true;
	}

	if (perform_descriptor_discovery) {
		bool raise_discovery_complete;

		db_discovery->curr_char_idx = 0;

		nrf_err = descriptors_discover(db_discovery, &raise_discovery_complete,
					       gattc_evt->conn_handle);

		if (nrf_err) {
			discovery_error_handler(gattc_evt->conn_handle, nrf_err, db_discovery);

			return;
		}
		if (raise_discovery_complete) {
			/* No more characteristics and descriptors need to be discovered. Discovery
			 * is complete. Send a discovery complete event to the user application.
			 */
			LOG_DBG("Discovery of service with UUID %#x completed with success"
				" on connection handle %#x",
				srv_being_discovered->srv_uuid.uuid, gattc_evt->conn_handle);

			discovery_complete_evt_trigger(db_discovery, true, gattc_evt->conn_handle);
			on_srv_disc_completion(db_discovery, gattc_evt->conn_handle);
		}
	}
}

static void on_descriptor_discovery_rsp(struct ble_db_discovery *const db_discovery,
					const ble_gattc_evt_t *const gattc_evt)
{
	const ble_gattc_evt_desc_disc_rsp_t *desc_disc_rsp_evt;
	struct ble_gatt_db_srv *const srv_being_discovered = curr_discovered_srv_get(db_discovery);

	if (gattc_evt->conn_handle != db_discovery->conn_handle) {
		return;
	}

	desc_disc_rsp_evt = &(gattc_evt->params.desc_disc_rsp);

	struct ble_gatt_db_char *char_being_discovered =
		&(srv_being_discovered->characteristics[db_discovery->curr_char_idx]);

	if (gattc_evt->gatt_status == BLE_GATT_STATUS_SUCCESS) {
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

	bool raise_discovery_complete = false;

	if ((db_discovery->curr_char_idx + 1) == srv_being_discovered->char_count) {
		/* No more characteristics and descriptors need to be discovered. Discovery is
		 * complete. Send a discovery complete event to the user application.
		 */
		raise_discovery_complete = true;
	} else {
		/* Begin discovery of descriptors for the next characteristic.*/
		uint32_t nrf_err;

		db_discovery->curr_char_idx++;

		nrf_err = descriptors_discover(db_discovery, &raise_discovery_complete,
					       gattc_evt->conn_handle);

		if (nrf_err) {
			discovery_error_handler(gattc_evt->conn_handle, nrf_err, db_discovery);

			return;
		}
	}

	if (raise_discovery_complete) {
		LOG_DBG("Discovery of service with UUID %#x completed with success"
			" on connection handle %#x",
			srv_being_discovered->srv_uuid.uuid, gattc_evt->conn_handle);

		discovery_complete_evt_trigger(db_discovery, true, gattc_evt->conn_handle);
		on_srv_disc_completion(db_discovery, gattc_evt->conn_handle);
	}
}

static uint32_t discovery_start(struct ble_db_discovery *const db_discovery, uint16_t conn_handle)
{
	int nrf_err;
	struct ble_gatt_db_srv *const srv_being_discovered = curr_discovered_srv_get(db_discovery);

	nrf_err = ble_gq_conn_handle_register(db_discovery->gatt_queue, conn_handle);
	if (nrf_err) {
		return nrf_err;
	}

	db_discovery->conn_handle = conn_handle;

	srv_being_discovered->srv_uuid = db_discovery->registered_uuids[db_discovery->curr_srv_idx];

	LOG_DBG("Starting discovery of service with UUID %#x on connection handle %#x",
		srv_being_discovered->srv_uuid.uuid, conn_handle);

	struct ble_gq_req req = {
		.type = BLE_GQ_REQ_SRV_DISCOVERY,
		.evt_handler = discovery_gq_event_handler,
		.ctx = db_discovery,
		.gattc_srv_disc = {
			.srvc_uuid = srv_being_discovered->srv_uuid,
			.start_handle = CONFIG_BLE_DB_DISCOVERY_SRV_DISC_START_HANDLE,
		},
	};

	nrf_err = ble_gq_item_add(db_discovery->gatt_queue, &req, conn_handle);
	if (nrf_err) {
		return nrf_err;
	}

	db_discovery->discovery_in_progress = true;

	return NRF_SUCCESS;
}

uint32_t ble_db_discovery_init(struct ble_db_discovery *db_discovery,
			       const struct ble_db_discovery_config *db_config)
{
	if (!db_discovery || !db_config || !db_config->evt_handler || !db_config->gatt_queue) {
		return NRF_ERROR_NULL;
	}

	db_discovery->num_registered_uuids = 0;
	db_discovery->evt_handler = db_config->evt_handler;
	db_discovery->gatt_queue = db_config->gatt_queue;

	return NRF_SUCCESS;
}

uint32_t ble_db_discovery_start(struct ble_db_discovery *db_discovery, uint16_t conn_handle)
{
	if (!db_discovery) {
		return NRF_ERROR_NULL;
	}
	if (!db_discovery->gatt_queue) {
		return NRF_ERROR_INVALID_STATE;
	}

	if (db_discovery->num_registered_uuids == 0) {
		/* No UUIDs have been registered. There are no services to discover. */
		return NRF_ERROR_INVALID_STATE;
	}

	if (db_discovery->discovery_in_progress) {
		return NRF_ERROR_BUSY;
	}

	return discovery_start(db_discovery, conn_handle);
}

uint32_t ble_db_discovery_service_register(struct ble_db_discovery *db_discovery,
					   const ble_uuid_t *uuid)
{
	if (!db_discovery || !uuid) {
		return NRF_ERROR_NULL;
	}
	if (!db_discovery->gatt_queue) {
		return NRF_ERROR_INVALID_STATE;
	}

	return uuid_register(db_discovery, uuid);
}

static void on_disconnected(struct ble_db_discovery *db_discovery, const ble_gap_evt_t *evt)
{
	if (evt->conn_handle == db_discovery->conn_handle) {
		db_discovery->discovery_in_progress = false;
		db_discovery->conn_handle = BLE_CONN_HANDLE_INVALID;
	}
}

void ble_db_discovery_on_ble_evt(const ble_evt_t *ble_evt, void *context)
{
	struct ble_db_discovery *const db_discovery = (struct ble_db_discovery *)context;

	if (!ble_evt || !db_discovery || !db_discovery->gatt_queue) {
		return;
	}

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
