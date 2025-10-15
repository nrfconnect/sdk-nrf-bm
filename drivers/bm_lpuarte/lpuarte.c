/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm/drivers/bm_lpuarte.h>
#include <nrfx_gpiote.h>
#include <nrfx_uarte.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lpuarte, CONFIG_BM_SW_LPUARTE_LOG_LEVEL);

static const nrfx_gpiote_t gpiote20_instance = NRFX_GPIOTE_INSTANCE(20);
static const nrfx_gpiote_t gpiote30_instance = NRFX_GPIOTE_INSTANCE(30);

static void req_pin_handler(nrfx_gpiote_pin_t pin,
			    nrfx_gpiote_trigger_t trigger,
			    void *context);

static void rdy_pin_handler(nrfx_gpiote_pin_t pin,
			    nrfx_gpiote_trigger_t trigger,
			    void *context);

static inline const nrfx_gpiote_t *gpiote_get(uint32_t pin)
{
	switch (NRF_PIN_NUMBER_TO_PORT(pin)) {
	case 0:
		return &gpiote30_instance;
	case 1:
		return &gpiote20_instance;
	default:
		return NULL;
	}
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

	nrfx_gpiote_trigger_disable(gpiote_get(lpu->req_pin), lpu->req_pin);
}

/* Reconfigure pin to input with pull up and high->low state detection.
 * Receiver will pull pin down for a moment when ready, which means that the transfer can start.
 */
static void req_pin_arm(struct bm_lpuarte *lpu)
{
	const nrf_gpio_pin_pull_t pull = NRF_GPIO_PIN_PULLUP;

	/* Add pull up before reconfiguring to input. */
	nrf_gpio_reconfigure(lpu->req_pin, NULL, NULL, &pull, NULL, NULL);

	nrfx_gpiote_trigger_enable(gpiote_get(lpu->req_pin), lpu->req_pin, true);
}

static nrfx_err_t req_pin_init(struct bm_lpuarte *lpu, nrfx_gpiote_pin_t pin)
{
	uint8_t ch;
	nrfx_err_t err;
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

	err = nrfx_gpiote_channel_alloc(gpiote_get(pin), &ch);
	if (err != NRFX_SUCCESS) {
		return err;
	}

	err = nrfx_gpiote_input_configure(gpiote_get(pin), pin, &input_config);
	if (err != NRFX_SUCCESS) {
		return err;
	}

	lpu->req_pin = pin;

	/* Set the request pin in idle state to indicate to the receiver that there is no pending
	 * transfer.
	 */
	req_pin_idle(lpu);

	return NRFX_SUCCESS;
}

static void req_pin_uninit(struct bm_lpuarte *lpu, nrfx_gpiote_pin_t pin)
{
	(void)nrfx_gpiote_pin_uninit(gpiote_get(pin), pin);
}

static void rdy_pin_suspend(struct bm_lpuarte *lpu)
{
	nrfx_gpiote_trigger_disable(gpiote_get(lpu->rdy_pin), lpu->rdy_pin);
}

static nrfx_err_t rdy_pin_init(struct bm_lpuarte *lpu, nrfx_gpiote_pin_t pin)
{
	nrfx_err_t err;
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

	err = nrfx_gpiote_channel_alloc(gpiote_get(pin), &lpu->rdy_ch);
	if (err != NRFX_SUCCESS) {
		return err;
	}

	err = nrfx_gpiote_input_configure(gpiote_get(pin), pin, &input_config);
	if (err != NRFX_SUCCESS) {
		return err;
	}

	lpu->rdy_pin = pin;
	nrf_gpio_pin_clear(pin);

	return NRFX_SUCCESS;
}

static void rdy_pin_uninit(struct bm_lpuarte *lpu, nrfx_gpiote_pin_t pin)
{
	(void)nrfx_gpiote_pin_uninit(gpiote_get(pin), pin);
}

/* Pin activated to detect high state (using SENSE). */
static void rdy_pin_idle(struct bm_lpuarte *lpu)
{
	nrfx_err_t err;
	nrfx_gpiote_trigger_config_t trigger_config = {
		.trigger = NRFX_GPIOTE_TRIGGER_HIGH
	};
	nrfx_gpiote_input_pin_config_t input_config = {
		.p_pull_config = NULL,
		.p_trigger_config = &trigger_config,
		.p_handler_config = NULL
	};
	const nrfx_gpiote_t *gpiote = gpiote_get(lpu->rdy_pin);

	err = nrfx_gpiote_input_configure(gpiote, lpu->rdy_pin, &input_config);
	__ASSERT(err == NRFX_SUCCESS, "Unexpected nrfx_err %#x", err);

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
	nrfx_err_t err;
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
	const nrfx_gpiote_t *gpiote = gpiote_get(lpu->rdy_pin);
	bool ret;

	/* Drive low for a moment */
	nrf_gpio_reconfigure(lpu->rdy_pin, &dir_out, NULL, NULL, NULL, NULL);

	err = nrfx_gpiote_input_configure(gpiote, lpu->rdy_pin, &input_config);
	__ASSERT(err == NRFX_SUCCESS, "Unexpected nrfx_err %#x", err);

	nrfx_gpiote_trigger_enable(gpiote, lpu->rdy_pin, true);

	NRFX_CRITICAL_SECTION_ENTER();

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
	NRFX_CRITICAL_SECTION_EXIT();

	return ret;
}

/* Set response pin to idle and disable RX. */
static void deactivate_rx(struct bm_lpuarte *lpu)
{
	nrfx_err_t err;

	/* abort rx */
	LOG_DBG("RX: Deactivate");
	lpu->rx_state = RX_TO_IDLE;
	err = nrfx_uarte_rx_abort(&lpu->uarte_inst, true, false);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("RX: Failed to disable, nrfx_err %#x", err);
	}

	rdy_pin_idle(lpu);
}

/* Enable RX and inform the transmitter that it is ready to receive by
 * clearing the RDY pin (configure to output, low level). RDY pin is then reconfigured to input with
 * pullup and high to low detection to detect end of transfer.
 */
static void activate_rx(struct bm_lpuarte *lpu)
{
	nrfx_err_t err;

	LOG_DBG("Activating uarte RX");

	err = nrfx_uarte_rx_enable(&lpu->uarte_inst,
				   NRFX_UARTE_RX_ENABLE_CONT | NRFX_UARTE_RX_ENABLE_STOP_ON_END);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("lpuarte rx enable failed, nrfx_err %#x", err);
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
	nrfx_err_t err;
	struct bm_lpuarte *lpu = context;

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

	NRFX_CRITICAL_SECTION_ENTER();
	lpu->tx_active = true;
	buf = lpu->tx_buf;
	len = lpu->tx_len;
	NRFX_CRITICAL_SECTION_EXIT();
	err = nrfx_uarte_tx(&lpu->uarte_inst, buf, len, 0);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("TX: Not started, nrfx_err %#x", err);
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
	nrfx_err_t err;
	struct bm_lpuarte *lpu = context;
	const uint8_t *buf = lpu->tx_buf;

	LOG_WRN("TX abort timeout");
	if (lpu->tx_active) {
		err = nrfx_uarte_tx_abort(&lpu->uarte_inst, true);
		if (err == NRFX_ERROR_INVALID_STATE) {
			LOG_DBG("No active transfer. Already finished?");
		} else if (err != NRFX_SUCCESS) {
			__ASSERT(0, "Unexpected tx_abort, nrfx_err %#x", err);
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

static void nrfx_uarte_evt_handler(nrfx_uarte_event_t const *event, void *ctx)
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

nrfx_err_t bm_lpuarte_init(struct bm_lpuarte *lpu,
			   struct bm_lpuarte_config *lpu_cfg,
			   nrfx_uarte_event_handler_t  event_handler)
{
	nrfx_err_t err;

	/* We use the uarte context for storing the pointer to the lpu instance */
	lpu_cfg->uarte_cfg.p_context = lpu;

	memcpy(&lpu->uarte_inst, &lpu_cfg->uarte_inst, sizeof(nrfx_uarte_t));
	lpu->req_pin = lpu_cfg->req_pin;
	lpu->rdy_pin = lpu_cfg->rdy_pin;
	lpu->rx_state = RX_OFF;

	lpu->callback = event_handler;

	if (!nrfx_gpiote_init_check(&gpiote20_instance)) {
		err = nrfx_gpiote_init(&gpiote20_instance, 0);
		if (err != NRFX_SUCCESS) {
			LOG_ERR("Failed to initialize gpiote20, nrfx_err: %#x", err);
			return NRFX_ERROR_INVALID_STATE;
		}
	}

	if (!nrfx_gpiote_init_check(&gpiote30_instance)) {
		err = nrfx_gpiote_init(&gpiote30_instance, 0);
		if (err != NRFX_SUCCESS) {
			LOG_ERR("Failed to initialize gpiote30, nrfx_err: %#x", err);
			return NRFX_ERROR_INVALID_STATE;
		}
	}

	err = req_pin_init(lpu, lpu_cfg->req_pin);
	if (err < 0) {
		LOG_ERR("req pin init failed, nrfx_err %#x", err);
		return err;
	}

	err = rdy_pin_init(lpu, lpu_cfg->rdy_pin);
	if (err < 0) {
		LOG_ERR("rdy pin init failed, nrfx_err %#x", err);
		return err;
	}

	bm_timer_init(&lpu->tx_timer, BM_TIMER_MODE_SINGLE_SHOT, tx_timeout);

	err = nrfx_uarte_init(&lpu->uarte_inst, &lpu_cfg->uarte_cfg, nrfx_uarte_evt_handler);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("Failed to initialize UARTE, nrfx_err %#x", err);
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

	nrfx_uarte_uninit(&lpu->uarte_inst);
	req_pin_uninit(lpu, lpu->req_pin);
	rdy_pin_uninit(lpu, lpu->rdy_pin);

	/* Don't uninitialize gpiote instances as they can be used by other drivers and libraries */
}

nrfx_err_t bm_lpuarte_tx(struct bm_lpuarte *lpu, uint8_t const *data, size_t len,
			 int32_t timeout)
{
	if (!lpu || !data) {
		return NRFX_ERROR_NULL;
	}
	if (!len) {
		return NRFX_ERROR_INVALID_LENGTH;
	}
	if (!atomic_ptr_cas((atomic_ptr_t *)&lpu->tx_buf, NULL, (void *)data)) {
		return NRFX_ERROR_BUSY;
	}

	lpu->tx_len = len;
	bm_timer_start(&lpu->tx_timer, BM_TIMER_MS_TO_TICKS(timeout), lpu);

	/* Enable interrupt on pin going low. */
	req_pin_arm(lpu);

	return NRFX_SUCCESS;
}

bool bm_lpuarte_tx_in_progress(struct bm_lpuarte *lpu)
{
	return (lpu->tx_buf != NULL);
}

nrfx_err_t bm_lpuarte_tx_abort(struct bm_lpuarte *lpu, bool sync)
{
	nrfx_err_t err;
	const uint8_t *buf = lpu->tx_buf;

	if (!lpu) {
		return NRFX_ERROR_NULL;
	}
	if (!lpu->tx_buf) {
		return NRFX_ERROR_INVALID_STATE;
	}

	bm_timer_stop(&lpu->tx_timer);
	NRFX_CRITICAL_SECTION_ENTER();
	tx_complete(lpu);
	NRFX_CRITICAL_SECTION_EXIT();

	err = nrfx_uarte_tx_abort(&lpu->uarte_inst, sync);
	if (err == NRFX_ERROR_INVALID_STATE && !sync) {
		/* If abort is before TX is started we report ABORT from here. */
		err = NRFX_SUCCESS;

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

nrfx_err_t bm_lpuarte_rx_enable(struct bm_lpuarte *lpu)
{
	if (!atomic_cas((atomic_t *)&lpu->rx_state, (atomic_val_t)RX_OFF, (atomic_val_t)RX_IDLE)) {
		return NRFX_ERROR_BUSY;
	}

	rdy_pin_idle(lpu);

	return NRFX_SUCCESS;
}

nrfx_err_t bm_lpuarte_rx_buffer_set(struct bm_lpuarte *lpu,
				    uint8_t *data,
				    size_t length)
{
	return nrfx_uarte_rx_buffer_set(&lpu->uarte_inst, data, length);
}

nrfx_err_t bm_lpuarte_rx_abort(struct bm_lpuarte *lpu, bool sync)
{
	nrfx_err_t err;

	if (lpu->rx_state == RX_OFF) {
		return NRFX_ERROR_INVALID_STATE;
	}

	lpu->rx_state = RX_TO_OFF;
	err = nrfx_uarte_rx_abort(&lpu->uarte_inst, true, sync);
	if (err == NRFX_ERROR_INVALID_STATE || sync) {
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
