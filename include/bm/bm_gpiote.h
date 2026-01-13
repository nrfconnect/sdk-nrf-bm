/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup bm_gpiote NCS Bare Metal GPIOTE library
 * @{
 *
 * @brief GPIOTE management library.
 *
 * @details The bm_gpiote library handles initialization, ISR declaration and IRQ connection of
 *          the available GPIOTE instances. The library allows for multiple users of the GPIOTE
 *          instances and lets the user retrieve the correct GPIOTE instance based on the
 *          GPIO port.
 */

#ifndef BM_GPIOTE_H__
#define BM_GPIOTE_H__

#include <stdint.h>
#include <nrfx_gpiote.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the GPIOTE instance.
 *
 * @param[in] port GPIO port number.
 *
 * @return Pointer to the nrfx_gpiote instance or
 *         @c NULL if an instance was not found for the given port.
 */
nrfx_gpiote_t *bm_gpiote_instance_get(uint8_t port);

#ifdef __cplusplus
}
#endif

#endif /* BM_GPIOTE_H__ */

/** @} */
