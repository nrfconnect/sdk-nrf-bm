/*
 * Copyright (c) 2013 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/__assert.h>
#include <zephyr/logging/log.h>
#include <bm/ble/services/ble_lbs.h>
#include <bm/ble/services/uuid.h>

LOG_MODULE_REGISTER(ble_lbs, CONFIG_BLE_LBS_LOG_LEVEL);

static void on_write(struct ble_lbs *lbs, const ble_evt_t *ble_evt)
{
	if (!lbs->evt_handler) {
		return;
	}

	ble_gatts_evt_t const *gatts_evt = &ble_evt->evt.gatts_evt;
	struct ble_lbs_evt lbs_evt = {
		.evt_type = BLE_LBS_EVT_LED_WRITE,
		.led_write.conn_handle = gatts_evt->conn_handle,
	};

	if ((gatts_evt->params.write.handle != lbs->led_char_handles.value_handle) ||
	    (gatts_evt->params.write.len != 1)) {
		/* Nothing to do */
		return;
	}

	lbs_evt.led_write.value = gatts_evt->params.write.data[0];

	lbs->evt_handler(lbs, &lbs_evt);
}

void ble_lbs_on_ble_evt(const ble_evt_t *ble_evt, void *lbs_instance)
{
	__ASSERT(ble_evt, "BLE event is NULL");
	__ASSERT(lbs_instance, "LBS instance is NULL");

	struct ble_lbs *lbs = (struct ble_lbs *)lbs_instance;

	switch (ble_evt->header.evt_id) {
	case BLE_GATTS_EVT_WRITE:
		on_write(lbs, ble_evt);
		break;
	default:
		break;
	}
}

int ble_lbs_init(struct ble_lbs *lbs, const struct ble_lbs_config *cfg)
{
	int err;
	ble_uuid_t ble_uuid;
	uint8_t initial_value = 0;

	if (!lbs || !cfg) {
		return -EFAULT;
	}

	/* Initialize service structure. */
	lbs->evt_handler = cfg->evt_handler;

	ble_uuid128_t base_uuid = { .uuid128 = BLE_UUID_LBS_BASE };

	err = sd_ble_uuid_vs_add(&base_uuid, &lbs->uuid_type);
	if (err != NRF_SUCCESS) {
		LOG_ERR("Failed to add vendor UUID, nrf_error %#x", err);
		return -EINVAL;
	}

	ble_uuid = (ble_uuid_t) {
		.type = lbs->uuid_type,
		.uuid = BLE_UUID_LBS_SERVICE,
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
		.uuid = BLE_UUID_LBS_BUTTON_CHAR,
	};
	ble_gatts_attr_md_t cccd_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.read = true,
			.notify = true,
		},
		.p_cccd_md = &cccd_md,
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = &initial_value,
		.init_len = sizeof(uint8_t),
		.max_len = sizeof(uint8_t),
	};
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

	err = sd_ble_gatts_characteristic_add(lbs->service_handle, &char_md, &attr_char_value,
					      &lbs->button_char_handles);
	if (err) {
		LOG_ERR("Failed to add button GATT characteristic, nrf_error %#x", err);
		return -EINVAL;
	}

	/* Add LED characteristic. */

	char_uuid = (ble_uuid_t){
		.type = lbs->uuid_type,
		.uuid = BLE_UUID_LBS_LED_CHAR,
	};
	char_md = (ble_gatts_char_md_t){
		.char_props = {
			.read = true,
			.write = true,
		},
	};
	attr_md = (ble_gatts_attr_md_t){
		.vloc = BLE_GATTS_VLOC_STACK,
	};
	attr_char_value = (ble_gatts_attr_t){
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = &initial_value,
		.init_len = sizeof(uint8_t),
		.max_len = sizeof(uint8_t),
	};
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

	err = sd_ble_gatts_characteristic_add(lbs->service_handle, &char_md, &attr_char_value,
					      &lbs->led_char_handles);
	if (err) {
		LOG_ERR("Failed to add LED GATT characteristic, nrf_error %#x", err);
		return -EINVAL;
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
