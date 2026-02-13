/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <bm/bm_gpiote.h>
#include <helpers/nrfx_gppi.h>
#include <nrfx_timer.h>
#include <nrfx_gpiote.h>
#include <board-config.h>

LOG_MODULE_REGISTER(app, CONFIG_SAMPLE_PPI_LOG_LEVEL);

static nrfx_timer_t timer_inst = NRFX_TIMER_INSTANCE(NRF_TIMER_INST_GET(20));

ISR_DIRECT_DECLARE(timer_direct_isr)
{
	nrfx_timer_irq_handler(&timer_inst);
	return 0;
}

static void timer_handler(nrf_timer_event_t event_type, void *ctx)
{
	switch (event_type) {
	case NRF_TIMER_EVENT_COMPARE0:
	default:
		/* Do nothing. */
		break;
	}
}

int main(void)
{
	int err;
	uint8_t out_channel;
	nrfx_gppi_handle_t gppi_handle;
	uint32_t ticks_half_period;

	nrfx_gpiote_t *gpiote_inst =
		bm_gpiote_instance_get(NRF_PIN_NUMBER_TO_PORT(BOARD_PIN_LED_1));

	nrfx_timer_config_t timer_config = {
		.frequency = NRF_TIMER_BASE_FREQUENCY_GET(timer_inst.p_reg),
		.mode = NRF_TIMER_MODE_TIMER,
		.bit_width = NRF_TIMER_BIT_WIDTH_32,
		.interrupt_priority = CONFIG_SAMPLE_TIMER_IRQ_PRIO,
	};
	const nrfx_gpiote_output_config_t gpiote_output_config = {
		.drive = NRF_GPIO_PIN_S0S1,
		.input_connect = NRF_GPIO_PIN_INPUT_DISCONNECT,
		.pull = NRF_GPIO_PIN_NOPULL,
	};
	nrfx_gpiote_task_config_t gpiote_task_config = {
		.polarity = NRF_GPIOTE_POLARITY_TOGGLE,
		.init_val = NRF_GPIOTE_INITIAL_VALUE_LOW,
	};

	LOG_INF("PPI sample started");

	IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_TIMER20),
			   CONFIG_SAMPLE_TIMER_IRQ_PRIO,
			   timer_direct_isr, 0);

	err = nrfx_timer_init(&timer_inst, &timer_config, timer_handler);
	if (err) {
		LOG_ERR("nrfx_timer_init failed, err %d", err);
		goto idle;
	}

	ticks_half_period = nrfx_timer_ms_to_ticks(&timer_inst,
						   CONFIG_SAMPLE_LED_BLINK_INTERVAL_MS);

	nrfx_timer_extended_compare(&timer_inst, NRF_TIMER_CC_CHANNEL0, ticks_half_period,
				    NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, true);

	err = nrfx_gpiote_channel_alloc(gpiote_inst, &out_channel);
	if (err) {
		LOG_ERR("nrfx_gpiote_channel_alloc failed, err %d", err);
		goto idle;
	}

	gpiote_task_config.task_ch = out_channel;

	err = nrfx_gpiote_output_configure(
		gpiote_inst, BOARD_PIN_LED_1, &gpiote_output_config, &gpiote_task_config);
	if (err) {
		LOG_ERR("nrfx_gpiote_output_configure failed, err %d", err);
		goto idle;
	}

	nrfx_gpiote_out_task_enable(gpiote_inst, BOARD_PIN_LED_1);

	/* Allocate a GPPI channel and setup the connection between the timer and LED1 */
	err = nrfx_gppi_conn_alloc(
		nrfx_timer_compare_event_address_get(&timer_inst, NRF_TIMER_CC_CHANNEL0),
		nrfx_gpiote_out_task_address_get(gpiote_inst, BOARD_PIN_LED_1),
		&gppi_handle);
	if (err) {
		LOG_ERR("nrfx_gppi_conn_alloc failed, err %d", err);
		goto idle;
	}

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);
	LOG_INF("PPI sample initialized");

	nrfx_gppi_conn_enable(gppi_handle);

	nrfx_timer_enable(&timer_inst);

idle:
	while (true) {
		log_flush();
		k_cpu_idle();
	}

	return 0;
}
