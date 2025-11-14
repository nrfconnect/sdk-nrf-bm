/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <board-config.h>
#include <nrfx_uarte.h>

LOG_MODULE_REGISTER(app, CONFIG_APP_UARTE_LOG_LEVEL);

/** Application UARTE instance */
static const nrfx_uarte_t uarte_inst = NRFX_UARTE_INSTANCE(BOARD_APP_UARTE_INST);

/* Receive buffer used in UARTE ISR callback */
static uint8_t uarte_rx_buf[4];
static int buf_idx;

/* Handle data received from UARTE. */
static void uarte_rx_handler(char *data, size_t data_len)
{
	nrfx_err_t nrfx_err;
	uint8_t c;
	static char rx_buf[CONFIG_APP_UARTE_DATA_LEN_MAX];
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

			LOG_INF("Echo data, len %d", rx_buf_idx);

			/* Add newline if not already output at the end */
			if ((rx_buf[rx_buf_idx - 1] != '\n') && (rx_buf_idx < sizeof(rx_buf))) {
				rx_buf[rx_buf_idx++] = '\n';
			}

			nrfx_err = nrfx_uarte_tx(&uarte_inst, rx_buf, rx_buf_idx,
					    NRFX_UARTE_TX_BLOCKING);
			if (nrfx_err != NRFX_SUCCESS) {
				LOG_ERR("nrfx_uarte_tx failed, nrfx_err %#x", nrfx_err);
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
		LOG_INF("Received data from UARTE: %c", event->data.rx.p_buffer[0]);
		if (event->data.rx.length > 0) {
			uarte_rx_handler(event->data.rx.p_buffer, event->data.rx.length);
		}

		/* Provide new UARTE RX buffer. */
		nrfx_uarte_rx_enable(&uarte_inst, 0);
		break;
	case NRFX_UARTE_EVT_RX_BUF_REQUEST:
		nrfx_uarte_rx_buffer_set(&uarte_inst, &uarte_rx_buf[buf_idx], 1);

		buf_idx++;
		buf_idx = (buf_idx < sizeof(uarte_rx_buf)) ? buf_idx : 0;
		break;
	case NRFX_UARTE_EVT_ERROR:
		LOG_ERR("UARTE error %#x", event->data.error.error_mask);
		break;
	default:
		break;
	}
}

ISR_DIRECT_DECLARE(uarte_direct_isr)
{
	NRFX_UARTE_INST_HANDLER_GET(BOARD_APP_UARTE_INST)();
	return 0;
}

/* Initialize UARTE driver. */
static nrfx_err_t uarte_init(void)
{
	nrfx_err_t nrfx_err;

	nrfx_uarte_config_t uarte_config = NRFX_UARTE_DEFAULT_CONFIG(BOARD_APP_UARTE_PIN_TX,
								     BOARD_APP_UARTE_PIN_RX);

#if defined(CONFIG_APP_UARTE_HWFC)
	uarte_config.config.hwfc = NRF_UARTE_HWFC_ENABLED;
	uarte_config.cts_pin = BOARD_APP_UARTE_PIN_CTS;
	uarte_config.rts_pin = BOARD_APP_UARTE_PIN_RTS;
#endif

#if defined(CONFIG_APP_UARTE_PARITY)
	uarte_config.parity = NRF_UARTE_PARITY_INCLUDED;
#endif

	uarte_config.interrupt_priority = CONFIG_APP_UARTE_IRQ_PRIO;

	/* We need to connect the IRQ ourselves. */
	IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(BOARD_APP_UARTE_INST)),
			   CONFIG_APP_UARTE_IRQ_PRIO,
			   uarte_direct_isr, 0);

	irq_enable(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(BOARD_APP_UARTE_INST)));

	nrfx_err = nrfx_uarte_init(&uarte_inst, &uarte_config, uarte_event_handler);
	if (nrfx_err != NRFX_SUCCESS) {
		LOG_ERR("Failed to initialize UARTE, nrfx_err %#x", nrfx_err);
		return nrfx_err;
	}

	return NRFX_SUCCESS;
}

int main(void)
{
	nrfx_err_t nrfx_err;

	LOG_INF("UARTE sample started");

	nrfx_err = uarte_init();
	if (nrfx_err != NRFX_SUCCESS) {
		LOG_ERR("Failed to enable UARTE, nrfx_err %#x", nrfx_err);
		goto idle;
	}

	const uint8_t out[] = "Hello world! I will echo the lines you enter:\r\n";

	nrfx_err = nrfx_uarte_tx(&uarte_inst, out, sizeof(out), NRFX_UARTE_TX_BLOCKING);
	if (nrfx_err != NRFX_SUCCESS) {
		LOG_ERR("UARTE TX failed, nrfx_err %#x", nrfx_err);
		goto idle;
	}

	/* Start reception */
	nrfx_err = nrfx_uarte_rx_enable(&uarte_inst, 0);
	if (nrfx_err != NRFX_SUCCESS) {
		LOG_ERR("UARTE RX failed, nrfx_err %#x", nrfx_err);
	}

idle:
	while (true) {
		while (LOG_PROCESS()) {
		}

		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();
	}

	return 0;
}
