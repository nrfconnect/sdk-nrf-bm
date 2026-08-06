/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <bm/bm_irq.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_uarte.h>
#include <board-config.h>
#include <nrfx_uarte.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_UARTE_LOG_LEVEL);

/** Application UARTE instance */
static nrfx_uarte_t uarte_inst = NRFX_UARTE_INSTANCE(BOARD_APP_UARTE_INST);

/* Double-buffered RX: one buffer is always queued as "next",
 * so when the current one is filled or aborted, DMA continues into the other.
 */
static uint8_t uarte_rx_buf[2][CONFIG_SAMPLE_UARTE_DATA_LEN_MAX];
static int buf_idx;

/* Handle data received from UARTE. */
static void uarte_rx_handler(char *data, size_t data_len)
{
	int err;

	LOG_HEXDUMP_INF(data, data_len, "Received data from UARTE:");

	err = nrfx_uarte_tx(&uarte_inst, data, data_len, NRFX_UARTE_TX_BLOCKING);
	if (err) {
		LOG_ERR("nrfx_uarte_tx failed, err %d", err);
	}
}

/* UARTE event handler */
static void uarte_event_handler(const nrfx_uarte_event_t *event, void *ctx)
{
	switch (event->type) {
	case NRFX_UARTE_EVT_RX_DONE:
		if (event->data.rx.length > 0) {
			uarte_rx_handler(event->data.rx.p_buffer, event->data.rx.length);
		}
		break;
	case NRFX_UARTE_EVT_RX_BUF_REQUEST:
		/* Hand over the other buffer to keep the pipeline full. */
		(void)nrfx_uarte_rx_buffer_set(&uarte_inst, uarte_rx_buf[buf_idx],
					       sizeof(uarte_rx_buf[buf_idx]));
		buf_idx = buf_idx ? 0 : 1;
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
	/* The compare-match filter raises MATCH0 on '\r' and MATCH1 on '\n',
	 * either one ends a line, so both are handled the same way here.
	 * Clear it and abort the current RX. A second buffer is already queued,
	 * so reception continues into it without a gap.
	 */
	NRF_UARTE_Type *reg = uarte_inst.p_reg;
	bool match = false;

	if (reg->EVENTS_DMA.RX.MATCH[0]) {
		nrf_uarte_event_clear(reg,
			(nrf_uarte_event_t)offsetof(NRF_UARTE_Type, EVENTS_DMA.RX.MATCH[0]));
		match = true;
	}

	if (reg->EVENTS_DMA.RX.MATCH[1]) {
		nrf_uarte_event_clear(reg,
			(nrf_uarte_event_t)offsetof(NRF_UARTE_Type, EVENTS_DMA.RX.MATCH[1]));
		match = true;
	}

	if (match) {
		(void)nrfx_uarte_rx_abort(&uarte_inst, false, false);
	}

	nrfx_uarte_irq_handler(&uarte_inst);
	return 0;
}

/* Initialize UARTE driver. */
static int uarte_init(void)
{
	int err;

	nrfx_uarte_config_t uarte_config = NRFX_UARTE_DEFAULT_CONFIG(BOARD_APP_UARTE_PIN_TX,
								     BOARD_APP_UARTE_PIN_RX);

#if defined(CONFIG_SAMPLE_UARTE_HWFC)
	uarte_config.config.hwfc = NRF_UARTE_HWFC_ENABLED;
	uarte_config.cts_pin = BOARD_APP_UARTE_PIN_CTS;
	uarte_config.rts_pin = BOARD_APP_UARTE_PIN_RTS;
#endif

#if defined(CONFIG_SAMPLE_UARTE_PARITY)
	uarte_config.parity = NRF_UARTE_PARITY_INCLUDED;
#endif

	uarte_config.interrupt_priority = CONFIG_SAMPLE_UARTE_IRQ_PRIO;

	/* We need to connect the IRQ ourselves. */
	BM_IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(BOARD_APP_UARTE_INST),
			      CONFIG_SAMPLE_UARTE_IRQ_PRIO,
			      uarte_direct_isr, 0);

	irq_enable(NRFX_IRQ_NUMBER_GET(BOARD_APP_UARTE_INST));

	err = nrfx_uarte_init(&uarte_inst, &uarte_config, uarte_event_handler);
	if (err) {
		LOG_ERR("Failed to initialize UARTE, err %d", err);
		return err;
	}

	/* Configure the compare-match filter so reception stops on '\r' or '\n':
	 * CANDIDATE[0] = '\r', CANDIDATE[1] = '\n', both enabled and both raising
	 * an interrupt (uarte_direct_isr aborts RX on either match).
	 */
	NRF_UARTE_Type *reg = uarte_inst.p_reg;

	reg->DMA.RX.MATCH.CANDIDATE[0] = '\r';
	reg->DMA.RX.MATCH.CANDIDATE[1] = '\n';
	reg->DMA.RX.MATCH.CONFIG       = UARTE_DMA_RX_MATCH_CONFIG_ENABLE0_Msk |
					 UARTE_DMA_RX_MATCH_CONFIG_ENABLE1_Msk;
	reg->INTENSET                  = UARTE_INTENSET_DMARXMATCH0_Msk |
					 UARTE_INTENSET_DMARXMATCH1_Msk;

	return 0;
}

int main(void)
{
	int err;

	LOG_INF("UARTE sample started");

	err = uarte_init();
	if (err) {
		LOG_ERR("Failed to enable UARTE, err %d", err);
		goto idle;
	}

	const uint8_t out[] = "Hello world! I will echo the lines you enter:\r\n";

	err = nrfx_uarte_tx(&uarte_inst, out, sizeof(out), NRFX_UARTE_TX_BLOCKING);
	if (err) {
		LOG_ERR("UARTE TX failed, err %d", err);
		goto idle;
	}

	/* Continuous mode keeps RX running across buffers. With a second buffer queued,
	 * an abort on match ends the current one and continues into the other.
	 */
	err = nrfx_uarte_rx_enable(&uarte_inst, NRFX_UARTE_RX_ENABLE_CONT);
	if (err) {
		LOG_ERR("UARTE RX failed, err %d", err);
	}

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);

	LOG_INF("UARTE sample initialized");

idle:
	while (true) {
		log_flush();

		k_cpu_idle();
	}

	return 0;
}
