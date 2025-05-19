/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/printk.h>

#include <board-config.h>
#include <nrfx_uarte.h>

/** Application UARTE instance */
static const nrfx_uarte_t uarte_inst = NRFX_UARTE_INSTANCE(BOARD_APP_UARTE_INST);

/* Receive buffer used in UART ISR callback */
static char uarte_rx_buf[1];

/* Handle data received from UART. */
static void uarte_rx_handler(char *data, size_t data_len)
{
	nrfx_err_t err;
	uint8_t c;
	static char rx_buf[CONFIG_UARTE_DATA_LEN_MAX];
	static uint16_t rx_buf_idx;

	for (int i = 0; i < data_len; i++) {
		c = data[i];

		if (rx_buf_idx < sizeof(rx_buf)) {
			rx_buf[rx_buf_idx++] = c;
		}

		if ((c == '\n' || c == '\r') || (rx_buf_idx >= sizeof(rx_buf))) {
			if (rx_buf_idx == 0) {
				/* RX buffer is empty, nothing to send. */
				continue;
			}

			printk("Echo data, len %d\n", rx_buf_idx);

			/* Add newline if not already output at the end */
			if (rx_buf[rx_buf_idx - 1] != '\n') {
				rx_buf[rx_buf_idx++] = '\n';
			}

			err = nrfx_uarte_tx(&uarte_inst, rx_buf, rx_buf_idx,
					    NRFX_UARTE_TX_BLOCKING);
			if (err != NRFX_SUCCESS) {
				printk("nrfx_uarte_tx failed, nrfx_err %d\n", err);
			}

			rx_buf_idx = 0;
		}
	}
}

/* UARTE event handler */
static void uarte_event_handler(nrfx_uarte_event_t const *event, void *ctx)
{
	switch (event->type) {
	case NRFX_UARTE_EVT_RX_DONE:
		printk("Received data from UART: %c\n", event->data.rx.p_buffer[0]);
		if (event->data.rx.length > 0) {
			uarte_rx_handler(event->data.rx.p_buffer, event->data.rx.length);
		}

		/* Provide new UART RX buffer. */
		nrfx_uarte_rx(&uarte_inst, uarte_rx_buf, sizeof(uarte_rx_buf));
		break;
	case NRFX_UARTE_EVT_RX_BUF_REQUEST:
		nrfx_uarte_rx_buffer_set(&uarte_inst, uarte_rx_buf, sizeof(uarte_rx_buf));
		break;
	case NRFX_UARTE_EVT_ERROR:
		printk("uarte error %#x\n", event->data.error.error_mask);
		break;
	default:
		break;
	}
}

/* Initialize UARTE driver. */
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

	/* We need to connect the IRQ ourselves. */
	IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(BOARD_APP_UARTE_INST)),
		    CONFIG_UARTE_IRQ_PRIO,
		    NRFX_UARTE_INST_HANDLER_GET(BOARD_APP_UARTE_INST), 0, 0);

	irq_enable(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(BOARD_APP_UARTE_INST)));

	err = nrfx_uarte_init(&uarte_inst, &uarte_config, uarte_event_handler);
	if (err != NRFX_SUCCESS) {
		printk("Failed to initialize UART, nrfx err %d\n", err);
		return err;
	}

	return 0;
}

int main(void)
{
	int err;

	printk("UART sample started\n");

	err = uarte_init();
	if (err) {
		printk("Failed to enable UARTE, err %d\n", err);
		return -1;
	}

	const uint8_t out[] = "Hello world! I will echo the lines you enter:\r\n";

	err = nrfx_uarte_tx(&uarte_inst, out, sizeof(out), NRFX_UARTE_TX_BLOCKING);
	if (err != NRFX_SUCCESS) {
		printk("UARTE TX failed, nrfx err %d\n", err);
		return -1;
	}

	/* Start reception */
	err = nrfx_uarte_rx(&uarte_inst, uarte_rx_buf, sizeof(uarte_rx_buf));
	if (err != NRFX_SUCCESS) {
		printk("UART RX failed, nrfx err %d\n", err);
	}

	while (true) {
		/* Sleep */
		__WFE();
	}

	/* Unreachable */
	return 0;
}
