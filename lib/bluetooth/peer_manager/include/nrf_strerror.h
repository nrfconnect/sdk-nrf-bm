/*
 * Copyright (c) 2017-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @defgroup nrf_strerror Error code to string converter
 * @ingroup app_common
 *
 * @brief Module for converting error code into a printable string.
 * @{
 */
#ifndef NRF_STRERROR_H__
#define NRF_STRERROR_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function for getting a printable error string.
 *
 * @param code Error code to convert.
 *
 * @note This function cannot fail.
 *       For the function that may fail with error translation, see @ref nrf_strerror_find.
 *
 * @return Pointer to the printable string.
 *         If the string is not found,
 *         it returns a simple string that says that the error is unknown.
 */
char const *nrf_strerror_get(uint32_t code);

/**
 * @brief Function for finding a printable error string.
 *
 * This function gets the error string in the same way as @ref nrf_strerror_get,
 * but if the string is not found, it returns NULL.
 *
 * @param code  Error code to convert.
 * @return      Pointer to the printable string.
 *              If the string is not found, NULL is returned.
 */
char const *nrf_strerror_find(uint32_t code);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* NRF_STRERROR_H__ */
