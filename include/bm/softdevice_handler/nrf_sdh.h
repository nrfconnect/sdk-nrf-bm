/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup nrf_sdh SoftDevice Handler
 * @{
 * @brief    API for initializing and disabling the SoftDevice.
 */

#ifndef NRF_SDH_H__
#define NRF_SDH_H__

#include <stdbool.h>
#include <zephyr/toolchain.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/iterable_sections.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup softdevice_observer_prio SoftDevice event observer priority levels
 *
 * A SoftDevice observer has a defined priority, which determines the order with
 * which the observer receives relevant events compared to other observers.
 *
 * Five priority levels are defined: highest, high, user, user low, lowest.
 * These can be selected using the tokens HIGHEST, HIGH, USER, USER_LOW, and LOWEST respectively.
 *
 * In general, an observer priority must be defined in such a way that an observer
 * has a lower priority than that of other observers (libraries, etc.) it depends on.
 *
 * @{
 */

/* Helper macros to check for equality */

#define H_NRF_SDH_OBSERVER_PRIO_HIGHEST_HIGHEST 1
#define H_NRF_SDH_OBSERVER_PRIO_HIGH_HIGH 1
#define H_NRF_SDH_OBSERVER_PRIO_USER_USER 1
#define H_NRF_SDH_OBSERVER_PRIO_USER_LOW_USER_LOW 1
#define H_NRF_SDH_OBSERVER_PRIO_LOWEST_LOWEST 1

/**
 * @brief Utility macro to check for observer priority validity.
 * @internal
 */
#define PRIO_LEVEL_IS_VALID(level)                                                                 \
	COND_CODE_1(H_NRF_SDH_OBSERVER_PRIO_HIGHEST_##level, (),                                   \
	(COND_CODE_1(H_NRF_SDH_OBSERVER_PRIO_HIGH_##level, (),                                     \
	(COND_CODE_1(H_NRF_SDH_OBSERVER_PRIO_USER_##level, (),                                     \
	(COND_CODE_1(H_NRF_SDH_OBSERVER_PRIO_USER_LOW_##level, (),                                 \
	(COND_CODE_1(H_NRF_SDH_OBSERVER_PRIO_LOWEST_##level, (),                                   \
	(BUILD_ASSERT(0, "Invalid priority level")))))))))))

/**
 * @brief Utility macro to convert a priority token to its numerical value
 * @internal
 */
#define PRIO_LEVEL_ORD(level)                                                                      \
	COND_CODE_1(H_NRF_SDH_OBSERVER_PRIO_HIGHEST_##level, (0),                                  \
	(COND_CODE_1(H_NRF_SDH_OBSERVER_PRIO_HIGH_##level, (1),                                    \
	(COND_CODE_1(H_NRF_SDH_OBSERVER_PRIO_USER_##level, (2),                                    \
	(COND_CODE_1(H_NRF_SDH_OBSERVER_PRIO_USER_LOW_##level, (3),                                \
	(COND_CODE_1(H_NRF_SDH_OBSERVER_PRIO_LOWEST_##level, (4),                                  \
	(BUILD_ASSERT(0, "Invalid priority level")))))))))))

/**
 * @}
 */

/**
 * @brief SoftDevice Handler state events.
 */
enum nrf_sdh_state_evt {
	/**
	 * @brief SoftDevice is going to be enabled.
	 *
	 * The state change can be halted by returning non-zero when receiving this event.
	 */
	NRF_SDH_STATE_EVT_ENABLE_PREPARE,
	/**
	 * @brief SoftDevice is enabled.
	 */
	NRF_SDH_STATE_EVT_ENABLED,
	/**
	 * @brief Bluetooth enabled.
	 */
	NRF_SDH_STATE_EVT_BLE_ENABLED,
	/**
	 * @brief SoftDevice is going to be disabled.
	 *
	 * The state change can be halted by returning non-zero when receiving this event.
	 */
	NRF_SDH_STATE_EVT_DISABLE_PREPARE,
	/**
	 * @brief SoftDevice is disabled.
	 */
	NRF_SDH_STATE_EVT_DISABLED,
};

/**
 * @brief SoftDevice Handler state event handler.
 *
 * @retval 0 If ready for the SoftDevice to change state.
 * @retval 1 If not ready for the SoftDevice to change state (state change is halted).
 */
typedef int (*nrf_sdh_state_evt_handler_t)(enum nrf_sdh_state_evt state, void *context);

/**
 * @brief SoftDevice Handler state observer.
 */
struct nrf_sdh_state_evt_observer {
	/**
	 * @brief State event handler.
	 */
	nrf_sdh_state_evt_handler_t handler;
	/**
	 * @brief A context parameter to the event handler.
	 */
	void *context;
};

/**
 * @brief Register a SoftDevice state observer.
 *
 * A SoftDevice state observer receives events when the SoftDevice state has changed
 * or is about to change. An observer may return non-zero when receiving
 * @ref NRF_SDH_STATE_EVT_ENABLE_PREPARE or @ref NRF_SDH_STATE_EVT_DISABLE_PREPARE
 * to halt the state change. A state change halted this way can be resumed
 * by calling @ref nrf_sdh_request_continue().
 *
 * @param _observer Name of the observer.
 * @param _handler State request handler.
 * @param _ctx A context passed to the state request handler.
 * @param _prio Priority of the observer's event handler.
 *		Allowed input: `HIGHEST`, `HIGH`, `USER`, `USER_LOW`, `LOWEST`.
 */
#define NRF_SDH_STATE_EVT_OBSERVER(_observer, _handler, _ctx, _prio)                               \
	PRIO_LEVEL_IS_VALID(_prio);                                                                \
	static int _handler(enum nrf_sdh_state_evt, void *);                                       \
	const TYPE_SECTION_ITERABLE(struct nrf_sdh_state_evt_observer, _observer,                  \
				    nrf_sdh_state_evt_observers, PRIO_LEVEL_ORD(_prio)) = {        \
		.handler = _handler,                                                               \
		.context = _ctx,                                                                   \
	};

/**
 * @brief SoftDevice stack event handler.
 */
typedef void (*nrf_sdh_stack_evt_handler_t)(void *context);

/**
 * @brief SoftDevice stack event observer.
 */
struct nrf_sdh_stack_evt_observer {
	/**
	 * @brief SoftDevice event handler.
	 */
	nrf_sdh_stack_evt_handler_t handler;
	/**
	 * @brief A context parameter to the event handler.
	 */
	void *context;
};

/**
 * @brief Register a SoftDevice stack event observer.
 *
 * A SoftDevice stack event observer receives all events from the SoftDevice. These events can be
 * either BLE or SoC events. If you need to receive BLE or SoC events separately, use
 * @ref NRF_SDH_BLE_OBSERVER or @ref NRF_SDH_SOC_OBSERVER respectively.
 *
 * @param _observer Name of the observer.
 * @param _handler Stack event handler.
 * @param _ctx A context passed to the state request handler.
 * @param _prio Priority of the observer's event handler.
 *		Allowed input: `HIGHEST`, `HIGH`, `USER`, `USER_LOW`, `LOWEST`.
 */
#define NRF_SDH_STACK_EVT_OBSERVER(_observer, _handler, _ctx, _prio)                               \
	PRIO_LEVEL_IS_VALID(_prio);                                                                \
	static void _handler(void *);                                                              \
	const TYPE_SECTION_ITERABLE(struct nrf_sdh_stack_evt_observer, _observer,                  \
				    nrf_sdh_stack_evt_observers, PRIO_LEVEL_ORD(_prio)) = {        \
		.handler = _handler,                                                               \
		.context = _ctx,                                                                   \
	};

/**
 * @brief Enable the SoftDevice.
 *
 * Enable the SoftDevice and send state events to registered observers.
 * An observer may halt the SoftDevice state change by returning non-zero when receiving
 * @ref NRF_SDH_STATE_EVT_ENABLE_PREPARE.
 *
 * @retval 0 On success.
 * @retval -EALREADY The SoftDevice is already enabled.
 * @retval -EBUSY An observer was busy, retry later.
 */
int nrf_sdh_enable_request(void);

/**
 * @brief Disable the SoftDevice.
 *
 * Disable the SoftDevice and send state events to registered observers.
 * An observer may halt the SoftDevice state change by returning non-zero when receiving
 * @ref NRF_SDH_STATE_EVT_DISABLE_PREPARE.
 *
 * @retval 0 On success.
 * @retval -EALREADY The SoftDevice is already disabled.
 * @retval -EBUSY An observer was busy, retry later.
 */
int nrf_sdh_disable_request(void);

/**
 * @brief Function for restarting the SoftDevice Enable/Disable process.
 *
 * Modules which did not acknowledge a @ref NRF_SDH_STATE_REQ_ENABLE or
 * @ref NRF_SDH_STATE_REQ_DISABLE request must call this function to restart the
 * SoftDevice state change process.
 *
 * @retval 0 On success.
 * @retval -EINVAL No state change request was pending.
 */
int nrf_sdh_request_continue(void);

/**
 * @brief Retrieve the SoftDevice state.
 *
 * @retval true If the SoftDevice is enabled.
 * @retval false If the SoftDevice is disabled.
 */
bool nrf_sdh_is_enabled(void);

/**
 * @brief Stop processing SoftDevice events.
 *
 * This function disables the SoftDevice interrupt.
 * To resume re-enable it and resume dispatching events, call @ref nrf_sdh_resume.
 */
void nrf_sdh_suspend(void);

/**
 * @brief Resuming processing SoftDevice events.
 *
 * This function enables the SoftDevice interrupt.
 */
void nrf_sdh_resume(void);

/**
 * @brief Retrieve the module state.
 *
 * @retval  true    The SoftDevice handler is paused, and it will not fetch events from the stack.
 * @retval  false   The SoftDevice handler is running, and it will fetch and dispatch events from
 *                  the stack to the registered stack observers.
 */
bool nrf_sdh_is_suspended(void);

/**
 * @brief Poll the SoftDevice for events.
 *
 * The events are passed to the application using the registered event handlers.
 * This function is called automatically unless @ref NRF_SDH_DISPATCH_MODEL_POLL is selected.
 */
void nrf_sdh_evts_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* NRF_SDH_H__ */

/** @} */
