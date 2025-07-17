/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/init.h>
#include <zephyr/irq.h>
#include <zephyr/shell/shell.h>
#include <bm/shell/backend_bm_uarte.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/util.h>

#include <nrfx_uarte.h>
#include <board-config.h>

#define STATE_ATOM_RX_BYTE_BIT 0
#define STATE_ATOM_RX_DONE_BIT 1
#define RX_ABORT_TIMEOUT_US 10000

static shell_transport_handler_t sh_handler;
static void *sh_context;
static nrfx_uarte_t uarte_inst = NRFX_UARTE_INSTANCE(BOARD_SHELL_UARTE_INST);
static const nrfx_uarte_config_t uarte_config = {
	.txd_pin = BOARD_SHELL_UARTE_PIN_TX,
	.rxd_pin = BOARD_SHELL_UARTE_PIN_RX,
	.rts_pin = COND_CODE_1(CONFIG_SHELL_BACKEND_BM_UARTE_USE_HWFC,
			       (BOARD_SHELL_UARTE_PIN_RTS),
			       (NRF_UARTE_PSEL_DISCONNECTED)),
	.cts_pin = COND_CODE_1(CONFIG_SHELL_BACKEND_BM_UARTE_USE_HWFC,
			       (BOARD_SHELL_UARTE_PIN_CTS),
			       (NRF_UARTE_PSEL_DISCONNECTED)),
	.baudrate = NRF_UARTE_BAUDRATE_115200,
	.config = {
		.hwfc = COND_CODE_1(CONFIG_SHELL_BACKEND_BM_UARTE_USE_HWFC,
				    (NRF_UARTE_HWFC_ENABLED),
				    (NRF_UARTE_HWFC_DISABLED)),
		.parity = COND_CODE_1(CONFIG_SHELL_BACKEND_BM_UARTE_PARITY_INCLUDED,
				      (NRF_UARTE_PARITY_INCLUDED),
				      (NRF_UARTE_PARITY_EXCLUDED)),
		NRFX_COND_CODE_1(NRF_UART_HAS_STOP_BITS,
				 (.stop = (nrf_uarte_stop_t)NRF_UARTE_STOP_ONE,), ())
		NRFX_COND_CODE_1(NRF_UART_HAS_PARITY_BIT,
				(.paritytype = NRF_UARTE_PARITYTYPE_EVEN,), ())
	},
	.interrupt_priority = CONFIG_SHELL_BACKEND_BM_UARTE_IRQ_PRIO,
};

static uint8_t dbuf[2][CONFIG_SHELL_BACKEND_BM_UARTE_RX_DBUF_SIZE / 2];
static uint8_t dbuf_idx;
static struct ring_buf rbuf;
static uint8_t rbuf_data[CONFIG_SHELL_BACKEND_BM_UARTE_RX_RBUF_SIZE];
static atomic_t state_atom;

static void uarte_event_handler(nrfx_uarte_event_t const *p_event, void *p_context)
{
	switch (p_event->type) {
	case NRFX_UARTE_EVT_RX_DONE:
		ring_buf_put(&rbuf, p_event->data.rx.p_buffer, p_event->data.rx.length);
		sh_handler(SHELL_TRANSPORT_EVT_RX_RDY, sh_context);
		atomic_set_bit(&state_atom, STATE_ATOM_RX_DONE_BIT);
		break;

	case NRFX_UARTE_EVT_RX_BUF_REQUEST:
		dbuf_idx = dbuf_idx ? 0 : 1;
		(void)nrfx_uarte_rx_buffer_set(&uarte_inst,
					       dbuf[dbuf_idx],
					       sizeof(dbuf[dbuf_idx]));
		break;

	case NRFX_UARTE_EVT_RX_BYTE:
		atomic_set_bit(&state_atom, STATE_ATOM_RX_BYTE_BIT);
		break;

	default:
		break;
	}
}

ISR_DIRECT_DECLARE(shell_backend_bm_uarte_direct_isr)
{
	nrfx_uarte_irq_handler(&uarte_inst);
	return 0;
}

static int backend_init(const struct shell_transport *transport,
			const void *config,
			shell_transport_handler_t evt_handler,
			void *context)
{
	int ret;

	sh_handler = evt_handler;
	sh_context = context;

	/** We need to connect the IRQ ourselves. */
	IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(BOARD_SHELL_UARTE_INST),
			   CONFIG_SHELL_BACKEND_BM_UARTE_IRQ_PRIO,
			   shell_backend_bm_uarte_direct_isr, 0);

	irq_enable(NRFX_IRQ_NUMBER_GET(BOARD_SHELL_UARTE_INST));

	dbuf_idx = 1;
	ring_buf_init(&rbuf, sizeof(rbuf_data), rbuf_data);
	atomic_set(&state_atom, 0);

	ret = nrfx_uarte_init(&uarte_inst, &uarte_config, uarte_event_handler);
	if (ret) {
		return -ENODEV;
	}

	ret = nrfx_uarte_rx_enable(&uarte_inst, NRFX_UARTE_RX_ENABLE_CONT);
	if (ret) {
		return -ENODEV;
	}

	nrfx_uarte_rxdrdy_enable(&uarte_inst);
	return 0;
}

static int backend_uninit(const struct shell_transport *transport)
{
	int ret;

	nrfx_uarte_rxdrdy_disable(&uarte_inst);

	ret = nrfx_uarte_rx_abort(&uarte_inst, true, true);
	if (ret) {
		return -EIO;
	}

	nrfx_uarte_uninit(&uarte_inst);
	return 0;
}

static int backend_write(const struct shell_transport *transport,
			 const void *data,
			 size_t length,
			 size_t *cnt)
{
	*cnt = length;

	return nrfx_uarte_tx(&uarte_inst, data, length, NRFX_UARTE_TX_BLOCKING);
}

static int backend_read(const struct shell_transport *transport,
			void *data,
			size_t length,
			size_t *cnt)
{
	/* We only need to stop rx if there has been RX activity since last read */
	if (atomic_test_and_clear_bit(&state_atom, STATE_ATOM_RX_BYTE_BIT)) {
		atomic_clear_bit(&state_atom, STATE_ATOM_RX_DONE_BIT);
		(void)nrfx_uarte_rx_abort(&uarte_inst, false, false);
		WAIT_FOR(
			(atomic_test_bit(&state_atom, STATE_ATOM_RX_DONE_BIT)),
			RX_ABORT_TIMEOUT_US,
			NULL
		);
	}

	/* The RX ring buffer is shared between IRQ and thread so disable IRQ while reading */
	irq_disable(NRFX_IRQ_NUMBER_GET(BOARD_SHELL_UARTE_INST));
	*cnt = ring_buf_get(&rbuf, data, length);
	irq_enable(NRFX_IRQ_NUMBER_GET(BOARD_SHELL_UARTE_INST));
	return 0;
}

const struct shell_transport_api bm_shell_uarte_transport_api = {
	.init = backend_init,
	.uninit = backend_uninit,
	.write = backend_write,
	.read = backend_read,
};

const struct shell_transport bm_shell_uarte_transport = {
	.api = &bm_shell_uarte_transport_api,
};

SHELL_DEFINE(bm_shell_uarte_shell,
	     "bm-uarte:~$ ",
	     &bm_shell_uarte_transport,
	     0,
	     0,
	     SHELL_FLAG_OLF_CRLF);

const struct shell *shell_backend_bm_uarte_get_ptr(void)
{
	return &bm_shell_uarte_shell;
}

bool shell_backend_bm_uarte_rx_ready(void)
{
	return atomic_test_bit(&state_atom, STATE_ATOM_RX_BYTE_BIT);
}
