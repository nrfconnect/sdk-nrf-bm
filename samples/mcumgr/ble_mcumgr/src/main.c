/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <ble_adv.h>
#include <ble_conn_params.h>
#include <ble_gap.h>
#include <zephyr/kernel.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/sys/reboot.h>
#include <nrf_soc.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <ble_conn_params.h>
#include <bluetooth/services/ble_mcumgr.h>
#include <zephyr/settings/settings.h>
#include <settings/bluetooth_name.h>

LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

BLE_ADV_DEF(ble_adv); /* BLE advertising instance */

/** Handle of the current connection */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
static volatile bool should_reboot;
static volatile bool notification_sent;
static volatile bool device_disconnected;

static enum mgmt_cb_return os_mgmt_reboot_hook(uint32_t event, enum mgmt_cb_return prev_status,
					       int32_t *rc, uint16_t *group, bool *abort_more,
					       void *data, size_t data_size);

static struct mgmt_callback os_mgmt_reboot_callback = {
	.callback = os_mgmt_reboot_hook,
	.event_id = MGMT_EVT_OP_OS_MGMT_RESET,
};

/**
 * @brief BLE event handler.
 *
 * @param[in] evt BLE event.
 * @param[in] ctx Context.
 */
static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	int err;

	__ASSERT(ble_evt, "BLE event is NULL");

	if (evt == NULL) {
		return;
	}

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
	{
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;
		err = sd_ble_gatts_sys_attr_set(evt->evt.gap_evt.conn_handle, NULL, 0, 0);

		if (err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", err);
		}
		break;
	}

	case BLE_GAP_EVT_DISCONNECTED:
	{
		LOG_INF("Peer disconnected");
		conn_handle = BLE_CONN_HANDLE_INVALID;

		if (should_reboot == true) {
			device_disconnected = true;
		}
		break;
	}

	case BLE_GAP_EVT_AUTH_STATUS:
	{
		LOG_INF("Authentication status: %#x",
			evt->evt.gap_evt.params.auth_status.auth_status);
		break;
	}

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
	{
		/* Pairing not supported */
		err = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle,
						  BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);

		if (err) {
			LOG_ERR("Failed to reply with Security params, nrf_error %#x", err);
		}

		break;
	}

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
	{
		LOG_INF("BLE_GATTS_EVT_SYS_ATTR_MISSING");
		/* No system attributes have been stored */
		err = sd_ble_gatts_sys_attr_set(evt->evt.gap_evt.conn_handle, NULL, 0, 0);

		if (err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", err);
		}

		break;
	}

	case BLE_GATTS_EVT_HVN_TX_COMPLETE:
	{
		if (should_reboot == true) {
			notification_sent = true;
		}

		break;
	}
	};
}

NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, 0);

/**
 * @brief BLE advertising event handler
 *
 * @param[in] evt BLE advertising event type.
 */
static void ble_adv_evt_handler(struct ble_adv *adv, const struct ble_adv_evt *adv_evt)
{
	/* ignore */
}

static enum mgmt_cb_return os_mgmt_reboot_hook(uint32_t event, enum mgmt_cb_return prev_status,
					       int32_t *rc, uint16_t *group, bool *abort_more,
					       void *data, size_t data_size)
{
	if (event == MGMT_EVT_OP_OS_MGMT_RESET) {
		should_reboot = true;
		*rc = MGMT_ERR_EOK;

		return MGMT_CB_ERROR_RC;
	}

	return MGMT_CB_OK;
}

/**
 * @brief Change Bluetooth address from the default random address
 *
 * @param[out] err Error code
 */
static int ble_change_address(void)
{
	int err;
	ble_gap_addr_t device_address;

	err = sd_ble_gap_addr_get(&device_address);

	if (err != 0) {
		return err;
	}

	device_address.addr[0] ^= 0x1;

	return sd_ble_gap_addr_set(&device_address);
}

int main(void)
{
	int err;
	const char *custom_advertising_name;
	uint8_t custom_advertising_name_size;
	ble_gap_conn_sec_mode_t sec_mode = {0};
	struct ble_adv_config ble_adv_cfg = {
		.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
		.evt_handler = ble_adv_evt_handler,
		.adv_data = {
			.name_type = BLE_ADV_DATA_FULL_NAME,
			.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		},
	};
	ble_uuid_t adv_uuid_list[] = {
		{
			.uuid = BLE_MCUMGR_SERVICE_UUID_SUB,
		},
	};

	LOG_INF("BLE MCUmgr sample started");
	mgmt_callback_register(&os_mgmt_reboot_callback);

	err = nrf_sdh_enable_request();

	if (err) {
		LOG_ERR("Failed to enable SoftDevice, err %d", err);
		return 0;
	}

	LOG_INF("SoftDevice enabled");

	err = nrf_sdh_ble_enable(CONFIG_NRF_SDH_BLE_CONN_TAG);

	if (err) {
		LOG_ERR("Failed to enable BLE, err %d", err);
		return 0;
	}

	LOG_INF("Bluetooth enabled");

	err = settings_subsys_init();

	if (err) {
		LOG_ERR("Failed to enable settings, err %d", err);
	}

	settings_load();
	err = ble_mcumgr_init();

	if (err) {
		LOG_ERR("Failed to initialize MCUmgr service, err %d", err);
		return 0;
	}

	LOG_INF("Services initialized");

	err = ble_change_address();

	if (err) {
		LOG_ERR("Failed to change Bluetooth address, err %d", err);
	}

	/* Add MCUmgr Bluetooth service UUID to scan response */
	adv_uuid_list->type = ble_mcumgr_service_uuid_type();
	ble_adv_cfg.sr_data.uuid_lists.complete.uuid = &adv_uuid_list[0];
	ble_adv_cfg.sr_data.uuid_lists.complete.len = ARRAY_SIZE(adv_uuid_list);

	err = ble_adv_init(&ble_adv, &ble_adv_cfg);

	if (err) {
		LOG_ERR("Failed to initialize advertising, err %d", err);
		return 0;
	}

	custom_advertising_name = bluetooth_name_value_get();
	custom_advertising_name_size = strlen(custom_advertising_name);

	if (custom_advertising_name_size > 0) {
		/* Change advertising name to one from application */
		BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
		err = sd_ble_gap_device_name_set(&sec_mode, custom_advertising_name,
						 custom_advertising_name_size);

		if (err) {
			LOG_ERR("Failed to change advertising name, err %d", err);
			return 0;
		} else {
			err = ble_adv_data_encode(&ble_adv_cfg.adv_data, ble_adv.enc_adv_data[0],
						  &ble_adv.adv_data.adv_data.len);

			if (err) {
				LOG_ERR("Failed to update advertising data, err %d", err);
				return 0;
			}
		}
	}

	err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);

	if (err) {
		LOG_ERR("Failed to start advertising, err %d", err);
		return 0;
	}

	LOG_INF("Advertising as %s", (custom_advertising_name_size > 0 ? custom_advertising_name :
				      CONFIG_BLE_ADV_NAME));

	while (notification_sent == false && device_disconnected == false) {
		while (LOG_PROCESS()) {
		}

		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();
	}

	if (device_disconnected == false) {
		err = sd_ble_gap_disconnect(conn_handle,
					    BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);

		if (err != NRF_SUCCESS) {
			device_disconnected = true;
		}

		while (device_disconnected == false) {
			while (LOG_PROCESS()) {
			}

			/* Wait for an event. */
			__WFE();

			/* Clear Event Register */
			__SEV();
			__WFE();
		}
	}

	sys_reboot(SYS_REBOOT_WARM);

	return 0;
}
