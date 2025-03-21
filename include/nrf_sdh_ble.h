/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup nrf_sdh_ble BLE support in SoftDevice Handler
 * @{
 * @ingroup  nrf_sdh
 * @brief    Declarations of types and functions required for BLE stack support.
 */

#ifndef NRF_SDH_BLE_H__
#define NRF_SDH_BLE_H__

#include <stdint.h>
#include <ble.h>
#include <zephyr/sys/iterable_sections.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Size of the buffer for a BLE event.
 */
#define NRF_SDH_BLE_EVT_BUF_SIZE BLE_EVT_LEN_MAX(CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE)

/**
 * @brief BLE stack event handler.
 */
typedef void (*nrf_sdh_ble_evt_handler_t)(const ble_evt_t *ble_evt, void *context);

/**
 * @brief BLE event observer.
 */
struct nrf_sdh_ble_evt_observer {
	/**
	 * @brief BLE event handler.
	 */
	nrf_sdh_ble_evt_handler_t handler;
	/**
	 * @brief A parameter to the event handler.
	 */
	void *context;
};

/**
 * @brief Register a SoftDevice BLE event observer.
 *
 * @param _observer Name of the observer.
 * @param _handler State request handler.
 * @param _ctx A context passed to the state request handler.
 * @param _prio Priority of the observer's event handler.
 *		The lower the number, the higher the priority.
 */
#define NRF_SDH_BLE_OBSERVER(_observer, _handler, _ctx, _prio)                                     \
	static const TYPE_SECTION_ITERABLE(struct nrf_sdh_ble_evt_observer, _observer,             \
					   nrf_sdh_ble_evt_observers, _prio) = {                   \
		.handler = _handler,                                                               \
		.context = _ctx,                                                                   \
	};

/**
 * @brief Retrieve the starting address of the application's RAM.
 *
 * @param[out] app_ram_start The starting address of the application's RAM.
 *
 * @retval 0 On success.
 * @retval -EFAULT @p app_ram_start is @c NULL.
 */
int nrf_sdh_ble_app_ram_start_get(uint32_t *app_ram_start);

/**
 * @brief Enable the SoftDevice Bluetooth stack.
 *
 * @param[in] conn_tag Connection configuration tag.
 *
 * @retval 0 On success.
 */
int nrf_sdh_ble_enable(uint8_t conn_cfg_tag);

#ifdef __cplusplus
}
#endif

#endif /* NRF_SDH_BLE_H__ */

/** @} */
