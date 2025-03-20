/*
 * Copyright (c) 2012 - 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <bluetooth/services/ble_bas.h>
#include <bluetooth/services/common.h>
#include <bluetooth/services/uuid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_bas, CONFIG_BLE_BAS_LOG_LEVEL);

static int battery_level_char_add(struct ble_bas *bas, const struct ble_bas_config *cfg)
{
	int err;
	ble_uuid_t char_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_BATTERY_LEVEL_CHAR,
	};
	ble_gatts_attr_md_t cccd_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.write_perm = cfg->cccd_wr_sec,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
			.notify = bas->can_notify,
		},
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.read_perm = cfg->batt_rd_sec,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = &bas->battery_level,
		.init_len = sizeof(bas->battery_level),
		.max_len = sizeof(bas->battery_level),
	};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);

	/* Set CCCD metadata if characteristic value can be notified. */
	if (bas->can_notify) {
		char_md.p_cccd_md = &cccd_md;
	}

	/* Add characteristic */
	err = sd_ble_gatts_characteristic_add(bas->service_handle, &char_md, &attr_char_value,
					      &bas->battery_level_handles);
	if (err) {
		LOG_ERR("Failed to add GATT characteristic, nrf_error %#x", err);
		return err;
	}

	return 0;
}

static int report_reference_descriptor_add(struct ble_bas *bas, const struct ble_bas_config *cfg)
{
	int err;
	ble_uuid_t desc_uuid = {
		.type = BLE_UUID_TYPE_BLE,
		.uuid = BLE_UUID_REPORT_REF_DESCR,
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.read_perm = cfg->report_ref_rd_sec,
	};
	uint8_t encoded_report_ref[sizeof(uint16_t)];
	ble_gatts_attr_t descr_params = {
		.p_uuid = &desc_uuid,
		.p_attr_md = &attr_md,
		.init_len = sizeof(uint16_t),
		.max_len = sizeof(uint16_t),
		.p_value = encoded_report_ref,
	};

	encoded_report_ref[0] = cfg->report_ref->report_id;
	encoded_report_ref[1] = cfg->report_ref->report_type;

	/* Add the descriptor */
	err = sd_ble_gatts_descriptor_add(bas->battery_level_handles.value_handle, &descr_params,
					  &bas->report_ref_handle);
	if (err) {
		LOG_ERR("Failed to add GATT report reference descriptor, nrf_error %#x", err);
		return -EINVAL;
	}

	return 0;
}

static void on_write(struct ble_bas *bas, const ble_gatts_evt_t *gatts_evt)
{
	struct ble_bas_evt bas_evt;

	if (!bas->can_notify || !bas->evt_handler) {
		return;
	}

	if ((gatts_evt->params.write.handle != bas->battery_level_handles.cccd_handle) ||
	    (gatts_evt->params.write.len != 2)) {
		/* Nothing to do */
		return;
	}

	bas_evt.conn_handle = gatts_evt->conn_handle;
	if (is_notification_enabled(gatts_evt->params.write.data)) {
		bas_evt.evt_type = BLE_BAS_EVT_NOTIFICATION_ENABLED;
	} else {
		bas_evt.evt_type = BLE_BAS_EVT_NOTIFICATION_DISABLED;
	}

	LOG_INF("Battery level notifications %sabled for peer %#x",
		(bas_evt.evt_type == BLE_BAS_EVT_NOTIFICATION_ENABLED ? "en" : "dis"),
		gatts_evt->conn_handle);

	bas->evt_handler(bas, &bas_evt);
}

void ble_bas_on_ble_evt(const ble_evt_t *ble_evt, void *bas_instance)
{
	__ASSERT(ble_evt, "BLE event is NULL");
	__ASSERT(bas_instance, "BAS instance is NULL");

	switch (ble_evt->header.evt_id) {
	case BLE_GATTS_EVT_WRITE:
		on_write(bas_instance, &ble_evt->evt.gatts_evt);
		break;

	default:
		break;
	}
}

int ble_bas_init(struct ble_bas *bas, const struct ble_bas_config *cfg)
{
	int err;
	ble_uuid_t ble_uuid;

	if (bas == NULL || cfg == NULL) {
		return -EFAULT;
	}

	/* Initialize service structure */
	bas->can_notify = cfg->can_notify;
	bas->evt_handler = cfg->evt_handler;
	bas->battery_level = cfg->battery_level;

	BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_BATTERY_SERVICE);

	/* Add service */
	err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid,
				       &bas->service_handle);
	if (err) {
		LOG_ERR("Failed to add battery service, nrf_error %#x", err);
		return -EINVAL;
	}

	/* Add battery level characteristic */
	err =  battery_level_char_add(bas, cfg);
	if (err) {
		return -EINVAL;
	}

	/* Add reference descriptor if present */
	if (cfg->report_ref != NULL) {
		err = report_reference_descriptor_add(bas, cfg);
		if (err) {
			return -EINVAL;
		}
	}

	LOG_DBG("Battery service initialized");

	return 0;
}

int ble_bas_battery_level_update(struct ble_bas *bas, uint16_t conn_handle, uint8_t battery_level)
{
	int err;
	ble_gatts_value_t gatts_value = {0};
	ble_gatts_hvx_params_t hvx = {0};

	if (!bas) {
		return -EFAULT;
	}

	if (bas->battery_level == battery_level) {
		/* Nothing to do */
		return 0;
	}

	/* Update database */
	gatts_value.len = sizeof(uint8_t);
	gatts_value.p_value = &battery_level;

	err = sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID,
				     bas->battery_level_handles.value_handle, &gatts_value);
	if (err) {
		LOG_ERR("Failed to update battery level, nrf_error %#x", err);
		return -EINVAL;
	}

	LOG_INF("Battery level: %d%%", battery_level);
	bas->battery_level = battery_level;

	if (!bas->can_notify) {
		/* We are done */
		return 0;
	}

	/* Notify */
	hvx.handle = bas->battery_level_handles.value_handle;
	hvx.type = BLE_GATT_HVX_NOTIFICATION;
	hvx.offset = gatts_value.offset;
	hvx.p_len = &gatts_value.len;
	hvx.p_data = gatts_value.p_value;

	err = sd_ble_gatts_hvx(conn_handle, &hvx);
	switch (err) {
	case NRF_SUCCESS:
		return 0;
	case BLE_ERROR_INVALID_CONN_HANDLE:
		return -ENOTCONN;
	case NRF_ERROR_INVALID_STATE:
		return -EPIPE;
	default:
		LOG_ERR("Failed to notify battery level, nrf_error %#x", err);
		return -EINVAL;
	}
}

int ble_bas_battery_level_notify(struct ble_bas *bas, uint16_t conn_handle)
{
	int err;
	ble_gatts_hvx_params_t hvx = {0};
	uint16_t len = sizeof(uint8_t);

	if (!bas) {
		return -EFAULT;
	}
	if (!bas->can_notify) {
		return -EINVAL;
	}

	hvx.handle = bas->battery_level_handles.value_handle;
	hvx.type = BLE_GATT_HVX_NOTIFICATION;
	hvx.offset = 0;
	hvx.p_len = &len;
	hvx.p_data = &bas->battery_level;

	err = sd_ble_gatts_hvx(conn_handle, &hvx);
	switch (err) {
	case NRF_SUCCESS:
		return 0;
	case BLE_ERROR_INVALID_CONN_HANDLE:
		return -ENOTCONN;
	case NRF_ERROR_INVALID_STATE:
		return -EPIPE;
	default:
		LOG_ERR("Failed to notify battery level, nrf_error %#x", err);
		return -EINVAL;
	}
}
