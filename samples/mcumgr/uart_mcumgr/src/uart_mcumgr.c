/*
 * Copyright Runtime.io 2018. All rights reserved.
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/mgmt/mcumgr/transport/serial.h>
#include <zephyr/drivers/console/uart_mcumgr.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>
#include <nrfx_uarte.h>
#include <smp_uart.h>
#include <board-config.h>

LOG_MODULE_REGISTER(uart_mcumgr, CONFIG_UART_MCUMGR_SAMPLE_LOG_LEVEL);

static char uarte_rx_buf[CONFIG_UART_MCUMGR_RX_BUF_SIZE];
static bool should_reboot;

static enum mgmt_cb_return os_mgmt_reboot_hook(uint32_t event, enum mgmt_cb_return prev_status,
					       int32_t *rc, uint16_t *group, bool *abort_more,
					       void *data, size_t data_size);

static struct mgmt_callback os_mgmt_reboot_callback = {
	.callback = os_mgmt_reboot_hook,
	.event_id = MGMT_EVT_OP_OS_MGMT_RESET,
};

/** MCUmgr UARTE instance */
static const nrfx_uarte_t uarte_inst = NRFX_UARTE_INSTANCE(BOARD_APP_UARTE_INST);

/** Callback to execute when a valid fragment has been received. */
static uart_mcumgr_recv_fn *uart_mcumgr_recv_cb;

/** Contains the fragment currently being received. */
static struct uart_mcumgr_rx_buf *uart_mcumgr_cur_buf;

/**
 * Whether the line currently being read should be ignored.  This is true if
 * the line is too long or if there is no buffer available to hold it.
 */
static bool uart_mcumgr_ignoring;

/** Contains buffers to hold incoming request fragments. */
K_MEM_SLAB_DEFINE(uart_mcumgr_slab, sizeof(struct uart_mcumgr_rx_buf),
		  CONFIG_UART_MCUMGR_RX_BUF_COUNT, 1);

static struct uart_mcumgr_rx_buf *uart_mcumgr_alloc_rx_buf(void)
{
	struct uart_mcumgr_rx_buf *rx_buf;
	void *block;
	int rc;

	rc = k_mem_slab_alloc(&uart_mcumgr_slab, &block, K_NO_WAIT);
	if (rc != 0) {
		return NULL;
	}

	rx_buf = block;
	rx_buf->length = 0;
	return rx_buf;
}

void uart_mcumgr_free_rx_buf(struct uart_mcumgr_rx_buf *rx_buf)
{
	void *block;

	block = rx_buf;
	k_mem_slab_free(&uart_mcumgr_slab, block);
}

/**
 * Processes a single incoming byte.
 */
static struct uart_mcumgr_rx_buf *uart_mcumgr_rx_byte(uint8_t byte)
{
	struct uart_mcumgr_rx_buf *rx_buf;

	if (!uart_mcumgr_ignoring) {
		if (uart_mcumgr_cur_buf == NULL) {
			uart_mcumgr_cur_buf = uart_mcumgr_alloc_rx_buf();
			if (uart_mcumgr_cur_buf == NULL) {
				/* Insufficient buffers; drop this fragment. */
				uart_mcumgr_ignoring = true;
			}
		}
	}

	rx_buf = uart_mcumgr_cur_buf;
	if (!uart_mcumgr_ignoring) {
		if (rx_buf->length >= sizeof(rx_buf->data)) {
			/* Line too long; drop this fragment. */
			uart_mcumgr_free_rx_buf(uart_mcumgr_cur_buf);
			uart_mcumgr_cur_buf = NULL;
			uart_mcumgr_ignoring = true;
		} else {
			rx_buf->data[rx_buf->length++] = byte;
		}
	}

	if (byte == '\n') {
		/* Fragment complete. */
		if (uart_mcumgr_ignoring) {
			uart_mcumgr_ignoring = false;
		} else {
			uart_mcumgr_cur_buf = NULL;
			return rx_buf;
		}
	}

	return NULL;
}

/**
 * @brief Handle data received from UART.
 *
 * @param[in] data Data received.
 * @param[in] data_len Size of data.
 */
static void uarte_rx_handler(char *data, size_t data_len)
{
	struct uart_mcumgr_rx_buf *rx_buf;

	for (int i = 0; i < data_len; i++) {
		rx_buf = uart_mcumgr_rx_byte(data[i]);

		if (rx_buf != NULL) {
			uart_mcumgr_recv_cb(rx_buf);
		}
	}
}

/**
 * @brief UARTE event handler
 *
 * @param[in] event UARTE event structure
 * @param[in] ctx Context. NULL in this case.
 */
static void uarte_event_handler(nrfx_uarte_event_t const *event, void *ctx)
{
	switch (event->type) {
	case NRFX_UARTE_EVT_RX_DONE:
	{
		LOG_DBG("Received data from UART: %c", event->data.rx.p_buffer[0]);
		if (event->data.rx.length > 0) {
			uarte_rx_handler(event->data.rx.p_buffer, event->data.rx.length);
		}

		/* Provide new UART RX buffer. */
		nrfx_uarte_rx(&uarte_inst, uarte_rx_buf, 1);
		break;
	}
	case NRFX_UARTE_EVT_RX_BUF_REQUEST:
	{
		nrfx_uarte_rx_buffer_set(&uarte_inst, uarte_rx_buf, 1);
		break;
	}
	case NRFX_UARTE_EVT_ERROR:
	{
		LOG_ERR("uarte error %#x", event->data.error.error_mask);
		break;
	}
	default:
	{
		break;
	}
	};
}

/**
 * Sends raw data over the UART.
 */
static int uart_mcumgr_send_raw(const void *data, int len)
{
	if ((uint32_t)data >= CONFIG_SRAM_BASE_ADDRESS) {
		/* Data is in RAM and can be sent out directly */
		nrfx_uarte_tx(&uarte_inst, data, len, NRFX_UARTE_TX_BLOCKING);
	} else {
		/* Data is in NVM or another non-RAM source, send byte-by-byte using RAM buffer */
		int pos = 0;
		uint8_t tmp_buf;

		while (pos < len) {
			tmp_buf = ((uint8_t *)data)[pos];
			nrfx_uarte_tx(&uarte_inst, &tmp_buf, sizeof(tmp_buf),
				      NRFX_UARTE_TX_BLOCKING);
			++pos;
		}
	}

	return 0;
}

int uart_mcumgr_send(const uint8_t *data, int len)
{
	return mcumgr_serial_tx_pkt(data, len, uart_mcumgr_send_raw);
}

void uart_mcumgr_register(uart_mcumgr_recv_fn *cb)
{
	uart_mcumgr_recv_cb = cb;
}

/**
 * @brief Initialize UARTE driver.
 */
static int uarte_init(void)
{
	int err;

	nrfx_uarte_config_t uarte_config = NRFX_UARTE_DEFAULT_CONFIG(BOARD_APP_UARTE_PIN_TX,
								     BOARD_APP_UARTE_PIN_RX);

#if defined(CONFIG_UARTE_HWFC)
	uarte_config.config.hwfc = NRF_UARTE_HWFC_ENABLED;
	uarte_config.cts_pin = BOARD_APP_UARTE_PIN_CTS;
	uarte_config.rts_pin = BOARD_APP_UARTE_PIN_RTS;
#endif

#if defined(CONFIG_UARTE_PARITY)
	uarte_config.parity = NRF_UARTE_PARITY_INCLUDED;
#endif

	uarte_config.interrupt_priority = CONFIG_UARTE_IRQ_PRIO;

	/** We need to connect the IRQ ourselves. */
	IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(BOARD_APP_UARTE_INST)),
		    CONFIG_UARTE_IRQ_PRIO, NRFX_UARTE_INST_HANDLER_GET(BOARD_APP_UARTE_INST),
		    0, 0);

	irq_enable(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(BOARD_APP_UARTE_INST)));

	err = nrfx_uarte_init(&uarte_inst, &uarte_config, uarte_event_handler);

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

int main(void)
{
	int err;

	LOG_INF("UART MCUmgr sample started");

	mgmt_callback_register(&os_mgmt_reboot_callback);

	err = uarte_init();

	if (err) {
		LOG_ERR("Failed to enable UARTE, err %d", err);
		return -1;
	}

	err = nrfx_uarte_rx(&uarte_inst, uarte_rx_buf, 1);

	if (err != NRFX_SUCCESS) {
		LOG_ERR("UART RX failed, nrfx err %d", err);
	}

	while (should_reboot == false) {
		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();

		smp_uart_process_rx_queue();
	}

	sys_reboot(SYS_REBOOT_WARM);
}
