/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <nrfx_pwm.h>
#include <hal/nrf_gpio.h>
#include <board-config.h>

LOG_MODULE_REGISTER(sample, CONFIG_SAMPLE_PWM_LOG_LEVEL);

/* nrfx PWM instance index. */
#define PWM_INST NRF_PWM20

nrfx_pwm_t pwm_instance = NRFX_PWM_INSTANCE(PWM_INST);
nrf_pwm_values_common_t pwm_val[] = {
	0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000,
	900, 800, 700, 600, 500, 400, 300, 200, 100, 0
};

static void pwm_handler(nrfx_pwm_event_type_t event_type, void *ctx)
{
	static uint32_t curr_loop = 1;

	LOG_INF("Loops: %u ", curr_loop);
	curr_loop++;
}

ISR_DIRECT_DECLARE(pwm_direct_isr)
{
	nrfx_pwm_irq_handler(&pwm_instance);
	return 0;
}

int main(void)
{
	int err;
	/*
	 * PWM signal can be exposed on GPIO pin only within the same domain.
	 * For nRF54L-series there is only one domain which contains both PWM and GPIO:
	 * PWM20/21/22 and GPIO Port P1.
	 * Only LEDs connected to P1 can work with PWM, in this case LED1 and LED3.
	 */
	nrfx_pwm_config_t config = NRFX_PWM_DEFAULT_CONFIG(BOARD_PIN_LED_1, BOARD_PIN_LED_3,
							   NRF_PWM_PIN_NOT_CONNECTED,
							   NRF_PWM_PIN_NOT_CONNECTED);
	nrf_pwm_sequence_t seq = {
		.values = {pwm_val},
		.length = NRFX_ARRAY_SIZE(pwm_val),
		.repeats = CONFIG_SAMPLE_PWM_VALUE_REPEATS,
		.end_delay = 0
	};

	LOG_INF("PWM sample started");

	IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(PWM_INST),
			   CONFIG_SAMPLE_PWM_IRQ_PRIO,
			   pwm_direct_isr, 0);

	err = nrfx_pwm_init(&pwm_instance, &config, pwm_handler, &pwm_instance);
	if (err) {
		LOG_ERR("Failed to initialize PWM, err %d", err);
		goto idle;
	}

	nrfx_pwm_simple_playback(&pwm_instance, &seq, CONFIG_SAMPLE_PWM_PLAYBACK_COUNT,
				 NRFX_PWM_FLAG_LOOP);

	nrf_gpio_cfg_output(BOARD_PIN_LED_0);
	nrf_gpio_pin_write(BOARD_PIN_LED_0, BOARD_LED_ACTIVE_STATE);

	LOG_INF("PWM sample initialized");

idle:
	while (true) {
		log_flush();

		k_cpu_idle();
	}

	return 0;
}
