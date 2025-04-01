/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_backend_std.h>
#include <zephyr/logging/log_msg.h>

#include <nrfx_uarte.h>
#include <board-config.h>

static const nrfx_uarte_t uarte_inst = NRFX_UARTE_INSTANCE(BOARD_CONSOLE_UARTE_INST);
static uint8_t lbu_buffer[CONFIG_LOG_BACKEND_BM_UARTE_BUFFER_SIZE];
static uint32_t log_format_current = CONFIG_LOG_BACKEND_BM_UARTE_OUTPUT_DEFAULT;

static char uarte_tx_buf[CONFIG_LOG_BACKEND_BM_UARTE_BUFFER_SIZE];

static int log_out(uint8_t *data, size_t length, void *ctx);
LOG_OUTPUT_DEFINE(bm_lbu_output, log_out, lbu_buffer, CONFIG_LOG_BACKEND_BM_UARTE_BUFFER_SIZE);

static int uarte_init(void)
{
	int err;

	nrfx_uarte_config_t uarte_config = NRFX_UARTE_DEFAULT_CONFIG(
		BOARD_CONSOLE_UARTE_PIN_TX, NRF_UARTE_PSEL_DISCONNECTED);

	if (IS_ENABLED(CONFIG_LOG_BACKEND_BM_UARTE_USE_HWFC)) {
		uarte_config.config.hwfc = NRF_UARTE_HWFC_ENABLED;
		uarte_config.cts_pin = BOARD_CONSOLE_UARTE_PIN_CTS;
		uarte_config.rts_pin = NRF_UARTE_PSEL_DISCONNECTED;
	}

	if (IS_ENABLED(CONFIG_LOG_BACKEND_BM_UARTE_PARITY_INCLUDED)) {
		uarte_config.config.parity = NRF_UARTE_PARITY_INCLUDED;
	}

	uarte_config.interrupt_priority = CONFIG_LOG_BACKEND_BM_UARTE_IRQ_PRIO;
	uarte_config.tx_cache.p_buffer = uarte_tx_buf;
	uarte_config.tx_cache.length = sizeof(uarte_tx_buf);

	/** We need to connect the IRQ ourselves. */
	IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(
		NRF_UARTE_INST_GET(BOARD_CONSOLE_UARTE_INST)),
		CONFIG_LOG_BACKEND_BM_UARTE_IRQ_PRIO,
		NRFX_UARTE_INST_HANDLER_GET(BOARD_CONSOLE_UARTE_INST), 0, 0);

	irq_enable(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(BOARD_CONSOLE_UARTE_INST)));

	err = nrfx_uarte_init(&uarte_inst, &uarte_config, NULL);
	if (err != NRFX_SUCCESS) {
		return err;
	}

	return 0;
}

static int log_out(uint8_t *data, size_t length, void *ctx)
{
	(void)nrfx_uarte_tx(&uarte_inst, data, length, NRFX_UARTE_TX_BLOCKING);

	return length;
}

static void process(const struct log_backend *const backend, union log_msg_generic *msg)
{
	uint32_t flags = log_backend_std_get_flags();
	log_format_func_t log_output_func = log_format_func_t_get(log_format_current);

	log_output_func(&bm_lbu_output, &msg->log, flags);
}

static void log_backend_uart_init(struct log_backend const *const backend)
{
	uarte_init();
}

static void dropped(const struct log_backend *const backend, uint32_t cnt)
{
	log_output_dropped_process(&bm_lbu_output, cnt);
}

static int format_set(const struct log_backend *const backend, uint32_t log_type)
{
	log_format_current = log_type;

	return 0;
}

static void panic(struct log_backend const *const backend)
{
	/* Empty until backend uses interrupts */
}

static const struct log_backend_api log_backend_api = {
	.process = process,
	.panic = panic,
	.init = log_backend_uart_init,
	.dropped = IS_ENABLED(CONFIG_LOG_MODE_IMMEDIATE) ? NULL : dropped,
	.format_set = format_set,
};

#define AUTO_START true
LOG_BACKEND_DEFINE(log_backend_bm_uarte, log_backend_api, AUTO_START, NULL);
