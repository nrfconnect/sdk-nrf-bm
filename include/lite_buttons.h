/*
 * Copyright (c) 2012-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup lite_buttons NCS-Lite Buttons library
 * @{
 *
 * @brief Buttons management module.
 *
 * @details The lite_buttons library uses the nrfx_gpiote to detect that a button has been
 *          pressed. To handle debouncing, it will start a lite_timer.
 *          The button will only be reported as pressed if the corresponding pin is still active
 *          when the timer expires.
 *          If there is a new GPIOTE event while the timer is running, the timer is restarted.
 */

#ifndef LITE_BUTTONS_H__
#define LITE_BUTTONS_H__

#include <stdint.h>
#include <stdbool.h>

#include <lite_timer.h>
#include <nrfx_gpiote.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Minimum detection delay in microseconds.
 *
 * Value will depend on the value of CONFIG_SYS_CLOCK_TICKS_PER_SEC.
 */
#define LITE_BUTTONS_DETECTION_DELAY_MIN_US                                                        \
	(2*k_ticks_to_us_ceil32(MAX(1, LITE_TIMER_MIN_TIMEOUT_TICKS)))

enum lite_buttons_event_type {
	/* Indicates that a button is released. */
	LITE_BUTTONS_RELEASE = 0,
	/* Indicates that a button is pressed. */
	LITE_BUTTONS_PRESS = 1
};

enum lite_buttons_active_state {
	/* Indicates that a button is active low. */
	LITE_BUTTONS_ACTIVE_LOW = 0,
	/* Indicates that a button is active high. */
	LITE_BUTTONS_ACTIVE_HIGH = 1
};

/**
 * @brief Enumerator used for selecting the pin to be pulled down or up at the time of pin
 *        configuration.
 */
enum lite_buttons_pin_pull {
	/* Pin pull-up resistor disabled. */
	LITE_BUTTONS_PIN_NOPULL = NRF_GPIO_PIN_NOPULL,
	/* Pin pull-down resistor enabled. */
	LITE_BUTTONS_PIN_PULLDOWN = NRF_GPIO_PIN_PULLDOWN,
	/* Pin pull-up resistor enabled. */
	LITE_BUTTONS_PIN_PULLUP = NRF_GPIO_PIN_PULLUP,
};

/**
 * @brief Button event handler type.
 */
typedef void (*lite_buttons_handler_t)(uint8_t pin_number, enum lite_buttons_event_type action);

/**
 * @brief Button configuration.
 */
struct lite_buttons_config {
	/* Pin to be used as a button. */
	uint8_t pin_number;
	/* LITE_BUTTONS_ACTIVE_HIGH or LITE_BUTTONS_ACTIVE_LOW. */
	enum lite_buttons_active_state active_state;
	/* Pull-up or pull-down configuration. */
	enum lite_buttons_pin_pull pull_config;
	/* Handler to be called when the button is pressed. */
	lite_buttons_handler_t handler;
};

/**
 * @brief Initialize buttons.
 *
 * @details This function will initialize the specified pins as buttons, and configure them.
 *
 * @note After this function returns, the @p configs configurations will still be referenced
 *       internally by the library. It is the user's responsibility to ensure that the @p configs
 *       configurations remain valid and exist until the end of the program or until
 *       @ref lite_buttons_deinit is called.
 *       If the @p configs configurations are altered or destroyed prematurely, it will result in
 *       undefined behavior.
 *
 * @note The @ref lite_buttons_enable function must be called in order to enable the button
 *       detection.
 *
 * @param[in] configs Array of button configurations to be used.
 * @param[in] num_configs Number of button configurations.
 * @param[in] detection_delay Delay (in microseconds) from a GPIOTE event until a button
 *                            is reported as pressed. Must be higher than
 *                            @ref LITE_BUTTONS_DETECTION_DELAY_MIN_US.
 *
 * @retval 0 on success.
 * @retval -NRF_EPERM If the lite_buttons library is already initialized.
 * @retval -NRF_EINVAL If input data is invalid.
 * @retval -NRF_EIO If an error occurred.
 */
int lite_buttons_init(const struct lite_buttons_config *configs, uint8_t num_configs,
		      uint32_t detection_delay);

/**
 * @brief Deinitialize buttons.
 *
 * @details This function will deinitialize the buttons library.
 *
 * @retval 0 on success.
 * @retval -NRF_EPERM If the lite_buttons library is already initialized.
 * @retval -NRF_EIO If an error occurred.
 */
int lite_buttons_deinit(void);

/**
 * @brief Enable button detection.
 *
 * @retval 0 on success.
 * @retval -NRF_EPERM If the lite_buttons library is not initialized.
 */
int lite_buttons_enable(void);

/**
 * @brief Disable button detection.
 *
 * @retval 0 on success.
 * @retval -NRF_EPERM If the lite_buttons library is not initialized.
 * @retval -NRF_EIO If an error occurred.
 */
int lite_buttons_disable(void);

/**
 * @brief Check if a button is being pressed.
 *
 * @param[in] pin Pin to be checked.
 *
 * @return true if the specified button is pressed, false otherwise.
 */
bool lite_buttons_is_pressed(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* LITE_BUTTONS_H__ */

/** @} */
