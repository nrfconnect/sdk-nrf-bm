/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrfx_uarte.h>
#include <bm/drivers/bm_lpuarte.h>
#include <bm/bm_timer.h>

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <board-config.h>

LOG_MODULE_REGISTER(app, CONFIG_LPUARTE_SAMPLE_LOG_LEVEL);

/** Application Low Power UARTE instance */
struct bm_lpuarte lpu;

/* Receive buffer used in UARTE ISR callback */
static uint8_t uarte_rx_buf[256];
static int buf_idx;

/* Handle data received from UARTE. */
static void uarte_rx_handler(char *data, size_t data_len)
{
	LOG_HEXDUMP_INF(data, data_len, "Received data from UARTE:");
}

/* UARTE event handler */
static void lpuarte_event_handler(nrfx_uarte_event_t const *event, void *ctx)
{
	nrfx_err_t nrfx_err;
	struct bm_lpuarte *lpu = ctx;

	switch (event->type) {
	case NRFX_UARTE_EVT_RX_DONE:
		if (event->data.rx.length > 0) {
			uarte_rx_handler(event->data.rx.p_buffer, event->data.rx.length);
		}
		break;
	case NRFX_UARTE_EVT_RX_BUF_REQUEST:
		nrfx_err = bm_lpuarte_rx_buffer_set(lpu, &uarte_rx_buf[buf_idx * 128], 128);
		buf_idx++;
		buf_idx = (buf_idx < 2) ? buf_idx : 0;
		break;
	case NRFX_UARTE_EVT_ERROR:
		LOG_ERR("UARTE error event, %#x", event->data.error.error_mask);
		break;
	default:
		break;
	}
}

ISR_DIRECT_DECLARE(gpiote_20_direct_isr)
{
	NRFX_GPIOTE_INST_HANDLER_GET(20)();
	return 0;
}

ISR_DIRECT_DECLARE(gpiote_30_direct_isr)
{
	NRFX_GPIOTE_INST_HANDLER_GET(30)();
	return 0;
}

ISR_DIRECT_DECLARE(lpuarte_direct_isr)
{
	NRFX_UARTE_INST_HANDLER_GET(BOARD_APP_LPUARTE_INST)();
	return 0;
}

/* Initialize UARTE driver. */
static nrfx_err_t lpuarte_init(void)
{
	nrfx_err_t nrfx_err;

	struct bm_lpuarte_config lpu_cfg = {
		.uarte_inst = NRFX_UARTE_INSTANCE(BOARD_APP_LPUARTE_INST),
		.uarte_cfg = NRFX_UARTE_DEFAULT_CONFIG(BOARD_APP_LPUARTE_PIN_TX,
						       BOARD_APP_LPUARTE_PIN_RX),
		.req_pin = BOARD_APP_LPUARTE_PIN_REQ,
		.rdy_pin = BOARD_APP_LPUARTE_PIN_RDY,
	};

#if defined(CONFIG_LPUARTE_PARITY)
	lpu_cfg.uarte_cfg.config.parity = NRF_UARTE_PARITY_INCLUDED;
#endif

	lpu_cfg.uarte_cfg.interrupt_priority = CONFIG_LPUARTE_IRQ_PRIO;

	IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_GPIOTE_INST_GET(20)) + NRF_GPIOTE_IRQ_GROUP,
			   CONFIG_LPUARTE_GPIOTE_IRQ_PRIO, gpiote_20_direct_isr, 0);

	IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_GPIOTE_INST_GET(30)) + NRF_GPIOTE_IRQ_GROUP,
			   CONFIG_LPUARTE_GPIOTE_IRQ_PRIO, gpiote_30_direct_isr, 0);

	IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(BOARD_APP_LPUARTE_INST)),
			   CONFIG_LPUARTE_IRQ_PRIO, lpuarte_direct_isr, 0);

	irq_enable(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(BOARD_APP_LPUARTE_INST)));

	nrfx_err = bm_lpuarte_init(&lpu, &lpu_cfg, lpuarte_event_handler);
	if (nrfx_err != NRFX_SUCCESS) {
		LOG_ERR("Failed to initialize UARTE, nrfx_err %#x", nrfx_err);
		return nrfx_err;
	}

	return NRFX_SUCCESS;
}

static struct bm_timer tx_timer;
static uint8_t out[] = {1, 2, 3, 4, 5};

static void tx_timeout(void *context)
{
	nrfx_err_t nrfx_err;

	nrfx_err = bm_lpuarte_tx(&lpu, out, sizeof(out), 3000);
	if (nrfx_err != NRFX_SUCCESS) {
		LOG_ERR("UARTE TX failed, nrfx err %#x", nrfx_err);
		return;
	}
}

int main(void)
{
	int err;
	nrfx_err_t nrfx_err;

	LOG_INF("LPUARTE sample started");
	LOG_INF("Disable console and logging for minimal power consumption");

	nrfx_err = lpuarte_init();
	if (nrfx_err != NRFX_SUCCESS) {
		LOG_ERR("Failed to enable UARTE, nrfx_err %#x", nrfx_err);
		goto idle;
	}

	/* Start reception */
	nrfx_err = bm_lpuarte_rx_enable(&lpu);
	if (nrfx_err != NRFX_SUCCESS) {
		LOG_ERR("UARTE RX failed, nrfx_err %#x", nrfx_err);
		goto idle;
	}

	err = bm_timer_init(&tx_timer, BM_TIMER_MODE_REPEATED, tx_timeout);
	if (err) {
		LOG_ERR("bm_timer_init failed, err %d", err);
		goto idle;
	}

	err = bm_timer_start(&tx_timer, BM_TIMER_MS_TO_TICKS(5000), NULL);
	if (err) {
		LOG_ERR("bm_timer_start failed, err %d", err);
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

	/* Unreachable */
	return 0;
}
