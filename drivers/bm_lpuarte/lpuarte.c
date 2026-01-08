/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm/bm_timer.h>
#include <bm/drivers/bm_lpuarte.h>
#include <nrfx_gpiote.h>
#include <nrfx_uarte.h>
#include <zephyr/irq.h>
#if defined(CONFIG_SOFTDEVICE)
#include <nrf_soc.h>
#include <nrf_sdm.h>
#endif /* CONFIG_SOFTDEVICE */
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lpuarte, CONFIG_BM_SW_LPUARTE_LOG_LEVEL);

static void req_pin_handler(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t trigger, void *context);
static void rdy_pin_handler(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t trigger, void *context);

static nrfx_gpiote_t *gpiote_get(struct bm_lpuarte *lpu, uint32_t pin)
{
	const uint32_t port = NRF_PIN_NUMBER_TO_PORT(pin);

	if (port >= lpu->gpiote_inst_num) {
		return NULL;
	}

	return &lpu->gpiote_inst[port];
}

/* Called when UARTE transfer is finished to indicate to the receiver that it can be closed. */
static void req_pin_idle(struct bm_lpuarte *lpu)
{
	nrf_gpio_cfg(lpu->req_pin,
		     NRF_GPIO_PIN_DIR_OUTPUT,
		     NRF_GPIO_PIN_INPUT_DISCONNECT,
		     NRF_GPIO_PIN_NOPULL,
		     NRF_GPIO_PIN_S0S1,
		     NRF_GPIO_PIN_NOSENSE);
}

static void pend_req_pin_idle(struct bm_lpuarte *lpu)
{
	/* Wait until the pin is high */
	while (!nrfx_gpiote_in_is_set(lpu->req_pin)) {
	}
}

/* Force pin assertion. Pin is kept high during uarte transfer. */
static void req_pin_set(struct bm_lpuarte *lpu)
{
	const nrf_gpio_pin_dir_t dir = NRF_GPIO_PIN_DIR_INPUT;
	const nrf_gpio_pin_input_t input = NRF_GPIO_PIN_INPUT_CONNECT;

	nrf_gpio_reconfigure(lpu->req_pin, &dir, &input, NULL, NULL, NULL);

	nrfx_gpiote_trigger_disable(gpiote_get(lpu, lpu->req_pin), lpu->req_pin);
}

/* Reconfigure pin to input with pull up and high->low state detection.
 * Receiver will pull pin down for a moment when ready, which means that the transfer can start.
 */
static void req_pin_arm(struct bm_lpuarte *lpu)
{
	const nrf_gpio_pin_pull_t pull = NRF_GPIO_PIN_PULLUP;

	/* Add pull up before reconfiguring to input. */
	nrf_gpio_reconfigure(lpu->req_pin, NULL, NULL, &pull, NULL, NULL);

	nrfx_gpiote_trigger_enable(gpiote_get(lpu, lpu->req_pin), lpu->req_pin, true);
}

static int req_pin_init(struct bm_lpuarte *lpu, nrfx_gpiote_pin_t pin)
{
	uint8_t ch;
	int err;
	nrf_gpio_pin_pull_t pull_config = NRF_GPIO_PIN_PULLDOWN;
	nrfx_gpiote_trigger_config_t trigger_config = {
		.trigger = NRFX_GPIOTE_TRIGGER_HITOLO,
		.p_in_channel = &ch,
	};
	nrfx_gpiote_handler_config_t handler_config = {
		.handler = req_pin_handler,
		.p_context = lpu,
	};
	nrfx_gpiote_input_pin_config_t input_config = {
		.p_pull_config = &pull_config,
		.p_trigger_config = &trigger_config,
		.p_handler_config = &handler_config
	};

	err = nrfx_gpiote_channel_alloc(gpiote_get(lpu, pin), &ch);
	if (err) {
		return err;
	}

	err = nrfx_gpiote_input_configure(gpiote_get(lpu, pin), pin, &input_config);
	if (err) {
		return err;
	}

	lpu->req_pin = pin;

	/* Set the request pin in idle state to indicate to the receiver that there is no pending
	 * transfer.
	 */
	req_pin_idle(lpu);

	return 0;
}

static void req_pin_uninit(struct bm_lpuarte *lpu, nrfx_gpiote_pin_t pin)
{
	(void)nrfx_gpiote_pin_uninit(gpiote_get(lpu, pin), pin);
}

static void rdy_pin_suspend(struct bm_lpuarte *lpu)
{
	nrfx_gpiote_trigger_disable(gpiote_get(lpu, lpu->rdy_pin), lpu->rdy_pin);
}

static int rdy_pin_init(struct bm_lpuarte *lpu, nrfx_gpiote_pin_t pin)
{
	int err;
	nrf_gpio_pin_pull_t pull_config = NRF_GPIO_PIN_NOPULL;
	nrfx_gpiote_handler_config_t handler_config = {
		.handler = rdy_pin_handler,
		.p_context = lpu
	};
	nrfx_gpiote_input_pin_config_t input_config = {
		.p_pull_config = &pull_config,
		.p_trigger_config = NULL,
		.p_handler_config = &handler_config
	};

	err = nrfx_gpiote_channel_alloc(gpiote_get(lpu, pin), &lpu->rdy_ch);
	if (err) {
		return err;
	}

	err = nrfx_gpiote_input_configure(gpiote_get(lpu, pin), pin, &input_config);
	if (err) {
		return err;
	}

	lpu->rdy_pin = pin;
	nrf_gpio_pin_clear(pin);

	return 0;
}

static void rdy_pin_uninit(struct bm_lpuarte *lpu, nrfx_gpiote_pin_t pin)
{
	(void)nrfx_gpiote_pin_uninit(gpiote_get(lpu, pin), pin);
}

/* Pin activated to detect high state (using SENSE). */
static void rdy_pin_idle(struct bm_lpuarte *lpu)
{
	int err;
	nrfx_gpiote_trigger_config_t trigger_config = {
		.trigger = NRFX_GPIOTE_TRIGGER_HIGH
	};
	nrfx_gpiote_input_pin_config_t input_config = {
		.p_pull_config = NULL,
		.p_trigger_config = &trigger_config,
		.p_handler_config = NULL
	};
	nrfx_gpiote_t *gpiote = gpiote_get(lpu, lpu->rdy_pin);

	err = nrfx_gpiote_input_configure(gpiote, lpu->rdy_pin, &input_config);
	__ASSERT(err == 0, "Unexpected err %d", err);

	nrfx_gpiote_trigger_enable(gpiote, lpu->rdy_pin, true);
}

/* Indicate to the transmitter that receiver is ready by pulling pin down for
 * a moment, then reconfiguring it back to input with low state detection to detect when
 * transmission is complete.
 *
 * Function checks if transmitter has request pin in the expected state and if not
 * false is returned.
 */
static bool rdy_pin_blink(struct bm_lpuarte *lpu)
{
	int err;
	nrfx_gpiote_trigger_config_t trigger_config = {
		.trigger = NRFX_GPIOTE_TRIGGER_HITOLO,
		.p_in_channel = &lpu->rdy_ch
	};
	nrfx_gpiote_input_pin_config_t input_config = {
		.p_pull_config = NULL,
		.p_trigger_config = &trigger_config,
		.p_handler_config = NULL
	};
	const nrf_gpio_pin_dir_t dir_in = NRF_GPIO_PIN_DIR_INPUT;
	const nrf_gpio_pin_dir_t dir_out = NRF_GPIO_PIN_DIR_OUTPUT;
	nrfx_gpiote_t *gpiote = gpiote_get(lpu, lpu->rdy_pin);
	unsigned int key;
	bool ret;

	/* Drive low for a moment */
	nrf_gpio_reconfigure(lpu->rdy_pin, &dir_out, NULL, NULL, NULL, NULL);

	err = nrfx_gpiote_input_configure(gpiote, lpu->rdy_pin, &input_config);
	__ASSERT(err == 0, "Unexpected err %d", err);

	nrfx_gpiote_trigger_enable(gpiote, lpu->rdy_pin, true);

	key = irq_lock();

	nrf_gpiote_event_t event = nrf_gpiote_in_event_get(lpu->rdy_ch);

	nrf_gpio_reconfigure(lpu->rdy_pin, &dir_in, NULL, NULL, NULL, NULL);

	/* Wait a bit. After switching to input the transmitter pin pullup should drive
	 * this pin high.
	 */
	k_busy_wait(1);
	if (nrf_gpio_pin_read(lpu->rdy_pin) == 0 &&
	    !nrf_gpiote_event_check(gpiote->p_reg, event)) {
		/* Suspicious pin state (low). It might be that context was preempted
		 * for long enough and transfer ended (in that case event will be set)
		 * or the transmitter is working abnormally or pin is just floating.
		 */
		ret = false;
		LOG_WRN("req pin low when expected high");
	} else {
		ret = true;
	}
	irq_unlock(key);

	return ret;
}

static void hfclk_enable(void)
{
#if defined(CONFIG_BM_SW_LPUARTE_HFXO)
	uint32_t nrf_err;
	uint8_t sd_enabled;
	uint32_t hfclk_running;

	(void)sd_softdevice_is_enabled(&sd_enabled);

	if (sd_enabled) {
		/* We need to start HFCLK through SoftDevice API. As the code
		 * executes from an IRQ it must be ensured that the GPIOTE IRQ priority
		 * is acceptable to call SoftDevice API.
		 */
		nrf_err = sd_clock_hfclk_request();
		if (nrf_err) {
			LOG_ERR("Failed to request HFCLK, nrf_error %#x", nrf_err);
			return;
		}

		do {
			sd_clock_hfclk_is_running(&hfclk_running);
		} while (!hfclk_running);

	} else {
		LOG_WRN("SoftDevice not running, HFCLK not enabled");
	}
#endif
}

static void hfclk_disable(void)
{
#if defined(CONFIG_BM_SW_LPUARTE_HFXO)
	uint8_t sd_enabled;

	(void)sd_softdevice_is_enabled(&sd_enabled);

	if (sd_enabled) {
		(void)sd_clock_hfclk_release();
	}
#endif
}


/* Set response pin to idle and disable RX. */
static void deactivate_rx(struct bm_lpuarte *lpu)
{
	int err;

	hfclk_disable();

	/* abort rx */
	LOG_DBG("RX: Deactivate");
	lpu->rx_state = RX_TO_IDLE;
	err = nrfx_uarte_rx_abort(lpu->uarte_inst, true, false);
	if (err) {
		LOG_ERR("RX: Failed to disable, err %d", err);
	}

	rdy_pin_idle(lpu);
}

/* Enable RX and inform the transmitter that it is ready to receive by
 * clearing the RDY pin (configure to output, low level). RDY pin is then reconfigured to input with
 * pullup and high to low detection to detect end of transfer.
 */
static void activate_rx(struct bm_lpuarte *lpu)
{
	int err;

	LOG_DBG("Activating uarte RX");

	err = nrfx_uarte_rx_enable(lpu->uarte_inst,
				   NRFX_UARTE_RX_ENABLE_CONT | NRFX_UARTE_RX_ENABLE_STOP_ON_END);
	if (err) {
		LOG_ERR("lpuarte rx enable failed, err %d", err);
	}

	lpu->rx_state = RX_ACTIVE;

	/* Ready. Confirm by toggling the pin. */
	if (!rdy_pin_blink(lpu)) {
		/* If tranmitter behaves abnormally deactivate RX. */
		rdy_pin_suspend(lpu);
		deactivate_rx(lpu);
		return;
	}

	LOG_DBG("RX activated");
}

static void start_rx_activation(struct bm_lpuarte *lpu)
{
	hfclk_enable();

	lpu->rx_state = RX_PREPARE;
	activate_rx(lpu);
}

static void tx_complete(struct bm_lpuarte *lpu)
{
	LOG_DBG("TX completed, pin idle");
	if (lpu->tx_active) {
		pend_req_pin_idle(lpu);
	} else {
		req_pin_set(lpu);
	}

	hfclk_disable();

	req_pin_idle(lpu);
	lpu->tx_buf = NULL;
	lpu->tx_active = false;
}

/* Called when the REQ pin transition to low state is detected, which indicates
 * that the receiver is ready for the transfer.
 */
static void req_pin_handler(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t trigger, void *context)
{
	ARG_UNUSED(trigger);
	ARG_UNUSED(pin);
	const uint8_t *buf;
	size_t len;
	int err;
	struct bm_lpuarte *lpu = context;
	unsigned int key;

	LOG_DBG("req_pin_evt");

	if (lpu->tx_buf == NULL) {
		LOG_WRN("TX: request confirmed but no data to send");
		tx_complete(lpu);
		/* aborted */
		return;
	}

	LOG_DBG("TX: Confirmed, starting.");

	req_pin_set(lpu);
	bm_timer_stop(&lpu->tx_timer);

	key = irq_lock();
	lpu->tx_active = true;
	buf = lpu->tx_buf;
	len = lpu->tx_len;
	irq_unlock(key);

	err = nrfx_uarte_tx(lpu->uarte_inst, buf, len, 0);
	if (err) {
		LOG_ERR("TX: Not started, err %d", err);
		tx_complete(lpu);

		const nrfx_uarte_event_t tx_done_aborted_evt = {
			.type = NRFX_UARTE_EVT_TX_DONE,
			.data.tx = {
				.p_buffer = buf,
				.length = 0,
				.flags = NRFX_UARTE_TX_DONE_ABORTED,
			},
		};

		lpu->callback(&tx_done_aborted_evt, lpu);
	}
}

/* RDY pin handler is called in two cases:
 * - high state detection. Receiver is idle and new transfer request is received.
 * - high->low state. Receiver is active and receiving a packet. Transmitter indicates
 *   end of the packet.
 */
static void rdy_pin_handler(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t trigger, void *context)
{
	struct bm_lpuarte *lpu = context;

	rdy_pin_suspend(lpu);
	if (trigger == NRFX_GPIOTE_TRIGGER_HIGH) {
		__ASSERT_NO_MSG(lpu->rx_state != RX_ACTIVE);

		LOG_DBG("RX: Request detected. RX state %d", lpu->rx_state);
		if (lpu->rx_state == RX_IDLE || lpu->rx_state == RX_TO_IDLE) {
			start_rx_activation(lpu);
		}
	} else { /* HITOLO */
		if (lpu->rx_state != RX_ACTIVE) {
			LOG_WRN("RX: End detected at unexpected state (%d).", lpu->rx_state);
			lpu->rx_state = RX_IDLE;
			rdy_pin_idle(lpu);
			return;
		}

		LOG_DBG("RX: End detected.");
		deactivate_rx(lpu);
	}
}

static void tx_timeout(void *context)
{
	int err;
	struct bm_lpuarte *lpu = context;
	const uint8_t *buf = lpu->tx_buf;

	LOG_WRN("TX abort timeout");
	if (lpu->tx_active) {
		err = nrfx_uarte_tx_abort(lpu->uarte_inst, true);
		if (err == -EINPROGRESS) {
			LOG_DBG("No active transfer. Already finished?");
		} else if (err) {
			__ASSERT(false, "Unexpected tx_abort, err %d", err);
		}
		return;
	}

	tx_complete(lpu);

	const nrfx_uarte_event_t tx_done_aborted_evt = {
		.type = NRFX_UARTE_EVT_TX_DONE,
		.data.tx = {
			.p_buffer = buf,
			.length = 0,
			.flags = NRFX_UARTE_TX_DONE_ABORTED,
		},
	};

	lpu->callback(&tx_done_aborted_evt, lpu);
}

static void nrfx_uarte_evt_handler(const nrfx_uarte_event_t *event, void *ctx)
{
	struct bm_lpuarte *lpu = ctx;

	switch (event->type) {
	case NRFX_UARTE_EVT_TX_DONE:
		LOG_DBG("TX complete event, %d, %x", event->data.tx.length, event->data.tx.flags);
		tx_complete(ctx);
		break;
	case NRFX_UARTE_EVT_RX_DONE:
		if (lpu->rx_state == RX_TO_OFF) {
			lpu->rx_state = RX_OFF;
			rdy_pin_idle(lpu);
		}
		break;
	case NRFX_UARTE_EVT_RX_DISABLED:
		/* UARTE receiver is disabled, we go to rx idle to allow for new RX initiation. */
		lpu->rx_state = RX_IDLE;

		rdy_pin_idle(lpu);
		break;
	case NRFX_UARTE_EVT_ERROR:
		LOG_ERR("UARTE error event, %#x", event->data.error.error_mask);
		break;
	default:
		break;
	}

	lpu->callback(event, ctx);
}

int bm_lpuarte_init(struct bm_lpuarte *lpu, struct bm_lpuarte_config *lpu_cfg,
		    nrfx_uarte_event_handler_t  event_handler)
{
	int err;

	/* We use the uarte context for storing the pointer to the lpu instance */
	lpu_cfg->uarte_cfg.p_context = lpu;

	lpu->uarte_inst = lpu_cfg->uarte_inst;
	lpu->gpiote_inst = lpu_cfg->gpiote_inst;
	lpu->gpiote_inst_num = lpu_cfg->gpiote_inst_num;
	lpu->req_pin = lpu_cfg->req_pin;
	lpu->rdy_pin = lpu_cfg->rdy_pin;
	lpu->rx_state = RX_OFF;

	lpu->callback = event_handler;

	for (uint8_t i = 0; i < lpu->gpiote_inst_num; i++) {
		nrfx_gpiote_t *inst = &lpu->gpiote_inst[i];

		if (inst == NULL || nrfx_gpiote_init_check(inst)) {
			continue;
		}

		err = nrfx_gpiote_init(inst, 0);
		if (err) {
			LOG_ERR("Failed to initialize gpiote %#x, err %d", (uint32_t)inst, err);
			return err;
		}
	}

	err = req_pin_init(lpu, lpu_cfg->req_pin);
	if (err) {
		LOG_ERR("req pin init failed, err %d", err);
		return err;
	}

	err = rdy_pin_init(lpu, lpu_cfg->rdy_pin);
	if (err) {
		LOG_ERR("rdy pin init failed, err %d", err);
		return err;
	}

	(void)bm_timer_init(&lpu->tx_timer, BM_TIMER_MODE_SINGLE_SHOT, tx_timeout);

	err = nrfx_uarte_init(lpu->uarte_inst, &lpu_cfg->uarte_cfg, nrfx_uarte_evt_handler);
	if (err) {
		LOG_ERR("Failed to initialize UARTE, err %d", err);
		return err;
	}

	return err;
}

void bm_lpuarte_uninit(struct bm_lpuarte *lpu)
{
	if (lpu->rx_state != RX_OFF) {
		(void)bm_lpuarte_rx_abort(lpu, true);
	}
	if (lpu->tx_buf) {
		(void)bm_lpuarte_tx_abort(lpu, true);
	}

	nrfx_uarte_uninit(lpu->uarte_inst);
	req_pin_uninit(lpu, lpu->req_pin);
	rdy_pin_uninit(lpu, lpu->rdy_pin);

	/* Don't uninitialize gpiote instances as they can be used by other drivers and libraries */
}

int bm_lpuarte_tx(struct bm_lpuarte *lpu, const uint8_t *data, size_t len, int32_t timeout)
{
	if (!lpu || !data) {
		return -EFAULT;
	}
	if (!len) {
		return -EINVAL;
	}
	if (!atomic_ptr_cas((atomic_ptr_t *)&lpu->tx_buf, NULL, (void *)data)) {
		return -EBUSY;
	}

	hfclk_enable();

	lpu->tx_len = len;
	bm_timer_start(&lpu->tx_timer, BM_TIMER_MS_TO_TICKS(timeout), lpu);

	/* Enable interrupt on pin going low. */
	req_pin_arm(lpu);

	return 0;
}

bool bm_lpuarte_tx_in_progress(struct bm_lpuarte *lpu)
{
	return (lpu->tx_buf != NULL);
}

int bm_lpuarte_tx_abort(struct bm_lpuarte *lpu, bool sync)
{
	int err;
	const uint8_t *buf = lpu->tx_buf;
	unsigned int key;

	if (!lpu) {
		return -EFAULT;
	}
	if (!lpu->tx_buf) {
		return -EINPROGRESS;
	}

	bm_timer_stop(&lpu->tx_timer);
	key = irq_lock();
	tx_complete(lpu);
	irq_unlock(key);

	err = nrfx_uarte_tx_abort(lpu->uarte_inst, sync);
	if (err == -EINPROGRESS && !sync) {
		/* If abort is before TX is started we report ABORT from here. */
		err = 0;

		const nrfx_uarte_event_t tx_done_aborted_evt = {
			.type = NRFX_UARTE_EVT_TX_DONE,
			.data.tx = {
				.p_buffer = buf,
				.length = 0,
				.flags = NRFX_UARTE_TX_DONE_ABORTED,
			},
		};

		lpu->callback(&tx_done_aborted_evt, lpu);
	}

	return err;
}

int bm_lpuarte_rx_enable(struct bm_lpuarte *lpu)
{
	if (!atomic_cas((atomic_t *)&lpu->rx_state, (atomic_val_t)RX_OFF, (atomic_val_t)RX_IDLE)) {
		return -EBUSY;
	}

	rdy_pin_idle(lpu);

	return 0;
}

int bm_lpuarte_rx_buffer_set(struct bm_lpuarte *lpu, uint8_t *data, size_t length)
{
	return nrfx_uarte_rx_buffer_set(lpu->uarte_inst, data, length);
}

int bm_lpuarte_rx_abort(struct bm_lpuarte *lpu, bool sync)
{
	int err;

	if (lpu->rx_state == RX_OFF) {
		return -EINPROGRESS;
	}

	lpu->rx_state = RX_TO_OFF;
	err = nrfx_uarte_rx_abort(lpu->uarte_inst, true, sync);
	if (err == -EINPROGRESS || sync) {
		lpu->rx_state = RX_OFF;
		rdy_pin_idle(lpu);

		if (!sync) {
			/* RX not started, report empty RX done ourselves without buffer as none is
			 * yet provided.
			 */
			const nrfx_uarte_event_t rx_done_aborted_evt = {
				.type = NRFX_UARTE_EVT_RX_DONE,
				.data.rx = {
					.p_buffer = NULL,
					.length = 0,
				},
			};

			lpu->callback(&rx_done_aborted_evt, lpu);
		}
	}

	return err;
}
