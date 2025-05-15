/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup bm_sdh_soc SoC support in SoftDevice Handler
 * @{
 * @ingroup  bm_sdh
 * @brief    Declarations of types and functions required for SoftDevice Handler SoC support.
 */

#ifndef BM_SDH_SOC_H__
#define BM_SDH_SOC_H__

#include <stdint.h>
#include <zephyr/sys/iterable_sections.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SoftDevice SoC event handler.
 */
typedef void (*bm_sdh_soc_evt_handler_t)(uint32_t evt_id, void *context);

/**
 * @brief SoftDevice SoC event observer.
 */
struct bm_sdh_soc_evt_observer {
	/**
	 * @brief SoC event handler.
	 */
	bm_sdh_soc_evt_handler_t handler;
	/**
	 * @brief A parameter to the event handler.
	 */
	void *context;
};

/**
 * @brief Register a SoftDevice SoC event observer.
 *
 * @param _observer Name of the observer.
 * @param _handler State request handler.
 * @param _ctx A context passed to the state request handler.
 * @param _prio Priority of the observer's event handler.
 *		The lower the number, the higher the priority.
 */
#define BM_SDH_SOC_OBSERVER(_observer, _handler, _ctx, _prio)                                      \
	const TYPE_SECTION_ITERABLE(struct bm_sdh_soc_evt_observer, _observer,                     \
				    bm_sdh_soc_evt_observers, _prio) = {                           \
		.handler = _handler,                                                               \
		.context = _ctx,                                                                   \
	};

#ifdef __cplusplus
}
#endif

#endif /* BM_SDH_SOC_H__ */

/** @} */
