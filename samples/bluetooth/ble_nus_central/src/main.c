/**
 * Copyright (c) 2016 - 2026, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/ble_scan.h>
#include <bm/bluetooth/services/ble_nus.h>
#include <bm/bluetooth/services/ble_nus_client.h>
#include <bm/bm_buttons.h>
#include <bm/bm_irq.h>
#include <nrfx_uarte.h>
#if defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
#include <bm/drivers/bm_lpuarte.h>
#endif

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_BLE_NUS_CENTRAL_LOG_LEVEL);

/** BLE Nordic UART Service (NUS) client instance. */
BLE_NUS_CLIENT_DEF(ble_nus_client);
/** Database discovery module instance. */
BLE_DB_DISCOVERY_DEF(ble_db_disc);
/** Scanning Module instance. */
BLE_SCAN_DEF(ble_scan);
/** BLE GATT Queue instance. */
BLE_GQ_DEF(ble_gq);

/** Handle of the current connection. */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

/** NUS UARTE instance and board config */
#if defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
#define NUS_UARTE_INST	  BOARD_APP_LPUARTE_INST
#define NUS_UARTE_PIN_TX  BOARD_APP_LPUARTE_PIN_TX
#define NUS_UARTE_PIN_RX  BOARD_APP_LPUARTE_PIN_RX
#define NUS_UARTE_PIN_RDY BOARD_APP_LPUARTE_PIN_RDY
#define NUS_UARTE_PIN_REQ BOARD_APP_LPUARTE_PIN_REQ

static struct bm_lpuarte lpu;
#else
#define NUS_UARTE_INST	  BOARD_APP_UARTE_INST
#define NUS_UARTE_PIN_TX  BOARD_APP_UARTE_PIN_TX
#define NUS_UARTE_PIN_RX  BOARD_APP_UARTE_PIN_RX
#define NUS_UARTE_PIN_CTS BOARD_APP_UARTE_PIN_CTS
#define NUS_UARTE_PIN_RTS BOARD_APP_UARTE_PIN_RTS
#endif /* CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE */

/** UUID type for the Nordic UART Service (vendor specific). */
#define NUS_SERVICE_UUID_TYPE BLE_UUID_TYPE_VENDOR_BEGIN

/** NUS Client UARTE instance. */
static nrfx_uarte_t nus_uarte_inst = NRFX_UARTE_INSTANCE(NUS_UARTE_INST);

/** Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART
 *  service module.
 */
static uint16_t ble_nus_max_data_len = BLE_NUS_CLIENT_MAX_DATA_LEN_CALC(BLE_GATT_ATT_MTU_DEFAULT);

/** Receive buffers used in UART ISR callback. */
static uint8_t uarte_rx_buf[CONFIG_SAMPLE_NUS_CENTRAL_UART_RX_BUF_SIZE][2];
static int buf_idx;

/* Forward declaration. */
static uint32_t scan_start(void);

#if defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
static void lpuarte_rx_handler(char *data, size_t data_len)
{
	uint32_t nrf_err;
	uint16_t len = data_len;

	LOG_INF("Sending NUS data, len %d", len);

	do {
		nrf_err = ble_nus_client_string_send(&ble_nus_client, data, len);
		if ((nrf_err) && (nrf_err != NRF_ERROR_INVALID_STATE) &&
		    (nrf_err != NRF_ERROR_RESOURCES) && (nrf_err != NRF_ERROR_NOT_FOUND)) {
			LOG_ERR("Failed to send NUS data, nrf_error %#x", nrf_err);
			return;
		}
	} while (nrf_err == NRF_ERROR_RESOURCES);
}
#else
static void uarte_rx_handler(char *data, size_t data_len)
{
	uint32_t nrf_err;
	uint8_t c;
	/** receive buffer used in UART ISR callback. */
	static char rx_buf[BLE_NUS_MAX_DATA_LEN];
	static uint16_t rx_buf_idx;
	uint16_t len;

	for (int i = 0; i < data_len; i++) {
		c = data[i];

		if (rx_buf_idx < sizeof(rx_buf)) {
			rx_buf[rx_buf_idx++] = c;
		}

		if ((c == '\n' || c == '\r') || (rx_buf_idx >= ble_nus_max_data_len)) {
			if (rx_buf_idx == 0) {
				/** RX buffer is empty, nothing to send. */
				continue;
			}

			len = rx_buf_idx;
			LOG_INF("Sending NUS data, len %d", len);

			do {
				nrf_err = ble_nus_client_string_send(&ble_nus_client, rx_buf,
									 len);
				if ((nrf_err != NRF_ERROR_INVALID_STATE) &&
				    (nrf_err != NRF_ERROR_RESOURCES) && nrf_err) {
					LOG_ERR("Failed to send NUS data, nrf_error %#x", nrf_err);
					return;
				}
			} while (nrf_err == NRF_ERROR_RESOURCES);
			rx_buf_idx = 0;
		}
	}
}
#endif /* CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE */

static void uarte_evt_handler(const nrfx_uarte_event_t *event, void *ctx)
{
	switch (event->type) {
	case NRFX_UARTE_EVT_RX_DONE:
		LOG_INF("Received data from UART: %.*s (%d)", event->data.rx.length,
			event->data.rx.p_buffer, event->data.rx.length);
		if (event->data.rx.length > 0) {
#if defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
			lpuarte_rx_handler(event->data.rx.p_buffer, event->data.rx.length);
#else
			uarte_rx_handler(event->data.rx.p_buffer, event->data.rx.length);
#endif
		}
#if !defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
		(void)nrfx_uarte_rx_enable(&nus_uarte_inst, 0);
#endif
		break;
	case NRFX_UARTE_EVT_RX_BUF_REQUEST:
#if defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
		(void)bm_lpuarte_rx_buffer_set(&lpu, uarte_rx_buf[buf_idx],
					       CONFIG_SAMPLE_NUS_CENTRAL_UART_RX_BUF_SIZE);
#else
		(void)nrfx_uarte_rx_buffer_set(&nus_uarte_inst, uarte_rx_buf[buf_idx],
					       CONFIG_SAMPLE_NUS_CENTRAL_UART_RX_BUF_SIZE);
#endif
		buf_idx = buf_idx ? 0 : 1;
		break;
	case NRFX_UARTE_EVT_ERROR:
		LOG_ERR("UARTE error %#x", event->data.error.error_mask);
		break;
	default:
		break;
	}
}

static void on_ble_evt(ble_evt_t const *ble_evt, void *context)
{
	uint32_t nrf_err;
	ble_gap_evt_t const *gap_evt = &ble_evt->evt.gap_evt;
	ble_gap_phys_t const phys = {
		.rx_phys = BLE_GAP_PHY_AUTO,
		.tx_phys = BLE_GAP_PHY_AUTO,
	};

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		nrf_err = ble_nus_client_handles_assign(&ble_nus_client,
							    ble_evt->evt.gap_evt.conn_handle,
							    NULL);
		conn_handle = ble_evt->evt.gap_evt.conn_handle;
		if (nrf_err) {
			LOG_ERR("Failed to assign handles, nrf_error %#x", nrf_err);
		}

		/** Start discovery of services. The NUS Client waits for a discovery result. */
		nrf_err = ble_db_discovery_start(&ble_db_disc,
						     ble_evt->evt.gap_evt.conn_handle);
		if (nrf_err) {
			LOG_ERR("Failed to start db discovery, nrf_error %#x", nrf_err);
		}
#if !defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
		nrf_gpio_pin_write(BOARD_PIN_LED_1, BOARD_LED_ACTIVE_STATE);
#endif
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Disconnected. conn_handle: 0x%x, reason: 0x%x", gap_evt->conn_handle,
			gap_evt->params.disconnected.reason);
		if (conn_handle == gap_evt->conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}
#if !defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
		nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
#endif
		break;
	case BLE_GAP_EVT_TIMEOUT:
		if (gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN) {
			LOG_INF("Connection request timed out");
		}
		break;
	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		/** Pairing not supported. */
		nrf_err = sd_ble_gap_sec_params_reply(ble_evt->evt.gap_evt.conn_handle,
						       BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL,
						       NULL);
		if (nrf_err) {
			LOG_ERR("gap_sec_params_reply failed, nrf_error %#x", nrf_err);
		}
		break;
	case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
		/** Accepting parameters requested by peer. */
		nrf_err = sd_ble_gap_conn_param_update(
			gap_evt->conn_handle,
			&gap_evt->params.conn_param_update_request.conn_params);
		if (nrf_err) {
			LOG_ERR("gap_conn_param_update failed, nrf_error %#x", nrf_err);
		}
		break;
	case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
		LOG_DBG("PHY update request");
		nrf_err = sd_ble_gap_phy_update(ble_evt->evt.gap_evt.conn_handle, &phys);
		if (nrf_err) {
			LOG_ERR("gap_phy_update failed, nrf_error %#x", nrf_err);
		}
		break;
	case BLE_GATTC_EVT_TIMEOUT:
		/** Disconnect on GATT Client timeout event. */
		LOG_DBG("GATT Client Timeout");
		nrf_err = sd_ble_gap_disconnect(ble_evt->evt.gattc_evt.conn_handle,
						 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (nrf_err) {
			LOG_ERR("gap_disconnect failed, nrf_error %#x", nrf_err);
		}
		break;
	case BLE_GATTS_EVT_TIMEOUT:
		/** Disconnect on GATT Server timeout event. */
		LOG_DBG("GATT Server Timeout");
		nrf_err = sd_ble_gap_disconnect(ble_evt->evt.gatts_evt.conn_handle,
						 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (nrf_err) {
			LOG_ERR("gap_disconnect failed, nrf_error %#x", nrf_err);
		}
		break;
	default:
		break;
	}
}

NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

static void on_conn_params_evt(const struct ble_conn_params_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED:
		LOG_INF("GATT ATT MTU on connection 0x%x changed to %d", evt->conn_handle,
			evt->att_mtu);
		ble_nus_max_data_len = BLE_NUS_MAX_DATA_LEN_CALC(evt->att_mtu);
		break;
	case BLE_CONN_PARAMS_EVT_DATA_LENGTH_UPDATED:
		LOG_INF("Data length for connection 0x%x updated to %d", evt->conn_handle,
			evt->data_length.rx);
		break;
	default:
		LOG_WRN("unhandled conn params event %d", evt->evt_type);
		break;
	}
}

static void on_db_disc_evt(struct ble_db_discovery *ble_db_discovery,
			       struct ble_db_discovery_evt *evt)
{
	ble_nus_client_on_db_disc_evt(&ble_nus_client, evt);
}

static void on_ble_scan_evt(struct ble_scan_evt const *ble_scan_evt)
{
	uint32_t nrf_err;

	switch (ble_scan_evt->evt_type) {
	case BLE_SCAN_EVT_CONNECTING_ERROR:
		nrf_err = ble_scan_evt->connecting_err.reason;
		if (nrf_err) {
			LOG_ERR("Failed to connect, nrf_error %#x", nrf_err);
		}
		break;
	case BLE_SCAN_EVT_CONNECTED:
		ble_gap_evt_connected_t const *connected = ble_scan_evt->connected.connected;
		/** Scan is automatically stopped by the connection. */
		LOG_INF("Connecting to target %02x%02x%02x%02x%02x%02x",
			connected->peer_addr.addr[0], connected->peer_addr.addr[1],
			connected->peer_addr.addr[2], connected->peer_addr.addr[3],
			connected->peer_addr.addr[4], connected->peer_addr.addr[5]);
		break;
	case BLE_SCAN_EVT_SCAN_TIMEOUT:
		LOG_INF("Scan timed out");
		(void)scan_start();
		break;
	default:
		break;
	}
}

static void on_ble_nus_client_evt(struct ble_nus_client *ble_nus_c,
				  const struct ble_nus_client_evt *ble_nus_evt)
{
	int err;
	uint32_t nrf_err;

	switch (ble_nus_evt->evt_type) {
	case BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE:
		LOG_INF("Discovery complete");
		nrf_err = ble_nus_client_handles_assign(ble_nus_c, ble_nus_evt->conn_handle,
							&ble_nus_evt->discovery_complete.handles);
		if (nrf_err) {
			LOG_ERR("Failed to assign handles, nrf_error %#x", nrf_err);
		}
		nrf_err = ble_nus_client_tx_notif_enable(ble_nus_c);
		if (nrf_err) {
			LOG_ERR("Failed to enable peer tx notifications, nrf_error %#x", nrf_err);
		}
		LOG_INF("Connected to device with Nordic UART Service");
		break;
	case BLE_NUS_CLIENT_EVT_TX_DATA:
		LOG_INF("NUS TX data event, len: %d",
			ble_nus_evt->tx_data.length);
#if defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
		err = bm_lpuarte_tx(&lpu, ble_nus_evt->tx_data.data,
				    ble_nus_evt->tx_data.length, 3000);
		if (err) {
			LOG_ERR("bm_lpuarte_tx failed, err %d", err);
		}
#else
		err = nrfx_uarte_tx(&nus_uarte_inst, ble_nus_evt->tx_data.data,
				    ble_nus_evt->tx_data.length, NRFX_UARTE_TX_BLOCKING);
		if (err) {
			LOG_ERR("nrfx_uarte_tx failed, err %d", err);
		}
#endif
		break;
	case BLE_NUS_CLIENT_EVT_DISCONNECTED:
		LOG_INF("Disconnected");
		(void)scan_start();
		break;
	case BLE_NUS_CLIENT_EVT_ERROR:
		LOG_ERR("NUS error, nrf_error %#x", ble_nus_evt->error.reason);
		break;
	default:
		LOG_ERR("Unhandled ble_nus_evt %d", ble_nus_evt->evt_type);
		break;
	}
}

static void button_disconnect_handler(uint8_t pin, uint8_t action)
{
	uint32_t nrf_err =
		sd_ble_gap_disconnect(conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);

	if (nrf_err) {
		LOG_ERR("sd_ble_gap_disconnect failed, nrf_error %#x", nrf_err);
	}
}

ISR_DIRECT_DECLARE(uarte_direct_isr)
{
	nrfx_uarte_irq_handler(&nus_uarte_inst);
	return 0;
}

static int uarte_init(void)
{
	int err;
	nrfx_uarte_config_t *uarte_cfg;

#if defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
	struct bm_lpuarte_config lpu_cfg = {
		.uarte_inst = &nus_uarte_inst,
		.uarte_cfg = NRFX_UARTE_DEFAULT_CONFIG(BOARD_APP_LPUARTE_PIN_TX,
						       BOARD_APP_LPUARTE_PIN_RX),
		.req_pin = BOARD_APP_LPUARTE_PIN_REQ,
		.rdy_pin = BOARD_APP_LPUARTE_PIN_RDY,
	};

	uarte_cfg = &lpu_cfg.uarte_cfg;
#else
	nrfx_uarte_config_t uarte_config =
		NRFX_UARTE_DEFAULT_CONFIG(NUS_UARTE_PIN_TX, NUS_UARTE_PIN_RX);

	uarte_cfg = &uarte_config;

#if defined(CONFIG_SAMPLE_NUS_UART_HWFC)
	uarte_cfg->config.hwfc = NRF_UARTE_HWFC_ENABLED;
	uarte_cfg->cts_pin = NUS_UARTE_PIN_CTS;
	uarte_cfg->rts_pin = NUS_UARTE_PIN_RTS;
#endif /* CONFIG_SAMPLE_NUS_UART_HWFC */
#endif /* CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE */

#if defined(CONFIG_SAMPLE_NUS_UART_PARITY)
	uarte_cfg->parity = NRF_UARTE_PARITY_INCLUDED;
#endif

	uarte_cfg->interrupt_priority = CONFIG_SAMPLE_NUS_UART_IRQ_PRIO;

	/** We need to connect the IRQ ourselves. */
	BM_IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(NUS_UARTE_INST),
			      CONFIG_SAMPLE_NUS_UART_IRQ_PRIO,
			      uarte_direct_isr, 0);

	irq_enable(NRFX_IRQ_NUMBER_GET(NUS_UARTE_INST));

#if defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
	err = bm_lpuarte_init(&lpu, &lpu_cfg, uarte_evt_handler);
	if (err) {
		LOG_ERR("Failed to initialize UART, err %d", err);
		return err;
	}
	err = bm_lpuarte_rx_enable(&lpu);
	if (err) {
		LOG_ERR("UART RX failed, err %d", err);
	}
#else
	err = nrfx_uarte_init(&nus_uarte_inst, &uarte_config, uarte_evt_handler);
	if (err) {
		LOG_ERR("Failed to initialize UART, err %d", err);
		return err;
	}
	err = nrfx_uarte_rx_enable(&nus_uarte_inst, 0);
	if (err) {
		LOG_ERR("UART RX failed, err %d", err);
	}
#endif

	return 0;
}

static uint32_t nus_client_init(void)
{
	uint32_t nrf_err;

	struct ble_nus_client_config ble_nus_client_config = {
		.evt_handler = on_ble_nus_client_evt,
		.gatt_queue = &ble_gq,
		.db_discovery = &ble_db_disc,
	};

	nrf_err = ble_nus_client_init(&ble_nus_client, &ble_nus_client_config);
	if (nrf_err) {
		LOG_ERR("Failed to initialize NUS, nrf_error %#x", nrf_err);
	}

	return nrf_err;
}

static int buttons_leds_init(void)
{
	int err;

	static struct bm_buttons_config btn_configs[] = {
		{
			.pin_number = BOARD_PIN_BTN_3,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_disconnect_handler,
		},
	};

	err = bm_buttons_init(btn_configs, ARRAY_SIZE(btn_configs),
			      BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		LOG_ERR("bm_buttons_init failed, err %d", err);
		return err;
	}

	err = bm_buttons_enable();
	if (err) {
		LOG_ERR("bm_buttons_enable failed, err %d", err);
		return err;
	}
#if !defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_1);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
#endif

	return 0;
}

static uint32_t scan_init(void)
{
	uint32_t nrf_err;

	struct ble_scan_config ble_scan_cfg = {
		.scan_params = {
			.active = 0x01,
			.interval = BLE_GAP_SCAN_INTERVAL_US_MIN * 6,
			.window = BLE_GAP_SCAN_WINDOW_US_MIN * 6,
			.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
			.timeout = BLE_GAP_SCAN_TIMEOUT_UNLIMITED,
			.scan_phys = BLE_GAP_PHY_AUTO,
		},
		.conn_params = BLE_SCAN_CONN_PARAMS_DEFAULT,
		.connect_if_match = true,
		.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
		.evt_handler = on_ble_scan_evt,
	};

	nrf_err = ble_scan_init(&ble_scan, &ble_scan_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize ble_scanning, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	const struct ble_scan_filter_data uuid_filter = {
		.uuid_filter = {.uuid = {.uuid = BLE_UUID_NUS_SERVICE, .type = BLE_UUID_TYPE_BLE}}};

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_UUID_FILTER, &uuid_filter);
	if (nrf_err) {
		LOG_ERR("nrf_ble_scan_filter_add uuid failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

#if defined(CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_NAME)
	const struct ble_scan_filter_data name_filter = {
		.name_filter = {.name = CONFIG_SAMPLE_TARGET_PERIPHERAL_NAME}};

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_NAME_FILTER, &name_filter);
	if (nrf_err) {
		LOG_ERR("Failed to set filter, nrf_error %#x", nrf_err);
		return nrf_err;
	}
#endif
	nrf_err = ble_scan_filters_enable(&ble_scan,
					      BLE_SCAN_NAME_FILTER | BLE_SCAN_UUID_FILTER,
					      false);
	if (nrf_err) {
		LOG_ERR("Enabling filter failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static uint32_t db_discovery_init(void)
{
	uint32_t nrf_err;

	struct ble_db_discovery_config db_cfg = {
		.evt_handler = on_db_disc_evt,
		.gatt_queue = &ble_gq,
	};

	nrf_err = ble_db_discovery_init(&ble_db_disc, &db_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to enable db discovery, nrf_error %#x", nrf_err);
	}

	return nrf_err;
}

static uint32_t scan_start(void)
{
	uint32_t nrf_err;

	nrf_err = ble_scan_start(&ble_scan);
	if (nrf_err) {
		LOG_ERR("Failed to start ble_scanning, nrf_error %#x", nrf_err);
	}

	return nrf_err;
}

int main(void)
{
	int err;
	uint32_t nrf_err;
	ble_gap_conn_sec_mode_t device_name_write_sec;

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

	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&device_name_write_sec);
	nrf_err = sd_ble_gap_device_name_set(&device_name_write_sec, CONFIG_SAMPLE_BLE_DEVICE_NAME,
					     strlen(CONFIG_SAMPLE_BLE_DEVICE_NAME));
	if (nrf_err) {
		LOG_ERR("Failed to set device name, nrf_error %#x", nrf_err);
		goto idle;
	}

	err = uarte_init();
	if (err) {
		LOG_ERR("Failed to enable UARTE, err %d", err);
		goto idle;
	}

	nrf_err = ble_conn_params_evt_handler_set(on_conn_params_evt);
	if (nrf_err) {
		LOG_ERR("ble_conn_params_evt_handler_set failed, nrf_error %#x", nrf_err);
	}

	err = buttons_leds_init();
	if (err) {
		goto idle;
	}

	nrf_err = db_discovery_init();
	if (nrf_err) {
		goto idle;
	}

	nrf_err = nus_client_init();
	if (nrf_err) {
		goto idle;
	}

	nrf_err = scan_init();
	if (nrf_err) {
		goto idle;
	}

	/** Start execution.*/
	LOG_INF("BLE NUS central example started");
	nrf_err = scan_start();
	if (nrf_err) {
		goto idle;
	}

#if !defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
#endif

idle:
	/** Enter main loop.*/
	while (true) {
		log_flush();

		k_cpu_idle();
	}
}
