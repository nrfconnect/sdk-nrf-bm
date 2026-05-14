/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <ble_gatt.h>
#include <ble_gap.h>
#include <ble_gatts.h>
#include <nrf_error.h>

#include <bm/bluetooth/ble_common.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/services/ble_mds.h>

#include <memfault/config.h>
#include <memfault/core/data_packetizer.h>
#include <memfault/core/log.h>
#include <memfault/core/platform/device_info.h>
#include <memfault/http/http_client.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(ble_mds, CONFIG_BLE_MDS_LOG_LEVEL);

#if defined(CONFIG_MEMFAULT_NCS_PROJECT_KEY)
#define BLE_MDS_PROJECT_KEY CONFIG_MEMFAULT_NCS_PROJECT_KEY
#elif defined(CONFIG_MEMFAULT_PROJECT_KEY)
#define BLE_MDS_PROJECT_KEY CONFIG_MEMFAULT_PROJECT_KEY
#elif defined(MEMFAULT_PROJECT_KEY)
#define BLE_MDS_PROJECT_KEY MEMFAULT_PROJECT_KEY
#else
#define BLE_MDS_PROJECT_KEY ""
#endif

BUILD_ASSERT(sizeof(BLE_MDS_PROJECT_KEY) > 1,
	     "Set CONFIG_MEMFAULT_NCS_PROJECT_KEY or CONFIG_MEMFAULT_PROJECT_KEY for MDS");

#define BLE_MDS_ATT_OVERHEAD (ATT_OPCODE_LEN + ATT_HANDLE_LEN)
#define BLE_MDS_SEQ_NUM_COUNT 32
#define BLE_MDS_SUPPORTED_FEATURES 0x00
#define BLE_MDS_URI_BASE                                                                         \
	MEMFAULT_HTTP_APIS_DEFAULT_SCHEME "://" MEMFAULT_HTTP_CHUNKS_API_HOST "/api/v0/chunks/"
#define BLE_MDS_AUTH_PREFIX MEMFAULT_HTTP_PROJECT_KEY_HEADER ":"

static uint8_t supported_features = BLE_MDS_SUPPORTED_FEATURES;
static uint8_t data_export_mode;
static char device_id[MEMFAULT_DEVICE_INFO_MAX_STRING_SIZE + 1];
static char data_uri[CONFIG_BLE_MDS_DATA_URI_MAX_LEN];
static char authorization[sizeof(BLE_MDS_AUTH_PREFIX) + sizeof(BLE_MDS_PROJECT_KEY) - 1];

static uint32_t readable_char_add(uint16_t service_handle, uint8_t uuid_type, uint16_t uuid,
				  const ble_gap_conn_sec_mode_t *read_perm, const void *value,
				  uint16_t len, uint16_t max_len,
				  ble_gatts_char_handles_t *handles)
{
	ble_uuid_t char_uuid = {
		.type = uuid_type,
		.uuid = uuid,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
		},
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.vlen = true,
		.read_perm = *read_perm,
	};
	ble_gatts_attr_t attr = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = (uint8_t *)value,
		.init_len = len,
		.max_len = max_len,
	};

	return sd_ble_gatts_characteristic_add(service_handle, &char_md, &attr, handles);
}

static uint32_t data_export_char_add(struct ble_mds *mds, const struct ble_mds_config *cfg)
{
	ble_uuid_t char_uuid = {
		.type = mds->uuid_type,
		.uuid = BLE_UUID_MDS_DATA_EXPORT_CHAR,
	};
	ble_gatts_attr_md_t cccd_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.read_perm = BLE_GAP_CONN_SEC_MODE_OPEN,
		.write_perm = cfg->sec_mode.data_export_char.cccd_write,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.write = true,
			.notify = true,
		},
		.p_cccd_md = &cccd_md,
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.vlen = true,
		.write_perm = cfg->sec_mode.data_export_char.write,
		.wr_auth = true,
	};
	ble_gatts_attr_t attr = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = &data_export_mode,
		.init_len = sizeof(data_export_mode),
		.max_len = sizeof(mds->pending_payload),
	};

	return sd_ble_gatts_characteristic_add(mds->service_handle, &char_md, &attr,
					       &mds->data_export_handles);
}

static uint32_t values_prepare(void)
{
	sMemfaultDeviceInfo info;
	int len;

	memfault_platform_get_device_info(&info);
	if ((info.device_serial == NULL) || (info.device_serial[0] == '\0')) {
		LOG_ERR("Memfault device serial is empty");
		return NRF_ERROR_NOT_FOUND;
	}

	len = snprintf(device_id, sizeof(device_id), "%s", info.device_serial);
	if ((len < 0) || ((size_t)len >= sizeof(device_id))) {
		LOG_ERR("Memfault device serial too long");
		return NRF_ERROR_DATA_SIZE;
	}

	len = snprintf(data_uri, sizeof(data_uri), "%s%s", BLE_MDS_URI_BASE, device_id);
	if ((len < 0) || ((size_t)len >= sizeof(data_uri))) {
		LOG_ERR("MDS data URI too long");
		return NRF_ERROR_DATA_SIZE;
	}

	len = snprintf(authorization, sizeof(authorization), "%s%s", BLE_MDS_AUTH_PREFIX,
		       BLE_MDS_PROJECT_KEY);
	if ((len < 0) || ((size_t)len >= sizeof(authorization))) {
		LOG_ERR("MDS authorization value too long");
		return NRF_ERROR_DATA_SIZE;
	}

	return NRF_SUCCESS;
}

static bool notification_enabled(struct ble_mds *mds, uint16_t conn_handle)
{
	uint16_t cccd_value;
	ble_gatts_value_t gatts_value = {
		.p_value = (uint8_t *)&cccd_value,
		.len = sizeof(cccd_value),
	};
	uint32_t nrf_err;

	nrf_err = sd_ble_gatts_value_get(conn_handle, mds->data_export_handles.cccd_handle,
					 &gatts_value);
	if (nrf_err) {
		return false;
	}

	return is_notification_enabled((const uint8_t *)&cccd_value);
}

static void subscription_disable(struct ble_mds *mds)
{
	mds->subscriber_active = false;
	mds->streaming_enabled = false;
	mds->tx_blocked = false;
	mds->hvx_pending = false;
	mds->conn_handle = BLE_CONN_HANDLE_INVALID;
	mds->seq_num = 0;
	mds->pending_len = 0;
	mds->next_log_collection_ms = 0;
}

static void cccd_write_handle(struct ble_mds *mds, uint16_t conn_handle,
			      const ble_gatts_evt_write_t *write)
{
	if ((write->offset != 0) || (write->len != sizeof(uint16_t))) {
		return;
	}

	if (is_notification_enabled(write->data)) {
		if (mds->subscriber_active && (mds->conn_handle != conn_handle)) {
			LOG_WRN("Ignoring second MDS subscriber on handle %#x", conn_handle);
			return;
		}

		mds->subscriber_active = true;
		mds->conn_handle = conn_handle;
		mds->tx_blocked = false;
		LOG_INF("MDS notifications enabled");
	} else if (mds->conn_handle == conn_handle) {
		subscription_disable(mds);
		LOG_INF("MDS notifications disabled");
	}
}

static uint16_t data_export_write_apply(struct ble_mds *mds, uint16_t conn_handle,
					const ble_gatts_evt_write_t *write)
{
	uint8_t mode;

	if (write->offset != 0) {
		return BLE_GATT_STATUS_ATTERR_INVALID_OFFSET;
	}

	if (write->len != sizeof(mode)) {
		return BLE_GATT_STATUS_ATTERR_INVALID_ATT_VAL_LENGTH;
	}

	if (!mds->subscriber_active || (mds->conn_handle != conn_handle) ||
	    !notification_enabled(mds, conn_handle)) {
		return BLE_GATT_STATUS_ATTERR_CPS_CCCD_CONFIG_ERROR;
	}

	mode = write->data[0];
	switch (mode) {
	case 0x00:
		mds->streaming_enabled = false;
		mds->tx_blocked = false;
		LOG_INF("MDS streaming disabled");
		return BLE_GATT_STATUS_SUCCESS;
	case 0x01:
		mds->streaming_enabled = true;
		mds->tx_blocked = false;
		mds->next_empty_poll_ms = 0;
		mds->next_log_collection_ms = 0;
		LOG_INF("MDS streaming enabled");
		return BLE_GATT_STATUS_SUCCESS;
	default:
		return BLE_GATT_STATUS_ATTERR_REQUEST_NOT_SUPPORTED;
	}
}

static bool conn_handle_matches(const struct ble_mds *mds, uint16_t conn_handle)
{
	return (mds->conn_handle == BLE_CONN_HANDLE_INVALID) || (mds->conn_handle == conn_handle);
}

static void rw_authorize_request_handle(struct ble_mds *mds, const ble_gatts_evt_t *gatts_evt)
{
	const ble_gatts_evt_rw_authorize_request_t *auth = &gatts_evt->params.authorize_request;
	ble_gatts_rw_authorize_reply_params_t reply = {
		.type = auth->type,
	};
	uint32_t nrf_err;
	uint16_t status;

	if ((auth->type != BLE_GATTS_AUTHORIZE_TYPE_WRITE) ||
	    (auth->request.write.handle != mds->data_export_handles.value_handle)) {
		return;
	}

	if (conn_handle_matches(mds, gatts_evt->conn_handle)) {
		status = data_export_write_apply(mds, gatts_evt->conn_handle, &auth->request.write);
	} else {
		status = BLE_GATT_STATUS_ATTERR_CPS_CCCD_CONFIG_ERROR;
	}

	reply.params.write.gatt_status = status;
	reply.params.write.update = (status == BLE_GATT_STATUS_SUCCESS);
	reply.params.write.offset = 0;
	reply.params.write.len = auth->request.write.len;
	reply.params.write.p_data = auth->request.write.data;

	nrf_err = sd_ble_gatts_rw_authorize_reply(gatts_evt->conn_handle, &reply);
	if (nrf_err) {
		LOG_ERR("MDS authorize reply failed, nrf_error %#x", nrf_err);
	}
}

static void write_handle(struct ble_mds *mds, const ble_gatts_evt_t *gatts_evt)
{
	const ble_gatts_evt_write_t *write = &gatts_evt->params.write;

	if ((write->handle != mds->data_export_handles.cccd_handle) &&
	    (write->handle != mds->data_export_handles.value_handle)) {
		return;
	}

	if (!conn_handle_matches(mds, gatts_evt->conn_handle)) {
		LOG_WRN("Ignoring MDS write from unexpected handle %#x", gatts_evt->conn_handle);
		return;
	}

	if (write->handle == mds->data_export_handles.cccd_handle) {
		cccd_write_handle(mds, gatts_evt->conn_handle, write);
	} else if (write->handle == mds->data_export_handles.value_handle) {
		(void)data_export_write_apply(mds, gatts_evt->conn_handle, write);
	}
}

static bool can_send(const struct ble_mds *mds)
{
	return mds->initialized && mds->subscriber_active && mds->streaming_enabled &&
	       !mds->tx_blocked && !mds->hvx_pending &&
	       (mds->conn_handle != BLE_CONN_HANDLE_INVALID);
}

static bool pending_payload_prepare(struct ble_mds *mds)
{
	uint16_t att_mtu;
	size_t chunk_len;
	uint32_t nrf_err;
	uint16_t value_len_max;

	if (mds->pending_len != 0) {
		return true;
	}

#if MEMFAULT_LOG_DATA_SOURCE_ENABLED && IS_ENABLED(CONFIG_BLE_MDS_TRIGGER_LOG_COLLECTION)
	const int64_t now = k_uptime_get();

	if (now >= mds->next_log_collection_ms) {
		memfault_log_trigger_collection();
		mds->next_log_collection_ms = now + CONFIG_BLE_MDS_LOG_COLLECTION_INTERVAL_MS;
	}
#endif

	if (!memfault_packetizer_data_available()) {
		const int64_t now = k_uptime_get();

		mds->next_empty_poll_ms = now + CONFIG_BLE_MDS_EMPTY_POLL_INTERVAL_MS;
		return false;
	}

	nrf_err = ble_conn_params_att_mtu_get(mds->conn_handle, &att_mtu);
	if (nrf_err) {
		LOG_WRN("Failed to get ATT MTU for MDS, nrf_error %#x", nrf_err);
		att_mtu = BLE_GATT_ATT_MTU_DEFAULT;
	}

	value_len_max = MIN((uint16_t)sizeof(mds->pending_payload),
			    (uint16_t)(att_mtu - BLE_MDS_ATT_OVERHEAD));
	if (value_len_max <= 1U) {
		return false;
	}

	mds->pending_payload[0] = mds->seq_num & 0x1f;
	chunk_len = value_len_max - 1U;
	if (!memfault_packetizer_get_chunk(&mds->pending_payload[1], &chunk_len)) {
		mds->next_empty_poll_ms = k_uptime_get() + CONFIG_BLE_MDS_EMPTY_POLL_INTERVAL_MS;
		return false;
	}

	mds->pending_len = (uint16_t)(chunk_len + 1U);
	return true;
}

void ble_mds_process(struct ble_mds *mds)
{
	ble_gatts_hvx_params_t hvx;
	uint16_t len;
	uint32_t nrf_err;

	if ((mds == NULL) || !can_send(mds)) {
		return;
	}

	if ((mds->pending_len == 0) && (k_uptime_get() < mds->next_empty_poll_ms)) {
		return;
	}

	if (!pending_payload_prepare(mds)) {
		return;
	}

	len = mds->pending_len;
	hvx = (ble_gatts_hvx_params_t){
		.handle = mds->data_export_handles.value_handle,
		.type = BLE_GATT_HVX_NOTIFICATION,
		.p_len = &len,
		.p_data = mds->pending_payload,
	};

	nrf_err = sd_ble_gatts_hvx(mds->conn_handle, &hvx);
	if (nrf_err == NRF_SUCCESS) {
		mds->pending_len = 0;
		mds->hvx_pending = true;
		mds->seq_num = (mds->seq_num + 1U) % BLE_MDS_SEQ_NUM_COUNT;
		return;
	}

	if ((nrf_err == NRF_ERROR_RESOURCES) || (nrf_err == NRF_ERROR_INVALID_STATE)) {
		mds->tx_blocked = true;
		return;
	}

	if (nrf_err == BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
		LOG_WRN("MDS notifications unavailable, nrf_error %#x", nrf_err);
		mds->tx_blocked = true;
		return;
	}

	LOG_ERR("MDS notification failed, nrf_error %#x", nrf_err);
	mds->pending_len = 0;
	mds->tx_blocked = true;
}

void ble_mds_on_ble_evt(const ble_evt_t *ble_evt, void *context)
{
	struct ble_mds *mds = context;

	if ((ble_evt == NULL) || (mds == NULL) || !mds->initialized) {
		return;
	}

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		mds->tx_blocked = false;
		mds->hvx_pending = false;
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		if (mds->conn_handle == ble_evt->evt.gap_evt.conn_handle) {
			subscription_disable(mds);
		}
		break;
	case BLE_GATTS_EVT_WRITE:
		write_handle(mds, &ble_evt->evt.gatts_evt);
		break;
	case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
		rw_authorize_request_handle(mds, &ble_evt->evt.gatts_evt);
		break;
	case BLE_GATTS_EVT_HVN_TX_COMPLETE:
		mds->tx_blocked = false;
		mds->hvx_pending = false;
		break;
	default:
		break;
	}
}

uint32_t ble_mds_init(struct ble_mds *mds, const struct ble_mds_config *cfg)
{
	uint32_t nrf_err;
	ble_uuid_t ble_uuid;
	ble_uuid128_t uuid_base = { .uuid128 = BLE_MDS_UUID_BASE };

	if ((mds == NULL) || (cfg == NULL)) {
		return NRF_ERROR_NULL;
	}

	memset(mds, 0, sizeof(*mds));
	mds->conn_handle = BLE_CONN_HANDLE_INVALID;

	nrf_err = values_prepare();
	if (nrf_err) {
		return nrf_err;
	}

	nrf_err = sd_ble_uuid_vs_add(&uuid_base, &mds->uuid_type);
	if (nrf_err) {
		LOG_ERR("Failed to add MDS UUID base, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	ble_uuid.type = mds->uuid_type;
	ble_uuid.uuid = BLE_UUID_MDS_SERVICE;

	nrf_err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid,
					   &mds->service_handle);
	if (nrf_err) {
		LOG_ERR("Failed to add MDS service, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	nrf_err = readable_char_add(mds->service_handle, mds->uuid_type,
				    BLE_UUID_MDS_SUPPORTED_FEATURES_CHAR,
				    &cfg->sec_mode.feature_char.read, &supported_features,
				    sizeof(supported_features), sizeof(supported_features),
				    &mds->supported_features_handles);
	if (nrf_err) {
		LOG_ERR("Failed to add MDS supported features, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	nrf_err = readable_char_add(mds->service_handle, mds->uuid_type,
				    BLE_UUID_MDS_DEVICE_IDENTIFIER_CHAR,
				    &cfg->sec_mode.device_id_char.read, device_id,
				    strlen(device_id), sizeof(device_id),
				    &mds->device_id_handles);
	if (nrf_err) {
		LOG_ERR("Failed to add MDS device identifier, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	nrf_err = readable_char_add(mds->service_handle, mds->uuid_type,
				    BLE_UUID_MDS_DATA_URI_CHAR,
				    &cfg->sec_mode.data_uri_char.read, data_uri,
				    strlen(data_uri), sizeof(data_uri),
				    &mds->data_uri_handles);
	if (nrf_err) {
		LOG_ERR("Failed to add MDS data URI, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	nrf_err = readable_char_add(mds->service_handle, mds->uuid_type,
				    BLE_UUID_MDS_AUTHORIZATION_CHAR,
				    &cfg->sec_mode.auth_char.read, authorization,
				    strlen(authorization), sizeof(authorization),
				    &mds->auth_handles);
	if (nrf_err) {
		LOG_ERR("Failed to add MDS authorization, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	nrf_err = data_export_char_add(mds, cfg);
	if (nrf_err) {
		LOG_ERR("Failed to add MDS data export, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	mds->initialized = true;
	LOG_INF("MDS initialized for device %s", device_id);

	return NRF_SUCCESS;
}

uint8_t ble_mds_service_uuid_type(const struct ble_mds *mds)
{
	if (mds == NULL) {
		return BLE_UUID_TYPE_UNKNOWN;
	}

	return mds->uuid_type;
}
