/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ble_gap.h>
#include <nrf_soc.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/services/ble_mcumgr.h>

#include <zephyr/kernel.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/settings/settings.h>
#include <bm/settings/bluetooth_name.h>
#include <zephyr/retention/retention.h>

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
	uint32_t nrf_err;

	__ASSERT(evt, "BLE event is NULL");

	if (evt == NULL) {
		return;
	}

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
	{
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;
		nrf_err = sd_ble_gatts_sys_attr_set(evt->evt.gap_evt.conn_handle, NULL, 0, 0);

		if (nrf_err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", nrf_err);
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
		nrf_err = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle,
						      BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP,
						      NULL, NULL);

		if (nrf_err) {
			LOG_ERR("Failed to reply with Security params, nrf_error %#x", nrf_err);
		}

		break;
	}

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
	{
		LOG_INF("BLE_GATTS_EVT_SYS_ATTR_MISSING");
		/* No system attributes have been stored */
		nrf_err = sd_ble_gatts_sys_attr_set(evt->evt.gap_evt.conn_handle, NULL, 0, 0);

		if (nrf_err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", nrf_err);
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

NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

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
 */
static uint32_t ble_change_address(void)
{
	uint32_t nrf_err;
	ble_gap_addr_t device_address;

	nrf_err = sd_ble_gap_addr_get(&device_address);

	if (nrf_err) {
		return nrf_err;
	}

	device_address.addr[0] ^= 0x1;

	return sd_ble_gap_addr_set(&device_address);
}

int main(void)
{
	int err;
	uint32_t nrf_err;
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
	struct ble_mcumgr_config mcumgr_cfg = {
		.sec_mode = BLE_MCUMGR_CONFIG_SEC_MODE_DEFAULT,
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

#if CONFIG_NCS_BM_SETTINGS_BLUETOOTH_NAME
	/* Initialize setting subsystem with SRAM retention backend
	 * for fetching ADV device name provided by the application.
	 */
	err = settings_subsys_init();

	if (err) {
		LOG_ERR("Failed to enable settings, err %d", err);
	}

	settings_load();
#endif

	nrf_err = ble_mcumgr_init(&mcumgr_cfg);

	if (nrf_err) {
		LOG_ERR("Failed to initialize MCUmgr service, nrf_error %#x", nrf_err);
		return 0;
	}

	LOG_INF("Services initialized");

	nrf_err = ble_change_address();

	if (nrf_err) {
		LOG_ERR("Failed to change Bluetooth address, nrf_error %#x", nrf_err);
	}

	/* Add MCUmgr Bluetooth service UUID to scan response */
	adv_uuid_list->type = ble_mcumgr_service_uuid_type();
	ble_adv_cfg.sr_data.uuid_lists.complete.uuid = &adv_uuid_list[0];
	ble_adv_cfg.sr_data.uuid_lists.complete.len = ARRAY_SIZE(adv_uuid_list);

	nrf_err = ble_adv_init(&ble_adv, &ble_adv_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize advertising, nrf_error %#x", nrf_err);
		return 0;
	}

#if CONFIG_NCS_BM_SETTINGS_BLUETOOTH_NAME
	const char *custom_advertising_name;
	uint8_t custom_advertising_name_size;
	ble_gap_conn_sec_mode_t sec_mode = {0};

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
		}

		err = ble_adv_data_encode(&ble_adv_cfg.adv_data, ble_adv.enc_adv_data[0],
					  &ble_adv.adv_data.adv_data.len);

		if (err) {
			LOG_ERR("Failed to update advertising data, err %d", err);
			return 0;
		}

		/* Clear settings after device name has been set so it does not persist */
		err = retention_clear(DEVICE_DT_GET(DT_CHOSEN(zephyr_settings_partition)));

		if (err) {
			LOG_ERR("Failed to clear retention area, err %d", err);
			return 0;
		}
	}
#endif /* CONFIG_NCS_BM_SETTINGS_BLUETOOTH_NAME */

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (nrf_err) {
		LOG_ERR("Failed to start advertising, nrf_error %#x", nrf_err);
		return 0;
	}

#if CONFIG_NCS_BM_SETTINGS_BLUETOOTH_NAME
	LOG_INF("Advertising as %s", (custom_advertising_name_size > 0 ? custom_advertising_name :
				      CONFIG_BLE_ADV_NAME));
#else
	LOG_INF("Advertising as %s", CONFIG_BLE_ADV_NAME);
#endif /* CONFIG_NCS_BM_SETTINGS_BLUETOOTH_NAME */

	while (notification_sent == false && device_disconnected == false) {
		log_flush();

		k_cpu_idle();
	}

	if (device_disconnected == false) {
		nrf_err = sd_ble_gap_disconnect(conn_handle,
						BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);

		if (nrf_err) {
			device_disconnected = true;
		}

		while (device_disconnected == false) {
			log_flush();

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
