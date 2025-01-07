/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <nrf_sdh_soc.h>
#include <ble_adv.h>
#include <ble_conn_params.h>
#include <ble_gap.h>
#include <bluetooth/services/ble_nus.h>
#include <bluetooth/services/ble_dis.h>
#include <lite_timer.h>
#include <sensorsim.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#define CONN_TAG 1

BLE_ADV_DEF(ble_adv); /* BLE advertising instance */
BLE_NUS_DEF(ble_nus); /* BLE NUS service instance */

/** Handle of the current connection. */
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

#define UART_DEVICE_NODE DT_CHOSEN(nordic_nus_uart)
#define TRACE_UART_BAUD_REQUIRED 115200

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* Maximum length of data (in bytes) that can be transmitted to the peer by the
 * Nordic UART service module.
 */
static uint16_t m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;

void serial_cb(const struct device *dev, void *user_data)
{
	int err;
	uint8_t c;
	/* receive buffer used in UART ISR callback */
	static char uart_rx_buf[BLE_NUS_MAX_DATA_LEN];
	static int uart_rx_buf_pos;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		if (((c == '\n' || c == '\r') || (uart_rx_buf_pos >= m_ble_nus_max_data_len)) && uart_rx_buf_pos) {

			printk("Ready to send data over BLE NUS\n");
			//LOG_HEXDUMP_DBG(data_array, index);

			do {
				uint16_t length = (uint16_t)uart_rx_buf_pos;
				err = ble_nus_data_send(&ble_nus, uart_rx_buf, &length, conn_handle);
				if ((err != 0) &&
				    (err != NRF_ERROR_INVALID_STATE) &&
				    (err != NRF_ERROR_RESOURCES) &&
				    (err != NRF_ERROR_NOT_FOUND))
				{
					printk("Failed to send NUS data, err %d\n", err);
					return;
				}
			} while (err == NRF_ERROR_RESOURCES);


			/* reset the buffer (it was copied to the msgq) */
			uart_rx_buf_pos = 0;
		} else if (uart_rx_buf_pos < (sizeof(uart_rx_buf) - 1)) {
			uart_rx_buf[uart_rx_buf_pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}
}

static void on_ble_evt(const ble_evt_t *evt, void *ctx)
{
	int err;

	switch (evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		printk("Peer connected\n");
		conn_handle = evt->evt.gap_evt.conn_handle;
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			printk("Failed to set system attributes, nrf_error %d\n", err);
		}
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		printk("Peer disconnected\n");
		if (conn_handle == evt->evt.gap_evt.conn_handle) {
			conn_handle = BLE_CONN_HANDLE_INVALID;
		}
		break;

	case BLE_GAP_EVT_AUTH_STATUS:
		printk("Authentication status: %#x\n",
		       evt->evt.gap_evt.params.auth_status.auth_status);
		break;

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		/* Pairing not supported */
		err = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle,
						  BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
		if (err) {
			printk("Failed to reply with Security params, nrf_error %d\n", err);
		}
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		printk("BLE_GATTS_EVT_SYS_ATTR_MISSING\n");
		/* No system attributes have been stored */
		err = sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
		if (err) {
			printk("Failed to set system attributes, nrf_error %d\n", err);
		}
		break;
	}
}
NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, 0);

void on_conn_params_evt(const struct ble_conn_params_evt *evt)
{
	int err;

	switch (evt->id) {
	case BLE_CONN_PARAMS_EVT_REJECTED:
		err = sd_ble_gap_disconnect(evt->conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
		if (err) {
			printk("Disconnect failed on conn params update rejection, err %d\n", err);
		}
		printk("Disconnected from peer, unacceptable conn params\n");
		break;

	case BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED:
		if (evt->conn_handle != conn_handle) {
			printk("Connection handle does not match, expected %d, was %d\n",
				conn_handle, evt->conn_handle);
			break;;
		}
		m_ble_nus_max_data_len = evt->att_mtu;
		printk("Attribute MTU is set to 0x%X(%d)\n", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
		break;
	default:
		break;
	}
}

static void ble_adv_evt_handler(enum ble_adv_evt evt)
{
	/* ignore */
}

static void ble_adv_error_handler(uint32_t error)
{
	printk("Advertising error %d\n", error);
}

static void ble_nus_data_handler(struct ble_nus_evt *evt)
{
	if (evt->type != BLE_NUS_EVT_RX_DATA) {
		return;
	}

	/* Handle incomming data */

	printk("Received data from BLE NUS. Writing data on UART.\n");
	//LOG_HEXDUMP(evt->params.rx_data.p_data, evt->params.rx_data.length);

	for (uint32_t i = 0; i < evt->params.rx_data.length; i++)
	{
		uart_poll_out(uart_dev, evt->params.rx_data.data[i]);
	}

	if (evt->params.rx_data.data[evt->params.rx_data.length - 1] == '\r')
	{
		uart_poll_out(uart_dev, '\n');
	}
}

int uart_init(void)
{
	if (!device_is_ready(uart_dev)) {
		printk("UART device not found!");
		return 0;
	}

	/* configure interrupt and callback to receive data */
	int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);

	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return ret;
	}

	uart_irq_rx_enable(uart_dev);

	return 0;
}

int main(void)
{
	int err;
	uint32_t ram_start;
	struct ble_adv_config ble_adv_cfg = {
		.conn_cfg_tag = CONN_TAG,
		.evt_handler = ble_adv_evt_handler,
		.error_handler = ble_adv_error_handler,
		.adv_data = {
			.name_type = BLE_ADV_DATA_FULL_NAME,
			.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		},
	};

	struct ble_nus_config nus_cfg = {
		.data_handler = ble_nus_data_handler,
	};

	err = uart_init();
	if (err) {
		printk("Failed to enable UART");
		return -1;
	}

	err = nrf_sdh_enable_request();
	if (err) {
		printk("Failed to setup default configuration, err %d\n", err);
		return -1;
	}

	printk("SoftDevice enabled\n");

	nrf_sdh_ble_app_ram_start_get(&ram_start);

	err = nrf_sdh_ble_default_cfg_set(CONN_TAG);
	if (err) {
		printk("Failed to setup default configuration, err %d\n", err);
		return -1;
	}

	err = nrf_sdh_ble_enable(ram_start);
	if (err) {
		printk("Failed to enable BLE, err %d\n", err);
		return -1;
	}

	printk("Bluetooth is enabled\n");

	err = ble_conn_params_event_handler_set(on_conn_params_evt);
	if (err) {
		printk("Failed to setup conn param event handler, err %d\n", err);
		return -1;
	}

	err = ble_nus_init(&ble_nus, &nus_cfg);
	if (err) {
		printk("Failed to initialize Nordic uart service, err %d\n", err);
		return -1;
	}

	err = ble_dis_init();
	if (err) {
		printk("Failed to initialize device information service, err %d\n", err);
		return -1;
	}

	err = ble_adv_init(&ble_adv, &ble_adv_cfg);
	if (err) {
		printk("Failed to initialize BLE advertising, err %d\n", err);
		return -1;
	}

	printk("Nordic UART Service sample started\n");

	err = ble_adv_start(&ble_adv, BLE_ADV_MODE_FAST);
	if (err) {
		printk("Failed to start advertising, err %d\n", err);
		return -1;
	}

	while (true) {
		sd_app_evt_wait();
	}
}
