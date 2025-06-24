/*
 * Copyright (c) 2012-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
#include <nrfx_gpiote.h>
#include <nrfx_glue.h>

#include <bm_timer.h>
#include <bm_buttons.h>

LOG_MODULE_REGISTER(bm_buttons, CONFIG_BM_BUTTONS_LOG_LEVEL);

#define IRQ_PRIO     3
#define BITS_PER_PIN 4
#define NUM_PINS     CONFIG_BM_BUTTONS_NUM_PINS

/*
 * In this module, a state machine is used for each pin (button).
 * Since the GPIOTE PORT event is shared among all pins, individual pin events might be missed.
 * To ensure reliable detection, the module relies on the GPIOTE interrupt only to activate a
 * periodic `bm_timer` that samples the state of each pin.
 * When all buttons are in the idle state (i.e., there are no active buttons), the timer is stopped
 * to save power.
 *
 * State transitions are based on the sampled values of the buttons.
 * The state machine has the following transitions:
 *
 * -----------------------------------------------------------------
 * | value (is_active) | current state    | new state              |
 * |---------------------------------------------------------------|
 * |         0         | IDLE             | IDLE                   |
 * |         1         | IDLE             | PRESS_ARMED            |
 * |         0         | PRESS_ARMED      | IDLE                   |
 * |         1         | PRESS_ARMED      | PRESSED (push event)   |
 * |         0         | PRESSED          | RELEASE_DETECTED       |
 * |         1         | PRESSED          | PRESSED                |
 * |         0         | RELEASE_DETECTED | IDLE (release event)   |
 * |         1         | RELEASE_DETECTED | PRESSED                |
 * -----------------------------------------------------------------
 *
 */
enum button_state {
	BUTTON_IDLE,
	BUTTON_PRESS_ARMED,
	BUTTON_PRESSED,
	BUTTON_RELEASE_DETECTED
};

#if defined(CONFIG_SOC_SERIES_NRF52X)
static const nrfx_gpiote_t gpiote0_instance = NRFX_GPIOTE_INSTANCE(0);
#elif defined(CONFIG_SOC_SERIES_NRF54LX)
static const nrfx_gpiote_t gpiote20_instance = NRFX_GPIOTE_INSTANCE(20);
static const nrfx_gpiote_t gpiote30_instance = NRFX_GPIOTE_INSTANCE(30);

static inline const nrfx_gpiote_t *gpiote_get(uint32_t port)
{
	switch (port) {
	case 0:
		return &gpiote30_instance;
	case 1:
		return &gpiote20_instance;
	default:
		return NULL;
	}
}
#endif

static void gpiote_trigger_enable(nrfx_gpiote_pin_t pin, bool enable)
{
#if defined(CONFIG_SOC_SERIES_NRF52X)
	nrfx_gpiote_trigger_enable(&gpiote0_instance, pin, enable);
#elif defined(CONFIG_SOC_SERIES_NRF54LX)
	const nrfx_gpiote_t *gpiote_inst = gpiote_get(NRF_PIN_NUMBER_TO_PORT(pin));

	nrfx_gpiote_trigger_enable(gpiote_inst, pin, enable);
#endif
}

static void gpiote_uninit(void)
{
#if defined(CONFIG_SOC_SERIES_NRF52X)
	nrfx_gpiote_uninit(&gpiote0_instance);
#elif defined(CONFIG_SOC_SERIES_NRF54LX)
	nrfx_gpiote_uninit(&gpiote20_instance);
	nrfx_gpiote_uninit(&gpiote30_instance);
#endif
}

static int gpiote_init(void)
{
	int err;

#if defined(CONFIG_SOC_SERIES_NRF52X)
	if (!nrfx_gpiote_init_check(&gpiote0_instance)) {
		err = nrfx_gpiote_init(&gpiote0_instance, 0);
		if (err != NRFX_SUCCESS) {
			LOG_ERR("Failed to initialize gpiote, err: 0x%08X", err);
			return -EIO;
		}

		IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_GPIOTE_INST_GET(0)), IRQ_PRIO,
			    NRFX_GPIOTE_INST_HANDLER_GET(0), 0, 0);
	}
#elif defined(CONFIG_SOC_SERIES_NRF54LX)
	if (!nrfx_gpiote_init_check(&gpiote20_instance)) {
		err = nrfx_gpiote_init(&gpiote20_instance, 0);
		if (err != NRFX_SUCCESS) {
			LOG_ERR("Failed to initialize gpiote20, err: 0x%08X", err);
			return -EIO;
		}

		IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_GPIOTE_INST_GET(20)) + NRF_GPIOTE_IRQ_GROUP,
			    IRQ_PRIO, NRFX_GPIOTE_INST_HANDLER_GET(20), 0, 0);
	}

	if (!nrfx_gpiote_init_check(&gpiote30_instance)) {
		err = nrfx_gpiote_init(&gpiote30_instance, 0);
		if (err != NRFX_SUCCESS) {
			LOG_ERR("Failed to initialize gpiote30, err: 0x%08X", err);
			return -EIO;
		}

		IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_GPIOTE_INST_GET(30)) + NRF_GPIOTE_IRQ_GROUP,
			    IRQ_PRIO, NRFX_GPIOTE_INST_HANDLER_GET(30), 0, 0);
	}
#endif
	return 0;
}

static int gpiote_input_configure(nrfx_gpiote_pin_t pin,
				  const nrfx_gpiote_input_pin_config_t *input_config)
{
	int err;

#if defined(CONFIG_SOC_SERIES_NRF52X)
	err = nrfx_gpiote_input_configure(&gpiote0_instance, pin, input_config);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("nrfx_gpiote_input_configure, err: 0x%08X", err);
		return -EIO;
	}
#elif defined(CONFIG_SOC_SERIES_NRF54LX)
	const nrfx_gpiote_t *gpiote_inst = gpiote_get(NRF_PIN_NUMBER_TO_PORT(pin));

	err = nrfx_gpiote_input_configure(gpiote_inst, pin, input_config);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("nrfx_gpiote_input_configure, err: 0x%08X", err);
		return -EIO;
	}
#endif

	return 0;
}

struct bm_buttons_state {
	uint32_t pin_active;
	uint32_t detection_delay;
	struct bm_timer timer;
	struct bm_buttons_config const *configs;
	uint8_t num_configs;
	bool is_init;
	uint8_t pin_states[((NUM_PINS + 1) * BITS_PER_PIN) / 8];
};

static struct bm_buttons_state global;

static enum button_state state_get(uint8_t pin_index)
{
	uint8_t pair_state = global.pin_states[pin_index >> 1];
	uint8_t state = (pin_index & 0x1) ? (pair_state >> BITS_PER_PIN) : (pair_state & 0x0F);

	return (enum button_state)state;
}

static struct bm_buttons_config const *button_get(uint8_t pin)
{
	for (int i = 0; i < global.num_configs; i++) {
		struct bm_buttons_config const *config = &global.configs[i];

		if (pin == config->pin_number) {
			return config;
		}
	}

	return NULL;
}

static void state_set(uint8_t pin_index, uint8_t state)
{
	uint8_t mask = (pin_index & 1) ? 0x0F : 0xF0;
	uint8_t state_mask = (pin_index & 1) ? ((uint8_t)state << BITS_PER_PIN) : (uint8_t)state;

	global.pin_states[pin_index >> 1] &= mask;
	global.pin_states[pin_index >> 1] |= state_mask;
}

static void user_event(uint8_t pin, enum bm_buttons_evt_type type)
{
	struct bm_buttons_config const *config = button_get(pin);

	if (config && config->handler) {
		LOG_DBG("Pin %d %s", pin, (type == BM_BUTTONS_PRESS) ? "pressed" : "released");
		config->handler(pin, type);
	}
}

static void evt_handle(uint8_t pin, uint8_t index, bool is_active)
{
	switch (state_get(index)) {
	case BUTTON_IDLE:
		if (is_active) {
			state_set(index, BUTTON_PRESS_ARMED);
			LOG_DBG("Pin %d %s -> %s", pin, STRINGIFY(BUTTON_IDLE),
				STRINGIFY(BUTTON_PRESS_ARMED));
			NRFX_CRITICAL_SECTION_ENTER();
			global.pin_active |= 1ULL << index;
			NRFX_CRITICAL_SECTION_EXIT();
		} else {
			/* Stay in BUTTON_IDLE. */
		}
		break;
	case BUTTON_PRESS_ARMED:
		LOG_DBG("Pin %d %s -> %s", pin, STRINGIFY(BUTTON_PRESS_ARMED),
			is_active ? STRINGIFY(BUTTON_PRESSED) : STRINGIFY(BUTTON_IDLE));
		if (is_active) {
			state_set(index, BUTTON_PRESSED);
			user_event(pin, BM_BUTTONS_PRESS);
		} else {
			state_set(index, BUTTON_IDLE);
			NRFX_CRITICAL_SECTION_ENTER();
			global.pin_active &= ~(1ULL << index);
			NRFX_CRITICAL_SECTION_EXIT();
		}
		break;
	case BUTTON_PRESSED:
		if (is_active) {
			/* Stay in BUTTON_PRESSED. */
		} else {
			LOG_DBG("Pin %d %s -> %s", pin, STRINGIFY(BUTTON_PRESSED),
				STRINGIFY(BUTTON_RELEASE_DETECTED));
			state_set(index, BUTTON_RELEASE_DETECTED);
		}
		break;
	case BUTTON_RELEASE_DETECTED:
		LOG_DBG("Pin %d %s -> %s", pin, STRINGIFY(BUTTON_RELEASE_DETECTED),
			is_active ? STRINGIFY(BUTTON_PRESSED) : STRINGIFY(BUTTON_IDLE));
		if (is_active) {
			state_set(index, BUTTON_PRESSED);
		} else {
			state_set(index, BUTTON_IDLE);
			user_event(pin, BM_BUTTONS_RELEASE);
			NRFX_CRITICAL_SECTION_ENTER();
			global.pin_active &= ~(1ULL << index);
			NRFX_CRITICAL_SECTION_EXIT();
		}
		break;
	}
}

static void timer_start(void)
{
	int err;

	/* Timer needs to trigger two times before the button is detected as pressed/released. */
	err = bm_timer_start(&global.timer, BM_TIMER_US_TO_TICKS(global.detection_delay / 2),
			       NULL);
	if (err) {
		LOG_WRN("Failed to start app_timer (err:%d)", err);
	}
}

static int buttons_disable(void)
{
	int err;

	err = bm_timer_stop(&global.timer);
	if (err) {
		return -EIO;
	}

	for (int i = 0; i < global.num_configs; i++) {
		gpiote_trigger_enable(global.configs[i].pin_number, false);
	}

	NRFX_CRITICAL_SECTION_ENTER();
	global.pin_active = 0;
	NRFX_CRITICAL_SECTION_EXIT();

	return 0;
}

static void detection_delay_timeout_handler(void *ctx)
{
	bool is_set;
	bool is_active;
	struct bm_buttons_config const *config;

	for (int i = 0; i < global.num_configs; i++) {
		config = &global.configs[i];
		is_set = nrfx_gpiote_in_is_set(config->pin_number);
		is_active = !((config->active_state == BM_BUTTONS_ACTIVE_HIGH) ^ is_set);

		evt_handle(config->pin_number, i, is_active);
	}

	if (global.pin_active) {
		timer_start();
	} else {
		LOG_DBG("No active buttons, stopping timer");
	}
}

static void gpiote_evt_handler(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t action, void *ctx)
{
	struct bm_buttons_config const *config = button_get(pin);
	bool is_set = nrfx_gpiote_in_is_set(config->pin_number);
	bool is_active = !((config->active_state == BM_BUTTONS_ACTIVE_HIGH) ^ is_set);

	/* If event indicates that pin is active and no other pin is active start the timer. All
	 * action happens in timeout event.
	 */
	if (is_active && (global.pin_active == 0)) {
		LOG_DBG("First active button, starting periodic timer");
		timer_start();
	}
}

int bm_buttons_init(struct bm_buttons_config const *configs, uint8_t num_configs,
		    uint32_t detection_delay)
{
	int err;

	if (global.is_init) {
		return -EPERM;
	}

	if (!configs) {
		return -EINVAL;
	}

	if (num_configs > CONFIG_BM_BUTTONS_NUM_PINS) {
		return -EINVAL;
	}

	/* Timer needs to trigger two times before the button is detected as pressed/released. */
	if (BM_TIMER_US_TO_TICKS(detection_delay) < 2 * BM_TIMER_MIN_TIMEOUT_TICKS) {
		return -EINVAL;
	}

	err = gpiote_init();
	if (err) {
		return err;
	}

	global.configs = configs;
	global.num_configs = num_configs;
	global.detection_delay = detection_delay;

	const nrfx_gpiote_trigger_config_t trigger_config = {
		.trigger = NRFX_GPIOTE_TRIGGER_HITOLO,
	};

	const nrfx_gpiote_handler_config_t handler_config = {
		.handler = gpiote_evt_handler,
	};

	for (int i = 0; i < num_configs; i++) {
		const nrf_gpio_pin_pull_t pull_config = configs[i].pull_config;
		const nrfx_gpiote_input_pin_config_t input_config = {
			.p_pull_config = &pull_config,
			.p_trigger_config = &trigger_config,
			.p_handler_config = &handler_config,
		};

		const uint32_t pin_number = configs[i].pin_number;

		err = gpiote_input_configure(pin_number, &input_config);
		if (err) {
			return err;
		}
	}

	err = bm_timer_init(&global.timer, BM_TIMER_MODE_SINGLE_SHOT,
			      detection_delay_timeout_handler);
	if (err) {
		LOG_ERR("bm_timer_init failed, err: %d", err);
		return -EIO;
	}

	global.is_init = true;

	return 0;
}

int bm_buttons_deinit(void)
{
	int err;

	if (!global.is_init) {
		return -EPERM;
	}

	err = buttons_disable();
	if (err) {
		return -EIO;
	}

	gpiote_uninit();

	memset(&global, 0, sizeof(global));

	global.is_init = false;

	return 0;
}

int bm_buttons_enable(void)
{
	if (!global.is_init) {
		return -EPERM;
	}

	for (int i = 0; i < global.num_configs; i++) {
		gpiote_trigger_enable(global.configs[i].pin_number, true);
	}

	return 0;
}

int bm_buttons_disable(void)
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

bool bm_buttons_is_pressed(uint8_t pin)
{
	if (!global.is_init) {
		return false;
	}

	struct bm_buttons_config const *config = button_get(pin);

	if (config) {
		bool is_set = nrfx_gpiote_in_is_set(config->pin_number);

		return !(is_set ^ (config->active_state == BM_BUTTONS_ACTIVE_HIGH));
	}

	return false;
}
