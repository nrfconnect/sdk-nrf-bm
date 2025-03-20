/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/printk-hooks.h>
#include <zephyr/sys/libc-hooks.h>

#include <nrfx_uarte.h>

#include <board-config.h>

static const nrfx_uarte_t uarte_inst = NRFX_UARTE_INSTANCE(LITE_UARTE_CONSOLE_UARTE_INST);

static int uarte_init(void)
{
	int err;

	nrfx_uarte_config_t uarte_config = NRFX_UARTE_DEFAULT_CONFIG(
		LITE_UARTE_CONSOLE_PIN_TX, NRF_UARTE_PSEL_DISCONNECTED);

#if CONFIG_LITE_UARTE_CONSOLE_UARTE_USE_HWFC
	uarte_config.config.hwfc = CONFIG_LITE_UARTE_CONSOLE_HWFC_ENABLED;
	uarte_config.cts_pin = LITE_UARTE_CONSOLE_PIN_CTS;
	uarte_config.rts_pin = LITE_UARTE_CONSOLE_PIN_RTS;
#endif

#if CONFIG_LITE_UARTE_CONSOLE_UARTE_PARITY_INCLUDED
	uarte_config.parity = NRF_UARTE_PARITY_INCLUDED;
#endif

	uarte_config.interrupt_priority = CONFIG_LITE_UARTE_CONSOLE_UARTE_IRQ_PRIO;

	/** We need to connect the IRQ ourselves. */
	IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(
		NRF_UARTE_INST_GET(LITE_UARTE_CONSOLE_UARTE_INST)),
		CONFIG_LITE_UARTE_CONSOLE_UARTE_IRQ_PRIO,
		NRFX_UARTE_INST_HANDLER_GET(LITE_UARTE_CONSOLE_UARTE_INST), 0, 0);

	irq_enable(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(LITE_UARTE_CONSOLE_UARTE_INST)));

	err = nrfx_uarte_init(&uarte_inst, &uarte_config, NULL);
	if (err != NRFX_SUCCESS) {
		return err;
	}

	return 0;
}

static int console_out(int c)
{
	const char c2 = c;

	nrfx_uarte_tx(&uarte_inst, &c2, 1, NRFX_UARTE_TX_BLOCKING);

	/* Return the character passed as input. */
	return c;
}

static int uart_log_backend_sys_init(void)
{
	uarte_init();

#if defined(CONFIG_STDOUT_CONSOLE)
	__stdout_hook_install(console_out);
#endif
#if defined(CONFIG_PRINTK)
	__printk_hook_install(console_out);
#endif
	return 0;
}

SYS_INIT(uart_log_backend_sys_init, APPLICATION, 0);
