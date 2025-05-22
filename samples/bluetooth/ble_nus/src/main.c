/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <string.h>
#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <ble_adv.h>
#include <ble_conn_params.h>
#include <ble_gap.h>
#include <ble_qwr.h>
#include <bluetooth/services/ble_nus.h>
#include <nrf_soc.h>
#include <nrfx_uarte.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(app, CONFIG_BLE_NUS_SAMPLE_LOG_LEVEL);

BLE_ADV_DEF(ble_adv); /* BLE advertising instance */
BLE_NUS_DEF(ble_nus); /* BLE NUS service instance */
BLE_QWR_DEF(ble_qwr); /* BLE QWR instance */

/** Handle of the current connection. */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

/** NUS UARTE instance */
#if defined(CONFIG_SOC_SERIES_NRF52X)
static const nrfx_uarte_t uarte_inst = NRFX_UARTE_INSTANCE(0);
#elif defined(CONFIG_SOC_SERIES_NRF54LX)
static const nrfx_uarte_t uarte_inst = NRFX_UARTE_INSTANCE(30);
#endif

/* Maximum length of data (in bytes) that can be transmitted to the peer by the
 * Nordic UART service module.
 */
static volatile uint16_t ble_nus_max_data_len = BLE_NUS_MAX_DATA_LEN_CALC(BLE_GATT_ATT_MTU_DEFAULT);

/* Receive buffer used in UART ISR callback */
static uint8_t uarte_rx_buf[4];
static int buf_idx;

/**
 * @brief Handle data received from UART.
 *
 * @param[in] data Data received.
 * @param[in] data_len Size of data.
 */
static void uarte_rx_handler(char *data, size_t data_len)
{
	int err;
	uint8_t c;
	/* receive buffer used in UART ISR callback */
	static char rx_buf[BLE_NUS_MAX_DATA_LEN];
	static uint16_t rx_buf_idx;
	uint16_t len;

	for (int i = 0; i < data_len; i++) {
		c = data[i];

		if ((rx_buf_idx < sizeof(rx_buf)) && (c != '\n') && (c != '\r')) {
			rx_buf[rx_buf_idx++] = c;
		}

		if ((c == '\n' || c == '\r') || (rx_buf_idx >= ble_nus_max_data_len)) {
			if (rx_buf_idx == 0) {
				/* RX buffer is empty, nothing to send. */
				continue;
			}

			len = rx_buf_idx;
			LOG_INF("Sending data over BLE NUS, len %d", len);

			do {
				err = ble_nus_data_send(&ble_nus, rx_buf, &len, conn_handle);
				if ((err != 0) &&
				    (err != -EPIPE) &&
				    (err != -EAGAIN) &&
				    (err != -EBADF)) {
					LOG_ERR("Failed to send NUS data, err %d", err);
					return;
				}
			} while (err == -EAGAIN);

			if (len == rx_buf_idx) {
				rx_buf_idx = 0;
			} else {
				/* Not all data in RX buffer was transmitted.
				 * Move what is left to start of buffer.
				 */
				memmove(&rx_buf[len], &rx_buf[0], rx_buf_idx - len);
				rx_buf_idx -= len;
			}
		}
	}
}

/**
 * @brief UARTE event handler
 *
 * @param[in] event UARTE event structure
 * @param[in] ctx Context. NULL in this case.
 */
static void uarte_evt_handler(nrfx_uarte_event_t const *event, void *ctx)
{
	switch (event->type) {
	case NRFX_UARTE_EVT_RX_DONE:
		LOG_DBG("Received data from UART: %c", event->data.rx.p_buffer[0]);
		if (event->data.rx.length > 0) {
			uarte_rx_handler(event->data.rx.p_buffer, event->data.rx.length);
		}

		nrfx_uarte_rx_enable(&uarte_inst, 0);
		break;
	case NRFX_UARTE_EVT_RX_BUF_REQUEST:
		nrfx_uarte_rx_buffer_set(&uarte_inst, &uarte_rx_buf[buf_idx], 1);

		buf_idx++;
		buf_idx = (buf_idx < sizeof(uarte_rx_buf)) ? buf_idx : 0;
		break;
	case NRFX_UARTE_EVT_ERROR:
		LOG_ERR("uarte error %#x", event->data.error.error_mask);
		break;
	default:
		break;
	}
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

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		ble_nus_max_data_len = BLE_NUS_MAX_DATA_LEN_CALC(BLE_GATT_ATT_MTU_DEFAULT);
		conn_handle = evt->evt.gap_evt.conn_handle;
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", err);
		}

		err = ble_qwr_conn_handle_assign(&ble_qwr, conn_handle);
		if (err) {
			LOG_ERR("Failed to assign qwr handle, err %d", err);
			return;
		}
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Peer disconnected");
		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}
		break;

	case BLE_GAP_EVT_AUTH_STATUS:
		LOG_INF("Authentication status: %#x",
			evt->evt.gap_evt.params.auth_status.auth_status);
		break;

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		/* Pairing not supported */
		err = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle,
						  BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
		if (err) {
			LOG_ERR("Failed to reply with Security params, nrf_error %#x", err);
		}
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		LOG_INF("BLE_GATTS_EVT_SYS_ATTR_MISSING");
		/* No system attributes have been stored */
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", err);
		}
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, 0);

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
		err = sd_ble_gap_disconnect(evt->conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
		if (err) {
			LOG_ERR("Disconnect failed on conn params update rejection, nrf_error %#x",
				err);
		} else {
			LOG_INF("Disconnected from peer, unacceptable conn params");
		}
		break;

	case BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED:
		if (evt->conn_handle != conn_handle) {
			LOG_DBG("Connection handle does not match, expected %d, was %d",
				conn_handle, evt->conn_handle);
			break;
		}
		ble_nus_max_data_len = BLE_NUS_MAX_DATA_LEN_CALC(evt->att_mtu);
		LOG_INF("NUS max data length updated to %d", ble_nus_max_data_len);
		break;

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
	switch (adv_evt->evt_type) {
	case BLE_ADV_EVT_ERROR:
		LOG_ERR("Advertising error %d", adv_evt->error.reason);
		break;
	default:
		break;
	}
}

uint16_t ble_qwr_evt_handler(struct ble_qwr *qwr, const struct ble_qwr_evt *qwr_evt)
{
	switch (qwr_evt->evt_type) {
	case BLE_QWR_EVT_ERROR:
		LOG_ERR("QWR error event, nrf_error 0x%x", qwr_evt->error.reason);
		break;
	case BLE_QWR_EVT_EXECUTE_WRITE:
		LOG_INF("QWR execute write event");
		break;
	case BLE_QWR_EVT_AUTH_REQUEST:
		LOG_INF("QWR auth request event");
		break;
	}

	return BLE_GATT_STATUS_SUCCESS;
}

/**
 * @brief BLE NUS data handler
 *
 * @param[in] evt NUS event parameters.
 */
static void ble_nus_evt_handler(const struct ble_nus_evt *evt)
{
	const char newline = '\n';

	if (evt->type != BLE_NUS_EVT_RX_DATA) {
		return;
	}

	/* Handle incoming data */
	LOG_DBG("Received data from BLE NUS: %s", evt->params.rx_data.data);

	for (uint32_t i = 0; i < evt->params.rx_data.length; i++) {
		nrfx_uarte_tx(&uarte_inst, &evt->params.rx_data.data[i], 1, NRFX_UARTE_TX_BLOCKING);
	}

	if (evt->params.rx_data.data[evt->params.rx_data.length - 1] == '\r') {
		nrfx_uarte_tx(&uarte_inst, &newline, 1, NRFX_UARTE_TX_BLOCKING);
	}
}

/**
 * @brief Initalize UARTE driver.
 */
static int uarte_init(void)
{
	int err;

	nrfx_uarte_config_t uarte_config = NRFX_UARTE_DEFAULT_CONFIG(CONFIG_BLE_UART_PIN_TX,
								     CONFIG_BLE_UART_PIN_RX);

#if defined(CONFIG_BLE_UART_HWFC)
	uarte_config.config.hwfc = NRF_UARTE_HWFC_ENABLED;
	uarte_config.cts_pin = CONFIG_BLE_UART_PIN_CTS;
	uarte_config.rts_pin = CONFIG_BLE_UART_PIN_RTS;
#endif

#if defined(CONFIG_BLE_UART_PARITY)
	uarte_config.parity = NRF_UARTE_PARITY_INCLUDED;
#endif

	uarte_config.interrupt_priority = CONFIG_BLE_UART_IRQ_PRIO;

	/** We need to connect the IRQ ourselves. */
#if defined(CONFIG_SOC_SERIES_NRF52X)
	IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(0)), CONFIG_BLE_UART_IRQ_PRIO,
		    NRFX_UARTE_INST_HANDLER_GET(0), 0, 0);

		irq_enable(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(0)));
#elif defined(CONFIG_SOC_SERIES_NRF54LX)
	IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(30)), CONFIG_BLE_UART_IRQ_PRIO,
		    NRFX_UARTE_INST_HANDLER_GET(30), 0, 0);

	irq_enable(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(30)));
#endif

	err = nrfx_uarte_init(&uarte_inst, &uarte_config, uarte_evt_handler);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("Failed to initialize UART, nrfx err %d", err);
		return err;
	}

	/* optional: enable pull-up on RX pin in case pin may become floating.
	 * Induced noise on a floating RX input may lead to an UARTE error condition
	 */
#if defined(CONFIG_SOC_SERIES_NRF52X)
	NRF_GPIO->PIN_CNF[uarte_config.rxd_pin] |=
		(GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos);
#endif

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

	struct ble_nus_config nus_cfg = {
		.evt_handler = ble_nus_evt_handler,
	};
	struct ble_qwr_config qwr_config = {
		.evt_handler = ble_qwr_evt_handler,
	};

	LOG_INF("BLE NUS sample started");

	err = uarte_init();
	if (err) {
		LOG_ERR("Failed to enable UARTE, err %d", err);
		return -1;
	}

	err = nrf_sdh_enable_request();
	if (err) {
		LOG_ERR("Failed to enable SoftDevice, err %d", err);
		return -1;
	}

	LOG_INF("SoftDevice enabled");

	err = nrf_sdh_ble_enable(CONFIG_NRF_SDH_BLE_CONN_TAG);
	if (err) {
		LOG_ERR("Failed to enable BLE, err %d", err);
		return -1;
	}

	LOG_INF("Bluetooth enabled");

	err = ble_qwr_init(&ble_qwr, &qwr_config);
	if (err) {
		LOG_ERR("ble_qwr_init failed, err %d", err);
		return -1;
	}

	err = ble_nus_init(&ble_nus, &nus_cfg);
	if (err) {
		LOG_ERR("Failed to initialize Nordic uart service, err %d", err);
		return -1;
	}

	/* Adding the Nordic UART Service UUID to the scan response data. */
	ble_uuid_t adv_uuid_list[] = {
		/* Using a vendor specific UUID type that was added during NUS initialization. */
		{ .uuid = BLE_UUID_NUS_SERVICE, .type = ble_nus.uuid_type },
	};
	ble_adv_cfg.sr_data.uuid_lists.complete.uuid = &adv_uuid_list[0];
	ble_adv_cfg.sr_data.uuid_lists.complete.len = ARRAY_SIZE(adv_uuid_list);

	LOG_INF("Services initialized");

	err = ble_conn_params_evt_handler_set(on_conn_params_evt);
	if (err) {
		LOG_ERR("Failed to setup conn param event handler, err %d", err);
		return -1;
	}

	err = ble_adv_init(&ble_adv, &ble_adv_cfg);
	if (err) {
		LOG_ERR("Failed to initialize advertising, err %d", err);
		return -1;
	}

	const uint8_t out[] = "UART started.\r\n";

	err = nrfx_uarte_tx(&uarte_inst, out, sizeof(out), NRFX_UARTE_TX_BLOCKING);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("UARTE TX failed, nrfx err %d", err);
		return -1;
	}

	err = nrfx_uarte_rx_enable(&uarte_inst, 0);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("UART RX failed, nrfx err %d", err);
	}

	err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (err) {
		LOG_ERR("Failed to start advertising, err %d", err);
		return -1;
	}

	LOG_INF("Advertising as %s", CONFIG_BLE_ADV_NAME);
#if defined(CONFIG_SOC_SERIES_NRF54LX)
	LOG_INF("The NUS service is handled at a separate uart instance");
#endif

	while (true) {
		while (LOG_PROCESS()) {
			/* Empty. */
		}

		sd_app_evt_wait();
	}
}
