/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/reboot.h>
#include <zephyr/retention/bootmode.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log_ctrl.h>

#include <nrf_sdh.h>
#include <ble_adv.h>
#include <bluetooth/services/ble_mcumgr.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>

BLE_ADV_DEF(ble_adv); /* BLE advertising instance */

static bool should_reboot;

static enum mgmt_cb_return os_mgmt_reboot_hook(uint32_t event, enum mgmt_cb_return prev_status,
					       int32_t *rc, uint16_t *group, bool *abort_more,
					       void *data, size_t data_size)
{
	if (event == MGMT_EVT_OP_OS_MGMT_RESET) {
		struct os_mgmt_reset_data *cb_data = (struct os_mgmt_reset_data *)data;

		(void)bootmode_set(cb_data->boot_mode);
		should_reboot = true;
		*rc = MGMT_ERR_EOK;

		return MGMT_CB_ERROR_RC;
	}

	return MGMT_CB_OK;
}

static struct mgmt_callback os_mgmt_reboot_callback = {
	.callback = os_mgmt_reboot_hook,
	.event_id = MGMT_EVT_OP_OS_MGMT_RESET,
};

static void ble_adv_evt_handler(struct ble_adv *adv, const struct ble_adv_evt *adv_evt)
{
	switch (adv_evt->evt_type) {
	case BLE_ADV_EVT_ERROR:
		printk("Advertising error %d\n", adv_evt->error.reason);
		break;
	default:
		break;
	}
}

int main(void)
{
	int err;
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

	mgmt_callback_register(&os_mgmt_reboot_callback);

	err = nrf_sdh_enable_request();
	if (err) {
		printk("Failed to enable SoftDevice, err %d\n", err);
		return 0;
	}

	printk("SoftDevice enabled\n");

	err = nrf_sdh_ble_enable(CONFIG_NRF_SDH_BLE_CONN_TAG);
	if (err) {
		printk("Failed to enable BLE, err %d\n", err);
		return 0;
	}

	printk("Bluetooth enabled\n");

	err = ble_mcumgr_init();

	if (err) {
		printk("Failed to initialize MCUmgr, err %d\n", err);
		return 0;
	}

	/* Add MCUmgr Bluetooth service UUID to scan response */
	adv_uuid_list->type = ble_mcumgr_service_uuid_type();
	ble_adv_cfg.sr_data.uuid_lists.complete.uuid = &adv_uuid_list[0];
	ble_adv_cfg.sr_data.uuid_lists.complete.len = ARRAY_SIZE(adv_uuid_list);

	err = ble_adv_init(&ble_adv, &ble_adv_cfg);

	if (err) {
		printk("Failed to initialize advertising, err %d\n", err);
		return 0;
	}

	err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);

	if (err) {
		printk("Failed to start advertising, err %d\n", err);
		return 0;
	}

	printk("Advertising as %s\n", CONFIG_BLE_ADV_NAME);

	while (should_reboot == false) {
		while (LOG_PROCESS()) {
		}

		sd_app_evt_wait();
	}

	sys_reboot(SYS_REBOOT_WARM);

	return 0;
}
