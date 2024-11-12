/*
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <string.h>
#include <ble_gap.h>
#include <ble_types.h>
#include <ble_gatts.h>
#include <bluetooth/services/ble_hids.h>
#include <bluetooth/services/uuid.h>
#include <bluetooth/services/common.h>
#include <errno.h>
#include <zephyr/toolchain.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_REGISTER(ble_hids, CONFIG_BLE_HIDS_LOG_LEVEL);

/* Boot Protocol Mode. */
#define PROTOCOL_MODE_BOOT 0x00
/* Report Protocol Mode. */
#define PROTOCOL_MODE_REPORT 0x01

/* Suspend command. */
#define CONTROL_POINT_SUSPEND 0x00
/* Exit Suspend command. */
#define CONTROL_POINT_EXIT_SUSPEND 0x01

struct ble_hids_context {
	uint8_t protocol_mode;
	uint8_t control_point;
#if CONFIG_BLE_HIDS_BOOT_KEYBOARD
	uint8_t boot_key_in_rep[BLE_HIDS_BOOT_KB_INPUT_REP_MAX_SIZE];
	uint8_t boot_key_out_rep[BLE_HIDS_BOOT_KB_OUTPUT_REP_MAX_SIZE];
#endif
#if CONFIG_BLE_HIDS_BOOT_MOUSE
	uint8_t boot_mouse_in_rep[BLE_HIDS_BOOT_MOUSE_INPUT_REP_MAX_SIZE];
#endif
	struct {
		uint8_t data[CONFIG_BLE_HIDS_INPUT_REPORT_MAX_LEN];
	} input_reports[CONFIG_BLE_HIDS_MAX_INPUT_REP];
	struct {
		uint8_t data[CONFIG_BLE_HIDS_OUTPUT_REPORT_MAX_LEN];
	} output_reports[CONFIG_BLE_HIDS_MAX_OUTPUT_REP];
	struct {
		uint8_t data[CONFIG_BLE_HIDS_FEATURE_REPORT_MAX_LEN];
	} feature_reports[CONFIG_BLE_HIDS_MAX_FEATURE_REP];
};

static struct ble_hids_context contexts[CONFIG_BLE_HIDS_MAX_CLIENTS];

static struct ble_hids_context *ble_hids_context_get(uint16_t conn_handle)
{
	return &contexts[conn_handle];
}

/**@brief Function for making a HID Service characteristic id.
 *
 * @param[in]   uuid        UUID of characteristic.
 * @param[in]   rep_type    Type of report.
 * @param[in]   rep_index   Index of the characteristic.
 *
 * @return      HID Service characteristic id structure.
 */
static struct ble_hids_char_id make_char_id(uint16_t uuid, uint8_t rep_type, uint8_t rep_index)
{
	struct ble_hids_char_id char_id = {
		.uuid = uuid,
		.rep_type = rep_type,
		.rep_index = rep_index,
	};

	return char_id;
}

static void on_connected(struct ble_hids *hids, const ble_evt_t *ble_evt)
{
	uint32_t nrf_err;
	struct ble_hids_context *ctx;

	if (!IS_ENABLED(CONFIG_BLE_HIDS_BOOT_KEYBOARD) && !IS_ENABLED(CONFIG_BLE_HIDS_BOOT_MOUSE)) {
		return;
	}

	/* Set Protocol Mode characteristic value to default value */
	ctx = ble_hids_context_get(ble_evt->evt.gap_evt.conn_handle);
	ctx->protocol_mode = CONFIG_BLE_HIDS_DEFAULT_PROTOCOL_MODE;

	ble_gatts_value_t gatts_value = {
		.len = sizeof(ctx->protocol_mode),
		.p_value = &ctx->protocol_mode,
	};

	nrf_err = sd_ble_gatts_value_set(ble_evt->evt.gap_evt.conn_handle,
				     hids->protocol_mode_handles.value_handle, &gatts_value);
	if (nrf_err != NRF_SUCCESS) {
		LOG_WRN("Failed to set protocol mode value to default, nrf_error %#x", nrf_err);
	}

	LOG_DBG("Protocol mode is %s",
		IS_ENABLED(CONFIG_BLE_HIDS_PROTOCOL_MODE_BOOT) ? "boot" : "report");
}

static void on_disconnected(struct ble_hids *hids, const ble_evt_t *ble_evt)
{
	struct ble_hids_context *ctx;

	ctx = ble_hids_context_get(ble_evt->evt.gap_evt.conn_handle);

	/* Reset client's context */
	memset(ctx, 0, sizeof(struct ble_hids_context));
}

static void on_control_point_write(struct ble_hids *hids, const ble_evt_t *ble_evt)
{
	struct ble_hids_context *ctx;
	struct ble_hids_evt evt = {0};
	ble_gatts_evt_write_t const *gatts_write = &ble_evt->evt.gatts_evt.params.write;

	LOG_INF("Control point value write");

	ctx = ble_hids_context_get(ble_evt->evt.gatts_evt.conn_handle);

	switch (gatts_write->data[0]) {
	case CONTROL_POINT_SUSPEND:
		evt.evt_type = BLE_HIDS_EVT_HOST_SUSP;
		break;
	case CONTROL_POINT_EXIT_SUSPEND:
		evt.evt_type = BLE_HIDS_EVT_HOST_EXIT_SUSP;
		break;
	default:
		LOG_WRN("Unknown control point value %#x, ignoring", gatts_write->data[0]);
		return;
	}

	/* Store the new Control Point value for the host */
	ctx->control_point = gatts_write->data[0];

	if (hids->evt_handler != NULL) {
		evt.ble_evt = ble_evt;
		hids->evt_handler(hids, &evt);
	}
}

static void on_protocol_mode_write(struct ble_hids *hids, const ble_evt_t *ble_evt)
{
	struct ble_hids_evt evt = {0};
	struct ble_hids_context *ctx;
	const ble_gatts_evt_write_t *gatts_write = &ble_evt->evt.gatts_evt.params.write;

	LOG_INF("Protocol mode write");

	ctx = ble_hids_context_get(ble_evt->evt.gatts_evt.conn_handle);

	switch (gatts_write->data[0]) {
	case PROTOCOL_MODE_BOOT:
		evt.evt_type = BLE_HIDS_EVT_BOOT_MODE_ENTERED;
		break;
	case PROTOCOL_MODE_REPORT:
		evt.evt_type = BLE_HIDS_EVT_REPORT_MODE_ENTERED;
		break;
	default:
		LOG_WRN("Bad protocol mode write value %#x, ignoring", gatts_write->data[0]);
		return;
	}

	/* Store Protocol Mode of the host */
	ctx->protocol_mode = gatts_write->data[0];

	if (hids->evt_handler != NULL) {
		evt.ble_evt = ble_evt;
		hids->evt_handler(hids, &evt);
	}
}

static void on_report_cccd_write(struct ble_hids *hids, struct ble_hids_char_id *char_id,
				 const ble_evt_t *ble_evt)
{
	const ble_gatts_evt_write_t *gatts_write = &ble_evt->evt.gatts_evt.params.write;

	LOG_INF("Report CCCD write");

	if (!hids->evt_handler) {
		return;
	}

	struct ble_hids_evt evt = {
		.evt_type = is_notification_enabled(gatts_write->data)
				    ? BLE_HIDS_EVT_NOTIF_ENABLED
				    : BLE_HIDS_EVT_NOTIF_DISABLED,
		.params.notification.char_id = *char_id,
		.ble_evt = ble_evt,
	};

	hids->evt_handler(hids, &evt);
}

static void on_report_value_write(struct ble_hids *hids, const ble_evt_t *ble_evt,
				  struct ble_hids_char_id *char_id, void *dest,
				  uint16_t rep_max_len)
{
	const ble_gatts_evt_write_t *gatts_write = &ble_evt->evt.gatts_evt.params.write;

	LOG_DBG("Report value write");

	/** TODO: SoftDevice should have checked this, confirm? */
	__ASSERT_NO_MSG(gatts_write->offset + gatts_write->len <= rep_max_len);

	/* Store the values in host's report data */
	memcpy(dest, gatts_write->data, gatts_write->len);

	if (!hids->evt_handler) {
		return;
	}

	struct ble_hids_evt evt = {
		.evt_type = BLE_HIDS_EVT_REP_CHAR_WRITE,
		.params.char_write.char_id = *char_id,
		.params.char_write.offset = ble_evt->evt.gatts_evt.params.write.offset,
		.params.char_write.len = ble_evt->evt.gatts_evt.params.write.len,
		.params.char_write.data = ble_evt->evt.gatts_evt.params.write.data,
		.ble_evt = ble_evt,
	};

	hids->evt_handler(hids, &evt);
}

static void on_protocol_mode_read_auth(struct ble_hids *hids, const ble_gatts_evt_t *gatts_evt)
{
	uint32_t nrf_err;
	struct ble_hids_context *ctx;
	uint16_t read_offset;

	ctx = ble_hids_context_get(gatts_evt->conn_handle);
	read_offset = gatts_evt->params.authorize_request.request.read.offset;

	/* Update GATTS table with this host's Protocol Mode value and authorize Read */
	const ble_gatts_rw_authorize_reply_params_t auth_read_params = {
		.type = BLE_GATTS_AUTHORIZE_TYPE_READ,
		.params.read = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.len = sizeof(ctx->protocol_mode),
			.p_data = &ctx->protocol_mode,
			.offset = read_offset,
			.update = 1,
		},
	};

	nrf_err = sd_ble_gatts_rw_authorize_reply(gatts_evt->conn_handle, &auth_read_params);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to authorize protocol mode read, nrf_error %#x", nrf_err);
	}
}

static void on_report_value_read_auth(struct ble_hids *hids, struct ble_hids_char_id *char_id,
				      const ble_gatts_evt_t *gatts_evt, void *report,
				      uint16_t rep_max_len)
{
	uint32_t nrf_err;
	uint16_t read_offset;

	LOG_DBG("Report value read auth");

	read_offset = gatts_evt->params.authorize_request.request.read.offset;

	/* Update Report GATTS table with host's current report data */
	const ble_gatts_rw_authorize_reply_params_t auth_read_params = {
		.type = BLE_GATTS_AUTHORIZE_TYPE_READ,
		.params.read = {
			.gatt_status = BLE_GATT_STATUS_SUCCESS,
			.len = rep_max_len - read_offset,
			.p_data = (uint8_t *)report + read_offset,
			.offset = read_offset,
			.update = 1,
		},
	};

	nrf_err = sd_ble_gatts_rw_authorize_reply(gatts_evt->conn_handle, &auth_read_params);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to authorize report value read, nrf_error %#x", nrf_err);
	}

	if (!hids->evt_handler) {
		return;
	}

	struct ble_hids_evt evt = {
		.evt_type = BLE_HIDS_EVT_REPORT_READ,
		.params.char_auth_read.char_id = *char_id,
	};

	hids->evt_handler(hids, &evt);
}

static bool inp_rep_cccd_identify(struct ble_hids *hids, uint16_t handle,
				  struct ble_hids_char_id *char_id)
{
	LOG_DBG("Searching for input report CCCD");

	for (size_t i = 0; i < ARRAY_SIZE(hids->input_report); i++) {
		if (hids->input_report[i].char_handles.cccd_handle == handle) {
			*char_id = make_char_id(BLE_UUID_REPORT_CHAR,
						BLE_HIDS_REPORT_TYPE_INPUT, i);
			LOG_DBG("Input report CCCD found, handle %#x", handle);
			return true;
		}
	}

	return false;
}

static bool rep_value_identify(struct ble_hids *hids, uint16_t conn_handle, uint16_t handle,
			       struct ble_hids_char_id *char_id, void **report,
			       uint16_t *report_max_len)
{
	struct ble_hids_context *ctx;

	ctx = ble_hids_context_get(conn_handle);

	LOG_DBG("Searching for report value");

		for (size_t i = 0; i < hids->input_report_count; i++) {
			if (handle == hids->input_report[i].char_handles.value_handle) {
				*char_id = make_char_id(BLE_UUID_REPORT_CHAR,
							BLE_HIDS_REPORT_TYPE_INPUT, i);
				*report_max_len = hids->input_report[i].max_len;
				*report = &ctx->input_reports[i];
				LOG_DBG("Input report %d handle %#x", i, handle);
				return true;
			}
		}
		for (size_t i = 0; i < hids->output_report_count; i++) {
			if (handle == hids->output_report[i].char_handles.value_handle) {
				*char_id = make_char_id(BLE_UUID_REPORT_CHAR,
							BLE_HIDS_REPORT_TYPE_OUTPUT, i);
				*report_max_len = hids->output_report[i].max_len;
				*report = &ctx->output_reports[i];
				LOG_DBG("Output report %d handle %#x", i, handle);
				return true;
			}
		}
		for (size_t i = 0; i < hids->feature_report_count; i++) {
			if (handle == hids->feature_report[i].char_handles.value_handle) {
				*char_id = make_char_id(BLE_UUID_REPORT_CHAR,
							BLE_HIDS_REPORT_TYPE_FEATURE, i);
				*report_max_len = hids->feature_report[i].max_len;
				*report = &ctx->feature_reports[i];
				LOG_DBG("Feature report %d handle %#x", i, handle);
				return true;
			}
		}

	return false;
}

static void on_write(struct ble_hids *hids, const ble_evt_t *ble_evt)
{
	struct ble_hids_context *ctx;
	struct ble_hids_char_id char_id;
	void *report;
	uint16_t max_rep_len;
	uint16_t conn_handle = ble_evt->evt.gatts_evt.conn_handle;
	const ble_gatts_evt_write_t *gatts_write = &ble_evt->evt.gatts_evt.params.write;

	ctx = ble_hids_context_get(conn_handle);

	if (gatts_write->handle == hids->control_point_handles.value_handle) {
		on_control_point_write(hids, ble_evt);
		return;
	}
	if (gatts_write->handle == hids->protocol_mode_handles.value_handle) {
		on_protocol_mode_write(hids, ble_evt);
		return;
	}
	if (inp_rep_cccd_identify(hids, gatts_write->handle, &char_id)) {
		on_report_cccd_write(hids, &char_id, ble_evt);
		return;
	}
#if CONFIG_BLE_HIDS_BOOT_KEYBOARD
	if (gatts_write->handle == hids->boot_kb_inp_rep_handles.cccd_handle) {
		LOG_INF("Boot Keyboard input report CCCD");
		char_id = make_char_id(BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT_CHAR, 0, 0);
		on_report_cccd_write(hids, &char_id, ble_evt);
		return;
	}
	if (gatts_write->handle == hids->boot_kb_inp_rep_handles.value_handle) {
		LOG_INF("Boot keyboard input report value");
		char_id = make_char_id(BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT_CHAR, 0, 0);
		on_report_value_write(hids, ble_evt, &char_id, ctx->boot_key_in_rep,
				      BLE_HIDS_BOOT_KB_INPUT_REP_MAX_SIZE);
		return;
	}
	if (gatts_write->handle == hids->boot_kb_outp_rep_handles.value_handle) {
		LOG_INF("Boot keyboard output report value");
		char_id = make_char_id(BLE_UUID_BOOT_KEYBOARD_OUTPUT_REPORT_CHAR, 0, 0);
		on_report_value_write(hids, ble_evt, &char_id, ctx->boot_key_out_rep,
				      BLE_HIDS_BOOT_KB_OUTPUT_REP_MAX_SIZE);
		return;
	}
#endif
#if CONFIG_BLE_HIDS_BOOT_MOUSE
	if (gatts_write->handle == hids->boot_mouse_inp_rep_handles.cccd_handle) {
		LOG_INF("Boot Mouse input report CCCD");
		char_id = make_char_id(BLE_UUID_BOOT_MOUSE_INPUT_REPORT_CHAR, 0, 0);
		on_report_cccd_write(hids, &char_id, ble_evt);
		return;
	}
	if (gatts_write->handle == hids->boot_mouse_inp_rep_handles.value_handle) {
		LOG_INF("Boot mouse input report value");
		char_id = make_char_id(BLE_UUID_BOOT_MOUSE_INPUT_REPORT_CHAR, 0, 0);
		on_report_value_write(hids, ble_evt, &char_id, ctx->boot_mouse_in_rep,
				      BLE_HIDS_BOOT_MOUSE_INPUT_REP_MAX_SIZE);
		return;
	}
#endif
	if (rep_value_identify(hids, conn_handle, gatts_write->handle, &char_id, &report,
			       &max_rep_len)) {
		on_report_value_write(hids, ble_evt, &char_id, report, max_rep_len);
		return;
	}
}

static void on_rw_authorize_request(struct ble_hids *hids, const ble_evt_t *ble_evt)
{
	void *report;
	struct ble_hids_context *ctx;
	uint16_t conn_handle;
	uint16_t max_rep_len;
	struct ble_hids_char_id char_id;
	const ble_gatts_evt_rw_authorize_request_t *gatts_rw_auth =
		&ble_evt->evt.gatts_evt.params.authorize_request;

	conn_handle = ble_evt->evt.common_evt.conn_handle;

	if (gatts_rw_auth->type != BLE_GATTS_AUTHORIZE_TYPE_READ) {
		/* Unexpected operation */
		return;
	}

	ctx = ble_hids_context_get(conn_handle);

	/* Update SD GATTS values of appropriate host before SD sends the Read Response */
	if (gatts_rw_auth->request.read.handle == hids->protocol_mode_handles.value_handle) {
		on_protocol_mode_read_auth(hids, &ble_evt->evt.gatts_evt);
		return;
	}
#if CONFIG_BLE_HIDS_BOOT_KEYBOARD
	if (gatts_rw_auth->request.read.handle == hids->boot_kb_inp_rep_handles.value_handle) {
		char_id = make_char_id(BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT_CHAR, 0, 0);
		on_report_value_read_auth(hids, &char_id, &ble_evt->evt.gatts_evt,
					  ctx->boot_key_in_rep, sizeof(ctx->boot_key_in_rep));
		return;
	}
	if (gatts_rw_auth->request.read.handle == hids->boot_kb_outp_rep_handles.value_handle) {
		char_id = make_char_id(BLE_UUID_BOOT_KEYBOARD_OUTPUT_REPORT_CHAR, 0, 0);
		on_report_value_read_auth(hids, &char_id, &ble_evt->evt.gatts_evt,
					  ctx->boot_key_out_rep, sizeof(ctx->boot_key_out_rep));
		return;
	}
#endif
#if CONFIG_BLE_HIDS_BOOT_MOUSE
	if (gatts_rw_auth->request.read.handle == hids->boot_mouse_inp_rep_handles.value_handle) {
		char_id = make_char_id(BLE_UUID_BOOT_MOUSE_INPUT_REPORT_CHAR, 0, 0);
		on_report_value_read_auth(hids, &char_id, &ble_evt->evt.gatts_evt,
					  ctx->boot_mouse_in_rep, sizeof(ctx->boot_mouse_in_rep));
		return;
	}
#endif
	if (rep_value_identify(hids, conn_handle, gatts_rw_auth->request.read.handle, &char_id,
			       &report, &max_rep_len)) {
		on_report_value_read_auth(hids, &char_id, &ble_evt->evt.gatts_evt,
					  report, max_rep_len);
		return;
	}
}

void ble_hids_on_ble_evt(const ble_evt_t *ble_evt, void *p_context)
{
	struct ble_hids *hids = (struct ble_hids *)p_context;

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		on_connected(hids, ble_evt);
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnected(hids, ble_evt);
		break;
	case BLE_GATTS_EVT_WRITE:
		on_write(hids, ble_evt);
		break;
	case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
		on_rw_authorize_request(hids, ble_evt);
		break;
	default:
		break;
	}
}

#if defined(CONFIG_BLE_HIDS_BOOT_KEYBOARD) || defined(CONFIG_BLE_HIDS_BOOT_MOUSE)
static uint32_t protocol_mode_char_add(struct ble_hids *hids)
{
	uint32_t nrf_err;
	const ble_gatts_char_md_t char_md = {
		.char_props = { .read = true, .write_wo_resp = true, },
	};
	const ble_gatts_attr_t char_params = {
		.p_uuid = &(ble_uuid_t) {
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_PROTOCOL_MODE_CHAR,
		},
		.p_attr_md = &(ble_gatts_attr_md_t) {
			.vloc = BLE_GATTS_VLOC_STACK,
			.rd_auth = true,
			.read_perm = gap_conn_sec_mode_from_u8(
				CONFIG_BLE_HIDS_PROTOCOL_MODE_CHAR_READ_SEC_MODE),
			.write_perm = gap_conn_sec_mode_from_u8(
				CONFIG_BLE_HIDS_PROTOCOL_MODE_CHAR_WRITE_SEC_MODE),
		},
		.init_len = sizeof(uint8_t),
		.max_len = sizeof(uint8_t),
		.p_value = &(uint8_t){CONFIG_BLE_HIDS_DEFAULT_PROTOCOL_MODE},
	};

	/* Add characteristic */
	nrf_err = sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &char_params,
					      &hids->protocol_mode_handles);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to add protocol mode characteristic, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}
#endif

static uint32_t rep_char_add(struct ble_hids *hids, struct ble_hids_report *report,
			const struct ble_hids_report_config *report_init,
			const ble_gatt_char_props_t *props)
{
	uint32_t nrf_err;
	ble_gatts_char_md_t char_md = {
		.char_props = *props,
	};

	if (char_md.char_props.notify || char_md.char_props.notify) {
		/* There seems to be bug in gcc v12.2.0 where this structure is
		 * optimized in a bad way and not stored in memory correctly,
		 * thus the volatile keyword.
		 */
		volatile const ble_gatts_attr_md_t cccd_md = {
			.vloc = BLE_GATTS_VLOC_STACK,
			.read_perm = BLE_GAP_CONN_SEC_MODE_OPEN,
			.write_perm = report_init->sec.cccd_write,
		};

		char_md.p_cccd_md = (ble_gatts_attr_md_t *)&cccd_md;
	}

	const ble_gatts_attr_t char_params = {
		.p_uuid = &(ble_uuid_t) {
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_REPORT_CHAR,
		},
		.p_attr_md = &(ble_gatts_attr_md_t) {
			.vlen = true,
			.vloc = BLE_GATTS_VLOC_STACK,
			.rd_auth = true,
			.read_perm = report_init->sec.read,
			.write_perm = report_init->sec.write,
		},
		.max_len = report_init->len,
	};

	/* Add characteristic */
	nrf_err = sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &char_params,
					      &report->char_handles);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to add report characteristic, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	/* Store the characteristic maximum length */
	report->max_len = report_init->len;

	const ble_gatts_attr_t descr_params = {
		.p_uuid = &(ble_uuid_t){
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_REPORT_REF_DESCR,
		},
		.p_attr_md = &(ble_gatts_attr_md_t){
			.vloc = BLE_GATTS_VLOC_STACK,
			.read_perm = report_init->sec.read,
			.write_perm = report_init->sec.write,
		},
		.init_len = sizeof(uint16_t),
		.max_len = sizeof(uint16_t),
		.p_value = (uint8_t[]) {
			[0] = report_init->report_id,
			[1] = report_init->report_type,
		},
	};

	/* Add the descriptor */
	nrf_err = sd_ble_gatts_descriptor_add(report->char_handles.value_handle, &descr_params,
					      &report->ref_handle);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to add GATT report reference descriptor, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static uint32_t report_map_char_add(struct ble_hids *hids,
				    const struct ble_hids_config *hids_config)
{
	uint32_t nrf_err;
	const ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
		},
	};
	const ble_gatts_attr_t char_params = {
		.p_uuid = &(ble_uuid_t) {
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_REPORT_MAP_CHAR,
		},
		.p_attr_md = &(ble_gatts_attr_md_t) {
			.vlen = true,
			.vloc = BLE_GATTS_VLOC_STACK,
			.read_perm = hids_config->report_map.sec.read,
		},
		.max_len = hids_config->report_map.len,
		.init_len = hids_config->report_map.len,
		.p_value =   hids_config->report_map.data,
	};

	/* Add characteristic */
	nrf_err = sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &char_params,
						  &hids->rep_map_handles);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to add GATT characteristic, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	/** TODO why here? */
	if (hids_config->report_map.ext_rep_ref_count != 0 &&
	    hids_config->report_map.ext_rep_ref == NULL) {
		return NRF_ERROR_INVALID_PARAM;
	}

	for (size_t i = 0; i < hids_config->report_map.ext_rep_ref_count; ++i) {
		uint8_t encoded_rep_ref[sizeof(ble_uuid128_t)];
		uint8_t encoded_rep_ref_len;

		/* Add External Report Reference descriptor */
		nrf_err = sd_ble_uuid_encode(&hids_config->report_map.ext_rep_ref[i],
					     &encoded_rep_ref_len, encoded_rep_ref);
		if (nrf_err != NRF_SUCCESS) {
			return nrf_err;
		}

		const ble_gatts_attr_t descr_params = {
			.p_uuid = &(ble_uuid_t){
				.type = BLE_UUID_TYPE_BLE,
				.uuid = BLE_UUID_EXTERNAL_REPORT_REF_DESCR,
			},
			.p_attr_md = &(ble_gatts_attr_md_t){
				.vloc = BLE_GATTS_VLOC_STACK,
				.read_perm = hids_config->report_map.sec.read,
			},
			.init_len = encoded_rep_ref_len,
			.max_len = hids_config->report_map.len,
			.p_value = encoded_rep_ref
		};

		/* Add the descriptor */
		nrf_err = sd_ble_gatts_descriptor_add(hids->rep_map_handles.value_handle,
						      &descr_params, &(uint16_t){0} /* discard*/);
		if (nrf_err != NRF_SUCCESS) {
			LOG_ERR("Failed to add GATT report reference descriptor, nrf_error %#x",
				nrf_err);
			return nrf_err;
		}
	}

	return NRF_SUCCESS;
}

#if CONFIG_BLE_HIDS_BOOT_KEYBOARD
static uint32_t boot_kb_input_report_char_add(struct ble_hids *hids)
{
	uint32_t nrf_err;
	const ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
			.notify = true,
			/* for debug */
			.write = true,
		},
		.p_cccd_md = &(ble_gatts_attr_md_t) {
			.vloc = BLE_GATTS_VLOC_STACK,
			/* Use the same read permission as the characteristic's */
			.read_perm = gap_conn_sec_mode_from_u8(
				CONFIG_BLE_HIDS_BOOT_KEYBOARD_INPUT_CHAR_READ_SEC_MODE),
			.write_perm = gap_conn_sec_mode_from_u8(
				CONFIG_BLE_HIDS_BOOT_KEYBOARD_INPUT_CCCD_WRITE_SEC_MODE),
		},
	};
	const ble_gatts_attr_t char_params = {
		.p_uuid = &(ble_uuid_t) {
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT_CHAR,
		},
		.p_attr_md = &(ble_gatts_attr_md_t) {
			.vloc = BLE_GATTS_VLOC_STACK,
			.rd_auth = true,
			.read_perm = gap_conn_sec_mode_from_u8(
				CONFIG_BLE_HIDS_BOOT_KEYBOARD_INPUT_CHAR_READ_SEC_MODE),
			/* for debug */
			.write_perm = BLE_GAP_CONN_SEC_MODE_OPEN,
		},
		.max_len = BLE_HIDS_BOOT_KB_INPUT_REP_MAX_SIZE,
	};

	/* Add characteristic */
	nrf_err = sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &char_params,
						  &hids->boot_kb_inp_rep_handles);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to add Boot Keyboard input characteristic, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static uint32_t boot_kb_output_report_char_add(struct ble_hids *hids)
{
	uint32_t nrf_err;
	const ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
			.write = true,
			.write_wo_resp = true,
		},
	};
	const ble_gatts_attr_t char_params = {
		.p_uuid = &(ble_uuid_t) {
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_BOOT_KEYBOARD_OUTPUT_REPORT_CHAR,
		},
		.p_attr_md = &(ble_gatts_attr_md_t) {
			.vloc = BLE_GATTS_VLOC_STACK,
			.rd_auth = true,
			.read_perm = gap_conn_sec_mode_from_u8(
				CONFIG_BLE_HIDS_BOOT_KEYBOARD_OUTPUT_CHAR_READ_SEC_MODE),
			.write_perm = gap_conn_sec_mode_from_u8(
				CONFIG_BLE_HIDS_BOOT_KEYBOARD_OUTPUT_CHAR_WRITE_SEC_MODE),
		},
		.max_len = BLE_HIDS_BOOT_KB_INPUT_REP_MAX_SIZE,
	};

	/* Add characteristic */
	nrf_err = sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &char_params,
					      &hids->boot_kb_outp_rep_handles);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to add Boot Keyboard output char, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}
#endif

#if CONFIG_BLE_HIDS_BOOT_MOUSE
static uint32_t boot_mouse_input_report_char_add(struct ble_hids *hids)
{
	uint32_t nrf_err;
	const ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
			.notify = true,
			/* for debug */
			.write = true,
		},
		.p_cccd_md = &(ble_gatts_attr_md_t) {
			.vloc = BLE_GATTS_VLOC_STACK,
			/* Use the same read permission as the characteristic's */
			.read_perm = gap_conn_sec_mode_from_u8(
				CONFIG_BLE_HIDS_BOOT_MOUSE_INPUT_CHAR_READ_SEC_MODE),
			.write_perm = gap_conn_sec_mode_from_u8(
				CONFIG_BLE_HIDS_BOOT_MOUSE_INPUT_CCCD_WRITE_SEC_MODE),
		},
	};
	const ble_gatts_attr_t char_params = {
		.p_uuid = &(ble_uuid_t) {
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_BOOT_MOUSE_INPUT_REPORT_CHAR,
		},
		.p_attr_md = &(ble_gatts_attr_md_t) {
			.vloc = BLE_GATTS_VLOC_STACK,
			.rd_auth = true,
			.read_perm = gap_conn_sec_mode_from_u8(
				CONFIG_BLE_HIDS_BOOT_MOUSE_INPUT_CHAR_READ_SEC_MODE),
			/* for debug */
			.write_perm = BLE_GAP_CONN_SEC_MODE_OPEN,
		},
		.max_len = BLE_HIDS_BOOT_KB_INPUT_REP_MAX_SIZE,
	};

	/* Add characteristic */
	nrf_err = sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &char_params,
						  &hids->boot_mouse_inp_rep_handles);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to add GATT characteristic, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}
#endif

static uint32_t hid_information_char_add(struct ble_hids *hids,
					 const struct ble_hids_config *hids_config)
{
	uint32_t nrf_err;
	const ble_gatts_char_md_t char_md = {
		.char_props = { .read = true, },
	};
	const ble_gatts_attr_t char_params = {
		.p_uuid = &(ble_uuid_t){
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_HID_INFORMATION_CHAR,
		},
		.p_attr_md = &(ble_gatts_attr_md_t) {
			.vloc = BLE_GATTS_VLOC_STACK,
			.read_perm = gap_conn_sec_mode_from_u8(
				CONFIG_BLE_HIDS_INFORMATION_CHAR_READ_SEC_MODE)
		},
		.init_len = sizeof(hids_config->hid_information),
		.max_len = sizeof(hids_config->hid_information),
		.p_value = (uint8_t *)&hids_config->hid_information,
	};

	/* Add characteristic */
	nrf_err = sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &char_params,
						  &hids->hid_information_handles);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to add GATT characteristic, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static uint32_t hid_control_point_char_add(struct ble_hids *hids)
{
	uint32_t nrf_err;
	const ble_gatts_char_md_t char_md = {
		.char_props = { .write_wo_resp = true },
	};
	const ble_gatts_attr_t char_params = {
		.p_uuid = &(ble_uuid_t){
			.type = BLE_UUID_TYPE_BLE,
			.uuid = BLE_UUID_HID_CONTROL_POINT_CHAR,
		},
		.p_attr_md = &(ble_gatts_attr_md_t){
			.vloc = BLE_GATTS_VLOC_STACK,
			.write_perm = gap_conn_sec_mode_from_u8(
				CONFIG_BLE_HIDS_CONTROL_POINT_CHAR_WRITE_SEC_MODE),
		},
		.init_len = sizeof(uint8_t),
		.max_len = sizeof(uint8_t),
		.p_value = &(uint8_t){CONTROL_POINT_EXIT_SUSPEND},
	};

	/* Add characteristic */
	nrf_err = sd_ble_gatts_characteristic_add(hids->service_handle, &char_md, &char_params,
						  &hids->control_point_handles);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to add GATT characteristic, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static uint32_t input_report_char_add(struct ble_hids *hids,
				      const struct ble_hids_config *hids_config)
{
	uint32_t nrf_err;

	if ((hids_config->input_report_count == 0 && hids_config->input_report != NULL) ||
	    (hids_config->input_report_count != 0 && hids_config->input_report == NULL)) {
		return NRF_ERROR_INVALID_PARAM;
	}

	for (size_t i = 0; i < hids_config->input_report_count; i++) {
		const struct ble_hids_report_config *report = &hids_config->input_report[i];
		const ble_gatt_char_props_t properties = {
			.read = true,
			.notify = true,
			.write = !ble_gap_conn_sec_mode_equal(
					&report->sec.write, &BLE_GAP_CONN_SEC_MODE_NO_ACCESS),
		};

		nrf_err = rep_char_add(hids, &hids->input_report[i], report, &properties);
		if (nrf_err != NRF_SUCCESS) {
			return nrf_err;
		}
	}

	hids->input_report_count = hids_config->input_report_count;

	LOG_DBG("Input report characteristics added");

	return NRF_SUCCESS;
}

static uint32_t output_report_char_add(struct ble_hids *hids,
				       const struct ble_hids_config *hids_config)
{
	uint32_t nrf_err;

	if ((hids_config->output_report_count == 0 && hids_config->output_report != NULL) ||
	    (hids_config->output_report_count != 0 && hids_config->output_report == NULL)) {
		return NRF_ERROR_INVALID_PARAM;
	}

	for (size_t i = 0; i < hids_config->output_report_count; i++) {
		const struct ble_hids_report_config *report = &hids_config->output_report[i];
		const ble_gatt_char_props_t properties = {
			.read = true,
			.write = true,
			.write_wo_resp = true,
		};

		nrf_err = rep_char_add(hids, &hids->output_report[i], report, &properties);
		if (nrf_err != NRF_SUCCESS) {
			return nrf_err;
		}
	}

	hids->output_report_count = hids_config->output_report_count;

	LOG_DBG("Output report characteristics added");

	return NRF_SUCCESS;
}

static uint32_t feature_report_char_add(struct ble_hids *hids,
					const struct ble_hids_config *hids_config)
{
	uint32_t nrf_err;

	if ((hids_config->feature_report_count == 0 && hids_config->feature_report != NULL) ||
	    (hids_config->feature_report_count != 0 && hids_config->feature_report == NULL)) {
		return NRF_ERROR_INVALID_PARAM;
	}

	for (size_t i = 0; i < hids_config->feature_report_count; i++) {
		const struct ble_hids_report_config *report = &hids_config->feature_report[i];
		const ble_gatt_char_props_t properties = {
			.read = true,
			.write = true,
		};

		nrf_err = rep_char_add(hids, &hids->feature_report[i], report, &properties);
		if (nrf_err != NRF_SUCCESS) {
			return nrf_err;
		}
	}

	hids->feature_report_count = hids_config->feature_report_count;

	LOG_DBG("Feature report characteristics added");

	return NRF_SUCCESS;
}

static uint32_t includes_add(struct ble_hids *hids, const struct ble_hids_config *hids_config)
{
	uint32_t nrf_err;
	uint16_t unused_handle;

	for (size_t i = 0; i < hids_config->included_services_count; i++) {
		nrf_err = sd_ble_gatts_include_add(hids->service_handle,
						   hids_config->included_services_array[i],
						   &unused_handle);
		if (nrf_err != NRF_SUCCESS) {
			return nrf_err;
		}
	}

	return NRF_SUCCESS;
}

uint32_t ble_hids_init(struct ble_hids *hids, const struct ble_hids_config *hids_config)
{
	uint32_t nrf_err;
	ble_uuid_t ble_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE,
	};

	if (!hids || !hids_config) {
		return NRF_ERROR_NULL;
	}
	if ((hids_config->input_report_count > CONFIG_BLE_HIDS_MAX_INPUT_REP) ||
	    (hids_config->output_report_count > CONFIG_BLE_HIDS_MAX_OUTPUT_REP) ||
	    (hids_config->feature_report_count > CONFIG_BLE_HIDS_MAX_FEATURE_REP)) {
		return NRF_ERROR_INVALID_PARAM;
	}

	/* Add service */
	nrf_err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid,
					   &hids->service_handle);
	if (nrf_err != NRF_SUCCESS) {
		return nrf_err;
	}

	/* Add includes */
	nrf_err = includes_add(hids, hids_config);
	if (nrf_err != NRF_SUCCESS) {
		return nrf_err;
	}

	if (IS_ENABLED(CONFIG_BLE_HIDS_BOOT_KEYBOARD) || IS_ENABLED(CONFIG_BLE_HIDS_BOOT_MOUSE)) {
		/* Add Protocol Mode characteristic */
		nrf_err = protocol_mode_char_add(hids);
		if (nrf_err != NRF_SUCCESS) {
			return nrf_err;
		}
	}

	/* Add Input Report characteristics (if any) */
	nrf_err = input_report_char_add(hids, hids_config);
	if (nrf_err != NRF_SUCCESS) {
		return nrf_err;
	}

	/* Add Output Report characteristics (if any) */
	nrf_err = output_report_char_add(hids, hids_config);
	if (nrf_err != NRF_SUCCESS) {
		return nrf_err;
	}

	/* Add Feature Report characteristic (if any) */
	nrf_err = feature_report_char_add(hids, hids_config);
	if (nrf_err != NRF_SUCCESS) {
		return nrf_err;
	}

	/* Add Report Map characteristic */
	nrf_err = report_map_char_add(hids, hids_config);
	if (nrf_err != NRF_SUCCESS) {
		return nrf_err;
	}

#if CONFIG_BLE_HIDS_BOOT_KEYBOARD
	nrf_err = boot_kb_input_report_char_add(hids);
	if (nrf_err != NRF_SUCCESS) {
		return nrf_err;
	}

	nrf_err = boot_kb_output_report_char_add(hids);
	if (nrf_err != NRF_SUCCESS) {
		return nrf_err;
	}
#endif

#if CONFIG_BLE_HIDS_BOOT_MOUSE
	nrf_err = boot_mouse_input_report_char_add(hids);
	if (nrf_err != NRF_SUCCESS) {
		return nrf_err;
	}
#endif

	/* Add HID Information characteristic */
	nrf_err = hid_information_char_add(hids, hids_config);
	if (nrf_err != NRF_SUCCESS) {
		return nrf_err;
	}

	/* Add HID Control Point characteristic */
	nrf_err = hid_control_point_char_add(hids);
	if (nrf_err != NRF_SUCCESS) {
		return nrf_err;
	}

	LOG_INF("BLE HID service initialized");
	LOG_DBG("Size of HID client context is %u bytes", sizeof(struct ble_hids_context));

	hids->evt_handler = hids_config->evt_handler;

	return NRF_SUCCESS;
}

uint32_t ble_hids_event_handler_set(struct ble_hids *hids, ble_hids_evt_handler_t handler)
{
	if (!hids || !handler) {
		return NRF_ERROR_NULL;
	}

	hids->evt_handler = handler;

	return NRF_SUCCESS;
}

uint32_t ble_hids_inp_rep_send(struct ble_hids *hids, uint16_t conn_handle, uint8_t rep_index,
			       const void *data, const uint16_t len)
{
	uint32_t nrf_err;
	void *rep_data;
	struct ble_hids_context *ctx;

	if (!hids || !data) {
		return NRF_ERROR_NULL;
	}

	ctx = ble_hids_context_get(conn_handle);
	if (!ctx) {
		return NRF_ERROR_NOT_FOUND;
	}

	if (rep_index > ARRAY_SIZE(ctx->input_reports)) {
		LOG_ERR("Invalid report index %u (total: %u)",
			rep_index, ARRAY_SIZE(ctx->input_reports));
		return NRF_ERROR_INVALID_PARAM;
	}
	/** TODO: is this caught from sd_ble_gatts_hvx() ? */
	if (len > hids->input_report[rep_index].max_len) {
		LOG_ERR("Report is too big to fit in host data");
		return NRF_ERROR_DATA_SIZE;
	}

	/* Store the new report data in host's context */
	rep_data = ctx->input_reports[rep_index].data;
	memcpy(rep_data, data, len);

	const ble_gatts_hvx_params_t hvx = {
		.handle = hids->input_report[rep_index].char_handles.value_handle,
		.type = BLE_GATT_HVX_NOTIFICATION,
		.p_len = &(uint16_t){len},
		.p_data = data,
	};

	nrf_err = sd_ble_gatts_hvx(conn_handle, &hvx);
	if ((nrf_err == NRF_SUCCESS) && (*hvx.p_len != len)) {
		LOG_ERR("Wrong notification data size");
		return NRF_ERROR_INVALID_PARAM;
	}
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to send notification, nrf_error %#x", nrf_err);
		return NRF_ERROR_INVALID_PARAM;
	}

	LOG_DBG("Input report notification sent");

	return NRF_SUCCESS;
}

uint32_t ble_hids_boot_kb_inp_rep_send(struct ble_hids *hids, uint16_t conn_handle,
				       const struct ble_hids_boot_keyboard_input_report *report)
{
#if CONFIG_BLE_HIDS_BOOT_KEYBOARD
	uint32_t nrf_err;
	struct ble_hids_context *ctx;

	if (!hids || !report) {
		return NRF_ERROR_NULL;
	}

	ctx = ble_hids_context_get(conn_handle);
	if (!ctx) {
		return NRF_ERROR_NOT_FOUND;
	}

	/* Copy into context */
	memcpy(ctx->boot_key_in_rep, report, sizeof(ctx->boot_key_in_rep));

	const ble_gatts_hvx_params_t hvx = {
		.handle = hids->boot_kb_inp_rep_handles.value_handle,
		.type = BLE_GATT_HVX_NOTIFICATION,
		.p_len = &(uint16_t){sizeof(ctx->boot_key_in_rep)},
		.p_data = ctx->boot_key_in_rep,
	};

	nrf_err = sd_ble_gatts_hvx(conn_handle, &hvx);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to send notification, nrf_error %#x", nrf_err);
		return NRF_ERROR_INVALID_PARAM;
	}

	LOG_DBG("Boot keyboard input report sent");

	return NRF_SUCCESS;
#else
	return NRF_ERROR_FORBIDDEN;
#endif
}

uint32_t ble_hids_boot_mouse_inp_rep_send(struct ble_hids *hids, uint16_t conn_handle,
					  const struct ble_hids_boot_mouse_input_report *report)
{
#if CONFIG_BLE_HIDS_BOOT_MOUSE
	uint32_t nrf_err;
	struct ble_hids_context *ctx;

	if (!hids || !report) {
		return NRF_ERROR_NULL;
	}

	ctx = ble_hids_context_get(conn_handle);
	if (!ctx) {
		return NRF_ERROR_NOT_FOUND;
	}

	memcpy(&ctx->boot_mouse_in_rep, report, sizeof(ctx->boot_mouse_in_rep));

	const ble_gatts_hvx_params_t hvx = {
		.handle = hids->boot_mouse_inp_rep_handles.value_handle,
		.type = BLE_GATT_HVX_NOTIFICATION,
		.p_len = &(uint16_t){BLE_HIDS_BOOT_MOUSE_INPUT_REP_MAX_SIZE},
		.p_data = ctx->boot_mouse_in_rep,
	};

	nrf_err = sd_ble_gatts_hvx(conn_handle, &hvx);
	if (nrf_err != NRF_SUCCESS) {
		LOG_ERR("Failed to send notification, nrf_error %#x", nrf_err);
		return NRF_ERROR_INVALID_PARAM;
	}

	LOG_DBG("Boot mouse input report sent");

	return NRF_SUCCESS;
#else
	return NRF_ERROR_FORBIDDEN;
#endif
}

uint32_t ble_hids_outp_rep_get(struct ble_hids *hids, uint8_t rep_index, uint16_t len,
			       uint8_t offset, uint16_t conn_handle, uint8_t *outp_rep)
{
	void *src;
	struct ble_hids_context *ctx;

	if (!hids || !outp_rep) {
		return NRF_ERROR_NULL;
	}
	if (rep_index >= ARRAY_SIZE(hids->output_report)) {
		return NRF_ERROR_INVALID_PARAM;
	}

	ctx = ble_hids_context_get(conn_handle);
	if (!ctx) {
		return NRF_ERROR_NOT_FOUND;
	}

	if (offset + len > hids->output_report[rep_index].max_len) {
		LOG_ERR("Output buffer too small for report data");
		return NRF_ERROR_DATA_SIZE;
	}

	/* Copy the report data into user provided buffer */
	src = ctx->output_reports[rep_index].data;
	memcpy(outp_rep, (uint8_t *)src + offset, len);

	return NRF_SUCCESS;
}
