/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/printk-hooks.h>
#include <zephyr/sys/libc-hooks.h>

#include <nrfx_uarte.h>

#include <board-config.h>

static const nrfx_uarte_t uarte_inst = NRFX_UARTE_INSTANCE(BOARD_CONSOLE_UARTE_INST);

ISR_DIRECT_DECLARE(console_bm_uarte_direct_isr)
{
	NRFX_UARTE_INST_HANDLER_GET(BOARD_CONSOLE_UARTE_INST)();
	return 0;
}

static int uarte_init(void)
{
	int err;

	nrfx_uarte_config_t uarte_config = NRFX_UARTE_DEFAULT_CONFIG(
		BOARD_CONSOLE_UARTE_PIN_TX, NRF_UARTE_PSEL_DISCONNECTED);

#if CONFIG_BM_UARTE_CONSOLE_UARTE_USE_HWFC
	uarte_config.config.hwfc = NRF_UARTE_HWFC_ENABLED;
	uarte_config.cts_pin = BOARD_CONSOLE_UARTE_PIN_CTS;
	uarte_config.rts_pin = NRF_UARTE_PSEL_DISCONNECTED;
#endif

#if CONFIG_BM_UARTE_CONSOLE_UARTE_PARITY_INCLUDED
	uarte_config.config.parity = NRF_UARTE_PARITY_INCLUDED;
#endif

	uarte_config.interrupt_priority = CONFIG_BM_UARTE_CONSOLE_UARTE_IRQ_PRIO;

	/** We need to connect the IRQ ourselves. */
	IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(BOARD_CONSOLE_UARTE_INST)),
			   CONFIG_BM_UARTE_CONSOLE_UARTE_IRQ_PRIO,
			   console_bm_uarte_direct_isr, 0);

	irq_enable(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(BOARD_CONSOLE_UARTE_INST)));

	err = nrfx_uarte_init(&uarte_inst, &uarte_config, NULL);
	if (err != NRFX_SUCCESS) {
		return err;
	}

	return 0;
}

static int console_out(int c)
{
	const char c2 = c;

#if defined(CONFIG_BM_UARTE_CONSOLE_CR_LF_TERMINATION)
	const char r = '\r';

	if ('\n' == c) {
		nrfx_uarte_tx(&uarte_inst, &r, 1, NRFX_UARTE_TX_BLOCKING);
	}
#endif

	nrfx_uarte_tx(&uarte_inst, &c2, 1, NRFX_UARTE_TX_BLOCKING);

	/* Return the character passed as input. */
	return c;
}

static int uart_log_backend_sys_init(void)
{
	if (!IS_ENABLED(CONFIG_LOG_BACKEND_BM_UARTE)) {
		uarte_init();
	}

#if defined(CONFIG_STDOUT_CONSOLE)
	__stdout_hook_install(console_out);
#endif
#if defined(CONFIG_PRINTK)
	__printk_hook_install(console_out);
#endif
	return 0;
}

SYS_INIT(uart_log_backend_sys_init,
#if defined(CONFIG_EARLY_CONSOLE)
	 PRE_KERNEL_1,
#else
	 POST_KERNEL,
#endif
	 0);
