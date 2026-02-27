/**
 * Copyright (c) 2016 - 2026, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bm_buttons.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <hal/nrf_gpio.h>
#include <board-config.h>
#include "ble.h"
#include "ble_gap.h"
#include "ble_hci.h"
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>
#include <bm/bluetooth/services/ble_nus.h>
#include <bm/bluetooth/services/ble_nus_client.h>
#include <bm/bluetooth/ble_scan.h>
#include <nrfx_uarte.h>
#if defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
#include <bm/drivers/bm_lpuarte.h>
#endif

#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

/** Tag that refers to the BLE stack configuration set with @ref sd_ble_cfg_set. The default tag is
 *  @ref BLE_CONN_CFG_TAG_DEFAULT.
 */
#define APP_BLE_CONN_CFG_TAG  1
/** BLE observer priority of the application. There is no need to modify this value. */
#define APP_BLE_OBSERVER_PRIO 3

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

/** Echo the UART data that is received over the Nordic UART Service (NUS) back to the sender. */
#define ECHOBACK_BLE_UART_DATA 1

/** BLE Nordic UART Service (NUS) client instance. */
BLE_NUS_CLIENT_DEF(m_ble_nus_c);
/** Database discovery module instance. */
BLE_DB_DISCOVERY_DEF(m_db_disc);
/** Scanning Module instance. */
BLE_SCAN_DEF(m_scan);
/** BLE GATT Queue instance. */
BLE_GQ_DEF(m_ble_gatt_queue);

LOG_MODULE_REGISTER(app, CONFIG_SAMPLE_BLE_NUS_CENTRAL_LOG_LEVEL);

/** Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART
 * service module.
 */

static uint16_t current_rx_buf_len = 20;
/** Handle of the current connection. */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

static nrfx_uarte_t nus_uarte_inst = NRFX_UARTE_INSTANCE(NUS_UARTE_INST);

/** Receive buffers used in UART ISR callback. */
static uint8_t uarte_rx_buf[CONFIG_SAMPLE_NUS_CLIENT_UART_RX_BUF_SIZE][2];

static int buf_idx;

#if defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)

static void lpuarte_rx_handler(char *data, size_t data_len)
{
	uint32_t nrf_err;
	uint16_t len = data_len;

	LOG_INF("Sending data over BLE NUS, len %d", len);

	do {
		nrf_err = ble_nus_client_string_send(&m_ble_nus_c, data, len);
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

		if ((c == '\n' || c == '\r') || (rx_buf_idx >= current_rx_buf_len)) {
			if (rx_buf_idx == 0) {
				/** RX buffer is empty, nothing to send. */
				continue;
			}

			len = rx_buf_idx;
			LOG_INF("Sending data over BLE NUS, len %d", len);

			do {
				nrf_err = ble_nus_client_string_send(&m_ble_nus_c, rx_buf, len);
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

static void scan_start(void)
{
	uint32_t err;

	err = ble_scan_start(&m_scan);
	if (err) {
		LOG_ERR("Failed to start scanning, nrf_error %#x", err);
	}
}

static void scan_evt_handler(struct ble_scan_evt const *scan_evt)
{
	uint32_t nrf_err;

	switch (scan_evt->evt_type) {
	case BLE_SCAN_EVT_CONNECTING_ERROR: {
		nrf_err = scan_evt->params.connecting_err.reason;
		if (nrf_err) {
			LOG_ERR("Failed to connect, nrf_error %#x", nrf_err);
		}
	} break;

	case BLE_SCAN_EVT_CONNECTED: {
		ble_gap_evt_connected_t const *connected = scan_evt->params.connected.connected;
		/** Scan is automatically stopped by the connection. */
		LOG_INF("Connecting to target %02x%02x%02x%02x%02x%02x",
			connected->peer_addr.addr[0], connected->peer_addr.addr[1],
			connected->peer_addr.addr[2], connected->peer_addr.addr[3],
			connected->peer_addr.addr[4], connected->peer_addr.addr[5]);
	} break;

	case BLE_SCAN_EVT_SCAN_TIMEOUT: {
		LOG_INF("Scan timed out.");
		scan_start();
	} break;

	default:
		break;
	}
}

static void scan_init(void)
{
	uint32_t err;

	struct ble_scan_config init_scan = {
		.scan_params = {.active = 0x01,
				.interval = BLE_GAP_SCAN_INTERVAL_US_MIN * 6,
				.window = BLE_GAP_SCAN_WINDOW_US_MIN * 6,
				.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
				.timeout = BLE_GAP_SCAN_TIMEOUT_UNLIMITED,
				.scan_phys = BLE_GAP_PHY_AUTO},
		.conn_params = BLE_SCAN_CONN_PARAMS_DEFAULT,
		.connect_if_match = true,
		.conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
		.evt_handler = scan_evt_handler};

	err = ble_scan_init(&m_scan, &init_scan);
	if (err) {
		LOG_ERR("Failed to initialize scanning, nrf_error %#x", err);
	}
	err = ble_scan_filter_add(&m_scan, BLE_SCAN_NAME_FILTER, "nRF_BM_NUS");
	if (err) {
		LOG_ERR("Failed to set filter, nrf_error %#x", err);
	}
	err = ble_scan_filters_enable(&m_scan, BLE_SCAN_NAME_FILTER, false);
	if (err) {
		LOG_ERR("Enabling filter failed, nrf_error %#x", err);
	}
}

static void db_disc_handler(struct ble_db_discovery *db_discovery, struct ble_db_discovery_evt *evt)
{
	ble_nus_client_on_db_disc_evt(&m_ble_nus_c, evt);
}

static void ble_nus_client_evt_handler(struct ble_nus_client *ble_nus_c,
				       struct ble_nus_client_evt const *ble_nus_evt)
{
	uint32_t nrf_err;
	int err;

	switch (ble_nus_evt->evt_type) {
	case BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE:
		LOG_INF("Discovery complete.");
		nrf_err = ble_nus_client_handles_assign(
			ble_nus_c, ble_nus_evt->conn_handle,
			&ble_nus_evt->params.discovery_complete.handles);
		if (nrf_err) {
			LOG_ERR("Failed to assign handles, nrf_error %#x", nrf_err);
		}

		nrf_err = ble_nus_client_tx_notif_enable(ble_nus_c);
		if (nrf_err) {
			LOG_ERR("Failed to enable peer tx notifications, nrf_error %#x", nrf_err);
		}
		LOG_INF("Connected to device with Nordic UART Service.");
		break;

	case BLE_NUS_CLIENT_EVT_NUS_TX_EVT:
		LOG_INF("BLE_NUS_CLIENT_EVT_NUS_TX_EVT Data len: %d",
			ble_nus_evt->params.nus_tx_evt.data_len);

#if defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
		LOG_INF("BLE_NUS_CLIENT_EVT_NUS_TX_EVT event");
		err = bm_lpuarte_tx(&lpu, ble_nus_evt->params.nus_tx_evt.data,
				    ble_nus_evt->params.nus_tx_evt.data_len, 3000);

		if (err) {
			LOG_ERR("bm_lpuarte_tx failed, err %d", err);
		}
#else
		err = nrfx_uarte_tx(&nus_uarte_inst, ble_nus_evt->params.nus_tx_evt.data,
				    ble_nus_evt->params.nus_tx_evt.data_len,
				    NRFX_UARTE_TX_BLOCKING);
		if (err) {
			LOG_ERR("nrfx_uarte_tx failed, err %d", err);
		}
#endif
		break;

	case BLE_NUS_CLIENT_EVT_DISCONNECTED:
		LOG_INF("Disconnected.");
		scan_start();
		break;
	case BLE_NUS_CLIENT_EVT_ERROR:
		LOG_ERR("NUS error, nrf_error %#x", ble_nus_evt->params.error.reason);
		break;
	default:
		LOG_ERR("Unhandled ble_nus_evt: %d", ble_nus_evt->evt_type);
		break;
	}
}

static void ble_evt_handler(ble_evt_t const *ble_evt, void *context)
{
	uint32_t err_code;
	ble_gap_evt_t const *gap_evt = &ble_evt->evt.gap_evt;

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		err_code = ble_nus_client_handles_assign(&m_ble_nus_c,
							 ble_evt->evt.gap_evt.conn_handle, NULL);
		conn_handle = ble_evt->evt.gap_evt.conn_handle;
		if (err_code) {
			LOG_ERR("Failed to assign handles, nrf_error %#x", err_code);
		}

		/** Start discovery of services. The NUS Client waits for a discovery result. */
		err_code = ble_db_discovery_start(&m_db_disc, ble_evt->evt.gap_evt.conn_handle);
		if (err_code) {
			LOG_ERR("Failed to start db discovery, nrf_error %#x", err_code);
		}
		break;

	case BLE_GAP_EVT_DISCONNECTED:

		LOG_INF("Disconnected. conn_handle: 0x%x, reason: 0x%x", gap_evt->conn_handle,
			gap_evt->params.disconnected.reason);
		break;

	case BLE_GAP_EVT_TIMEOUT:
		if (gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN) {
			LOG_INF("Connection Request timed out.");
		}
		break;

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		/** Pairing not supported. */
		err_code = sd_ble_gap_sec_params_reply(ble_evt->evt.gap_evt.conn_handle,
						       BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL,
						       NULL);
		if (err_code) {
			LOG_ERR("gap_sec_params_reply failed, nrf_error %#x", err_code);
		}
		break;

	case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
		/** Accepting parameters requested by peer. */
		err_code = sd_ble_gap_conn_param_update(
			gap_evt->conn_handle,
			&gap_evt->params.conn_param_update_request.conn_params);
		if (err_code) {
			LOG_ERR("gap_conn_param_update failed, nrf_error %#x", err_code);
		}
		break;

	case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
		LOG_DBG("PHY update request.");
		ble_gap_phys_t const phys = {
			.rx_phys = BLE_GAP_PHY_AUTO,
			.tx_phys = BLE_GAP_PHY_AUTO,
		};
		err_code = sd_ble_gap_phy_update(ble_evt->evt.gap_evt.conn_handle, &phys);
		if (err_code) {
			LOG_ERR("gap_phy_update failed, nrf_error %#x", err_code);
		}
	} break;

	case BLE_GATTC_EVT_TIMEOUT:
		/** Disconnect on GATT Client timeout event. */
		LOG_DBG("GATT Client Timeout.");
		err_code = sd_ble_gap_disconnect(ble_evt->evt.gattc_evt.conn_handle,
						 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (err_code) {
			LOG_ERR("gap_disconnect failed, nrf_error %#x", err_code);
		}
		break;

	case BLE_GATTS_EVT_TIMEOUT:
		/** Disconnect on GATT Server timeout event. */
		LOG_DBG("GATT Server Timeout.");
		err_code = sd_ble_gap_disconnect(ble_evt->evt.gatts_evt.conn_handle,
						 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		if (err_code) {
			LOG_ERR("gap_disconnect failed, nrf_error %#x", err_code);
		}
		break;

	default:
		break;
	}
}

static void ble_stack_init(void)
{
	uint32_t err_code;

	err_code = nrf_sdh_enable_request();
	if (err_code) {
		LOG_ERR("sdh_enable_request failed, nrf_error %#x", err_code);
	}

	/** Configure the BLE stack using the default settings. */
	/** Fetch the start address of the application RAM. */
	uint32_t ram_start = 0;

	/** Enable BLE stack. */
	err_code = nrf_sdh_ble_enable((uint8_t)ram_start);
	if (err_code) {
		LOG_ERR("sdh_enable_enable failed, nrf_error %#x", err_code);
	}

	/** Register a handler for BLE events. */
	NRF_SDH_BLE_OBSERVER(m_ble_observer, ble_evt_handler, NULL, USER_LOW);
}

static void conn_params_evt_handler(const struct ble_conn_params_evt *evt)
{
	switch (evt->evt_type) {
	case BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED:
		LOG_INF("GATT ATT MTU on connection 0x%x changed to %d.", evt->conn_handle,
			evt->att_mtu);
		current_rx_buf_len = evt->att_mtu;
		break;

	case BLE_CONN_PARAMS_EVT_DATA_LENGTH_UPDATED:
		LOG_INF("Data length for connection 0x%x updated to %d.", evt->conn_handle,
			evt->data_length.rx);
		break;

	default:
		LOG_WRN("unhandled conn params event %d", evt->evt_type);
		break;
	}
}

static uint32_t gatt_init(void)
{
	uint32_t nrf_err = ble_conn_params_evt_handler_set(conn_params_evt_handler);

	if (nrf_err) {
		LOG_ERR("ble_conn_params_evt_handler_set failed, nrf_error %#x", nrf_err);
	}

	return nrf_err;
}

static void button_disconnect_handler(uint8_t pin, uint8_t action)
{
	uint32_t nrf_err =
		sd_ble_gap_disconnect(conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);

	if (nrf_err) {
		LOG_ERR("sd_ble_gap_disconnect failed, nrf_error %#x", nrf_err);
	}
}

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
					       CONFIG_SAMPLE_NUS_CLIENT_UART_RX_BUF_SIZE);
#else
		(void)nrfx_uarte_rx_buffer_set(&nus_uarte_inst, uarte_rx_buf[buf_idx],
					       CONFIG_SAMPLE_NUS_CLIENT_UART_RX_BUF_SIZE);
#endif

		buf_idx = buf_idx ? 0 : 1;
		break;
	case NRFX_UARTE_EVT_ERROR:
		LOG_ERR("uarte error %#x", event->data.error.error_mask);
		break;
	default:
		break;
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
	IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(NUS_UARTE_INST), CONFIG_SAMPLE_NUS_UART_IRQ_PRIO,
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

static void nus_c_init(void)
{
	uint32_t err_code;

	struct ble_nus_client_config init;

	init.evt_handler = ble_nus_client_evt_handler;
	init.gatt_queue = &m_ble_gatt_queue;
	init.db_discovery = &m_db_disc;

	err_code = ble_nus_client_init(&m_ble_nus_c, &init);
	if (err_code) {
		LOG_ERR("Failed to initialize NUS, nrf_error %#x", err_code);
	}
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
		LOG_ERR("bm_buttons_init error: %d", err);
		return err;
	}

	err = bm_buttons_enable();
	if (err) {
		LOG_ERR("bm_buttons_enable error: %d", err);
		return err;
	}
#if !defined(CONFIG_SAMPLE_NUS_CENTRAL_LPUARTE)
	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_1);
	nrf_gpio_cfg_output(BOARD_PIN_LED_3);

	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_2, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_3, BOARD_LED_ACTIVE_STATE);
#endif

	return 0;
}

static void db_discovery_init(void)
{
	uint32_t err_code;

	struct ble_db_discovery_config db_cfg = {0};

	db_cfg.evt_handler = db_disc_handler;
	db_cfg.gatt_queue = &m_ble_gatt_queue;

	err_code = ble_db_discovery_init(&m_db_disc, &db_cfg);

	if (err_code) {
		LOG_ERR("Failed to enable db discovery, nrf_error %#x", err_code);
	}
}

int main(void)
{
	int err;
	/** Initialize. */

	ble_stack_init();

	err = uarte_init();
	if (err) {
		LOG_ERR("Failed to enable UARTE, err %d", err);
	}

	gatt_init();
	buttons_leds_init();
	db_discovery_init();
	nus_c_init();
	scan_init();

	/** Start execution.*/
	LOG_INF("BLE NUS central example started.");
	scan_start();

	/** Enter main loop.*/
	while (true) {
		while (LOG_PROCESS()) {
		}

		/** Wait for an event. */
		__WFE();

		/** Clear Event Register */
		__SEV();
		__WFE();
	}
}
