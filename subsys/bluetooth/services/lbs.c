/*
 * Copyright (c) 2013 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <zephyr/logging/log.h>
#include <bluetooth/services/ble_lbs.h>
#include <bluetooth/services/common.h>
#include <bluetooth/services/uuid.h>

LOG_MODULE_REGISTER(ble_lbs, CONFIG_BLE_LBS_LOG_LEVEL);

static void on_write(struct ble_lbs *lbs, const ble_evt_t *ble_evt)
{
	if (!lbs->led_write_handler) {
		return;
	}

	ble_gatts_evt_t const *gatts_evt = &ble_evt->evt.gatts_evt;

	if ((gatts_evt->params.write.handle != lbs->led_char_handles.cccd_handle) ||
	    (gatts_evt->params.write.len != 2)) {
		/* Nothing to do */
		return;
	}

	ble_gap_evt_t const *gap_evt = &ble_evt->evt.gap_evt;

	lbs->led_write_handler(gap_evt->conn_handle, lbs, gatts_evt->params.write.data[0]);
}

void ble_lbs_on_ble_evt(const ble_evt_t *ble_evt, void *lbs_instance)
{
	__ASSERT(ble_evt, "BLE event is NULL");
	__ASSERT(lbs_instance, "LBS instance is NULL");

	switch (ble_evt->header.evt_id) {
	case BLE_GATTS_EVT_WRITE:
		on_write(lbs_instance, ble_evt);
		break;
	default:
		break;
	}
}

int ble_lbs_init(struct ble_lbs *lbs, const struct ble_lbs_config *cfg)
{
	int err;
	ble_uuid_t ble_uuid;

	if (!lbs || !cfg) {
		return -EFAULT;
	}

	/* Initialize service structure. */
	lbs->led_write_handler = cfg->led_write_handler;

	ble_uuid128_t base_uuid = {BLE_LBS_UUID_BASE};

	err = sd_ble_uuid_vs_add(&base_uuid, &lbs->uuid_type);
	if (err != NRF_SUCCESS) {
		LOG_ERR("Failed to add vendor UUID, nrf_error %#x", err);
		return -EINVAL;
	}

	ble_uuid = (ble_uuid_t) {
		.type = lbs->uuid_type,
		.uuid = BLE_LBS_UUID_SERVICE,
	};

	/* Add service. */
	err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid,
				       &lbs->service_handle);
	if (err != NRF_SUCCESS) {
		LOG_ERR("Failed to add GATT service, nrf_error %#x", err);
		return -EINVAL;
	}

	/* Add Button characteristic. */

	ble_uuid_t char_uuid = {
		.type = lbs->uuid_type,
		.uuid = BLE_LBS_UUID_BUTTON_CHAR,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
			.notify = true,
		},
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.init_len = sizeof(uint8_t),
		.max_len = sizeof(uint8_t),
	};
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

	/* Add characteristic */
	err = sd_ble_gatts_characteristic_add(lbs->service_handle, &char_md, &attr_char_value,
					      &lbs->button_char_handles);
	if (err) {
		LOG_ERR("Failed to add button GATT characteristic, nrf_error %#x", err);
		return err;
	}

	/* Add LED characteristic. */
	memset(&char_uuid, 0, sizeof(ble_uuid_t));
	memset(&char_md, 0, sizeof(ble_gatts_char_md_t));
	memset(&attr_md, 0, sizeof(ble_gatts_attr_md_t));
	memset(&attr_char_value, 0, sizeof(ble_gatts_attr_t));

	char_uuid = (ble_uuid_t) {
		.type = lbs->uuid_type,
		.uuid = BLE_LBS_UUID_LED_CHAR,
	};
	char_md = (ble_gatts_char_md_t) {
		.char_props = {
			.read = true,
			.write = true,
		},
	};
	attr_md = (ble_gatts_attr_md_t) {
		.vloc = BLE_GATTS_VLOC_STACK,
	};
	attr_char_value = (ble_gatts_attr_t) {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.init_len = sizeof(uint8_t),
		.max_len = sizeof(uint8_t),
	};
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

	/* Add characteristic */
	err = sd_ble_gatts_characteristic_add(lbs->service_handle, &char_md, &attr_char_value,
					      &lbs->led_char_handles);
	if (err) {
		LOG_ERR("Failed to add LED GATT characteristic, nrf_error %#x", err);
		return err;
	}

	return 0;
}

int ble_lbs_on_button_change(struct ble_lbs *lbs, uint16_t conn_handle, uint8_t button_state)
{
	int err;

	uint16_t len = sizeof(button_state);

	if (!lbs) {
		return -EFAULT;
	}

	ble_gatts_hvx_params_t hvx = {
		.type = BLE_GATT_HVX_NOTIFICATION,
		.handle = lbs->button_char_handles.value_handle,
		.p_data = &button_state,
		.p_len = &len,
	};

	err = sd_ble_gatts_hvx(conn_handle, &hvx);
	if (err) {
		LOG_ERR("Failed to notify button change, nrf_error %#x", err);
		return -EINVAL;
	}

	return 0;
}
