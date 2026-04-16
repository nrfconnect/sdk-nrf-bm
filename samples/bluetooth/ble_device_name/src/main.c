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
#include <bm/storage/bm_storage.h>
#include <bm/fs/bm_zms.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <board-config.h>

/* Storage partition configuration from devicetree */
#define STORAGE_NODE           DT_NODELABEL(storage0_partition)
#define STORAGE_OFFSET         DT_REG_ADDR(STORAGE_NODE)
#define STORAGE_SIZE           DT_REG_SIZE(STORAGE_NODE)

/* ZMS item ID used to store the device name */
#define DEVICE_NAME_STORAGE_ID 1

LOG_MODULE_REGISTER(sample, LOG_LEVEL_INF);

BLE_ADV_DEF(ble_adv); /* BLE advertising instance */

static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

/* Global so the BLE event handler can call ble_adv_data_update() to refresh the advertising packet
 * with the new device name when a peer writes it.
 * Without adv config being global, the advertised name would remain stale until reboot.
 */
static void ble_adv_evt_handler(struct ble_adv *adv, const struct ble_adv_evt *adv_evt);
static struct ble_adv_config adv_cfg = {
	.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
	.evt_handler  = ble_adv_evt_handler,
	.adv_data = {
		.name_type = BLE_ADV_DATA_FULL_NAME,
		.flags     = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
	},
};

/* ZMS filesystem instance */
static struct bm_zms_fs fs;
static volatile bool zms_ready;

/* Buffer to hold device name between event handler and async ZMS write */
static uint8_t pending_name[BLE_GAP_DEVNAME_MAX_LEN];

static void zms_evt_handler(const struct bm_zms_evt *evt)
{
	switch (evt->evt_type) {
	case BM_ZMS_EVT_MOUNT:
		if (evt->result) {
			LOG_ERR("ZMS mount failed: %d", evt->result);
		} else {
			LOG_INF("BM ZMS mounted");
		}
		zms_ready = true;
		break;

	case BM_ZMS_EVT_WRITE:
		if (evt->id == DEVICE_NAME_STORAGE_ID) {
			if (evt->result) {
				LOG_ERR("Device name storage write failed: %d", evt->result);
			} else {
				LOG_INF("Device name persisted to storage");
			}
		}
		break;

	default:
		break;
	}
}

static bool device_name_save(const uint8_t *name, uint16_t len)
{
	ssize_t bytes_queued = bm_zms_write(&fs, DEVICE_NAME_STORAGE_ID, name, len);

	if (bytes_queued < 0) {
		return false; /* Write failed */
	}
	return true; /* Name saved successfully */
}

static bool device_name_load(uint8_t *buf, uint16_t buf_size, uint16_t *len)
{
	ssize_t bytes_read = bm_zms_read(&fs, DEVICE_NAME_STORAGE_ID, buf, buf_size);

	if (bytes_read <= 0) {
		return false; /* No name found or read failed */
	}
	*len = (uint16_t)bytes_read;
	return true; /* Name loaded, *len set to bytes read */
}

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	uint32_t nrf_err;

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
		/* Pairing not supported */
		nrf_err = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle,
						      BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL,
						      NULL);
		if (nrf_err) {
			LOG_ERR("Failed to reply with Security params, nrf_error %#x", nrf_err);
		}
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		LOG_INF("BLE_GATTS_EVT_SYS_ATTR_MISSING");
		/* No system attributes have been stored */
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
		const ble_gatts_evt_write_t *write_evt = &evt->evt.gatts_evt.params.write;
		ble_gap_conn_sec_mode_t device_name_sec;
		bool saved;

		if (write_evt->uuid.uuid == BLE_UUID_GAP_CHARACTERISTIC_DEVICE_NAME &&
		    write_evt->uuid.type == BLE_UUID_TYPE_BLE) {
			/* Update SoftDevice GAP name immediately and refresh
			 * advertising data so the new name is used on next broadcast.
			 */
			BLE_GAP_CONN_SEC_MODE_SET_OPEN(&device_name_sec);
			sd_ble_gap_device_name_set(&device_name_sec, write_evt->data,
						   write_evt->len);
			ble_adv_data_update(&ble_adv, &adv_cfg.adv_data, NULL);

			/* Persist to storage.
			 * ZMS queues the write but doesn't copy the data, it only stores a pointer.
			 * Since write_evt->data is freed when this handler returns,
			 * must copy it to a buffer that outlives the event.
			 */
			memcpy(pending_name, write_evt->data, write_evt->len);
			saved = device_name_save(pending_name, write_evt->len);
			if (!saved) {
				LOG_ERR("Failed to save device name to storage");
				LOG_HEXDUMP_ERR(write_evt->data, write_evt->len, "Unsaved name");
			}

			LOG_INF("Device name updated (%u bytes)", write_evt->len);
			LOG_HEXDUMP_INF(write_evt->data, write_evt->len, "New name");
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

int main(void)
{
	int err;
	uint32_t nrf_err;
	int rc;
	struct bm_zms_fs_config bm_zms_cfg = {
		.offset       = STORAGE_OFFSET,
		.sector_size  = CONFIG_SAMPLE_BM_ZMS_SECTOR_SIZE,
		.sector_count = STORAGE_SIZE/CONFIG_SAMPLE_BM_ZMS_SECTOR_SIZE,
		.evt_handler  = zms_evt_handler,
		.storage_api  = &bm_storage_sd_api,
	};
	bool name_found;
	uint8_t saved_name[BLE_GAP_DEVNAME_MAX_LEN];
	uint16_t saved_len = 0;
	ble_gap_conn_sec_mode_t device_name_sec;

	LOG_INF("BLE GAP Device Name sample started");

	/* SoftDevice setup */
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
	 * Mount ZMS and wait for completion, must finish before we can read the stored name
	 */
	rc = bm_zms_mount(&fs, &bm_zms_cfg);
	if (rc) {
		LOG_ERR("Storage Init failed, rc %d", rc);
		goto idle;
	}

	LOG_INF("BM ZMS mounting scheduled");

	while (!zms_ready) {
		k_cpu_idle();
	}
	zms_ready = false;

	/* GAP Device Name setup.
	 * Try loading a previously saved name from storage.
	 * If none exists, fall back to the default configured name.
	 * The name is then set in the SoftDevice with open read/write permissions
	 * so peers can both read and update it over BLE.
	 */
	name_found = device_name_load(saved_name, sizeof(saved_name), &saved_len);
	if (name_found) {
		LOG_INF("Loaded saved device name");
		LOG_HEXDUMP_INF(saved_name, saved_len, "Name");
	} else {
		LOG_INF("No saved name, using default");
		saved_len = strlen(CONFIG_SAMPLE_BLE_DEVICE_NAME);
		memcpy(saved_name, CONFIG_SAMPLE_BLE_DEVICE_NAME, saved_len);
	}

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&device_name_sec);
	nrf_err = sd_ble_gap_device_name_set(&device_name_sec, saved_name, saved_len);
	if (nrf_err) {
		LOG_ERR("Failed to set device name, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Device name set to: %.*s", saved_len, saved_name);

	/* LED setup */
	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_1);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);

	LOG_INF("LEDs enabled");

	/* Advertising setup */
	nrf_err = ble_adv_init(&ble_adv, &adv_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize BLE advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Advertising configured");

	/* Start Advertising */
	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (nrf_err) {
		LOG_ERR("Adv start failed: %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Advertising as %.*s", saved_len, saved_name);

	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
	LOG_INF("BLE GAP Device Name sample initialized");

idle:
	while (true) {
		log_flush();

		k_cpu_idle();
	}

	return 0;
}
