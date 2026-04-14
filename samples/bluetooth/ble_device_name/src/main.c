/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <ble_gap.h>
#include <ble_gatts.h>
#include <nrf_soc.h>
#include <hal/nrf_gpio.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/ble_common.h>
#include <bm/storage/bm_storage.h>
#include <bm/fs/bm_zms.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_BLE_DEVICE_NAME_LOG_LEVEL);

/* Storage partition configuration from devicetree. */
#define STORAGE_NODE   DT_NODELABEL(storage0_partition)
#define STORAGE_OFFSET DT_REG_ADDR(STORAGE_NODE)
#define STORAGE_SIZE   DT_REG_SIZE(STORAGE_NODE)

/* ZMS item ID used to store the device name. */
#define DEVICE_NAME_STORAGE_ID 1

/* BLE advertising instance. */
BLE_ADV_DEF(ble_adv);

static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

/* ZMS filesystem instance. */
static struct bm_zms_fs fs;
static volatile bool zms_mounted;

/* Set device name sec open to allow updating the name without any security. */
ble_gap_conn_sec_mode_t device_name_sec = BLE_GAP_CONN_SEC_MODE_OPEN;

/* Buffer to hold device name between event handler and async ZMS write. */
static uint8_t pending_name[BLE_GAP_DEVNAME_MAX_LEN];

/* Global so the BLE event handler can call ble_adv_data_update() to refresh the advertising packet
 * with the new device name when a peer writes it.
 * Without adv data being global, the advertised name would remain stale until reboot.
 */
static const struct ble_adv_data adv_data = {
	.name_type = BLE_ADV_DATA_FULL_NAME,
	.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
};

static bool device_name_save(const uint8_t *name, uint16_t len)
{
	ssize_t written;

	/* Persist to storage.
	 * ZMS queues the write but doesn't copy the data, it only stores a pointer.
	 * Since write_evt->data is freed when this handler returns,
	 * must copy it to a buffer that outlives the event.
	 */
	memcpy(pending_name, name, len);

	written = bm_zms_write(&fs, DEVICE_NAME_STORAGE_ID, pending_name, len);
	if (written < 0) {
		/* Write failed. */
		return false;
	}

	/* Name saved successfully. */
	return true;
}

static bool device_name_load(uint8_t *buf, size_t *len)
{
	ssize_t bytes_read = bm_zms_read(&fs, DEVICE_NAME_STORAGE_ID, buf, *len);

	if (bytes_read <= 0) {
		/* No name found or read failed. */
		return false;
	}
	/* Name loaded, *len set to bytes read. */
	*len = (size_t)bytes_read;
	return true;
}

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	uint32_t nrf_err;
	const ble_gatts_evt_write_t *write_evt = &evt->evt.gatts_evt.params.write;
	bool saved;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;
		nrf_err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (nrf_err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", nrf_err);
		}
		nrf_gpio_pin_write(BOARD_PIN_LED_1, BOARD_LED_ACTIVE_STATE);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Peer disconnected");
		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}
		nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
		break;

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		/* Pairing not supported. */
		nrf_err = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle,
						      BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL,
						      NULL);
		if (nrf_err) {
			LOG_ERR("Failed to reply with Security params, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		LOG_INF("BLE_GATTS_EVT_SYS_ATTR_MISSING");
		/* No system attributes have been stored. */
		nrf_err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (nrf_err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_GATTS_EVT_WRITE:
		/* BLE_GATTS_EVT_WRITE fires for ALL GATT characteristic writes.
		 * Since the GAP Device Name has no dedicated event,
		 * filter by UUID and type to catch only Device Name writes.
		 * When a match is found, update the SoftDevice immediately
		 * and save to storage so it persists across reboots.
		 */
		if (write_evt->uuid.uuid == BLE_UUID_GAP_CHARACTERISTIC_DEVICE_NAME &&
		    write_evt->uuid.type == BLE_UUID_TYPE_BLE) {
			/* Update SoftDevice GAP name immediately and refresh
			 * advertising data so the new name is used on next broadcast.
			 */
			sd_ble_gap_device_name_set(&device_name_sec, write_evt->data,
						   write_evt->len);
			nrf_err = ble_adv_data_update(&ble_adv, &adv_data, NULL);
			if (nrf_err) {
				LOG_ERR("Failed to update adv data, nrf_error %#x", nrf_err);
			}

			saved = device_name_save(write_evt->data, write_evt->len);
			if (!saved) {
				LOG_ERR("Failed to save device name to storage");
				LOG_HEXDUMP_ERR(write_evt->data, write_evt->len, "Unsaved name");
			}

			LOG_INF("Device name updated to %.*s", write_evt->len, write_evt->data);
		}
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

static void ble_adv_evt_handler(struct ble_adv *adv, const struct ble_adv_evt *adv_evt)
{
	switch (adv_evt->evt_type) {
	case BLE_ADV_EVT_ERROR:
		LOG_ERR("Advertising error %#x", adv_evt->error.reason);
		break;
	default:
		break;
	}
}

static void zms_evt_handler(const struct bm_zms_evt *evt)
{
	switch (evt->evt_type) {
	case BM_ZMS_EVT_MOUNT:
		if (evt->result) {
			LOG_ERR("ZMS mount failed: %d", evt->result);
			break;
		}
		LOG_INF("BM ZMS mounted");
		zms_mounted = true;
		break;

	case BM_ZMS_EVT_WRITE:
		if (evt->id != DEVICE_NAME_STORAGE_ID) {
			break;
		}
		if (evt->result) {
			LOG_ERR("Device name storage write failed: %d", evt->result);
			break;
		}
		LOG_INF("Device name written to storage");
		break;

	default:
		break;
	}
}

int main(void)
{
	int err;
	uint32_t nrf_err;
	static struct ble_adv_config adv_cfg = {
		.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
		.evt_handler  = ble_adv_evt_handler,
		.adv_data = adv_data,
	};
	struct bm_zms_fs_config bm_zms_cfg = {
		.offset       = STORAGE_OFFSET,
		.sector_size  = CONFIG_SAMPLE_BM_ZMS_SECTOR_SIZE,
		.sector_count = STORAGE_SIZE/CONFIG_SAMPLE_BM_ZMS_SECTOR_SIZE,
		.evt_handler  = zms_evt_handler,
		.storage_api  = &bm_storage_sd_api,
	};
	bool name_found;
	uint8_t device_name[BLE_GAP_DEVNAME_MAX_LEN] = CONFIG_SAMPLE_BLE_DEVICE_NAME;
	size_t device_name_len = sizeof(device_name);

	LOG_INF("BLE GAP Device Name sample started");

	/* LED setup. */
	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_1);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);

	/* SoftDevice setup. */
	err = nrf_sdh_enable_request();
	if (err) {
		LOG_ERR("Failed to enable SoftDevice, err %d", err);
		goto idle;
	}

	LOG_INF("SoftDevice enabled");

	err = nrf_sdh_ble_enable(CONFIG_NRF_SDH_BLE_CONN_TAG);
	if (err) {
		LOG_ERR("Failed to enable BLE, err %d", err);
		goto idle;
	}

	LOG_INF("Bluetooth enabled");

	/* BM ZMS (Bare Metal Zephyr Memory Storage) setup.
	 * Mount ZMS and wait for completion, must finish before we can read the stored name.
	 */
	LOG_INF("BM ZMS mounting");
	err = bm_zms_mount(&fs, &bm_zms_cfg);
	if (err) {
		LOG_ERR("Storage Init failed, err %d", err);
		goto idle;
	}

	while (!zms_mounted) {
		k_cpu_idle();
	}

	/* GAP Device Name setup.
	 * Try loading a previously saved name from storage.
	 * The name is then set in the SoftDevice with open read/write permissions
	 * so peers can both read and update it over BLE.
	 */
	name_found = device_name_load(device_name, &device_name_len);
	if (!name_found) {
		device_name_len = strlen(CONFIG_SAMPLE_BLE_DEVICE_NAME);
		LOG_INF("No local name stored, using default");
	}

	nrf_err = sd_ble_gap_device_name_set(&device_name_sec, device_name,
					     (uint16_t)device_name_len);
	if (nrf_err) {
		LOG_ERR("Failed to set device name, nrf_error %#x", nrf_err);
		goto idle;
	}

	/* Advertising setup. */
	nrf_err = ble_adv_init(&ble_adv, &adv_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize BLE advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Advertising configured");

	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
	LOG_INF("BLE GAP Device Name sample initialized");

	/* Start Advertising. */
	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (nrf_err) {
		LOG_ERR("Adv start failed: %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Advertising as %.*s", device_name_len, device_name);

idle:
	while (true) {
		log_flush();

		k_cpu_idle();
	}

	return 0;
}
