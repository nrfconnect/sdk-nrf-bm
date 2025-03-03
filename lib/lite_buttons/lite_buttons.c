/*
 * Copyright (c) 2012-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
#include <nrfx_gpiote.h>

#include <lite_timer.h>
#include <lite_buttons.h>

LOG_MODULE_REGISTER(lite_buttons, CONFIG_LITE_BUTTONS_LOG_LEVEL);

/* Local implementation of critical section enter/exit support. */

#include <stdint.h>
#include <nrf.h>

#if defined(CONFIG_SOFTDEVICE)
#include <nrf_nvic.h>
#else
static uint32_t inside_critical_section;

static void lite_irq_disable(void)
{
	__disable_irq();
	inside_critical_section++;
}

static void lite_irq_enable(void)
{
	inside_critical_section--;
	if (inside_critical_section == 0) {
		__enable_irq();
	}
}
#endif

static void lite_critical_section_enter(uint8_t *nested)
{
#if defined(CONFIG_SOFTDEVICE)
	/* return value can be safely ignored */
	(void) sd_nvic_critical_region_enter(nested);
#else
	lite_irq_disable();
#endif
}

static void lite_critical_section_exit(uint8_t nested)
{
#if defined(CONFIG_SOFTDEVICE)
	/* return value can be safely ignored */
	(void) sd_nvic_critical_region_exit(nested);
#else
	lite_irq_enable();
#endif
}

/**@brief Macro for entering a critical section.
 *
 * @note Due to implementation details, there must exist one and only one call to
 *       CRITICAL_SECTION_EXIT() for each call to CRITICAL_SECTION_ENTER(), and they must be located
 *       in the same scope.
 */
#if defined(CONFIG_SOFTDEVICE)
#define LITE_CRITICAL_SECTION_ENTER()                                                              \
	{                                                                                          \
		uint8_t __CS_NESTED = 0;                                                           \
		lite_critical_section_enter(&__CS_NESTED);
#else
#define LITE_CRITICAL_SECTION_ENTER() lite_critical_section_enter(NULL)
#endif

/**@brief Macro for leaving a critical section.
 *
 * @note Due to implementation details, there must exist one and only one call to
 *       CRITICAL_SECTION_EXIT() for each call to CRITICAL_SECTION_ENTER(), and they must be located
 *       in the same scope.
 */
#if defined(CONFIG_SOFTDEVICE)
#define LITE_CRITICAL_SECTION_EXIT()                                                               \
		lite_critical_section_exit(__CS_NESTED);                                           \
	}
#else
#define LITE_CRITICAL_SECTION_EXIT() lite_critical_section_exit(0)
#endif

/* End of local implementation of critical section enter/exit support. */

#define GPIOTE_INST  0
#define IRQ_PRIO     3
#define BITS_PER_PIN 4
#define NUM_PINS     CONFIG_LITE_BUTTONS_NUM_PINS

/*
 * In this module, a state machine is used for each pin (button).
 * Since the GPIOTE PORT event is shared among all pins, individual pin events might be missed.
 * To ensure reliable detection, the module relies on the GPIOTE interrupt only to activate a
 * periodic `lite_timer` that samples the state of each pin.
 * When all buttons are in the idle state (i.e., there are no active buttons), the timer is stopped
 * to save power.
 *
 * State transitions are based on the sampled values of the buttons.
 * The state machine has the following transitions:
 *
 * -----------------------------------------------------
 * | value | current state    | new state              |
 * |---------------------------------------------------|
 * |  0    | IDLE             | IDLE                   |
 * |  1    | IDLE             | PRESS_ARMED            |
 * |  0    | PRESS_ARMED      | IDLE                   |
 * |  1    | PRESS_ARMED      | PRESS_DETECTED         |
 * |  1    | PRESS_DETECTED   | PRESSED (push event)   |
 * |  0    | PRESS_DETECTED   | PRESS_ARMED            |
 * |  0    | PRESSED          | RELEASE_DETECTED       |
 * |  1    | PRESSED          | PRESSED                |
 * |  0    | RELEASE_DETECTED | IDLE (release event)   |
 * |  1    | RELEASE_DETECTED | PRESSED                |
 * -----------------------------------------------------
 *
 */
enum button_state {
	BUTTON_IDLE,
	BUTTON_PRESS_ARMED,
	BUTTON_PRESS_DETECTED,
	BUTTON_PRESSED,
	BUTTON_RELEASE_DETECTED
};

static const nrfx_gpiote_t gpio_instance = NRFX_GPIOTE_INSTANCE(GPIOTE_INST);

struct lite_buttons_state {
	bool is_init;
	struct lite_buttons_config const *configs;
	uint8_t num_configs;
	uint32_t detection_delay;
	struct lite_timer timer;
	uint8_t pin_states[((NUM_PINS + 1) * BITS_PER_PIN) / 8];
	uint32_t pin_active;
};

static struct lite_buttons_state global;

static enum button_state state_get(uint8_t pin)
{
	uint8_t pair_state = global.pin_states[pin >> 1];
	uint8_t state = (pin & 0x1) ? (pair_state >> BITS_PER_PIN) : (pair_state & 0x0F);

	return (enum button_state)state;
}

static struct lite_buttons_config const *button_get(uint8_t pin)
{
	for (int i = 0; i < global.num_configs; i++) {
		struct lite_buttons_config const *config = &global.configs[i];

		if (pin == config->pin_number) {
			return config;
		}
	}

	return NULL;
}

static void state_set(uint8_t pin, uint8_t state)
{
	uint8_t mask = (pin & 1) ? 0x0F : 0xF0;
	uint8_t state_mask = (pin & 1) ? ((uint8_t)state << BITS_PER_PIN) : (uint8_t)state;

	global.pin_states[pin >> 1] &= mask;
	global.pin_states[pin >> 1] |= state_mask;
}

static void user_event(uint8_t pin, enum lite_buttons_event_type type)
{
	struct lite_buttons_config const *config = button_get(pin);

	if (config && config->handler) {
		LOG_DBG("Pin %d %s", pin, (type == LITE_BUTTONS_PRESS) ? "pressed" : "released");
		config->handler(pin, type);
	}
}

static void evt_handle(uint8_t pin, uint8_t value)
{
	switch (state_get(pin)) {
	case BUTTON_IDLE:
		if (value) {
			state_set(pin, BUTTON_PRESS_ARMED);
			LOG_DBG("Pin %d %s -> %s", pin, STRINGIFY(BUTTON_IDLE),
				STRINGIFY(BUTTON_PRESS_ARMED));
			LITE_CRITICAL_SECTION_ENTER();
			global.pin_active |= 1ULL << pin;
			LITE_CRITICAL_SECTION_EXIT();
		} else {
			/* stay in IDLE */
		}
		break;
	case BUTTON_PRESS_ARMED:
		state_set(pin, value ? BUTTON_PRESS_DETECTED : BUTTON_IDLE);
		LOG_DBG("Pin %d %s -> %s", pin, STRINGIFY(BUTTON_PRESS_ARMED),
			value ? STRINGIFY(BUTTON_PRESS_DETECTED) : STRINGIFY(BUTTON_IDLE));
		if (value == 0) {
			LITE_CRITICAL_SECTION_ENTER();
			global.pin_active &= ~(1ULL << pin);
			LITE_CRITICAL_SECTION_EXIT();
		}
		break;
	case BUTTON_PRESS_DETECTED:
		if (value) {
			state_set(pin, BUTTON_PRESSED);
			user_event(pin, LITE_BUTTONS_PRESS);
		} else {
			state_set(pin, BUTTON_PRESS_ARMED);
		}
		LOG_DBG("Pin %d %s -> %s", pin, STRINGIFY(BUTTON_PRESS_DETECTED),
			value ? STRINGIFY(BUTTON_PRESSED) : STRINGIFY(BUTTON_PRESS_ARMED));
		break;
	case BUTTON_PRESSED:
		if (value == 0) {
			state_set(pin, BUTTON_RELEASE_DETECTED);
			LOG_DBG("Pin %d %s -> %s", pin, STRINGIFY(BUTTON_PRESSED),
				STRINGIFY(BUTTON_RELEASE_DETECTED));
		} else {
			/* stay in pressed */
		}
		break;
	case BUTTON_RELEASE_DETECTED:
		if (value) {
			state_set(pin, BUTTON_PRESSED);
		} else {
			state_set(pin, BUTTON_IDLE);
			user_event(pin, LITE_BUTTONS_RELEASE);
			LITE_CRITICAL_SECTION_ENTER();
			global.pin_active &= ~(1ULL << pin);
			LITE_CRITICAL_SECTION_EXIT();
		}
		LOG_DBG("Pin %d %s -> %s", pin, STRINGIFY(BUTTON_RELEASE_DETECTED),
			value ? STRINGIFY(BUTTON_PRESSED) : STRINGIFY(BUTTON_IDLE));
		break;
	}
}

static void timer_start(void)
{
	int err;

	err = lite_timer_start(&global.timer, global.detection_delay / 2, NULL);
	if (err) {
		LOG_WRN("Failed to start app_timer (err:%d)", err);
	}
}

static int buttons_disable(void)
{
	int err;

	err = lite_timer_stop(&global.timer);
	if (err) {
		return -EIO;
	}

	for (int i = 0; i < global.num_configs; i++) {
		nrfx_gpiote_trigger_enable(&gpio_instance, global.configs[i].pin_number, false);
	}

	LITE_CRITICAL_SECTION_ENTER();
	global.pin_active = 0;
	LITE_CRITICAL_SECTION_EXIT();

	return 0;
}

static void detection_delay_timeout_handler(void *ctx)
{
	bool is_set;
	bool is_active;
	struct lite_buttons_config const *config;

	for (int i = 0; i < global.num_configs; i++) {
		config = &global.configs[i];
		is_set = nrfx_gpiote_in_is_set(config->pin_number);
		is_active = !((config->active_state == LITE_BUTTONS_ACTIVE_HIGH) ^ is_set);

		evt_handle(config->pin_number, is_active);
	}

	if (global.pin_active) {
		timer_start();
	} else {
		LOG_DBG("No active buttons, stopping timer");
	}
}

static void gpiote_event_handler(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t action, void *ctx)
{
	struct lite_buttons_config const *config = button_get(pin);
	bool is_set = nrfx_gpiote_in_is_set(config->pin_number);
	bool is_active = !((config->active_state == LITE_BUTTONS_ACTIVE_HIGH) ^ is_set);

	/* If event indicates that pin is active and no other pin is active start the timer. All
	 * action happens in timeout event.
	 */
	if (is_active && (global.pin_active == 0)) {
		LOG_DBG("First active button, starting periodic timer");
		timer_start();
	}
}

int lite_buttons_init(struct lite_buttons_config const *configs, uint8_t num_configs,
		      uint32_t detection_delay)
{
	int err;

	if (global.is_init) {
		return -EPERM;
	}

	if (!configs) {
		return -EINVAL;
	}

	/* todo: define limit somewhere (lite_timer?). */
	if (detection_delay < 2 * LITE_TIMER_MIN_TIMEOUT_US) {
		return -EINVAL;
	}

	if (!nrfx_gpiote_init_check(&gpio_instance)) {
		err = nrfx_gpiote_init(&gpio_instance, 0);
		if (err != NRFX_SUCCESS) {
			LOG_ERR("Failed to initialize gpiote, err: 0x%08X", err);
			return -EIO;
		}

		IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_GPIOTE_INST_GET(GPIOTE_INST)), IRQ_PRIO,
			    NRFX_GPIOTE_INST_HANDLER_GET(GPIOTE_INST), 0, 0);
	}

	global.configs = configs;
	global.num_configs = num_configs;
	global.detection_delay = detection_delay;

	const nrfx_gpiote_trigger_config_t trigger_config = {
		.trigger = NRFX_GPIOTE_TRIGGER_HITOLO,
	};

	const nrfx_gpiote_handler_config_t handler_config = {
		.handler = gpiote_event_handler,
	};

	for (int i = 0; i < num_configs; i++) {
		const nrf_gpio_pin_pull_t pull_config = configs[i].pull_config;
		const nrfx_gpiote_input_pin_config_t input_config = {
			.p_pull_config = &pull_config,
			.p_trigger_config = &trigger_config,
			.p_handler_config = &handler_config,
		};

		err = nrfx_gpiote_input_configure(&gpio_instance, configs[i].pin_number,
						  &input_config);
		if (err != NRFX_SUCCESS) {
			LOG_ERR("nrfx_gpiote_input_configure, err: 0x%08X", err);
			return -EIO;
		}
	}

	err = lite_timer_init(&global.timer, LITE_TIMER_MODE_SINGLE_SHOT,
			      detection_delay_timeout_handler);
	if (err) {
		LOG_ERR("lite_timer_init failed, err: %d", err);
		return -EIO;
	}

	global.is_init = true;

	return 0;
}

int lite_buttons_deinit(void)
{
	int err;

	if (!global.is_init) {
		return -EPERM;
	}

	err = buttons_disable();
	if (err) {
		return -EIO;
	}

	nrfx_gpiote_uninit(&gpio_instance);

	memset(&global, 0, sizeof(global));

	global.is_init = false;

	return 0;
}

int lite_buttons_enable(void)
{
	if (!global.is_init) {
		return -EPERM;
	}

	for (int i = 0; i < global.num_configs; i++) {
		nrfx_gpiote_trigger_enable(&gpio_instance, global.configs[i].pin_number, true);
	}

	return 0;
}

int lite_buttons_disable(void)
{
	int err;

	if (!global.is_init) {
		return -EPERM;
	}

	err = buttons_disable();
	if (err) {
		return err;
	}

	return 0;
}

bool lite_buttons_is_pressed(uint8_t pin)
{
	if (!global.is_init) {
		return false;
	}

	struct lite_buttons_config const *config = button_get(pin);

	if (config) {
		bool is_set = nrfx_gpiote_in_is_set(config->pin_number);

		return !(is_set ^ (config->active_state == LITE_BUTTONS_ACTIVE_HIGH));
	}

	return false;
}
