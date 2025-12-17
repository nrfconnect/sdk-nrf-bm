/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ble_gap.h>
#include <nrf_soc.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/services/common.h>
#include <bm/bluetooth/services/uuid.h>
#include <bm/bluetooth/services/ble_mcumgr.h>

#include <zephyr/kernel.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/smp/smp.h>
#include <zephyr/mgmt/mcumgr/mgmt/handlers.h>
#include <mgmt/mcumgr/transport/smp_internal.h>
#include <mgmt/mcumgr/transport/smp_reassembly.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mcumgr, CONFIG_BLE_MCUMGR_LOG_LEVEL);

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
static struct ble_mcumgr ble_mcumgr;
static struct smp_transport smp_ncs_bm_bt_transport;

static uint32_t mcumgr_characteristic_add(struct ble_mcumgr *service,
					  const struct ble_mcumgr_config *cfg)
{

	__ASSERT(service, "cfg is NULL");
	__ASSERT(cfg, "cfg is NULL");

	ble_uuid_t char_uuid = {
		.type = service->uuid_type_characteristic,
		.uuid = BLE_MCUMGR_CHARACTERISTIC_UUID_SUB,
	};
	ble_gatts_attr_md_t cccd_md = {
		.vloc = BLE_GATTS_VLOC_STACK,
		.read_perm = BLE_GAP_CONN_SEC_MODE_OPEN,
		.write_perm = cfg->sec_mode.mcumgr_char.cccd_write,
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
		.read_perm = cfg->sec_mode.mcumgr_char.read,
		.write_perm = cfg->sec_mode.mcumgr_char.write,
	};
	ble_gatts_attr_t attr_char_value = {
		.p_uuid = &char_uuid,
		.p_attr_md = &attr_md,
		.p_value = NULL,
		.init_len = 0,
		.max_len = BLE_GATT_MAX_DATA_LEN,
	};

	return sd_ble_gatts_characteristic_add(service->service_handle, &char_md, &attr_char_value,
					       &service->characteristic_handle);
}

/**
 * @brief Function for handling the @ref BLE_GATTS_EVT_WRITE event from the SoftDevice.
 *
 * @param[in] service MCUmgr service structure.
 * @param[in] ble_evt Pointer to the event received from BLE stack.
 */
static void on_write(struct ble_mcumgr *service, const ble_evt_t *ble_evt)
{
	const ble_gatts_evt_write_t *evt_write = &ble_evt->evt.gatts_evt.params.write;

	if (evt_write->handle == service->characteristic_handle.value_handle) {
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
		if (ret == NRF_ERROR_NO_MEM) {
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

static uint32_t ble_mcumgr_data_send(uint8_t *data, uint16_t *len)
{
	uint32_t nrf_err;
	ble_gatts_hvx_params_t hvx_params = {
		.handle = ble_mcumgr.characteristic_handle.value_handle,
		.p_data = data,
		.p_len = len,
		.type = BLE_GATT_HVX_NOTIFICATION
	};

	if (!data || !len) {
		return NRF_ERROR_NULL;
	} else if (*len > BLE_GATT_MAX_DATA_LEN) {
		return NRF_ERROR_INVALID_PARAM;
	}

	nrf_err = sd_ble_gatts_hvx(conn_handle, &hvx_params);

	switch (nrf_err) {
	case NRF_SUCCESS:
	{
		return NRF_SUCCESS;
	}
	case BLE_ERROR_INVALID_CONN_HANDLE:
	{
		return NRF_ERROR_NOT_FOUND;
	}
	case NRF_ERROR_INVALID_STATE:
	{
		return NRF_ERROR_INVALID_STATE;
	}
	case NRF_ERROR_RESOURCES:
	{
		return NRF_ERROR_RESOURCES;
	}
	case NRF_ERROR_NOT_FOUND:
	{
		return NRF_ERROR_NOT_FOUND;
	}
	default:
	{
		LOG_ERR("Failed to send MCUmgr data, nrf_error %#x", nrf_err);
		return NRF_ERROR_INTERNAL;
	}
	};
}

/* Return errno! */
static int smp_ncs_bm_bt_tx_pkt(struct net_buf *nb)
{
	uint32_t nrf_err;
	uint16_t notification_size;
	uint8_t *send_pos = nb->data;
	uint16_t send_size = 0;

	if (conn_handle == BLE_CONN_HANDLE_INVALID) {
		return -ENOENT;
	}

	nrf_err = ble_conn_params_att_mtu_get(conn_handle, &notification_size);
	if (nrf_err) {
		goto finish;
	}

	notification_size = BLE_GATT_MAX_DATA_LEN_CALC(notification_size);

	while ((nb->data + nb->len) > send_pos) {
		send_size = (nb->data + nb->len) - send_pos;

		if (send_size > notification_size) {
			send_size = notification_size;
		}

		nrf_err = ble_mcumgr_data_send(send_pos, &send_size);

		if (nrf_err) {
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
	struct ble_mcumgr *mcumgr_data = (struct ble_mcumgr *)ctx;

	__ASSERT(evt, "BLE event is NULL");
	__ASSERT(ctx, "context is NULL");

	if (ctx == NULL || evt == NULL) {
		return;
	}

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
	{
		conn_handle = evt->evt.gap_evt.conn_handle;
		break;
	}

	case BLE_GAP_EVT_DISCONNECTED:
	{
		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}
		break;
	}

	case BLE_GATTS_EVT_WRITE:
	{
		on_write(mcumgr_data, evt);
		break;
	}
	};
}

NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, &ble_mcumgr, HIGH);

uint32_t ble_mcumgr_init(const struct ble_mcumgr_config *cfg)
{
	uint32_t nrf_err;
	ble_uuid_t ble_uuid;
	ble_uuid128_t uuid_base_service = {
		.uuid128 = BLE_MCUMGR_SERVICE_UUID
	};
	ble_uuid128_t uuid_base_characteristic = {
		.uuid128 = BLE_MCUMGR_CHARACTERISTIC_UUID
	};

	if (!cfg) {
		return NRF_ERROR_NULL;
	}

	/* Initialize the service structure */
	ble_mcumgr.service_handle = BLE_CONN_HANDLE_INVALID;

	/* Add MCUmgr service/characteristic UUIDs */
	nrf_err = sd_ble_uuid_vs_add(&uuid_base_service, &ble_mcumgr.uuid_type_service);

	if (nrf_err) {
		LOG_ERR("sd_ble_uuid_vs_add failed, nrf_error %#x", nrf_err);
		return NRF_ERROR_INVALID_PARAM;
	}

	nrf_err = sd_ble_uuid_vs_add(&uuid_base_characteristic,
				     &ble_mcumgr.uuid_type_characteristic);

	if (nrf_err) {
		LOG_ERR("sd_ble_uuid_vs_add failed, nrf_error %#x", nrf_err);
		return NRF_ERROR_INVALID_PARAM;
	}

	ble_uuid.type = ble_mcumgr.uuid_type_service;
	ble_uuid.uuid = BLE_MCUMGR_SERVICE_UUID_SUB;

	/* Add the service */
	nrf_err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
					   &ble_uuid,
					   &ble_mcumgr.service_handle);

	if (nrf_err) {
		LOG_ERR("Failed to add MCUmgr service, nrf_error %#x", nrf_err);
		return NRF_ERROR_INVALID_PARAM;
	}

	/* Add MCUmgr characteristic */
	nrf_err = mcumgr_characteristic_add(&ble_mcumgr, cfg);

	if (nrf_err) {
		LOG_ERR("mcumgr_characteristic_add failed, nrf_error %#x", nrf_err);
		return NRF_ERROR_INVALID_PARAM;
	}

	return 0;
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

uint8_t ble_mcumgr_service_uuid_type(void)
{
	return ble_mcumgr.uuid_type_service;
}

MCUMGR_HANDLER_DEFINE(smp_ncs_bm_bt, smp_ncs_bm_bt_setup);
