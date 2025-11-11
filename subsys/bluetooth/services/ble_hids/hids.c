/*
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <ble_gap.h>
#include <ble_types.h>
#include <ble_gatts.h>
#include <bm/bluetooth/services/ble_hids.h>
#include <bm/bluetooth/services/uuid.h>
#include <bm/bluetooth/services/common.h>
#include <zephyr/toolchain.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/__assert.h>

/** @brief The number of bytes in a word. */
#define BYTES_PER_WORD (4)

LOG_MODULE_REGISTER(ble_hids, CONFIG_BLE_HIDS_LOG_LEVEL);

/* Boot Protocol Mode. */
#define PROTOCOL_MODE_BOOT 0x00
/* Report Protocol Mode. */
#define PROTOCOL_MODE_REPORT 0x01

/* HID Control Point values */
/* Suspend command. */
#define HIDS_CONTROL_POINT_SUSPEND 0x00
/* Exit Suspend command. */
#define HIDS_CONTROL_POINT_EXIT_SUSPEND 0x01

/* Maximum size of an encoded HID Information characteristic. */
#define ENCODED_HID_INFORMATION_LEN 4

/** Minimum size of a Boot Mouse Input Report (as per Appendix B in Device Class
 * Definition for Human Interface Devices (HID), Version 1.11).
 */
#define BOOT_MOUSE_INPUT_REPORT_MIN_SIZE 3

static uint32_t link_ctx_get(struct ble_hids *hids, uint16_t const conn_handle,
			     void const **ctx_data)
{
	int conn_id;

	__ASSERT_NO_MSG(hids && ctx_data);

	*ctx_data = NULL;

	if ((hids->link_ctx_storage.link_ctx_size % BYTES_PER_WORD) != 0) {
		return NRF_ERROR_INVALID_PARAM;
	}

	conn_id = nrf_sdh_ble_idx_get(conn_handle);
	if (conn_id == -1) {
		return NRF_ERROR_NOT_FOUND;
	}

	if (conn_id >= hids->link_ctx_storage.max_links_cnt) {
		return NRF_ERROR_NO_MEM;
	}

	*ctx_data = (void *)((uint8_t *)(&hids->link_ctx_storage.ctx_data_pool) +
			     (conn_id * hids->link_ctx_storage.link_ctx_size));
	return NRF_SUCCESS;
}

static size_t ble_hids_client_context_size_calc(struct ble_hids *hids)
{
	size_t client_size = sizeof(struct ble_hids_client_context) +
			     (BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE) +
			     (BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE) +
			     (BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE);

	if (hids->inp_rep_init_array != NULL) {
		for (uint32_t i = 0; i < hids->input_report_count; i++) {
			client_size += hids->inp_rep_init_array[i].len;
		}
	}

	if (hids->outp_rep_init_array != NULL) {
		for (uint32_t i = 0; i < hids->output_report_count; i++) {
			client_size += hids->outp_rep_init_array[i].len;
		}
	}

	if (hids->feature_rep_init_array != NULL) {
		for (uint32_t i = 0; i < hids->feature_report_count; i++) {
			client_size += hids->feature_rep_init_array[i].len;
		}
	}

	return client_size;
}

static struct ble_hids_char_id make_char_id(uint16_t uuid, uint8_t report_type,
					    uint8_t report_index)
{
	struct ble_hids_char_id char_id = {
		.uuid = uuid,
		.report_type = report_type,
		.report_index = report_index,
	};

	return char_id;
}

static void on_connect(struct ble_hids *hids, ble_evt_t const *ble_evt)
{
	uint32_t nrf_err;
	ble_gatts_value_t gatts_value;
	struct ble_hids_client_context *client = NULL;

	nrf_err = link_ctx_get(hids, ble_evt->evt.gap_evt.conn_handle, (void *)&client);
	if (nrf_err) {
		struct ble_hids_evt evt = {
			.evt_type = BLE_HIDS_EVT_ERROR,
			.params.error.reason = nrf_err,
			.conn_handle = ble_evt->evt.gap_evt.conn_handle,
		};

		LOG_ERR("Failed to get link context, nrf_error %#x", nrf_err);
		hids->evt_handler(hids, &evt);
	}

	if (client == NULL) {
		return;
	}

	memset(client, 0, ble_hids_client_context_size_calc(hids));

	if (hids->protocol_mode_handles.value_handle) {
		/* Set Protocol Mode characteristic value to default value */
		client->protocol_mode = CONFIG_BLE_HIDS_DEFAULT_PROTOCOL_MODE;

		/* Initialize value struct. */
		memset(&gatts_value, 0, sizeof(gatts_value));

		gatts_value.len = sizeof(uint8_t);
		gatts_value.offset = 0;
		gatts_value.p_value = &client->protocol_mode;

		nrf_err = sd_ble_gatts_value_set(ble_evt->evt.gap_evt.conn_handle,
						 hids->protocol_mode_handles.value_handle,
						 &gatts_value);
		if (nrf_err) {
			struct ble_hids_evt evt = {
				.evt_type = BLE_HIDS_EVT_ERROR,
				.params.error.reason = nrf_err,
				.conn_handle = ble_evt->evt.gap_evt.conn_handle,
			};

			LOG_ERR("Failed to set GATTS value, nrf_error %#x", nrf_err);
			hids->evt_handler(hids, &evt);
		}
	}
}

static void on_control_point_write(struct ble_hids *hids, ble_evt_t const *ble_evt)
{
	uint32_t nrf_err;
	struct ble_hids_client_context *host;
	ble_gatts_evt_write_t const *evt_write = &ble_evt->evt.gatts_evt.params.write;

	nrf_err = link_ctx_get(hids, ble_evt->evt.gatts_evt.conn_handle, (void *)&host);
	if (nrf_err) {
		struct ble_hids_evt evt = {
			.evt_type = BLE_HIDS_EVT_ERROR,
			.params.error.reason = nrf_err,
			.conn_handle = ble_evt->evt.gatts_evt.conn_handle,
		};

		LOG_ERR("Failed to get link context, nrf_error %#x", nrf_err);
		hids->evt_handler(hids, &evt);
	}

	if (host == NULL) {
		return;
	}

	if (evt_write->len == 1) {
		struct ble_hids_evt evt = {
			.conn_handle = ble_evt->evt.gatts_evt.conn_handle,
		};

		switch (evt_write->data[0]) {
		case HIDS_CONTROL_POINT_SUSPEND:
			evt.evt_type = BLE_HIDS_EVT_HOST_SUSP;
			break;

		case HIDS_CONTROL_POINT_EXIT_SUSPEND:
			evt.evt_type = BLE_HIDS_EVT_HOST_EXIT_SUSP;
			break;

		default:
			/* Illegal Control Point value, ignore */
			return;
		}

		/* Store the new Control Point value for the host */
		host->ctrl_pt = evt_write->data[0];

		/* HID Control Point written, propagate event to application */
		if (hids->evt_handler != NULL) {
			evt.ble_evt = ble_evt;
			hids->evt_handler(hids, &evt);
		}
	}
}

static void on_protocol_mode_write(struct ble_hids *hids, ble_evt_t const *ble_evt)
{
	uint32_t nrf_err;
	struct ble_hids_client_context *host;
	ble_gatts_evt_write_t const *evt_write = &ble_evt->evt.gatts_evt.params.write;

	nrf_err = link_ctx_get(hids, ble_evt->evt.gatts_evt.conn_handle, (void *)&host);
	if (nrf_err) {
		struct ble_hids_evt evt = {
			.evt_type = BLE_HIDS_EVT_ERROR,
			.params.error.reason = nrf_err,
			.conn_handle = ble_evt->evt.gatts_evt.conn_handle,
		};

		LOG_ERR("Failed to get link context, nrf_error %#x", nrf_err);
		hids->evt_handler(hids, &evt);
	}

	if (host == NULL) {
		return;
	}

	if (evt_write->len == 1) {
		struct ble_hids_evt evt = {
			.conn_handle = ble_evt->evt.gatts_evt.conn_handle,
		};

		switch (evt_write->data[0]) {
		case PROTOCOL_MODE_BOOT:
			evt.evt_type = BLE_HIDS_EVT_BOOT_MODE_ENTERED;
			break;

		case PROTOCOL_MODE_REPORT:
			evt.evt_type = BLE_HIDS_EVT_REPORT_MODE_ENTERED;
			break;

		default:
			/* Illegal Protocol Mode value, ignore */
			return;
		}

		/* Store Protocol Mode of the host */
		host->protocol_mode = evt_write->data[0];

		/* HID Protocol Mode written, propagate event to application */
		if (hids->evt_handler != NULL) {
			evt.ble_evt = ble_evt;
			hids->evt_handler(hids, &evt);
		}
	}
}

void on_protocol_mode_read_auth(struct ble_hids *hids, ble_evt_t const *ble_evt)
{
	uint32_t nrf_err;
	ble_gatts_rw_authorize_reply_params_t auth_read_params;
	struct ble_hids_client_context *host;
	ble_gatts_evt_rw_authorize_request_t const *read_auth =
		&ble_evt->evt.gatts_evt.params.authorize_request;

	nrf_err = link_ctx_get(hids, ble_evt->evt.gatts_evt.conn_handle, (void *)&host);
	if (nrf_err) {
		struct ble_hids_evt evt = {
			.evt_type = BLE_HIDS_EVT_ERROR,
			.params.error.reason = nrf_err,
			.conn_handle = ble_evt->evt.gatts_evt.conn_handle,
		};

		LOG_ERR("Failed to get link context, nrf_error %#x", nrf_err);
		hids->evt_handler(hids, &evt);
	}

	/* Update GATTS table with this host's Protocol Mode value and authorize Read */
	if (host != NULL) {
		memset(&auth_read_params, 0, sizeof(auth_read_params));

		auth_read_params.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
		auth_read_params.params.read.gatt_status = BLE_GATT_STATUS_SUCCESS;
		auth_read_params.params.read.offset = read_auth->request.read.offset;
		auth_read_params.params.read.len = sizeof(uint8_t);
		auth_read_params.params.read.p_data = &host->protocol_mode;
		auth_read_params.params.read.update = 1;

		nrf_err = sd_ble_gatts_rw_authorize_reply(ble_evt->evt.gap_evt.conn_handle,
							  &auth_read_params);
		if (nrf_err) {
			struct ble_hids_evt evt = {
				.evt_type = BLE_HIDS_EVT_ERROR,
				.params.error.reason = nrf_err,
				.conn_handle = ble_evt->evt.gatts_evt.conn_handle,
			};

			LOG_ERR("Failed to authorize rw request, nrf_error %#x", nrf_err);
			hids->evt_handler(hids, &evt);
		}
	}
}

static void on_report_cccd_write(struct ble_hids *hids, struct ble_hids_char_id *char_id,
				 ble_evt_t const *ble_evt)
{
	ble_gatts_evt_write_t const *evt_write = &ble_evt->evt.gatts_evt.params.write;

	if (evt_write->len == 2) {
		/* CCCD written, update notification state */
		if (hids->evt_handler != NULL) {
			struct ble_hids_evt evt = {
				.conn_handle = ble_evt->evt.gatts_evt.conn_handle,
			};

			if (is_notification_enabled(evt_write->data)) {
				evt.evt_type = BLE_HIDS_EVT_NOTIF_ENABLED;
			} else {
				evt.evt_type = BLE_HIDS_EVT_NOTIF_DISABLED;
			}
			evt.params.notification.char_id = *char_id;
			evt.ble_evt = ble_evt;

			hids->evt_handler(hids, &evt);
		}
	}
}

static void on_report_value_write(struct ble_hids *hids, ble_evt_t const *ble_evt,
				  struct ble_hids_char_id *char_id, uint16_t rep_offset,
				  uint16_t rep_max_len)
{
	/* Update host's Output Report data */
	uint32_t nrf_err;
	uint8_t *report;
	struct ble_hids_client_context *host;
	ble_gatts_evt_write_t const *write = &ble_evt->evt.gatts_evt.params.write;

	nrf_err = link_ctx_get(hids, ble_evt->evt.gatts_evt.conn_handle, (void *)&host);
	if (nrf_err) {
		struct ble_hids_evt evt = {
			.evt_type = BLE_HIDS_EVT_ERROR,
			.params.error.reason = nrf_err,
			.conn_handle = ble_evt->evt.gatts_evt.conn_handle,
		};

		LOG_ERR("Failed to get link context, nrf_error %#x", nrf_err);
		hids->evt_handler(hids, &evt);
	}

	/* Store the written values in host's report data */
	if ((host != NULL) && (write->len + write->offset <= rep_max_len)) {
		report = (uint8_t *)host + rep_offset;
		memcpy(report, write->data, write->len);
	} else {
		return;
	}

	/* Notify the applicartion */
	if (hids->evt_handler != NULL) {
		struct ble_hids_evt evt = {
			.conn_handle = ble_evt->evt.gatts_evt.conn_handle,
		};

		evt.evt_type = BLE_HIDS_EVT_REP_CHAR_WRITE;
		evt.params.char_write.char_id = *char_id;
		evt.params.char_write.offset = ble_evt->evt.gatts_evt.params.write.offset;
		evt.params.char_write.len = ble_evt->evt.gatts_evt.params.write.len;
		evt.params.char_write.data = ble_evt->evt.gatts_evt.params.write.data;
		evt.ble_evt = ble_evt;

		hids->evt_handler(hids, &evt);
	}
}

static void on_report_value_read_auth(struct ble_hids *hids, struct ble_hids_char_id *char_id,
				      ble_evt_t const *ble_evt, uint16_t rep_offset,
				      uint16_t rep_max_len)
{
	ble_gatts_rw_authorize_reply_params_t auth_read_params;
	struct ble_hids_client_context *host;
	uint8_t *report;
	uint16_t read_offset;
	uint32_t nrf_err = NRF_SUCCESS;

	nrf_err = link_ctx_get(hids, ble_evt->evt.gatts_evt.conn_handle, (void *)&host);
	if (nrf_err) {
		struct ble_hids_evt evt = {
			.evt_type = BLE_HIDS_EVT_ERROR,
			.params.error.reason = nrf_err,
			.conn_handle = ble_evt->evt.gatts_evt.conn_handle,
		};

		LOG_ERR("Failed to get link context, nrf_error %#x", nrf_err);
		hids->evt_handler(hids, &evt);
	}

	/* Update Report GATTS table with host's current report data */
	if (host != NULL) {
		read_offset = ble_evt->evt.gatts_evt.params.authorize_request.request.read.offset;
		report = (uint8_t *)host + rep_offset;
		memset(&auth_read_params, 0, sizeof(auth_read_params));

		auth_read_params.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
		auth_read_params.params.read.gatt_status = BLE_GATT_STATUS_SUCCESS;
		auth_read_params.params.read.offset = read_offset;
		auth_read_params.params.read.len = rep_max_len - read_offset;
		auth_read_params.params.read.p_data = report + read_offset;
		auth_read_params.params.read.update = 1;

		nrf_err = sd_ble_gatts_rw_authorize_reply(ble_evt->evt.gap_evt.conn_handle,
							  &auth_read_params);

		if (nrf_err) {
			struct ble_hids_evt evt = {
				.evt_type = BLE_HIDS_EVT_ERROR,
				.params.error.reason = nrf_err,
				.conn_handle = ble_evt->evt.gap_evt.conn_handle,
			};

			LOG_ERR("Failed to authorize rw request, nrf_error %#x", nrf_err);
			hids->evt_handler(hids, &evt);
		}
	} else {
		return;
	}

	if (hids->evt_handler != NULL) {
		struct ble_hids_evt evt = {
			.conn_handle = ble_evt->evt.gap_evt.conn_handle,
		};

		evt.evt_type = BLE_HIDS_EVT_REPORT_READ;
		evt.params.char_auth_read.char_id = *char_id;
		evt.ble_evt = ble_evt;

		hids->evt_handler(hids, &evt);
	}
}

static bool inp_rep_cccd_identify(struct ble_hids *hids, uint16_t handle,
				  struct ble_hids_char_id *char_id)
{
	uint8_t i;

	for (i = 0; i < hids->input_report_count; i++) {
		if (handle == hids->inp_rep_array[i].char_handles.cccd_handle) {
			*char_id =
				make_char_id(BLE_UUID_REPORT_CHAR, BLE_HIDS_REPORT_TYPE_INPUT, i);
			return true;
		}
	}

	return false;
}

static bool rep_value_identify(struct ble_hids *hids, uint16_t handle,
			       struct ble_hids_char_id *char_id, uint16_t *context_offset,
			       uint16_t *rep_max_len)
{
	uint8_t i;

	/* Initialize the offset with size of non-report-related data */
	*context_offset = sizeof(struct ble_hids_client_context) +
			  BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE +
			  BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE +
			  BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE;

	if (hids->inp_rep_init_array != NULL) {
		for (i = 0; i < hids->input_report_count; i++) {
			if (handle == hids->inp_rep_array[i].char_handles.value_handle) {
				*char_id = make_char_id(BLE_UUID_REPORT_CHAR,
							BLE_HIDS_REPORT_TYPE_INPUT, i);
				*rep_max_len = hids->inp_rep_init_array[i].len;
				return true;
			}
			*context_offset += hids->inp_rep_init_array[i].len;
		}
	}
	if (hids->outp_rep_init_array != NULL) {
		for (i = 0; i < hids->output_report_count; i++) {
			if (handle == hids->outp_rep_array[i].char_handles.value_handle) {
				*char_id = make_char_id(BLE_UUID_REPORT_CHAR,
							BLE_HIDS_REPORT_TYPE_OUTPUT, i);
				*rep_max_len = hids->outp_rep_init_array[i].len;
				return true;
			}
			*context_offset += hids->outp_rep_init_array[i].len;
		}
	}
	if (hids->feature_rep_init_array != NULL) {
		for (i = 0; i < hids->feature_report_count; i++) {
			if (handle == hids->feature_rep_array[i].char_handles.value_handle) {
				*char_id = make_char_id(BLE_UUID_REPORT_CHAR,
							BLE_HIDS_REPORT_TYPE_FEATURE, i);
				*rep_max_len = hids->feature_rep_init_array[i].len;
				return true;
			}
			*context_offset += hids->feature_rep_init_array[i].len;
		}
	}

	return false;
}

static void on_write(struct ble_hids *hids, ble_evt_t const *ble_evt)
{
	struct ble_hids_char_id char_id;
	ble_gatts_evt_write_t const *evt_write = &ble_evt->evt.gatts_evt.params.write;
	uint16_t rep_data_offset = sizeof(struct ble_hids_client_context);
	uint16_t max_rep_len = 0;

	if (evt_write->handle == hids->hid_control_point_handles.value_handle) {
		on_control_point_write(hids, ble_evt);
	} else if (evt_write->handle == hids->protocol_mode_handles.value_handle) {
		on_protocol_mode_write(hids, ble_evt);
	} else if (evt_write->handle == hids->boot_kb_inp_rep_handles.cccd_handle) {
		char_id = make_char_id(BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT_CHAR, 0, 0);
		on_report_cccd_write(hids, &char_id, ble_evt);
	} else if (evt_write->handle == hids->boot_kb_inp_rep_handles.value_handle) {
		char_id = make_char_id(BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT_CHAR, 0, 0);
		max_rep_len = BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE;
		on_report_value_write(hids, ble_evt, &char_id, rep_data_offset, max_rep_len);
	} else if (evt_write->handle == hids->boot_kb_outp_rep_handles.value_handle) {
		char_id = make_char_id(BLE_UUID_BOOT_KEYBOARD_OUTPUT_REPORT_CHAR, 0, 0);
		rep_data_offset += BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE;
		max_rep_len = BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE;
		on_report_value_write(hids, ble_evt, &char_id, rep_data_offset, max_rep_len);
	} else if (evt_write->handle == hids->boot_mouse_inp_rep_handles.cccd_handle) {
		char_id = make_char_id(BLE_UUID_BOOT_MOUSE_INPUT_REPORT_CHAR, 0, 0);
		on_report_cccd_write(hids, &char_id, ble_evt);
	} else if (evt_write->handle == hids->boot_mouse_inp_rep_handles.value_handle) {
		char_id = make_char_id(BLE_UUID_BOOT_MOUSE_INPUT_REPORT_CHAR, 0, 0);
		rep_data_offset += BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE +
				   BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE;
		max_rep_len = BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE;
		on_report_value_write(hids, ble_evt, &char_id, rep_data_offset, max_rep_len);
	} else if (inp_rep_cccd_identify(hids, evt_write->handle, &char_id)) {
		on_report_cccd_write(hids, &char_id, ble_evt);
	} else if (rep_value_identify(hids, evt_write->handle, &char_id, &rep_data_offset,
				      &max_rep_len)) {
		on_report_value_write(hids, ble_evt, &char_id, rep_data_offset, max_rep_len);
	} else {
		/* No implementation needed. */
	}
}

static void on_rw_authorize_request(struct ble_hids *hids, ble_evt_t const *ble_evt)
{
	ble_gatts_evt_rw_authorize_request_t const *evt_rw_auth =
		&ble_evt->evt.gatts_evt.params.authorize_request;

	struct ble_hids_char_id char_id;
	uint16_t rep_data_offset = sizeof(struct ble_hids_client_context);
	uint16_t max_rep_len = 0;

	if (evt_rw_auth->type != BLE_GATTS_AUTHORIZE_TYPE_READ) {
		/* Unexpected operation */
		return;
	}

	/* Update SD GATTS values of appropriate host before SD sends the Read Response */
	if (evt_rw_auth->request.read.handle == hids->protocol_mode_handles.value_handle) {
		on_protocol_mode_read_auth(hids, ble_evt);
	} else if (evt_rw_auth->request.read.handle == hids->boot_kb_inp_rep_handles.value_handle) {
		char_id = make_char_id(BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT_CHAR, 0, 0);
		max_rep_len = BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE;
		on_report_value_read_auth(hids, &char_id, ble_evt, rep_data_offset, max_rep_len);
	} else if (evt_rw_auth->request.read.handle ==
		   hids->boot_kb_outp_rep_handles.value_handle) {
		char_id = make_char_id(BLE_UUID_BOOT_KEYBOARD_OUTPUT_REPORT_CHAR, 0, 0);
		rep_data_offset += BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE;
		max_rep_len = BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE;
		on_report_value_read_auth(hids, &char_id, ble_evt, rep_data_offset, max_rep_len);
	} else if (evt_rw_auth->request.read.handle ==
		   hids->boot_mouse_inp_rep_handles.value_handle) {
		char_id = make_char_id(BLE_UUID_BOOT_MOUSE_INPUT_REPORT_CHAR, 0, 0);
		rep_data_offset += BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE +
				   BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE;
		max_rep_len = BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE;
		on_report_value_read_auth(hids, &char_id, ble_evt, rep_data_offset, max_rep_len);
	} else if (rep_value_identify(hids, evt_rw_auth->request.read.handle, &char_id,
				      &rep_data_offset, &max_rep_len)) {
		on_report_value_read_auth(hids, &char_id, ble_evt, rep_data_offset, max_rep_len);
	} else {
		/* Do nothing. */
	}
}

void ble_hids_on_ble_evt(ble_evt_t const *ble_evt, void *context)
{
	struct ble_hids *hids = (struct ble_hids *)context;

	if (!hids) {
		return;
	}

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		on_connect(hids, ble_evt);
		break;

	case BLE_GATTS_EVT_WRITE:
		on_write(hids, ble_evt);
		break;

	case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
		on_rw_authorize_request(hids, ble_evt);
		break;

	default:
		/* No implementation needed. */
		break;
	}
}

static uint32_t protocol_mode_char_add(struct ble_hids *hids, ble_gap_conn_sec_mode_t read_access,
				       ble_gap_conn_sec_mode_t write_access)
{
	uint8_t initial_protocol_mode = CONFIG_BLE_HIDS_DEFAULT_PROTOCOL_MODE;

	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = 1,
			.write_wo_resp = 1,
		},
	};
	ble_uuid_t char_uuid = {
		.uuid = BLE_UUID_PROTOCOL_MODE_CHAR,
		.type = BLE_UUID_TYPE_BLE,
	};
	ble_gatts_attr_md_t attr_md = {
		.rd_auth = 1,
		.vloc = BLE_GATTS_VLOC_STACK,
		.read_perm = read_access,
		.write_perm = write_access,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.max_len = sizeof(uint8_t),
		.init_len = sizeof(uint8_t),
		.p_value = &initial_protocol_mode,
	};

	return sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &attr_char_value,
					       &hids->protocol_mode_handles);
}

static uint32_t rep_char_add(struct ble_hids *hids, ble_gatt_char_props_t *properties,
			     struct ble_hids_report_config const *rep_cfg,
			     struct ble_hids_rep_char *rep_char)
{
	uint32_t nrf_err;
	uint8_t encoded_rep_ref[BLE_SRV_ENCODED_REPORT_REF_LEN] = {rep_cfg->report_id,
								   rep_cfg->report_type };

	ble_gatts_char_md_t char_md = {
		.char_props = *properties,
	};
	ble_uuid_t char_uuid = {
		.uuid = BLE_UUID_REPORT_CHAR,
		.type = BLE_UUID_TYPE_BLE,
	};
	ble_gatts_attr_md_t attr_md = {
		.rd_auth = 1,
		.wr_auth = 0,
		.vlen = 1,
		.vloc = BLE_GATTS_VLOC_STACK,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.max_len = rep_cfg->len,
	};
	ble_gatts_attr_md_t cccd_md = {0};

	attr_md.read_perm = rep_cfg->sec_mode.read;
	attr_md.write_perm = rep_cfg->sec_mode.write;

	if ((properties->notify == 1) || (properties->indicate == 1)) {

		cccd_md.write_perm = rep_cfg->sec_mode.cccd_write;
		BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);

		cccd_md.vloc = BLE_GATTS_VLOC_STACK;
		char_md.p_cccd_md = &cccd_md;
	}

	nrf_err = sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &attr_char_value,
						  &rep_char->char_handles);
	if (nrf_err) {
		return nrf_err;
	}

	ble_uuid_t desc_uuid = {
		.uuid = BLE_UUID_REPORT_REF_DESCR,
		.type = BLE_UUID_TYPE_BLE,
	};
	ble_gatts_attr_md_t desc_attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
	};
	ble_gatts_attr_t desc_params = {
		.p_uuid = &desc_uuid,
		.p_attr_md = &desc_attr_md,
		.init_len = sizeof(encoded_rep_ref),
		.max_len = sizeof(encoded_rep_ref),
		.p_value = encoded_rep_ref,
	};

	desc_attr_md.read_perm = rep_cfg->sec_mode.read;
	desc_attr_md.write_perm = rep_cfg->sec_mode.write;

	return sd_ble_gatts_descriptor_add(rep_char->char_handles.value_handle, &desc_params,
					   &rep_char->ref_handle);
}

static uint32_t rep_map_char_add(struct ble_hids *hids, const struct ble_hids_config *hids_cfg)
{
	uint32_t nrf_err;
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = 1,
		},
	};
	ble_uuid_t char_uuid = {
		.uuid = BLE_UUID_REPORT_MAP_CHAR,
		.type = BLE_UUID_TYPE_BLE,
	};
	ble_gatts_attr_md_t attr_md = {
		.vlen = 1,
		.vloc = BLE_GATTS_VLOC_STACK,
		.read_perm = hids_cfg->sec_mode.report_map_char.read,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.max_len = hids_cfg->report_map.len,
		.init_len = hids_cfg->report_map.len,
		.p_value = hids_cfg->report_map.data,
	};

	nrf_err = sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &attr_char_value,
						  &hids->rep_map_handles);
	if (nrf_err) {
		return nrf_err;
	}

	if (hids_cfg->report_map.ext_rep_ref_num != 0 && hids_cfg->report_map.ext_rep_ref == NULL) {
		return NRF_ERROR_INVALID_PARAM;
	}

	for (int i = 0; i < hids_cfg->report_map.ext_rep_ref_num; ++i) {
		uint8_t encoded_rep_ref[sizeof(ble_uuid128_t)];
		uint8_t encoded_rep_ref_len;

		nrf_err = sd_ble_uuid_encode(&hids_cfg->report_map.ext_rep_ref[i],
					     &encoded_rep_ref_len, encoded_rep_ref);
		if (nrf_err) {
			return nrf_err;
		}

		ble_uuid_t desc_uuid = {
			.uuid = BLE_UUID_EXTERNAL_REPORT_REF_DESCR,
			.type = BLE_UUID_TYPE_BLE,
		};
		ble_gatts_attr_md_t attr_md = {
			.vloc = BLE_GATTS_VLOC_STACK,
			.read_perm = hids_cfg->sec_mode.report_map_char.read,
		};
		ble_gatts_attr_t descr_params = {
			.p_uuid = &desc_uuid,
			.p_attr_md = &attr_md,
			.init_len = encoded_rep_ref_len,
			.max_len = encoded_rep_ref_len,
			.p_value = encoded_rep_ref,
		};

		nrf_err = sd_ble_gatts_descriptor_add(hids->rep_map_handles.value_handle,
						      &descr_params,
						      &hids->rep_map_ext_rep_ref_handle);
		if (nrf_err) {
			return nrf_err;
		}
	}

	return NRF_SUCCESS;
}

static uint32_t boot_inp_rep_char_add(struct ble_hids *hids, uint16_t uuid, uint16_t max_data_len,
				      ble_gap_conn_sec_mode_t sec_read,
				      ble_gap_conn_sec_mode_t sec_write,
				      ble_gap_conn_sec_mode_t sec_cccd_write,
				      ble_gatts_char_handles_t *char_handles)
{
	ble_gatts_attr_md_t cccd_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.write_perm = sec_cccd_write,
		.read_perm = BLE_GAP_CONN_SEC_MODE_OPEN,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = 1,
			.write = (sec_write.sm && sec_write.lv) ? 1 : 0,
			.notify = 1,
		},
		.p_cccd_md = &cccd_md
	};
	ble_uuid_t char_uuid = {
		.uuid = uuid,
		.type = BLE_UUID_TYPE_BLE,
	};
	ble_gatts_attr_md_t attr_md = {
		.rd_auth = 1,
		.vloc = BLE_GATTS_VLOC_STACK,
		.read_perm = sec_read,
		.write_perm = sec_write,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.max_len = max_data_len,
	};

	return sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &attr_char_value,
					       char_handles);
}

#if defined(CONFIG_BLE_HIDS_BOOT_KEYBOARD)
static uint32_t boot_kb_outp_rep_char_add(struct ble_hids *hids,
					  const struct ble_hids_config *hids_cfg)
{
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = 1,
			.write = 1,
			.write_wo_resp = 1,
		},
	};
	ble_uuid_t char_uuid = {
		.uuid = BLE_UUID_BOOT_KEYBOARD_OUTPUT_REPORT_CHAR,
		.type = BLE_UUID_TYPE_BLE,
	};
	ble_gatts_attr_md_t attr_md = {
		.rd_auth = 1,
		.vloc = BLE_GATTS_VLOC_STACK,
		.read_perm = hids_cfg->sec_mode.boot_kb_outp_rep_char.read,
		.write_perm = hids_cfg->sec_mode.boot_kb_outp_rep_char.write,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.max_len = BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE,
	};

	return sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &attr_char_value,
					       &hids->boot_kb_outp_rep_handles);
}
#endif

static uint32_t hid_information_char_add(struct ble_hids *hids,
					 const struct ble_hids_config *hids_cfg)
{
	__ASSERT_NO_MSG(hids && hids_cfg);

	uint8_t encoded_hid_information[ENCODED_HID_INFORMATION_LEN] = {
		((hids_cfg->hid_information.bcd_hid & 0x00FF) >> 0) & 0xff,
		((hids_cfg->hid_information.bcd_hid & 0xFF00) >> 8) & 0xff,
		hids_cfg->hid_information.b_country_code,
		((hids_cfg->hid_information.flags.normally_connectable << 1) |
		 (hids_cfg->hid_information.flags.remote_wake << 0))
	};

	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = 1,
		},
	};
	ble_uuid_t char_uuid = {
		.uuid = BLE_UUID_HID_INFORMATION_CHAR,
		.type = BLE_UUID_TYPE_BLE,
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.read_perm = hids_cfg->sec_mode.hid_info_char.read,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.max_len = sizeof(encoded_hid_information),
		.init_len = sizeof(encoded_hid_information),
		.p_value = encoded_hid_information,
	};

	return sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &attr_char_value,
					       &hids->hid_information_handles);
}

static uint32_t hid_control_point_char_add(struct ble_hids *hids,
					   ble_gap_conn_sec_mode_t write_access)
{
	uint8_t initial_hid_control_point = HIDS_CONTROL_POINT_SUSPEND;

	ble_gatts_char_md_t char_md = {
		.char_props = {
			.write_wo_resp = 1,
		},
	};
	ble_uuid_t char_uuid = {
		.uuid = BLE_UUID_HID_CONTROL_POINT_CHAR,
		.type = BLE_UUID_TYPE_BLE,
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.write_perm = write_access,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.max_len = sizeof(uint8_t),
		.init_len = sizeof(uint8_t),
		.p_value = &initial_hid_control_point,
	};

	return sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &attr_char_value,
					       &hids->hid_control_point_handles);
}

static uint32_t inp_rep_characteristics_add(struct ble_hids *hids,
					    const struct ble_hids_config *hids_cfg)
{
	uint32_t nrf_err;

	if ((hids_cfg->input_report_count != 0) && (hids_cfg->input_report != NULL)) {
		for (uint8_t i = 0; i < hids_cfg->input_report_count; i++) {
			struct ble_hids_report_config const *rep_init = &hids_cfg->input_report[i];
			ble_gatt_char_props_t properties = {
				.read = true,
				.write = (rep_init->sec_mode.write.sm &&
					  rep_init->sec_mode.write.lv) ? 1 : 0,
				.notify = true,
			};

			nrf_err = rep_char_add(hids, &properties, rep_init,
					       &hids->inp_rep_array[i]);
			if (nrf_err) {
				return nrf_err;
			}
		}
	}

	return NRF_SUCCESS;
}

static uint32_t outp_rep_characteristics_add(struct ble_hids *hids,
					     const struct ble_hids_config *hids_cfg)
{
	uint32_t nrf_err;

	if ((hids_cfg->output_report_count != 0) && (hids_cfg->output_report != NULL)) {
		for (uint8_t i = 0; i < hids_cfg->output_report_count; i++) {

			struct ble_hids_report_config const *rep_init = &hids_cfg->output_report[i];
			ble_gatt_char_props_t properties = {
				.read = true,
				.write = true,
				.write_wo_resp = true,
			};

			nrf_err = rep_char_add(hids, &properties, rep_init,
					       &hids->outp_rep_array[i]);
			if (nrf_err) {
				return nrf_err;
			}
		}
	}

	return NRF_SUCCESS;
}

static uint32_t feature_rep_characteristics_add(struct ble_hids *hids,
						const struct ble_hids_config *hids_cfg)
{
	uint32_t nrf_err;

	if ((hids_cfg->feature_report_count != 0) && (hids_cfg->feature_report != NULL)) {
		for (uint8_t i = 0; i < hids_cfg->feature_report_count; i++) {

			struct ble_hids_report_config const *rep_init =
				&hids_cfg->feature_report[i];
			ble_gatt_char_props_t properties = {
				.read = true,
				.write = true,
			};

			nrf_err = rep_char_add(hids, &properties, rep_init,
					       &hids->feature_rep_array[i]);
			if (nrf_err) {
				return nrf_err;
			}
		}
	}

	return NRF_SUCCESS;
}

static uint32_t includes_add(struct ble_hids *hids, const struct ble_hids_config *hids_cfg)
{
	uint32_t nrf_err;
	uint16_t unused_include_handle;

	for (uint8_t i = 0; i < hids_cfg->included_services_count; i++) {
		nrf_err = sd_ble_gatts_include_add(hids->service_handle,
						   hids_cfg->included_services_array[i],
						   &unused_include_handle);
		if (nrf_err) {
			return nrf_err;
		}
	}

	return NRF_SUCCESS;
}

uint32_t ble_hids_init(struct ble_hids *hids, const struct ble_hids_config *hids_cfg)
{
	uint32_t nrf_err;
	ble_uuid_t ble_uuid;

	if (!hids || !hids_cfg) {
		return NRF_ERROR_NULL;
	}

	if ((hids_cfg->input_report_count > CONFIG_BLE_HIDS_INPUT_REPORT_MAX_NUM) ||
	    (hids_cfg->output_report_count > CONFIG_BLE_HIDS_OUTPUT_REPORT_MAX_NUM) ||
	    (hids_cfg->feature_report_count > CONFIG_BLE_HIDS_FEATURE_REPORT_MAX_NUM)) {
		return NRF_ERROR_INVALID_PARAM;
	}

	/* Initialize service structure. */
	hids->evt_handler = hids_cfg->evt_handler;
	hids->input_report_count = hids_cfg->input_report_count;
	hids->output_report_count = hids_cfg->output_report_count;
	hids->feature_report_count = hids_cfg->feature_report_count;
	hids->inp_rep_init_array = hids_cfg->input_report;
	hids->outp_rep_init_array = hids_cfg->output_report;
	hids->feature_rep_init_array = hids_cfg->feature_report;

	BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE);

	nrf_err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid,
					   &hids->service_handle);
	if (nrf_err) {
		return nrf_err;
	}

	nrf_err = includes_add(hids, hids_cfg);
	if (nrf_err) {
		return nrf_err;
	}

#if defined(CONFIG_BLE_HIDS_BOOT_KEYBOARD) || defined(CONFIG_BLE_HIDS_BOOT_MOUSE)
	/* Add Protocol Mode characteristic. */
	nrf_err = protocol_mode_char_add(hids, hids_cfg->sec_mode.protocol_mode_char.read,
					 hids_cfg->sec_mode.protocol_mode_char.write);
	if (nrf_err) {
		return nrf_err;
	}
#endif

	/* Add Input Report characteristics (if any). */
	nrf_err = inp_rep_characteristics_add(hids, hids_cfg);
	if (nrf_err) {
		return nrf_err;
	}

	/* Add Output Report characteristics (if any). */
	nrf_err = outp_rep_characteristics_add(hids, hids_cfg);
	if (nrf_err) {
		return nrf_err;
	}

	/* Add Feature Report characteristic (if any). */
	nrf_err = feature_rep_characteristics_add(hids, hids_cfg);
	if (nrf_err) {
		return nrf_err;
	}

	/* Add Report Map characteristic. */
	nrf_err = rep_map_char_add(hids, hids_cfg);
	if (nrf_err) {
		return nrf_err;
	}

#if defined(CONFIG_BLE_HIDS_BOOT_KEYBOARD)
	/* Add Boot Keyboard Input Report characteristic. */
	nrf_err = boot_inp_rep_char_add(hids, BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT_CHAR,
					BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE,
					hids_cfg->sec_mode.boot_kb_inp_rep_char.read,
					hids_cfg->sec_mode.boot_kb_inp_rep_char.write,
					hids_cfg->sec_mode.boot_kb_inp_rep_char.cccd_write,
					&hids->boot_kb_inp_rep_handles);
	if (nrf_err) {
		return nrf_err;
	}

	/* Add Boot Keyboard Output Report characteristic. */
	nrf_err = boot_kb_outp_rep_char_add(hids, hids_cfg);
	if (nrf_err) {
		return nrf_err;
	}
#endif

#if defined(CONFIG_BLE_HIDS_BOOT_MOUSE)
	/* Add Boot Mouse Input Report characteristic. */
	nrf_err = boot_inp_rep_char_add(
		hids, BLE_UUID_BOOT_MOUSE_INPUT_REPORT_CHAR,
		BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE,
		hids_cfg->sec_mode.boot_mouse_inp_rep_char.read,
		hids_cfg->sec_mode.boot_mouse_inp_rep_char.write,
		hids_cfg->sec_mode.boot_mouse_inp_rep_char.cccd_write,
		&hids->boot_mouse_inp_rep_handles);
	if (nrf_err) {
		return nrf_err;
	}
#endif

	/* Add HID Information characteristic. */
	nrf_err = hid_information_char_add(hids, hids_cfg);
	if (nrf_err) {
		return nrf_err;
	}

	/* Add HID Control Point characteristic. */
	return hid_control_point_char_add(hids, hids_cfg->sec_mode.ctrl_point_char.write);
}

uint32_t ble_hids_inp_rep_send(struct ble_hids *hids, uint16_t conn_handle,
			       struct ble_hids_input_report *report)
{
	uint32_t nrf_err;

	if (!hids || !report || !report->data) {
		return NRF_ERROR_NULL;
	}

	if (report->report_index < hids->input_report_count) {
		struct ble_hids_rep_char *rep_char = &hids->inp_rep_array[report->report_index];

		if (conn_handle != BLE_CONN_HANDLE_INVALID) {
			ble_gatts_hvx_params_t hvx_params;
			uint8_t index = 0;
			uint16_t hvx_len = report->len;
			uint8_t *host_rep_data;

			nrf_err = link_ctx_get(hids, conn_handle, (void *)&host_rep_data);
			if (nrf_err) {
				LOG_ERR("Failed to get link context, nrf_error %#x", nrf_err);
				return nrf_err;
			}

			host_rep_data += sizeof(struct ble_hids_client_context) +
					 BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE +
					 BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE +
					 BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE;

			/* Store the new report data in host's context */
			while (index < report->report_index) {
				host_rep_data += hids->inp_rep_init_array[index].len;
				++index;
			}

			if (report->len <= hids->inp_rep_init_array[report->report_index].len) {
				memcpy(host_rep_data, report->data, report->len);
			} else {
				return NRF_ERROR_DATA_SIZE;
			}

			/* Notify host */
			memset(&hvx_params, 0, sizeof(hvx_params));

			hvx_params.handle = rep_char->char_handles.value_handle;
			hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
			hvx_params.offset = 0;
			hvx_params.p_len = &hvx_len;
			hvx_params.p_data = report->data;

			nrf_err = sd_ble_gatts_hvx(conn_handle, &hvx_params);
			if ((nrf_err == NRF_SUCCESS) && (*hvx_params.p_len != report->len)) {
				LOG_ERR("Failed to update attribute value, nrf_error %#x", nrf_err);
				nrf_err = NRF_ERROR_DATA_SIZE;
			}
		} else {
			nrf_err = NRF_ERROR_INVALID_STATE;
		}
	} else {
		nrf_err = NRF_ERROR_INVALID_PARAM;
	}

	return nrf_err;
}

uint32_t ble_hids_boot_kb_inp_rep_send(struct ble_hids *hids, uint16_t conn_handle,
				       struct ble_hids_boot_keyboard_input_report *report)
{
	if (!hids || !report || !report->data) {
		return NRF_ERROR_NULL;
	}

	uint32_t nrf_err;

	if (conn_handle != BLE_CONN_HANDLE_INVALID) {
		ble_gatts_hvx_params_t hvx_params;
		uint16_t hvx_len = report->len;
		uint8_t *host_rep_data;

		nrf_err = link_ctx_get(hids, conn_handle, (void *)&host_rep_data);
		if (nrf_err) {
			return nrf_err;
		}

		host_rep_data += sizeof(struct ble_hids_client_context);

		/* Store the new value in the host's context */
		if (report->len <= BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE) {
			memcpy(host_rep_data, report->data, report->len);
		}

		/* Notify host */
		memset(&hvx_params, 0, sizeof(hvx_params));

		hvx_params.handle = hids->boot_kb_inp_rep_handles.value_handle;
		hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
		hvx_params.offset = 0;
		hvx_params.p_len = &hvx_len;
		hvx_params.p_data = report->data;

		nrf_err = sd_ble_gatts_hvx(conn_handle, &hvx_params);
		if ((nrf_err == NRF_SUCCESS) && (*hvx_params.p_len != report->len)) {
			nrf_err = NRF_ERROR_DATA_SIZE;
		}
	} else {
		nrf_err = NRF_ERROR_INVALID_STATE;
	}

	return nrf_err;
}

uint32_t ble_hids_boot_mouse_inp_rep_send(struct ble_hids *hids, uint16_t conn_handle,
					  struct ble_hids_boot_mouse_input_report *report)
{
	if (!hids || !report) {
		return NRF_ERROR_NULL;
	}

	uint32_t nrf_err;

	if (conn_handle != BLE_CONN_HANDLE_INVALID) {
		uint16_t hvx_len = BOOT_MOUSE_INPUT_REPORT_MIN_SIZE + report->optional_data_len;

		if (hvx_len <= BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE) {
			uint8_t buffer[BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE];
			ble_gatts_hvx_params_t hvx_params;
			uint8_t *host_rep_data;

			nrf_err = link_ctx_get(hids, conn_handle, (void *)&host_rep_data);
			if (nrf_err) {
				return nrf_err;
			}

			host_rep_data += sizeof(struct ble_hids_client_context) +
					 BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE +
					 BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE;

			__ASSERT_NO_MSG(BOOT_MOUSE_INPUT_REPORT_MIN_SIZE == 3);

			/* Build buffer */
			buffer[0] = report->buttons;
			buffer[1] = (uint8_t)report->delta_x;
			buffer[2] = (uint8_t)report->delta_y;

			if (report->optional_data_len > 0) {
				memcpy(&buffer[3], report->optional_data,
				       report->optional_data_len);
			}

			/* Store the new value in the host's context */
			memcpy(host_rep_data, buffer, hvx_len);

			/* Pass buffer to stack */
			memset(&hvx_params, 0, sizeof(hvx_params));

			hvx_params.handle = hids->boot_mouse_inp_rep_handles.value_handle;
			hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
			hvx_params.offset = 0;
			hvx_params.p_len = &hvx_len;
			hvx_params.p_data = buffer;

			nrf_err = sd_ble_gatts_hvx(conn_handle, &hvx_params);
			if ((nrf_err == NRF_SUCCESS) &&
			    (*hvx_params.p_len != (BOOT_MOUSE_INPUT_REPORT_MIN_SIZE +
						   report->optional_data_len))) {
				nrf_err = NRF_ERROR_DATA_SIZE;
			}
		} else {
			nrf_err = NRF_ERROR_DATA_SIZE;
		}
	} else {
		nrf_err = NRF_ERROR_INVALID_STATE;
	}

	return nrf_err;
}

uint32_t ble_hids_outp_rep_get(struct ble_hids *hids, uint8_t report_index, uint16_t len,
			       uint8_t offset, uint16_t conn_handle, uint8_t *outp_rep)
{
	if (!hids || !outp_rep) {
		return NRF_ERROR_NULL;
	}

	uint32_t nrf_err;
	uint8_t *rep_data;
	uint8_t index;

	if (report_index >= hids->output_report_count) {
		return NRF_ERROR_INVALID_PARAM;
	}

	nrf_err = link_ctx_get(hids, conn_handle, (void *)&rep_data);
	if (nrf_err) {
		return nrf_err;
	}

	rep_data += sizeof(struct ble_hids_client_context) +
		    BLE_HIDS_BOOT_KB_INPUT_REPORT_MAX_SIZE +
		    BLE_HIDS_BOOT_KB_OUTPUT_REPORT_MAX_SIZE +
		    BLE_HIDS_BOOT_MOUSE_INPUT_REPORT_MAX_SIZE;

	for (index = 0; index < hids->input_report_count; index++) {
		rep_data += hids->inp_rep_init_array[index].len;
	}

	for (index = 0; index < report_index; index++) {
		rep_data += hids->outp_rep_init_array[index].len;
	}

	/* Copy the requested output report data */
	if (len + offset <= hids->outp_rep_init_array[report_index].len) {
		memcpy(outp_rep, rep_data + offset, len);
	} else {
		return NRF_ERROR_INVALID_LENGTH;
	}

	return NRF_SUCCESS;
}
