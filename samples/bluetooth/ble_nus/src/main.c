/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <string.h>
#include <ble_gap.h>
#include <hal/nrf_gpio.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <bm/bluetooth/ble_adv.h>
#include <bm/bluetooth/ble_conn_params.h>
#include <bm/bluetooth/ble_qwr.h>
#include <bm/bluetooth/services/ble_nus.h>
#include <nrf_soc.h>
#include <nrfx_uarte.h>
#if defined(CONFIG_SAMPLE_NUS_LPUARTE)
#include <bm/drivers/bm_lpuarte.h>
#endif

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/util.h>

#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_BLE_NUS_LOG_LEVEL);

BLE_ADV_DEF(ble_adv); /* BLE advertising instance */
BLE_NUS_DEF(ble_nus); /* BLE NUS service instance */
BLE_QWR_DEF(ble_qwr); /* BLE QWR instance */

/** Handle of the current connection. */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

/** NUS UARTE instance and board config */
#if defined(CONFIG_SAMPLE_NUS_LPUARTE)
#define NUS_UARTE_INST BOARD_APP_LPUARTE_INST
#define NUS_UARTE_PIN_TX BOARD_APP_LPUARTE_PIN_TX
#define NUS_UARTE_PIN_RX BOARD_APP_LPUARTE_PIN_RX
#define NUS_UARTE_PIN_RDY BOARD_APP_LPUARTE_PIN_RDY
#define NUS_UARTE_PIN_REQ BOARD_APP_LPUARTE_PIN_REQ

struct bm_lpuarte lpu;
#else
#define NUS_UARTE_INST BOARD_APP_UARTE_INST
#define NUS_UARTE_PIN_TX BOARD_APP_UARTE_PIN_TX
#define NUS_UARTE_PIN_RX BOARD_APP_UARTE_PIN_RX
#define NUS_UARTE_PIN_CTS BOARD_APP_UARTE_PIN_CTS
#define NUS_UARTE_PIN_RTS BOARD_APP_UARTE_PIN_RTS
#endif /* CONFIG_SAMPLE_NUS_LPUARTE */

static nrfx_uarte_t nus_uarte_inst = NRFX_UARTE_INSTANCE(NUS_UARTE_INST);

/* Maximum length of data (in bytes) that can be transmitted to the peer by the
 * Nordic UART service module.
 */
static volatile uint16_t ble_nus_max_data_len = BLE_NUS_MAX_DATA_LEN_CALC(BLE_GATT_ATT_MTU_DEFAULT);

/* Receive buffers used in UART ISR callback. */
static uint8_t uarte_rx_buf[CONFIG_SAMPLE_NUS_UART_RX_BUF_SIZE][2];
static int buf_idx;

/**
 * @brief Handle data received from UART.
 *
 * @param[in] data Data received.
 * @param[in] data_len Size of data.
 */
#if defined(CONFIG_SAMPLE_NUS_LPUARTE)
static void lpuarte_rx_handler(char *data, size_t data_len)
{
	uint32_t nrf_err;
	uint16_t len = data_len;

	LOG_INF("Sending data over BLE NUS, len %d", len);

	do {
		nrf_err = ble_nus_data_send(&ble_nus, data, &len, conn_handle);
		if ((nrf_err) &&
		    (nrf_err != NRF_ERROR_INVALID_STATE) &&
		    (nrf_err != NRF_ERROR_RESOURCES) &&
		    (nrf_err != NRF_ERROR_NOT_FOUND)) {
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
	/* receive buffer used in UART ISR callback */
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
			LOG_INF("Sending data over BLE NUS, len %d", len);

			do {
				nrf_err = ble_nus_data_send(&ble_nus, rx_buf, &len, conn_handle);
				if ((nrf_err) &&
				    (nrf_err != NRF_ERROR_INVALID_STATE) &&
				    (nrf_err != NRF_ERROR_RESOURCES) &&
				    (nrf_err != NRF_ERROR_NOT_FOUND)) {
					LOG_ERR("Failed to send NUS data, nrf_error %#x", nrf_err);
					return;
				}
			} while (nrf_err == NRF_ERROR_RESOURCES);

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
#endif

/**
 * @brief UARTE event handler
 *
 * @param[in] event UARTE event structure
 * @param[in] ctx Context. NULL in this case.
 */
static void uarte_evt_handler(const nrfx_uarte_event_t *event, void *ctx)
{
	switch (event->type) {
	case NRFX_UARTE_EVT_RX_DONE:
		LOG_DBG("Received data from UART: %.*s (%d)",
			event->data.rx.length, event->data.rx.p_buffer, event->data.rx.length);
		if (event->data.rx.length > 0) {
#if defined(CONFIG_SAMPLE_NUS_LPUARTE)
			lpuarte_rx_handler(event->data.rx.p_buffer, event->data.rx.length);
#else
			uarte_rx_handler(event->data.rx.p_buffer, event->data.rx.length);
#endif
		}

#if !defined(CONFIG_SAMPLE_NUS_LPUARTE)
		(void)nrfx_uarte_rx_enable(&nus_uarte_inst, 0);
#endif
		break;
	case NRFX_UARTE_EVT_RX_BUF_REQUEST:
#if defined(CONFIG_SAMPLE_NUS_LPUARTE)
		(void)bm_lpuarte_rx_buffer_set(&lpu, uarte_rx_buf[buf_idx],
					       CONFIG_SAMPLE_NUS_UART_RX_BUF_SIZE);
#else
		(void)nrfx_uarte_rx_buffer_set(&nus_uarte_inst, uarte_rx_buf[buf_idx],
					       CONFIG_SAMPLE_NUS_UART_RX_BUF_SIZE);
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

/**
 * @brief BLE event handler.
 *
 * @param[in] evt BLE event.
 * @param[in] ctx Context.
 */
static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	uint32_t nrf_err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		LOG_INF("Peer connected");
		ble_nus_max_data_len = BLE_NUS_MAX_DATA_LEN_CALC(BLE_GATT_ATT_MTU_DEFAULT);
		conn_handle = evt->evt.gap_evt.conn_handle;
		nrf_err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (nrf_err) {
			LOG_ERR("Failed to set system attributes, nrf_error %#x", nrf_err);
		}

		nrf_err = ble_qwr_conn_handle_assign(&ble_qwr, conn_handle);
		if (nrf_err) {
			LOG_ERR("Failed to assign qwr handle, nrf_error %#x", nrf_err);
		}

#if !defined(CONFIG_SAMPLE_NUS_LPUARTE)
		nrf_gpio_pin_write(BOARD_PIN_LED_1, BOARD_LED_ACTIVE_STATE);
#endif

		break;

	case BLE_GAP_EVT_DISCONNECTED:
		LOG_INF("Peer disconnected");
		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}

#if !defined(CONFIG_SAMPLE_NUS_LPUARTE)
		nrf_gpio_pin_write(BOARD_PIN_LED_1, !BOARD_LED_ACTIVE_STATE);
#endif

		break;

	case BLE_GAP_EVT_AUTH_STATUS:
		LOG_INF("Authentication status: %#x",
			evt->evt.gap_evt.params.auth_status.auth_status);
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
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);

/**
 * @brief Connection parameters event handler
 *
 * @param[in] evt BLE connection parameters event.
 */
void on_conn_params_evt(const struct ble_conn_params_evt *evt)
{
	uint32_t nrf_err;

	switch (evt->evt_type) {
	case BLE_CONN_PARAMS_EVT_REJECTED:
		nrf_err = sd_ble_gap_disconnect(evt->conn_handle,
						BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
		if (nrf_err) {
			LOG_ERR("Disconnect failed on conn params update rejection, nrf_error %#x",
				nrf_err);
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
 */
static void ble_nus_evt_handler(struct ble_nus *nus, const struct ble_nus_evt *evt)
{
	const char newline = '\n';
	int err;

	if (evt->evt_type == BLE_NUS_EVT_ERROR) {
		LOG_ERR("NUS error event, error %d", evt->error.reason);
	}

	if (evt->evt_type != BLE_NUS_EVT_RX_DATA) {
		return;
	}

	/* Handle incoming data */
	LOG_DBG("Received data from BLE NUS: %.*s (%d)",
		evt->rx_data.length, evt->rx_data.data, evt->rx_data.length);

#if defined(CONFIG_SAMPLE_NUS_LPUARTE)
	err = bm_lpuarte_tx(&lpu, evt->rx_data.data, evt->rx_data.length, 3000);
	if (err) {
		LOG_ERR("bm_lpuarte_tx failed, err %d", err);
	}
#else
	err = nrfx_uarte_tx(&nus_uarte_inst, evt->rx_data.data,
			    evt->rx_data.length, NRFX_UARTE_TX_BLOCKING);
	if (err) {
		LOG_ERR("nrfx_uarte_tx failed, err %d", err);
	}
#endif


	if (evt->rx_data.data[evt->rx_data.length - 1] == '\r') {
#if defined(CONFIG_SAMPLE_NUS_LPUARTE)
		(void)bm_lpuarte_tx(&lpu, &newline, 1, 3000);
#else
		(void)nrfx_uarte_tx(&nus_uarte_inst, &newline, 1, NRFX_UARTE_TX_BLOCKING);
#endif
	}
}

ISR_DIRECT_DECLARE(uarte_direct_isr)
{
	nrfx_uarte_irq_handler(&nus_uarte_inst);
	return 0;
}

/**
 * @brief Initalize UARTE driver.
 */
static int uarte_init(void)
{
	int err;
	nrfx_uarte_config_t *uarte_cfg;
#if defined(CONFIG_SAMPLE_NUS_LPUARTE)
	struct bm_lpuarte_config lpu_cfg = {
		.uarte_inst = &nus_uarte_inst,
		.uarte_cfg = NRFX_UARTE_DEFAULT_CONFIG(NUS_UARTE_PIN_TX,
						       NUS_UARTE_PIN_RX),
		.req_pin = BOARD_APP_LPUARTE_PIN_REQ,
		.rdy_pin = BOARD_APP_LPUARTE_PIN_RDY,
	};

	uarte_cfg = &lpu_cfg.uarte_cfg;
#else
	nrfx_uarte_config_t uarte_config = NRFX_UARTE_DEFAULT_CONFIG(NUS_UARTE_PIN_TX,
								     NUS_UARTE_PIN_RX);

	uarte_cfg = &uarte_config;

#if defined(CONFIG_SAMPLE_NUS_UART_HWFC)
	uarte_cfg->config.hwfc = NRF_UARTE_HWFC_ENABLED;
	uarte_cfg->cts_pin = NUS_UARTE_PIN_CTS;
	uarte_cfg->rts_pin = NUS_UARTE_PIN_RTS;
#endif /* CONFIG_SAMPLE_NUS_UART_HWFC */
#endif /* CONFIG_SAMPLE_NUS_LPUARTE */

#if defined(CONFIG_SAMPLE_NUS_UART_PARITY)
	uarte_cfg->parity = NRF_UARTE_PARITY_INCLUDED;
#endif

	uarte_cfg->interrupt_priority = CONFIG_SAMPLE_NUS_UART_IRQ_PRIO;

	/** We need to connect the IRQ ourselves. */

	IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(NUS_UARTE_INST),
			   CONFIG_SAMPLE_NUS_UART_IRQ_PRIO, uarte_direct_isr, 0);

	irq_enable(NRFX_IRQ_NUMBER_GET(NUS_UARTE_INST));

#if defined(CONFIG_SAMPLE_NUS_LPUARTE)
	err = bm_lpuarte_init(&lpu, &lpu_cfg, uarte_evt_handler);
	if (err) {
		LOG_ERR("Failed to initialize UART, err %d", err);
		return err;
	}
#else
	err = nrfx_uarte_init(&nus_uarte_inst, &uarte_config, uarte_evt_handler);
	if (err) {
		LOG_ERR("Failed to initialize UART, err %d", err);
		return err;
	}
#endif /* CONFIG_SAMPLE_NUS_LPUARTE */

	return 0;
}

int main(void)
{
	int err;
	uint32_t nrf_err;
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
		.sec_mode = BLE_NUS_CONFIG_SEC_MODE_DEFAULT,
	};
	struct ble_qwr_config qwr_config = {
		.evt_handler = ble_qwr_evt_handler,
	};

	LOG_INF("BLE NUS sample started");

#if !defined(CONFIG_SAMPLE_NUS_LPUARTE)
	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_1);
#endif

	err = uarte_init();
	if (err) {
		LOG_ERR("Failed to enable UARTE, err %d", err);
		goto idle;
	}

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

	nrf_err = ble_qwr_init(&ble_qwr, &qwr_config);
	if (nrf_err) {
		LOG_ERR("ble_qwr_init failed, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_nus_init(&ble_nus, &nus_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize Nordic uart service, nrf_error %#x", nrf_err);
		goto idle;
	}

	/* Adding the Nordic UART Service UUID to the scan response data. */
	ble_uuid_t adv_uuid_list[] = {
		/* Using a vendor specific UUID type that was added during NUS initialization. */
		{ .uuid = BLE_UUID_NUS_SERVICE, .type = ble_nus.uuid_type },
	};
	ble_adv_cfg.sr_data.uuid_lists.complete.uuid = &adv_uuid_list[0];
	ble_adv_cfg.sr_data.uuid_lists.complete.len = ARRAY_SIZE(adv_uuid_list);

	LOG_INF("Services initialized");

	nrf_err = ble_conn_params_evt_handler_set(on_conn_params_evt);
	if (nrf_err) {
		LOG_ERR("Failed to setup conn param event handler, nrf_error %#x", nrf_err);
		goto idle;
	}

	nrf_err = ble_adv_init(&ble_adv, &ble_adv_cfg);
	if (nrf_err) {
		LOG_ERR("Failed to initialize advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

#if defined(CONFIG_SAMPLE_NUS_LPUARTE)
	err = bm_lpuarte_rx_enable(&lpu);
	if (err) {
		LOG_ERR("UART RX failed, err %d", err);
	}
#else
	const uint8_t out[] = "UART started.\r\n";

	err = nrfx_uarte_tx(&nus_uarte_inst, out, sizeof(out), NRFX_UARTE_TX_BLOCKING);
	if (err) {
		LOG_ERR("UARTE TX failed, err %d", err);
		goto idle;
	}

	err = nrfx_uarte_rx_enable(&nus_uarte_inst, 0);
	if (err) {
		LOG_ERR("UART RX failed, err %d", err);
	}
#endif

	nrf_err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (nrf_err) {
		LOG_ERR("Failed to start advertising, nrf_error %#x", nrf_err);
		goto idle;
	}

	LOG_INF("Advertising as %s", CONFIG_BLE_ADV_NAME);

#if !defined(CONFIG_SAMPLE_NUS_LPUARTE)
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
#endif

	LOG_INF("BLE NUS sample initialized");

idle:
	while (true) {
		log_flush();

		k_cpu_idle();
	}

	return 0;
}
