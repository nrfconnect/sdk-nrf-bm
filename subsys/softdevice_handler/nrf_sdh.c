/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdint.h>
#include <nrf_sdh.h>
#include <nrf_sdm.h>
#include <nrf_nvic.h>
#include <event_scheduler.h>
#include <zephyr/toolchain.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/iterable_sections.h>

LOG_MODULE_REGISTER(nrf_sdh, CONFIG_NRF_SDH_LOG_LEVEL);

#if ((CONFIG_NRF_SDH_CLOCK_LF_SRC == NRF_CLOCK_LF_SRC_RC) &&                                       \
	(CONFIG_NRF_SDH_CLOCK_LF_ACCURACY != NRF_CLOCK_LF_ACCURACY_500_PPM))
#warning Please select NRF_CLOCK_LF_ACCURACY_500_PPM when using NRF_CLOCK_LF_SRC_RC
#endif

/* Global NVIC state instance, required by nrf_nvic.h */
nrf_nvic_state_t nrf_nvic_state;

static atomic_t sdh_enabled;	/* Whether the SoftDevice is enabled. */
static atomic_t sdh_suspended;	/* Whether this module is suspended. */
static atomic_t sdh_transition; /* Whether enable/disable process was started. */

static char *req_tostr(enum nrf_sdh_state_req r)
{
	switch (r) {
	case NRF_SDH_STATE_REQ_ENABLE:
		return "enable";
	case NRF_SDH_STATE_REQ_DISABLE:
		return "disable";
	default:
		return "unknown";
	};
}
static char *state_tostr(enum nrf_sdh_state_evt s)
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

static int sdh_state_req_observer_notify(enum nrf_sdh_state_req req)
{
	if (IS_ENABLED(CONFIG_NRF_SDH_STR_TABLES)) {
		LOG_INF("State change request: %s", req_tostr(req));
	} else {
		LOG_INF("State change request: %#x", req);
	}

	TYPE_SECTION_FOREACH(struct nrf_sdh_state_req_observer, nrf_sdh_state_req_observers, obs) {
		if (obs->handler(req, obs->context)) {
			LOG_DBG("Notify observer %p => ready", obs);
		} else {
			/* Do not let SoftDevice change state now */
			LOG_DBG("Notify observer %p => busy", obs);
			return -EBUSY;
		}
	}

	return 0;
}

static void sdh_state_evt_observer_notify(enum nrf_sdh_state_evt state)
{
	if (IS_ENABLED(CONFIG_NRF_SDH_STR_TABLES)) {
		LOG_DBG("State change: %s", state_tostr(state));
	} else {
		LOG_DBG("State change: %#x", state);
	}

	TYPE_SECTION_FOREACH(struct nrf_sdh_state_evt_observer, nrf_sdh_state_evt_observers, obs) {
		obs->handler(state, obs->context);
	}
}

static int softdevices_evt_irq_enable(void)
{
	int err;

	err = sd_nvic_EnableIRQ((IRQn_Type)SD_EVT_IRQn);
	if (err) {
		LOG_ERR("Unable to enable SoftDevice IRQ, nrf_error %d", err);
	}

	return err;
}

static int softdevice_evt_irq_disable(void)
{
	int err;

	err = sd_nvic_DisableIRQ((IRQn_Type)SD_EVT_IRQn);
	if (err) {
		LOG_ERR("Unable to disable SoftDevice IRQ, nrf_error %d", err);
	}

	return err;
}

__weak void softdevice_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
	LOG_ERR("SoftDevice fault! ID %#x, PC %#x, Info %#x\n", id, pc, info);

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

int nrf_sdh_enable_request(void)
{
	int err;
	const nrf_clock_lf_cfg_t clock_lf_cfg = {
		.source = CONFIG_NRF_SDH_CLOCK_LF_SRC,
		.rc_ctiv = CONFIG_NRF_SDH_CLOCK_LF_RC_CTIV,
		.rc_temp_ctiv = CONFIG_NRF_SDH_CLOCK_LF_RC_TEMP_CTIV,
		.accuracy = CONFIG_NRF_SDH_CLOCK_LF_ACCURACY
	};

	if (sdh_enabled) {
		return -EALREADY;
	}

	atomic_set(&sdh_transition, true);

	err = sdh_state_req_observer_notify(NRF_SDH_STATE_REQ_ENABLE);
	if (err) {
		/** TODO: should this be Success instead? */
		/* Leave sdh_transition to 1, so process can be continued */
		__ASSERT(err == -EBUSY, "Unknown return value %d from sdh req observer", err);
		return -EBUSY;
	}

	atomic_set(&sdh_transition, false);

	/* Notify observers about starting SoftDevice enable process. */
	sdh_state_evt_observer_notify(NRF_SDH_STATE_EVT_ENABLE_PREPARE);

	err = sd_softdevice_enable(&clock_lf_cfg, softdevice_fault_handler);
	if (err) {
		LOG_ERR("Failed to enable SoftDevice, nrf_error %d", err);
		return -EINVAL;
	}

	atomic_set(&sdh_enabled, true);
	atomic_set(&sdh_suspended, false);

	/* Enable event interrupt, the priority has already been set by the stack. */
	err = softdevices_evt_irq_enable();
	if (err) {
		return -EINVAL;
	}

	/* Notify observers about a finished SoftDevice enable process. */
	sdh_state_evt_observer_notify(NRF_SDH_STATE_EVT_ENABLED);

	return 0;
}

int nrf_sdh_disable_request(void)
{
	int err;

	if (!sdh_enabled) {
		return -EALREADY;
	}

	atomic_set(&sdh_transition, true);

	/* Notify observers about SoftDevice disable request. */
	err = sdh_state_req_observer_notify(NRF_SDH_STATE_REQ_DISABLE);
	if (err) {
		/** TODO: should this be Success instead? */
		/* Leave sdh_transition to 1, so process can be continued */
		__ASSERT(err == -EBUSY, "Unknown return value %d from sdh req observer", err);
		return -EBUSY;
	}

	atomic_set(&sdh_transition, false);

	/* Notify observers about starting SoftDevice disable process. */
	sdh_state_evt_observer_notify(NRF_SDH_STATE_EVT_DISABLE_PREPARE);

	err = sd_softdevice_disable();
	if (err) {
		LOG_ERR("Failed to disable SoftDevice, nrf_error %d", err);
		return -EINVAL;
	}

	atomic_set(&sdh_enabled, false);

	err = softdevice_evt_irq_disable();
	if (err) {
		return -EINVAL;
	}

	/* Notify observers about a finished SoftDevice enable process. */
	sdh_state_evt_observer_notify(NRF_SDH_STATE_EVT_DISABLED);

	return 0;
}

int nrf_sdh_request_continue(void)
{
	if (!sdh_transition) {
		return -EINVAL;
	}

	if (sdh_enabled) {
		return nrf_sdh_disable_request();
	} else {
		return nrf_sdh_enable_request();
	}
}

bool nrf_sdh_is_enabled(void)
{
	return sdh_enabled;
}

void nrf_sdh_suspend(void)
{
	int err;

	if (!sdh_enabled || sdh_suspended) {
		return;
	}

	err = softdevice_evt_irq_disable();
	if (err) {
		return;
	}

	atomic_set(&sdh_suspended, true);
}

void nrf_sdh_resume(void)
{
	int err;

	if ((!sdh_suspended) || (!sdh_enabled)) {
		return;
	}

	/* Force calling ISR again to make sure we dispatch pending events */
	err = sd_nvic_SetPendingIRQ((IRQn_Type)SD_EVT_IRQn);
	if (err) {
		LOG_ERR("Unable to set SoftDevice IRQ, nrf_error %d", err);
		return;
	}

	err = softdevices_evt_irq_enable();
	if (err) {
		return;
	}

	atomic_set(&sdh_suspended, false);
}

bool nrf_sdh_is_suspended(void)
{
	return (!sdh_enabled) || (sdh_suspended);
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

static void isr_handler(const void *arg)
{
	ARG_UNUSED(arg);
	SD_EVT_IRQHandler();
}

static int sd_irq_init(void)
{
	IRQ_CONNECT(SD_EVT_IRQn, 1, isr_handler, NULL, 0);
	irq_enable(SD_EVT_IRQn);

	return 0;
}

SYS_INIT(sd_irq_init, POST_KERNEL, 0);
