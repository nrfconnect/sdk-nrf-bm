/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdint.h>
#include <nrf_sdm.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/event_scheduler.h>
#include <zephyr/toolchain.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/iterable_sections.h>

LOG_MODULE_REGISTER(nrf_sdh, CONFIG_NRF_SDH_LOG_LEVEL);

#if ((CONFIG_NRF_SDH_CLOCK_LF_SRC == NRF_CLOCK_LF_SRC_RC) &&                                       \
	(CONFIG_NRF_SDH_CLOCK_LF_ACCURACY != NRF_CLOCK_LF_ACCURACY_500_PPM))
#warning Please select NRF_CLOCK_LF_ACCURACY_500_PPM when using NRF_CLOCK_LF_SRC_RC
#endif

static atomic_t sdh_suspended;	/* Whether this module is suspended. */
static atomic_t sdh_transition; /* Whether enable/disable process was started. */

static char *state_to_str(enum nrf_sdh_state_evt s)
{
	switch (s) {
	case NRF_SDH_STATE_EVT_ENABLE_PREPARE:
		return "enabling";
	case NRF_SDH_STATE_EVT_ENABLED:
		return "enabled";
	case NRF_SDH_STATE_EVT_DISABLE_PREPARE:
		return "disabling";
	case NRF_SDH_STATE_EVT_DISABLED:
		return "disabled";
	default:
		return "unknown";
	};
}

/**
 * @brief Notify a state change to state observers.
 *
 * Extern in nrf_sdh_ble.c
 *
 * @param state The state to be notified.
 * @return true If any observers are busy.
 * @return false If no observers are busy.
 */
bool sdh_state_evt_observer_notify(enum nrf_sdh_state_evt state)
{
	bool busy;
	bool all_ready;
	bool busy_is_allowed;

	if (IS_ENABLED(CONFIG_NRF_SDH_STR_TABLES)) {
		LOG_DBG("State change: %s", state_to_str(state));
	} else {
		LOG_DBG("State change: %#x", state);
	}

	all_ready = true;
	busy_is_allowed = (state == NRF_SDH_STATE_EVT_ENABLE_PREPARE) ||
			  (state == NRF_SDH_STATE_EVT_DISABLE_PREPARE);

	TYPE_SECTION_FOREACH(struct nrf_sdh_state_evt_observer, nrf_sdh_state_evt_observers, obs) {
		/* If it's a _PREPARE event, dispatch only to busy observers, and update
		 * their busy state in RAM. Otherwise dispatch unconditionally to all observers.
		 */
		if (busy_is_allowed && obs->is_busy) {
			obs->is_busy = !!obs->handler(state, obs->context);
			if (obs->is_busy) {
				LOG_DBG("SoftDevice observer %p is busy", obs);
			}
			all_ready &= !obs->is_busy;
		} else {
			busy = obs->handler(state, obs->context);
			(void) busy;
			__ASSERT(!busy, "Returning non-zero from these events is ignored");
		}
	}

	return !all_ready;
}

__weak void softdevice_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
	LOG_ERR("SoftDevice fault! ID %#x, PC %#x, Info %#x", id, pc, info);

	switch (id) {
	case NRF_FAULT_ID_SD_ASSERT:
		LOG_ERR("NRF_FAULT_ID_SD_ASSERT: SoftDevice assert");
		break;
	case NRF_FAULT_ID_APP_MEMACC:
		LOG_ERR("NRF_FAULT_ID_APP_MEMACC: Application bad memory access");
		if (info == 0x00) {
			LOG_ERR("Application tried to access SoftDevice RAM");
		} else {
			LOG_ERR("Application tried to access SoftDevice peripheral at %#x", info);
		}
		break;
	}

	for (;;) {
		/* loop */
	}
}

static int nrf_sdh_enable(void)
{
	int err;
	const nrf_clock_lf_cfg_t clock_lf_cfg = {
		.source = CONFIG_NRF_SDH_CLOCK_LF_SRC,
		.rc_ctiv = CONFIG_NRF_SDH_CLOCK_LF_RC_CTIV,
		.rc_temp_ctiv = CONFIG_NRF_SDH_CLOCK_LF_RC_TEMP_CTIV,
		.accuracy = CONFIG_NRF_SDH_CLOCK_LF_ACCURACY,
		.hfclk_latency = CONFIG_NRF_SDH_CLOCK_HFCLK_LATENCY,
		.hfint_ctiv = CONFIG_NRF_SDH_CLOCK_HFINT_CALIBRATION_INTERVAL,
	};

	err = sd_softdevice_enable(&clock_lf_cfg, softdevice_fault_handler);
	if (err) {
		LOG_ERR("Failed to enable SoftDevice, nrf_error %#x", err);
		return -EINVAL;
	}

	atomic_set(&sdh_suspended, false);
	atomic_set(&sdh_transition, false);

	/* Enable event interrupt, the priority has already been set by the stack. */
	NVIC_EnableIRQ((IRQn_Type)SD_EVT_IRQn);

	(void)sdh_state_evt_observer_notify(NRF_SDH_STATE_EVT_ENABLED);

	return 0;
}

static int nrf_sdh_disable(void)
{
	int err;

	err = sd_softdevice_disable();
	if (err) {
		LOG_ERR("Failed to disable SoftDevice, nrf_error %#x", err);
		return -EINVAL;
	}

	atomic_set(&sdh_transition, false);

	NVIC_DisableIRQ((IRQn_Type)SD_EVT_IRQn);

	(void)sdh_state_evt_observer_notify(NRF_SDH_STATE_EVT_DISABLED);

	return 0;
}

int nrf_sdh_enable_request(void)
{
	bool busy;
	uint8_t enabled;

	(void)sd_softdevice_is_enabled(&enabled);
	if (enabled) {
		return -EALREADY;
	}

	if (sdh_transition) {
		return -EINPROGRESS;
	}

	atomic_set(&sdh_transition, true);

	/* Assume all observers to be busy */
	TYPE_SECTION_FOREACH(struct nrf_sdh_state_evt_observer,
			     nrf_sdh_state_evt_observers, obs) {
		obs->is_busy = true;
	}

	busy = sdh_state_evt_observer_notify(NRF_SDH_STATE_EVT_ENABLE_PREPARE);
	if (busy) {
		/* Leave sdh_transition to 1, so process can be continued */
		return -EBUSY;
	}

	return nrf_sdh_enable();
}

int nrf_sdh_disable_request(void)
{
	bool busy;
	uint8_t enabled;

	(void)sd_softdevice_is_enabled(&enabled);
	if (!enabled) {
		return -EALREADY;
	}

	if (sdh_transition) {
		return -EINPROGRESS;
	}

	atomic_set(&sdh_transition, true);

	/* Assume all observers to be busy */
	TYPE_SECTION_FOREACH(struct nrf_sdh_state_evt_observer,
			     nrf_sdh_state_evt_observers, obs) {
		obs->is_busy = true;
	}

	busy = sdh_state_evt_observer_notify(NRF_SDH_STATE_EVT_DISABLE_PREPARE);
	if (busy) {
		/* Leave sdh_transition to 1, so process can be continued */
		return -EBUSY;
	}

	return nrf_sdh_disable();
}

int nrf_sdh_observer_ready(struct nrf_sdh_state_evt_observer *obs)
{
	int err;
	bool busy;
	uint8_t enabled;

	if (!obs) {
		return -EFAULT;
	}
	if (!sdh_transition) {
		return -EPERM;
	}
	if (!obs->is_busy) {
		LOG_WRN("Observer %p is not busy", obs);
		return 0;
	}

	obs->is_busy = false;

	(void)sd_softdevice_is_enabled(&enabled);

	busy = sdh_state_evt_observer_notify(enabled ? NRF_SDH_STATE_EVT_DISABLE_PREPARE
						     : NRF_SDH_STATE_EVT_ENABLE_PREPARE);

	/* Another observer needs to ready itself */
	if (busy) {
		return 0;
	}

	if (enabled) {
		err = nrf_sdh_disable();
	} else {
		err = nrf_sdh_enable();
	}

	__ASSERT(!err, "Failed to change SoftDevice state");
	(void) err;

	return 0;
}

void nrf_sdh_suspend(void)
{
	uint8_t sd_is_enabled;

	(void)sd_softdevice_is_enabled(&sd_is_enabled);

	if (!sd_is_enabled) {
		LOG_WRN("Tried to suspend, but SoftDevice is disabled");
		return;
	}
	if (sdh_suspended) {
		LOG_WRN("Tried to suspend, but already is suspended");
		return;
	}

	NVIC_DisableIRQ((IRQn_Type)SD_EVT_IRQn);

	atomic_set(&sdh_suspended, true);
}

void nrf_sdh_resume(void)
{
	uint8_t sd_is_enabled;

	(void)sd_softdevice_is_enabled(&sd_is_enabled);

	if (!sd_is_enabled) {
		LOG_WRN("Tried to resume, but SoftDevice is disabled");
		return;
	}
	if (!sdh_suspended) {
		LOG_WRN("Tried to resume, but not suspended");
		return;
	}

	/* Force calling ISR again to make sure we dispatch pending events */
	NVIC_SetPendingIRQ((IRQn_Type)SD_EVT_IRQn);
	NVIC_EnableIRQ((IRQn_Type)SD_EVT_IRQn);

	atomic_set(&sdh_suspended, false);
}

bool nrf_sdh_is_suspended(void)
{
	uint8_t sd_is_enabled;

	(void)sd_softdevice_is_enabled(&sd_is_enabled);

	return (!sd_is_enabled || sdh_suspended);
}

void nrf_sdh_evts_poll(void)
{
	/* Notify observers about pending SoftDevice event. */
	TYPE_SECTION_FOREACH(struct nrf_sdh_stack_evt_observer, nrf_sdh_stack_evt_observers, obs) {
		obs->handler(obs->context);
	}
}

#if defined(CONFIG_NRF_SDH_DISPATCH_MODEL_IRQ)

void SD_EVT_IRQHandler(void)
{
	nrf_sdh_evts_poll();
}

#elif defined(CONFIG_NRF_SDH_DISPATCH_MODEL_SCHED)

static void sdh_events_poll(void *data, size_t len)
{
	(void)data;
	(void)len;

	nrf_sdh_evts_poll();
}

void SD_EVT_IRQHandler(void)
{
	int err;

	err = event_scheduler_defer(sdh_events_poll, NULL, 0);
	if (err) {
		LOG_WRN("Unable to schedule SoftDevice event, err %d", err);
	}
}

#elif defined(CONFIG_NRF_SDH_DISPATCH_MODEL_POLL)

#endif /* NRF_SDH_DISPATCH_MODEL */

ISR_DIRECT_DECLARE(sd_direct_isr)
{
	SD_EVT_IRQHandler();
	return 0;
}

static int sd_irq_init(void)
{
	IRQ_DIRECT_CONNECT(SD_EVT_IRQn, 4, sd_direct_isr, 0);
	irq_enable(SD_EVT_IRQn);

	return 0;
}

SYS_INIT(sd_irq_init, APPLICATION, 0);
