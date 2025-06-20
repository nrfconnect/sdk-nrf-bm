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
#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/smp/smp.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/mgmt/mcumgr/mgmt/handlers.h>
#include <mgmt/mcumgr/transport/smp_internal.h>
#include <mgmt/mcumgr/transport/smp_reassembly.h>
#include <zephyr/sys/reboot.h>
#include <nrf_soc.h>
#include <zephyr/logging/log.h>
#include <bluetooth/services/common.h>
#include <bluetooth/services/uuid.h>
#include <ble_conn_params.h>

LOG_MODULE_REGISTER(app, CONFIG_BLE_MCUMGR_LOG_LEVEL);

BLE_ADV_DEF(ble_adv); /* BLE advertising instance */

/** Used vendor specific UUID */
#define BLE_MCUMGR_SERVICE_UUID { 0x84, 0xAA, 0x60, 0x74, 0x52, 0x8A, 0x8B, 0x86, \
				  0xD3, 0x4C, 0xB7, 0x1D, 0x1D, 0xDC, 0x53, 0x8D  }

#define BLE_MCUMGR_CHARACTERISTIC_UUID { 0x48, 0x7C, 0x99, 0x74, 0x11, 0x26, 0x9E, 0xAE, \
					 0x01, 0x4E, 0xCE, 0xFB, 0x28, 0x78, 0x2E, 0xDA  }

/** The UUID of the TX Characteristic */
#define BLE_MCUMGR_SERVICE_UUID_SUB 0xdc1d
#define BLE_MCUMGR_CHARACTERISTIC_UUID_SUB 0x7828

#define OPCODE_LENGTH 1
#define HANDLE_LENGTH 2

/**
 * @brief Macro for calculating maximum length of data (in bytes) that can be transmitted to
 *        the peer over GATT, given the ATT MTU size.
 */
#define BLE_GATT_MAX_DATA_LEN_CALC(mtu_size) ((mtu_size) - OPCODE_LENGTH - HANDLE_LENGTH)

/**
 * @brief Maximum length of data (in bytes) that can be transmitted to the peer over GATT
 */
#define BLE_GATT_MAX_DATA_LEN BLE_GATT_MAX_DATA_LEN_CALC(CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE)

/**
 * @brief MCUmgr Bluetooth service client context structure.
 *
 * @details This structure contains state context related to hosts.
 */
struct ble_mcumgr_client_context {
	/** Indicate if the peer has enabled notification of the RX characteristic */
	bool is_notification_enabled;
};

/**
 * @brief MCUmgr Bluetooth service structure.
 *
 * @details This structure contains status information related to the service.
 */
struct ble_mcumgr {
	/** UUID types for MCUmgr Bluetooth service Base UUID and characteristic UUID */
	uint8_t uuid_type_service;
	uint8_t uuid_type_characteristic;
	/** Handle of MCUmgr Bluetooth service (as provided by the SoftDevice) */
	uint16_t service_handle;
	/** Handle of the MCUmgr characteristic (as provided by the SoftDevice) */
	ble_gatts_char_handles_t characteristic_handle;
};

/** Handle of the current connection */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
static bool should_reboot;
static bool device_disconnected;
static struct ble_mcumgr ble_mcumgr;

static enum mgmt_cb_return os_mgmt_reboot_hook(uint32_t event, enum mgmt_cb_return prev_status,
					       int32_t *rc, uint16_t *group, bool *abort_more,
					       void *data, size_t data_size);

static struct mgmt_callback os_mgmt_reboot_callback = {
	.callback = os_mgmt_reboot_hook,
	.event_id = MGMT_EVT_OP_OS_MGMT_RESET,
};

static struct smp_transport smp_ncs_bm_bt_transport;
static struct ble_mcumgr_client_context contexts[CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT];

static struct ble_mcumgr_client_context *ble_mcumgr_client_context_get(uint16_t conn_handle)
{
	const int idx = nrf_sdh_ble_idx_get(conn_handle);

	return ((idx >= 0) ? &contexts[idx] : NULL);
}

static uint32_t mcumgr_characteristic_add(struct ble_mcumgr *service)
{
	ble_uuid_t char_uuid = {
		.type = service->uuid_type_characteristic,
		.uuid = BLE_MCUMGR_CHARACTERISTIC_UUID_SUB,
	};
	ble_gatts_attr_md_t cccd_md = {
		.vloc = BLE_GATTS_VLOC_STACK
	};
	ble_gatts_char_md_t char_md = {
		.char_props = {
			.write_wo_resp = true,
			.notify = true,
		},
		.p_cccd_md = &cccd_md,
	};
	ble_gatts_attr_md_t attr_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.vlen = true,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = NULL,
		.init_len = 0,
		.max_len = BLE_GATT_MAX_DATA_LEN,
	};

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

	/* Setup CCCD */
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

	return sd_ble_gatts_characteristic_add(service->service_handle, &char_md, &attr_char_value,
					       &service->characteristic_handle);
}

/**
 * @brief Function for handling the @ref BLE_GAP_EVT_CONNECTED event from the SoftDevice.
 *
 * @param[in] service MCUmgr service structure.
 * @param[in] ble_evt Pointer to the event received from BLE stack.
 */
static void on_connect(struct ble_mcumgr *service, ble_evt_t const *ble_evt)
{
	int err;
	uint8_t cccd_value[2];
	ble_gatts_value_t gatts_val = {
		.p_value = cccd_value,
		.len = sizeof(cccd_value),
		.offset = 0,
	};
	struct ble_mcumgr_client_context *ctx;

	ctx = ble_mcumgr_client_context_get(conn_handle);

	if (ctx == NULL) {
		LOG_ERR("Could not fetch MCUmgr context for connection handle %#x", conn_handle);
		return;
	}

	/* Check the hosts CCCD value to inform of readiness to send data using the
	 * RX characteristic
	 */
	err = sd_ble_gatts_value_get(conn_handle, service->characteristic_handle.cccd_handle,
				     &gatts_val);

	if (err == 0 && is_notification_enabled(gatts_val.p_value)) {
		ctx->is_notification_enabled = true;
	}
}

/**
 * @brief Function for handling the @ref BLE_GATTS_EVT_WRITE event from the SoftDevice.
 *
 * @param[in] service MCUmgr service structure.
 * @param[in] ble_evt Pointer to the event received from BLE stack.
 */
static void on_write(struct ble_mcumgr *service, ble_evt_t const *ble_evt)
{
	const uint16_t conn_handle = ble_evt->evt.gatts_evt.conn_handle;
	const ble_gatts_evt_write_t *evt_write = &ble_evt->evt.gatts_evt.params.write;
	struct ble_mcumgr_client_context *ctx;

	ctx = ble_mcumgr_client_context_get(conn_handle);

	if (ctx == NULL) {
		LOG_ERR("Could not fetch MCUmgr context for connection handle %#x", conn_handle);
		return;
	}

	LOG_DBG("Link ctx %p", ctx);

	if (evt_write->handle == service->characteristic_handle.cccd_handle &&
	    evt_write->len == 2) {
		if (is_notification_enabled(evt_write->data)) {
			ctx->is_notification_enabled = true;
		} else {
			ctx->is_notification_enabled = false;
		}
	} else if (evt_write->handle == service->characteristic_handle.value_handle) {
#ifdef CONFIG_MCUMGR_TRANSPORT_REASSEMBLY
		int ret;
		bool started;

		started = (smp_reassembly_expected(&smp_ncs_bm_bt_transport) >= 0);

		LOG_DBG("Started = %s, buf len = %d", started ? "true" : "false", evt_write->len);

		ret = smp_reassembly_collect(&smp_ncs_bm_bt_transport, evt_write->data,
					     evt_write->len);
		LOG_DBG("Collect = %d", ret);

		/*
		 * Collection can fail only due to failing to allocate memory or by receiving
		 * more data than expected.
		 */
		if (ret == -ENOMEM) {
			/* Failed to collect the buffer */
			LOG_ERR("Failed to collect buffer");
			return;
		} else if (ret < 0) {
			/* Failed operation on already allocated buffer, drop the packet and report
			 * error.
			 */
			smp_reassembly_drop(&smp_ncs_bm_bt_transport);
			LOG_ERR("Failed with operation on buffer");
			return;
		}

		/* No more bytes are expected for this packet */
		if (ret == 0) {
			smp_reassembly_complete(&smp_ncs_bm_bt_transport, false);
		}
#else
		struct smp_bt_user_data *ud;
		struct net_buf *nb;

		nb = smp_packet_alloc();

		if (!nb) {
			LOG_ERR("Failed net_buf alloc for SMP packet");
			return;
		}

		if (net_buf_tailroom(nb) < evt_write->len) {
			LOG_ERR("SMP packet len (%" PRIu16 ") > net_buf len (%zu)",
			evt_write->len, net_buf_tailroom(nb));
			smp_packet_free(nb);
			return;
		}

		net_buf_add_mem(nb, evt_write->data, evt_write->len);
		smp_rx_req(&smp_ncs_bm_bt_transport, nb);
#endif
	}
}

int ble_mcumgr_data_send(uint8_t *data, uint16_t *len, struct ble_mcumgr_client_context *ctx)
{
	int err;
	ble_gatts_hvx_params_t hvx_params = {
		.handle = ble_mcumgr.characteristic_handle.value_handle,
		.p_data = data,
		.p_len = len,
		.type = BLE_GATT_HVX_NOTIFICATION
	};

	if (!data || !len) {
		return -EFAULT;
	} else if (*len > BLE_GATT_MAX_DATA_LEN) {
		return -EINVAL;
	} else if (!ctx->is_notification_enabled) {
		return -EINVAL;
	}

	err = sd_ble_gatts_hvx(conn_handle, &hvx_params);

	switch (err) {
	case NRF_SUCCESS:
	{
		return 0;
	}
	case BLE_ERROR_INVALID_CONN_HANDLE:
	{
		return -ENOTCONN;
	}
	case NRF_ERROR_INVALID_STATE:
	{
		return -EPIPE;
	}
	case NRF_ERROR_RESOURCES:
	{
		return -EAGAIN;
	}
	case NRF_ERROR_NOT_FOUND:
	{
		return -EBADF;
	}
	default:
	{
		LOG_ERR("Failed to send MCUmgr data, nrf_error %#x", err);
		return -EIO;
	}
	};
}

static int smp_ncs_bm_bt_tx_pkt(struct net_buf *nb)
{
	int rc;
	struct ble_mcumgr_client_context *ctx;
	uint16_t notification_size;
	uint8_t *send_pos = nb->data;
	uint16_t send_size = 0;

	if (conn_handle == BLE_CONN_HANDLE_INVALID) {
		return -ENOENT;
	}

	ctx = &contexts[0];
	rc = ble_conn_params_att_mtu_get(conn_handle, &notification_size);

	if (rc) {
		goto finish;
	}

	notification_size = BLE_GATT_MAX_DATA_LEN_CALC(notification_size);

	while ((nb->data + nb->len) > send_pos) {
		send_size = (nb->data + nb->len) - send_pos;

		if (send_size > notification_size) {
			send_size = notification_size;
		}

		rc = ble_mcumgr_data_send(send_pos, &send_size, ctx);

		if (rc) {
			break;
		}

		send_pos += send_size;
	}

finish:
	smp_packet_free(nb);

	return 0;
}

/**
 * @brief BLE event handler.
 *
 * @param[in] evt BLE event.
 * @param[in] ctx Context.
 */
static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	int err;
	struct ble_mcumgr *mcumgr_data = (struct ble_mcumgr *)ctx;

	__ASSERT(ble_evt, "BLE event is NULL");
	__ASSERT(ctx, "context is NULL");

	if (ctx == NULL || evt == NULL) {
		return;
	}

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
	{
		LOG_INF("Peer connected");
		conn_handle = evt->evt.gap_evt.conn_handle;
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);

		if (err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", err);
		}

		on_connect(mcumgr_data, evt);
		break;
	}

	case BLE_GAP_EVT_DISCONNECTED:
	{
		LOG_INF("Peer disconnected");
		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;

			if (should_reboot) {
				device_disconnected = true;
			}
		}
		break;
	}

	case BLE_GATTS_EVT_WRITE:
	{
		on_write(mcumgr_data, evt);
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
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);

		if (err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", err);
		}

		break;
	}
	};
}

NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, &ble_mcumgr, 0);

/**
 * @brief Connection parameters event handler
 *
 * @param[in] evt BLE connection parameters event.
 */
void on_conn_params_evt(const struct ble_conn_params_evt *evt)
{
	int err;

	switch (evt->id) {
	case BLE_CONN_PARAMS_EVT_REJECTED:
	{
		err = sd_ble_gap_disconnect(evt->conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);

		if (err) {
			LOG_ERR("Disconnect failed on conn params update rejection, nrf_error %#x",
				err);
		} else {
			LOG_INF("Disconnected from peer, unacceptable conn params");
		}

		break;
	}

	default:
		break;
	}
}

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

static int ble_mcumgr_init(struct ble_mcumgr *service)
{
	int err;
	ble_uuid_t ble_uuid;
	ble_uuid128_t uuid_base_service = {
		.uuid128 = BLE_MCUMGR_SERVICE_UUID
	};
	ble_uuid128_t uuid_base_characteristic = {
		.uuid128 = BLE_MCUMGR_CHARACTERISTIC_UUID
	};

	if (!service) {
		return -EFAULT;
	}

	/* Initialize the service structure */
	service->service_handle = BLE_CONN_HANDLE_INVALID;

	/* Add MCUmgr service/characteristic UUIDs */
	err = sd_ble_uuid_vs_add(&uuid_base_service, &service->uuid_type_service);

	if (err) {
		LOG_ERR("sd_ble_uuid_vs_add failed, nrf_error %#x", err);
		return -EINVAL;
	}

	err = sd_ble_uuid_vs_add(&uuid_base_characteristic, &service->uuid_type_characteristic);

	if (err) {
		LOG_ERR("sd_ble_uuid_vs_add failed, nrf_error %#x", err);
		return -EINVAL;
	}

	ble_uuid.type = service->uuid_type_service;
	ble_uuid.uuid = BLE_MCUMGR_SERVICE_UUID_SUB;

	/* Add the service */
	err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
				       &ble_uuid,
				       &service->service_handle);

	if (err) {
		LOG_ERR("Failed to add MCUmgr service, nrf_error %#x", err);
		return -EINVAL;
	}

	/* Add MCUmgr characteristic */
	err = mcumgr_characteristic_add(service);

	if (err) {
		LOG_ERR("mcumgr_characteristic_add failed, nrf_error %#x", err);
		return -EINVAL;
	}

	return 0;
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

	err = ble_mcumgr_init(&ble_mcumgr);

	if (err) {
		LOG_ERR("Failed to initialize MCUmgr service, err %d", err);
		return 0;
	}

	LOG_INF("Services initialized");

	err = ble_conn_params_evt_handler_set(on_conn_params_evt);

	if (err) {
		LOG_ERR("Failed to setup conn param event handler, err %d", err);
		return 0;
	}

	err = ble_adv_init(&ble_adv, &ble_adv_cfg);

	if (err) {
		LOG_ERR("Failed to initialize advertising, err %d", err);
		return 0;
	}

	err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);

	if (err) {
		LOG_ERR("Failed to start advertising, err %d", err);
		return 0;
	}

	LOG_INF("Advertising as %s", CONFIG_BLE_ADV_NAME);

	while (should_reboot == false) {
		sd_app_evt_wait();
	}

	err = sd_ble_gap_disconnect(conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);

	if (err != NRF_SUCCESS) {
		device_disconnected = true;
	}

	while (device_disconnected == false) {
		sd_app_evt_wait();
	}

	sys_reboot(SYS_REBOOT_WARM);
}

static uint16_t smp_ncs_bm_bt_get_mtu(const struct net_buf *nb)
{
	return BLE_GATT_MAX_DATA_LEN;
}

static void smp_ncs_bm_bt_setup(void)
{
	int rc;

	smp_ncs_bm_bt_transport.functions.output = smp_ncs_bm_bt_tx_pkt;
	smp_ncs_bm_bt_transport.functions.get_mtu = smp_ncs_bm_bt_get_mtu;

	rc = smp_transport_init(&smp_ncs_bm_bt_transport);
}

MCUMGR_HANDLER_DEFINE(smp_ncs_bm_bt, smp_ncs_bm_bt_setup);
