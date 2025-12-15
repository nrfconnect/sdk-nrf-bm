/*
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <stdbool.h>
#include <stdint.h>
#include <bm/bluetooth/services/ble_nus.h>
#include <bm/bluetooth/services/common.h>
#include <bm/bluetooth/services/uuid.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_nus, CONFIG_BLE_NUS_LOG_LEVEL);

static struct ble_nus_client_context *ble_nus_client_context_get(struct ble_nus *nus,
								 uint16_t conn_handle)
{
	const int idx = nrf_sdh_ble_idx_get(conn_handle);

	return ((idx >= 0) ? &nus->contexts[idx] : NULL);
}

static uint32_t nus_rx_char_add(struct ble_nus *nus, const struct ble_nus_config *cfg)
{
	ble_uuid_t char_uuid = {
		.type = nus->uuid_type,
		.uuid = BLE_UUID_NUS_RX_CHARACTERISTIC,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.write = true,
			.write_wo_resp = true,
		},
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.vlen = true,
		.read_perm = cfg->sec_mode.nus_rx_char.read,
		.write_perm = cfg->sec_mode.nus_rx_char.write,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = NULL,
		.init_len = sizeof(uint8_t),
		.max_len = BLE_NUS_MAX_DATA_LEN,
	};

	/* Add Nordic UART RX characteristic declaration and value attributes. */
	return sd_ble_gatts_characteristic_add(nus->service_handle, &char_md, &attr_char_value,
					       &nus->rx_handles);
}

static uint32_t nus_tx_char_add(struct ble_nus *nus, const struct ble_nus_config *cfg)
{
	ble_uuid_t char_uuid = {
		.type = nus->uuid_type,
		.uuid = BLE_UUID_NUS_TX_CHARACTERISTIC,
	};
	ble_gatts_attr_md_t cccd_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.read_perm = BLE_GAP_CONN_SEC_MODE_OPEN,
		.write_perm = cfg->sec_mode.nus_tx_char.cccd_write,
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.notify = true,
		},
		.p_cccd_md = &cccd_md,
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.vlen = true,
		.read_perm = cfg->sec_mode.nus_tx_char.read,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = NULL,
		.init_len = 0,
		.max_len = BLE_NUS_MAX_DATA_LEN,
	};

	/* Add Nordic UART TX declaration, value and CCCD attributes */
	return sd_ble_gatts_characteristic_add(nus->service_handle, &char_md, &attr_char_value,
					       &nus->tx_handles);
}

/**
 * @brief Function for handling the @ref BLE_GAP_EVT_CONNECTED event from the SoftDevice.
 *
 * @param[in] nus Nordic UART Service structure.
 * @param[in] ble_evt Pointer to the event received from BLE stack.
 */
static void on_connect(struct ble_nus *nus, const ble_evt_t *ble_evt)
{
	uint32_t nrf_err;
	const uint16_t conn_handle = ble_evt->evt.gap_evt.conn_handle;
	struct ble_nus_evt evt = {
		.evt_type = BLE_NUS_EVT_COMM_STARTED,
		.conn_handle = conn_handle,
	};
	uint8_t cccd_value[2];
	ble_gatts_value_t gatts_val = {
		.p_value = cccd_value,
		.len = sizeof(cccd_value),
		.offset = 0,
	};
	struct ble_nus_client_context *ctx;

	ctx = ble_nus_client_context_get(nus, conn_handle);
	if (ctx == NULL) {
		LOG_ERR("Could not fetch nus context for connection handle %#x", conn_handle);
		evt.evt_type = BLE_NUS_EVT_ERROR;
		evt.error.reason = NRF_ERROR_NOT_FOUND;
		if (nus->evt_handler != NULL) {
			nus->evt_handler(nus, &evt);
		}
	}

	/* Check the hosts CCCD value to inform of readiness to send data using the
	 * RX characteristic
	 */
	nrf_err = sd_ble_gatts_value_get(conn_handle, nus->tx_handles.cccd_handle, &gatts_val);
	if ((nrf_err == NRF_SUCCESS) && (nus->evt_handler != NULL) &&
	    is_notification_enabled(gatts_val.p_value)) {
		if (ctx != NULL) {
			ctx->is_notification_enabled = true;
		}

		evt.link_ctx = ctx;
		nus->evt_handler(nus, &evt);
	}
}

/**
 * @brief Function for handling the @ref BLE_GATTS_EVT_WRITE event from the SoftDevice.
 *
 * @param[in] nus Nordic UART Service structure.
 * @param[in] ble_evt Pointer to the event received from BLE stack.
 */
static void on_write(struct ble_nus *nus, const ble_evt_t *ble_evt)
{
	const uint16_t conn_handle = ble_evt->evt.gatts_evt.conn_handle;
	const ble_gatts_evt_write_t *evt_write = &ble_evt->evt.gatts_evt.params.write;
	struct ble_nus_evt evt = {
		.conn_handle = conn_handle,
	};
	struct ble_nus_client_context *ctx;

	ctx = ble_nus_client_context_get(nus, conn_handle);
	if (ctx == NULL) {
		LOG_ERR("Could not fetch nus context for connection handle %#x", conn_handle);
		evt.evt_type = BLE_NUS_EVT_ERROR;
		evt.error.reason = NRF_ERROR_NOT_FOUND;
		if (nus->evt_handler != NULL) {
			nus->evt_handler(nus, &evt);
		}
	}

	LOG_DBG("Link ctx %p", ctx);
	evt.link_ctx = ctx;

	if ((evt_write->handle == nus->tx_handles.cccd_handle) && (evt_write->len == 2)) {
		if (ctx != NULL) {
			if (is_notification_enabled(evt_write->data)) {
				ctx->is_notification_enabled = true;
				evt.evt_type = BLE_NUS_EVT_COMM_STARTED;
			} else {
				ctx->is_notification_enabled = false;
				evt.evt_type = BLE_NUS_EVT_COMM_STOPPED;
			}

			if (nus->evt_handler != NULL) {
				nus->evt_handler(nus, &evt);
			}
		}
	} else if ((evt_write->handle == nus->rx_handles.value_handle) &&
		   (nus->evt_handler != NULL)) {
		evt.evt_type = BLE_NUS_EVT_RX_DATA;
		evt.rx_data.data = evt_write->data;
		evt.rx_data.length = evt_write->len;

		nus->evt_handler(nus, &evt);
	} else {
		/* Do nothing. This event is not relevant for this service. */
	}
}

/**
 * @brief Function for handling the @ref BLE_GATTS_EVT_HVN_TX_COMPLETE event from the SoftDevice.
 *
 * @param[in] nus Nordic UART Service structure.
 * @param[in] ble_evt Pointer to the event received from BLE stack.
 */
static void on_hvx_tx_complete(struct ble_nus *nus, const ble_evt_t *ble_evt)
{
	const uint16_t conn_handle = ble_evt->evt.gatts_evt.conn_handle;
	struct ble_nus_evt evt = {
		.evt_type = BLE_NUS_EVT_TX_RDY,
		.conn_handle = conn_handle,
	};
	struct ble_nus_client_context *ctx;

	ctx = ble_nus_client_context_get(nus, conn_handle);
	if (ctx == NULL) {
		LOG_ERR("Could not fetch nus context for connection handle %#x", conn_handle);
		return;
	}

	if ((ctx->is_notification_enabled) && (nus->evt_handler != NULL)) {
		evt.link_ctx = ctx;
		nus->evt_handler(nus, &evt);
	}
}

void ble_nus_on_ble_evt(const ble_evt_t *ble_evt, void *ctx)
{
	__ASSERT(ble_evt, "BLE event is NULL");
	__ASSERT(ctx, "context is NULL");


	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		on_connect(ctx, ble_evt);
		break;

	case BLE_GATTS_EVT_WRITE:
		on_write(ctx, ble_evt);
		break;

	case BLE_GATTS_EVT_HVN_TX_COMPLETE:
		on_hvx_tx_complete(ctx, ble_evt);
		break;

	default:
		/* No implementation needed. */
		break;
	}
}

uint32_t ble_nus_init(struct ble_nus *nus, const struct ble_nus_config *cfg)
{
	uint32_t nrf_err;
	ble_uuid_t ble_uuid;
	ble_uuid128_t uuid_base = { .uuid128 = BLE_NUS_UUID_BASE };

	if (!nus || !cfg) {
		return NRF_ERROR_NULL;
	}

	/* Initialize the service structure. */
	nus->evt_handler = cfg->evt_handler;

	/* Add a custom base UUID. */
	nrf_err = sd_ble_uuid_vs_add(&uuid_base, &nus->uuid_type);
	if (nrf_err) {
		LOG_ERR("sd_ble_uuid_vs_add failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	ble_uuid.type = nus->uuid_type;
	ble_uuid.uuid = BLE_UUID_NUS_SERVICE;

	/* Add the service. */
	nrf_err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
					   &ble_uuid,
					   &nus->service_handle);
	if (nrf_err) {
		LOG_ERR("Failed to add NUS service, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	/* Add NUS RX characteristic. */
	nrf_err = nus_rx_char_add(nus, cfg);
	if (nrf_err) {
		LOG_ERR("nus_rx_char_add failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	/* Add NUS TX characteristic. */
	nrf_err = nus_tx_char_add(nus, cfg);
	if (nrf_err) {
		LOG_ERR("nus_tx_char_add failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return 0;
}

uint32_t ble_nus_data_send(struct ble_nus *nus, uint8_t *data,
			   uint16_t *len, uint16_t conn_handle)
{
	ble_gatts_hvx_params_t hvx = { 0 };

	if (!nus || !data || !len) {
		return NRF_ERROR_NULL;
	}

	hvx.type = BLE_GATT_HVX_NOTIFICATION;
	hvx.handle = nus->tx_handles.value_handle;
	hvx.p_data = data;
	hvx.p_len = len;

	return sd_ble_gatts_hvx(conn_handle, &hvx);
}
