/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <nrfx_pwm.h>
#include <board-config.h>

LOG_MODULE_REGISTER(app, CONFIG_PWM_SAMPLE_LOG_LEVEL);

/* nrfx PWM instance index. */
#define PWM_INST_IDX 20

nrf_pwm_values_common_t pwm_val[] = {
	0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000,
	900, 800, 700, 600, 500, 400, 300, 200, 100, 0
};

static void pwm_handler(nrfx_pwm_evt_type_t event_type, void *ctx)
{
	static uint32_t curr_loop = 1;

	LOG_INF("Loops: %u ", curr_loop);
	curr_loop++;
}

int main(void)
{
	nrfx_err_t err;
	nrfx_pwm_t pwm_instance = NRFX_PWM_INSTANCE(PWM_INST_IDX);
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
		.repeats = CONFIG_PWM_VALUE_REPEATS,
		.end_delay = 0
	};

	LOG_INF("PWM sample started");

	IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_PWM_INST_GET(PWM_INST_IDX)),
		    CONFIG_PWM_IRQ_PRIO, NRFX_PWM_INST_HANDLER_GET(20), 0, 0);

	err = nrfx_pwm_init(&pwm_instance, &config, pwm_handler, &pwm_instance);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("Failed to initialize PWM, nrfx_error %#x", err);
		goto idle;
	}

	nrfx_pwm_simple_playback(&pwm_instance, &seq, CONFIG_PWM_PLAYBACK_COUNT,
				 NRFX_PWM_FLAG_LOOP);

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

	return 0;
}
