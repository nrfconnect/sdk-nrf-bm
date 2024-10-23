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
#include <zephyr/sys/iterable_sections.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SoftDevice Handler state requests.
 */
enum nrf_sdh_state_req {
	/**
	 * @brief Request to disable the SoftDevice.
	 */
	NRF_SDH_STATE_REQ_DISABLE,
	/**
	 * @brief Request to enable the SoftDevice.
	 */
	NRF_SDH_STATE_REQ_ENABLE,
};

/**
 * @brief SoftDevice Handler state request handler.
 *
 * @retval  true    If ready for the SoftDevice to change state.
 * @retval  false   If not ready for the SoftDevice to change state.
 *                  If false is returned, the state change is aborted.
 */
typedef bool (*nrf_sdh_state_req_handler_t)(enum nrf_sdh_state_req request, void *context);

/**
 * @brief SoftDevice Handler state request observer.
 */
struct nrf_sdh_state_req_observer {
	/**
	 * @brief State request handler.
	 */
	nrf_sdh_state_req_handler_t handler;
	/**
	 * @brief A context parameter for the handler function.
	 */
	void *context;
};

/**
 * @brief Register a SoftDevice state request observer.
 *
 * An observer of SoftDevice state requests receives requests to change the state of the
 * SoftDevice from enabled to disabled and vice versa. These requests may or may not be
 * acknowledged by the observer, depending on the value returned by its request handler function.
 * Thus, a request observer has the capability to defer the change of state of the SoftDevice.
 * If it does so, it has the responsibility to call @ref nrf_sdh_request_continue when it is ready
 * to let the SoftDevice change its state. If such capability is not necessary and you only need
 * to be informed about changes of SoftDevice state, use @ref NRF_SDH_STATE_EVT_OBSERVER instead.
 *
 * @param _observer Name of the observer.
 * @param _handler State request handler.
 * @param _ctx A context passed to the state request handler.
 * @param _prio Priority of the observer's event handler.
 *		The lower the number, the higher the priority.
 */
#define NRF_SDH_STATE_REQ_OBSERVER(_observer, _handler, _ctx, _prio)                               \
	static bool _handler(enum nrf_sdh_state_req, void *);                                      \
	const TYPE_SECTION_ITERABLE(struct nrf_sdh_state_req_observer, _observer,                  \
				    nrf_sdh_state_req_observers, _prio) = {                        \
		.handler = _handler,                                                               \
		.context = _ctx,                                                                   \
	};

/**
 * @brief SoftDevice Handler state events.
 */
enum nrf_sdh_state_evt {
	/**
	 * @brief SoftDevice is going to be enabled.
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
	 */
	NRF_SDH_STATE_EVT_DISABLE_PREPARE,
	/**
	 * @brief SoftDevice is disabled.
	 */
	NRF_SDH_STATE_EVT_DISABLED,
};

/**
 * @brief SoftDevice Handler state event handler.
 */
typedef void (*nrf_sdh_state_evt_handler_t)(enum nrf_sdh_state_evt state, void *context);

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
 * A SoftDevice state observer receives events when the SoftDevice state has changed or is
 * about to change. These events are only meant to inform the state observer, which, contrary
 * to a state request observer, does not have the capability to defer the change of state.
 * If such capability is required, use @ref NRF_SDH_STATE_REQ_OBSERVER instead.
 *
 * @param _observer Name of the observer.
 * @param _handler State request handler.
 * @param _ctx A context passed to the state request handler.
 * @param _prio Priority of the observer's event handler.
 *		The lower the number, the higher the priority.
 */
#define NRF_SDH_STATE_EVT_OBSERVER(_observer, _handler, _ctx, _prio)                               \
	static void _handler(enum nrf_sdh_state_evt, void *);                                      \
	const TYPE_SECTION_ITERABLE(struct nrf_sdh_state_evt_observer, _observer,                  \
				    nrf_sdh_state_evt_observers, _prio) = {                        \
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
 *		The lower the number, the higher the priority.
 */
#define NRF_SDH_STACK_EVT_OBSERVER(_observer, _handler, _ctx, _prio)                               \
	static void _handler(void *);                                                              \
	const TYPE_SECTION_ITERABLE(struct nrf_sdh_stack_evt_observer, _observer,                  \
				    nrf_sdh_stack_evt_observers, _prio) = {                        \
		.handler = _handler,                                                               \
		.context = _ctx,                                                                   \
	};

/**
 * @brief Enable the SoftDevice.
 *
 * This function issues a @ref NRF_SDH_STATE_REQ_ENABLE request to all observers that
 * were registered using the @ref NRF_SDH_STATE_REQ_OBSERVER macro. The observers may or
 * may not acknowledge the request. If all observers acknowledge the request, the
 * SoftDevice is enabled. Otherwise, the process is stopped and the observers
 * that did not acknowledge have the responsibility to restart it by calling
 * @ref nrf_sdh_request_continue when they are ready for the SoftDevice to change state.
 *
 * @retval 0 On success.
 * @retval -EALREADY The SoftDevice is already enabled.
 */
int nrf_sdh_enable_request(void);

/**
 * @brief Disable the SoftDevice.
 *
 * This function issues a @ref NRF_SDH_STATE_REQ_DISABLE request to all observers that
 * were registered using the @ref NRF_SDH_STATE_REQ_OBSERVER macro. The observers may or
 * may not acknowledge the request. If all observers acknowledge the request, the
 * SoftDevice is disabled. Otherwise, the process is stopped and the observers
 * that did not acknowledge have the responsibility to restart it by calling
 * @ref nrf_sdh_request_continue when they are ready for the SoftDevice to change state.
 *
 * @retval 0 On success.
 * @retval -EALREADY The SoftDevice is already disabled.
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
