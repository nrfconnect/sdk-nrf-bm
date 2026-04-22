/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <bm/bm_buttons.h>
#include <bm/bm_irq.h>
#include <hal/nrf_gpio.h>
#include <board-config.h>
#include <nrfx_spim.h>
#include <nrfx_spis.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_SPI_LOG_LEVEL);

static nrfx_spim_t spim_inst = NRFX_SPIM_INSTANCE(BOARD_APP_SPIM_INST);
static nrfx_spis_t spis_inst = NRFX_SPIS_INSTANCE(BOARD_APP_SPIS_INST);

static uint8_t tx_buf[] = CONFIG_SAMPLE_SPI_MSG;
static uint8_t rx_buf[sizeof(CONFIG_SAMPLE_SPI_MSG)];

static void spim_handler(nrfx_spim_event_t const *event, void *context)
{
	if (event->type == NRFX_SPIM_EVENT_DONE) {
		LOG_INF("Message sent: \"%s\"", CONFIG_SAMPLE_SPI_MSG);
	}
}

static void spis_handler(nrfx_spis_event_t const *event, void *context)
{
	if (event->evt_type == NRFX_SPIS_XFER_DONE) {
		LOG_INF("Message received: \"%s\"", rx_buf);
		nrf_gpio_pin_toggle(BOARD_PIN_LED_2);

		nrfx_spis_buffers_set(&spis_inst, NULL, 0, rx_buf, sizeof(rx_buf));
	}
}

ISR_DIRECT_DECLARE(spim_isr_handler)
{
	nrfx_spim_irq_handler(&spim_inst);
	return 0;
}

ISR_DIRECT_DECLARE(spis_isr_handler)
{
	nrfx_spis_irq_handler(&spis_inst);
	return 0;
}

static void button_handler(uint8_t pin, uint8_t action)
{
	if (pin == BOARD_PIN_BTN_2 && action == BM_BUTTONS_PRESS) {
		nrfx_spim_xfer_desc_t xfer = NRFX_SPIM_XFER_TX(tx_buf, sizeof(tx_buf));

		nrfx_spim_xfer(&spim_inst, &xfer, 0);
	}
}

int main(void)
{
	int err;
	nrfx_spim_config_t spim_config = NRFX_SPIM_DEFAULT_CONFIG(BOARD_APP_SPIM_PIN_SCK,
								  BOARD_APP_SPIM_PIN_MOSI,
								  BOARD_APP_SPIM_PIN_MISO,
								  BOARD_APP_SPIM_PIN_CSN);
	spim_config.frequency = NRFX_MHZ_TO_HZ(4);
	nrfx_spis_config_t spis_config = NRFX_SPIS_DEFAULT_CONFIG(BOARD_APP_SPIS_PIN_SCK,
								  BOARD_APP_SPIS_PIN_MOSI,
								  BOARD_APP_SPIS_PIN_MISO,
								  BOARD_APP_SPIS_PIN_CSN);
	struct bm_buttons_config btn_configs[] = {
		{
			.pin_number = BOARD_PIN_BTN_2,
			.active_state = BM_BUTTONS_ACTIVE_LOW,
			.pull_config = BM_BUTTONS_PIN_PULLUP,
			.handler = button_handler,
		},
	};

	LOG_INF("SPI sample started");

	/* SPI setup */
	BM_IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(BOARD_APP_SPIM_INST), IRQ_PRIO_LOWEST,
			      spim_isr_handler, 0);
	irq_enable(NRFX_IRQ_NUMBER_GET(BOARD_APP_SPIM_INST));
	BM_IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(BOARD_APP_SPIS_INST), IRQ_PRIO_LOWEST,
			      spis_isr_handler, 0);
	irq_enable(NRFX_IRQ_NUMBER_GET(BOARD_APP_SPIS_INST));

	err = nrfx_spim_init(&spim_inst, &spim_config, spim_handler, NULL);
	if (err) {
		LOG_ERR("SPIM init failed: %d", err);
		goto idle;
	}
	err = nrfx_spis_init(&spis_inst, &spis_config, spis_handler, NULL);
	if (err) {
		LOG_ERR("SPIS init failed: %d", err);
		goto idle;
	}

	err = nrfx_spis_buffers_set(&spis_inst, NULL, 0, rx_buf, sizeof(rx_buf));
	if (err) {
		LOG_ERR("SPIS buffers set failed: %d", err);
		goto idle;
	}
	LOG_INF("SPI initialized");

	/* Button setup */
	err = bm_buttons_init(btn_configs, ARRAY_SIZE(btn_configs),
			      BM_BUTTONS_DETECTION_DELAY_MIN_US);
	if (err) {
		LOG_ERR("Buttons init failed: %d", err);
		goto idle;
	}

	err = bm_buttons_enable();
	if (err) {
		LOG_ERR("Buttons enable failed: %d", err);
		goto idle;
	}

	LOG_INF("Buttons initialized");

	/* LED setup */
	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_cfg_output(BOARD_PIN_LED_2);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, !BOARD_LED_ACTIVE_STATE);
	nrf_gpio_pin_write(BOARD_PIN_LED_2, !BOARD_LED_ACTIVE_STATE);

	LOG_INF("LEDs initialized");

	/* User info on startup */
	LOG_INF("Press Button 2 to send \"%s\" over SPI", CONFIG_SAMPLE_SPI_MSG);
	LOG_INF("Waiting for SPI master to send data...");

	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
	LOG_INF("SPI sample initialized");

idle:
	while (true) {
		log_flush();

		k_cpu_idle();
	}

	return 0;
}
