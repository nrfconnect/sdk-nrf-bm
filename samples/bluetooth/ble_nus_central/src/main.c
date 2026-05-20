/**
 * Copyright (c) 2016 - 2026, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <ble.h>
#include <bm/bm_buttons.h>
#include <bm/bm_irq.h>
#include <hal/nrf_gpio.h>
#include <nrfx_uarte.h>
#if defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
#include <bm/drivers/bm_lpuarte.h>
#endif

#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/ble_gq.h>
#include <bm/bluetooth/ble_scan.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>

#include <bm/bluetooth/services/ble_nus.h>
#include <bm/bluetooth/services/ble_nus_client.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_BLE_NUS_CENTRAL_LOG_LEVEL);

/* BLE Nordic UART Service (NUS) client instance. */
BLE_NUS_CLIENT_DEF(ble_nus_client);
/* Database discovery module instance. */
BLE_DB_DISCOVERY_DEF(ble_db_disc);
/* Scanning Module instance. */
BLE_SCAN_DEF(ble_scan);
/* BLE GATT Queue instance. */
BLE_GQ_DEF(ble_gq);

/* Handle of the current connection. */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

/* NUS UARTE instance and board config */
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

/* UUID type for the Nordic UART Service (vendor specific). */
#define NUS_SERVICE_UUID_TYPE BLE_UUID_TYPE_VENDOR_BEGIN

/* NUS Client UARTE instance. */
static nrfx_uarte_t nus_uarte_inst = NRFX_UARTE_INSTANCE(NUS_UARTE_INST);

/* Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART
 * service module.
 */
static uint16_t ble_nus_max_data_len = BLE_NUS_CLIENT_MAX_DATA_LEN_CALC(BLE_GATT_ATT_MTU_DEFAULT);

/* Receive buffers used in UART ISR callback. */
static uint8_t uarte_rx_buf[CONFIG_SAMPLE_NUS_CENTRAL_UART_RX_BUF_SIZE][2];
static int buf_idx;

#if defined(CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_ADDR)
/* Target peripheral address (little-endian). */
static const uint8_t target_periph_addr[BLE_GAP_ADDR_LEN] = {
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 8) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 16) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 24) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 32) & 0xff,
	(CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR >> 40) & 0xff,
};
#endif /* CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_ADDR */

/* Forward declaration. */
static void scan_start(void);

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
	/* Receive buffer used in UART ISR callback. */
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
				/* RX buffer is empty, nothing to send. */
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

static void on_ble_evt(const ble_evt_t *ble_evt, void *context)
{
	uint32_t nrf_err;
	const ble_gap_evt_t *const gap_evt = &ble_evt->evt.gap_evt;

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		conn_handle = gap_evt->conn_handle;

		/* Start discovery of services. The NUS Client waits for a discovery result. */
		nrf_err = ble_db_discovery_start(&ble_db_disc, gap_evt->conn_handle);
		if (nrf_err) {
			LOG_ERR("Failed to start db discovery, nrf_error %#x", nrf_err);
		}
#if !defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
		nrf_gpio_pin_write(BOARD_PIN_LED_1, BOARD_LED_ACTIVE_STATE);
#endif
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Disconnected conn_handle %#x, reason %#x",
			gap_evt->conn_handle, gap_evt->params.disconnected.reason);
		if (gap_evt->conn_handle == conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;

#if !defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
			nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
#endif
			scan_start();
		}
		break;
	case BLE_GAP_EVT_TIMEOUT:
		if (gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN) {
			LOG_INF("Connection request timed out");
		}
		break;
	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		/* Pairing not supported. */
		nrf_err = sd_ble_gap_sec_params_reply(ble_evt->evt.gap_evt.conn_handle,
						      BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL,
						      NULL);
		if (nrf_err) {
			LOG_ERR("gap_sec_params_reply failed, nrf_error %#x", nrf_err);
		}
		break;
	case BLE_GATTC_EVT_TIMEOUT:
		/* Disconnect on GATT Client timeout event. */
		LOG_DBG("GATT Client Timeout");
		nrf_err = sd_ble_gap_disconnect(ble_evt->evt.gattc_evt.conn_handle,
						 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (nrf_err) {
			LOG_ERR("gap_disconnect failed, nrf_error %#x", nrf_err);
		}
		break;
	case BLE_GATTS_EVT_TIMEOUT:
		/* Disconnect on GATT Server timeout event. */
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

static void conn_params_evt_handler(const struct ble_conn_params_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED:
		ble_nus_max_data_len = BLE_NUS_MAX_DATA_LEN_CALC(evt->att_mtu);
		break;

	default:
		break;
	}
}

static void db_disc_evt_handler(struct ble_db_discovery *ble_db_discovery,
				struct ble_db_discovery_evt *evt)
{
	ble_nus_client_on_db_disc_evt(&ble_nus_client, evt);
}

static void scan_evt_handler(const struct ble_scan_evt *scan_evt)
{
	switch (scan_evt->evt_type) {
	case BLE_SCAN_EVT_CONNECTING_ERROR:
		LOG_ERR("Failed to connect, nrf_error %#x", scan_evt->connecting_err.reason);
		break;

	case BLE_SCAN_EVT_CONNECTED:
		const ble_gap_addr_t *const peer_addr = &scan_evt->connected.connected->peer_addr;

		/* Scan is automatically stopped by the connection. */
		LOG_INF("Connecting to target %02X:%02X:%02X:%02X:%02X:%02X",
			peer_addr->addr[5], peer_addr->addr[4], peer_addr->addr[3],
			peer_addr->addr[2], peer_addr->addr[1], peer_addr->addr[0]);
		break;

	case BLE_SCAN_EVT_SCAN_TIMEOUT:
		LOG_INF("Scan timed out");
		scan_start();
		break;

	default:
		break;
	}
}

static void nus_client_evt_handler(struct ble_nus_client *nus_c,
				   const struct ble_nus_client_evt *nus_evt)
{
	int err;
	uint32_t nrf_err;

	switch (nus_evt->evt_type) {
	case BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE:
		LOG_INF("NUS discovered");
		nrf_err = ble_nus_client_handles_assign(nus_c, nus_evt->conn_handle,
							&nus_evt->discovery_complete.handles);
		if (nrf_err) {
			LOG_ERR("Failed to assign handles, nrf_error %#x", nrf_err);
			break;
		}
		nrf_err = ble_nus_client_tx_notif_enable(nus_c);
		if (nrf_err) {
			LOG_ERR("Failed to enable peer tx notifications, nrf_error %#x", nrf_err);
			break;
		}
		LOG_INF("Connected to device with Nordic UART Service");
		break;
	case BLE_NUS_CLIENT_EVT_TX_DATA:
		LOG_INF("NUS TX data event, len: %d", nus_evt->tx_data.length);
#if defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
		err = bm_lpuarte_tx(&lpu, nus_evt->tx_data.data,
				    nus_evt->tx_data.length, 3000);
		if (err) {
			LOG_ERR("bm_lpuarte_tx failed, err %d", err);
		}
#else
		err = nrfx_uarte_tx(&nus_uarte_inst, nus_evt->tx_data.data,
				    nus_evt->tx_data.length, NRFX_UARTE_TX_BLOCKING);
		if (err) {
			LOG_ERR("nrfx_uarte_tx failed, err %d", err);
		}
#endif
		break;
	case BLE_NUS_CLIENT_EVT_DISCONNECTED:
		LOG_INF("NUS disconnected");
		break;
	case BLE_NUS_CLIENT_EVT_ERROR:
		LOG_ERR("NUS error, nrf_error %#x", nus_evt->error.reason);
		break;
	default:
		LOG_ERR("Unhandled NUS event %d", nus_evt->evt_type);
		break;
	}
}

static void button_handler_disconnect(uint8_t pin, uint8_t action)
{
	if (conn_handle == BLE_CONN_HANDLE_INVALID || action != BM_BUTTONS_PRESS) {
		return;
	}

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

	/* We need to connect the IRQ ourselves. */
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
	struct ble_nus_client_config ble_nus_client_config = {
		.evt_handler = nus_client_evt_handler,
		.gatt_queue = &ble_gq,
		.db_discovery = &ble_db_disc,
	};

	return ble_nus_client_init(&ble_nus_client, &ble_nus_client_config);
}

static int buttons_leds_init(void)
{
	int err;

	static struct bm_buttons_config btn_configs[] = {
		{
			.pin_number = BOARD_PIN_BTN_3,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler_disconnect,
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
	struct ble_scan_config scan_cfg = {
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
		.evt_handler = scan_evt_handler,
	};
	struct ble_scan_filter_data filter_data = {
		.uuid_filter.uuid = {
			.uuid = BLE_UUID_NUS_SERVICE,
			.type = BLE_UUID_TYPE_BLE,
		},
	};
	uint8_t filter_mode_mask = BLE_SCAN_UUID_FILTER;

	nrf_err = ble_scan_init(&ble_scan, &scan_cfg);
	if (nrf_err) {
		LOG_ERR("ble_scan_init failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_UUID_FILTER, &filter_data);
	if (nrf_err) {
		LOG_ERR("ble_scan_filter_add uuid failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}

#if defined(CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_NAME)
	filter_data.name_filter.name = CONFIG_SAMPLE_TARGET_PERIPHERAL_NAME;
	filter_mode_mask |= BLE_SCAN_NAME_FILTER;

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_NAME_FILTER, &filter_data);
	if (nrf_err) {
		LOG_ERR("ble_scan_filter_add name failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}
#endif /* CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_NAME */

#if defined(CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_ADDR)
	filter_data.addr_filter.addr = target_periph_addr;
	filter_mode_mask |= BLE_SCAN_ADDR_FILTER;

	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_ADDR_FILTER, &filter_data);
	if (nrf_err) {
		LOG_ERR("ble_scan_filter_add address failed, nrf_error %#x", nrf_err);
		return nrf_err;
	}
#endif /* CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_ADDR */

	nrf_err = ble_scan_filters_enable(&ble_scan, filter_mode_mask, false);
	if (nrf_err) {
		LOG_ERR("Failed to enable scan filters, nrf_error %#x", nrf_err);
		return nrf_err;
	}

	return NRF_SUCCESS;
}

static uint32_t db_discovery_init(void)
{
	struct ble_db_discovery_config db_cfg = {
		.evt_handler = db_disc_evt_handler,
		.gatt_queue = &ble_gq,
	};

	return ble_db_discovery_init(&ble_db_disc, &db_cfg);
}

static void scan_start(void)
{
	uint32_t nrf_err;

	nrf_err = ble_scan_start(&ble_scan);
	if (nrf_err) {
		LOG_ERR("Failed to start scanning, nrf_error %#x", nrf_err);
	}
}

int main(void)
{
	int err;
	uint32_t nrf_err;
	ble_gap_conn_sec_mode_t device_name_write_sec;

	LOG_INF("BLE NUS central sample started");

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

	nrf_err = ble_conn_params_evt_handler_set(conn_params_evt_handler);
	if (nrf_err) {
		LOG_ERR("Failed to setup conn params event handler, nrf_error %#x", nrf_err);
		goto idle;
	}

	err = uarte_init();
	if (err) {
		LOG_ERR("Failed to enable UARTE, err %d", err);
		goto idle;
	}

	err = buttons_leds_init();
	if (err) {
		goto idle;
	}

	nrf_err = db_discovery_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize db discovery, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = nus_client_init();
	if (nrf_err) {
		LOG_ERR("Failed to initialize NUS client, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = scan_init();
	if (nrf_err) {
		goto idle;
	}

#if !defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
#endif

	LOG_INF("BLE NUS central sample initialized");

	scan_start();

idle:
	while (true) {
		log_flush();

		k_cpu_idle();
	}
}
