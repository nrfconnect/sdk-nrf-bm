/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @defgroup nrf_error Global Error Codes
 * @{
 *
 * @brief Global Error definitions
 */

/* Note: When the SoftDevice is enabled we include the nrf_error.h from there. */
#ifdef CONFIG_SOFTDEVICE
#include <softdevice/nrf_error.h>
#else /* CONFIG_SOFTDEVICE */

/* Header guard */
#ifndef NRF_ERROR_H__
#define NRF_ERROR_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup NRF_ERRORS_BASE Error Codes Base number definitions
 * @{
 */

/** Global error base */
#define NRF_ERROR_BASE_NUM (0x0)
/** @} */

/** Successful command */
#define NRF_SUCCESS                           (NRF_ERROR_BASE_NUM + 0)
/** SVC handler is missing */
#define NRF_ERROR_SVC_HANDLER_MISSING         (NRF_ERROR_BASE_NUM + 1)
/** SoftDevice has not been enabled */
#define NRF_ERROR_SOFTDEVICE_NOT_ENABLED      (NRF_ERROR_BASE_NUM + 2)
/** Internal Error */
#define NRF_ERROR_INTERNAL                    (NRF_ERROR_BASE_NUM + 3)
/** No Memory for operation */
#define NRF_ERROR_NO_MEM                      (NRF_ERROR_BASE_NUM + 4)
/** Not found */
#define NRF_ERROR_NOT_FOUND                   (NRF_ERROR_BASE_NUM + 5)
/** Not supported */
#define NRF_ERROR_NOT_SUPPORTED               (NRF_ERROR_BASE_NUM + 6)
/** Invalid Parameter */
#define NRF_ERROR_INVALID_PARAM               (NRF_ERROR_BASE_NUM + 7)
/** Invalid state, operation disallowed in this state */
#define NRF_ERROR_INVALID_STATE               (NRF_ERROR_BASE_NUM + 8)
/** Invalid Length */
#define NRF_ERROR_INVALID_LENGTH              (NRF_ERROR_BASE_NUM + 9)
/** Invalid Flags */
#define NRF_ERROR_INVALID_FLAGS               (NRF_ERROR_BASE_NUM + 10)
/** Invalid Data */
#define NRF_ERROR_INVALID_DATA                (NRF_ERROR_BASE_NUM + 11)
/** Invalid Data size */
#define NRF_ERROR_DATA_SIZE                   (NRF_ERROR_BASE_NUM + 12)
/** Operation timed out */
#define NRF_ERROR_TIMEOUT                     (NRF_ERROR_BASE_NUM + 13)
/** Null Pointer */
#define NRF_ERROR_NULL                        (NRF_ERROR_BASE_NUM + 14)
/** Forbidden Operation */
#define NRF_ERROR_FORBIDDEN                   (NRF_ERROR_BASE_NUM + 15)
/** Bad Memory Address */
#define NRF_ERROR_INVALID_ADDR                (NRF_ERROR_BASE_NUM + 16)
/** Busy */
#define NRF_ERROR_BUSY                        (NRF_ERROR_BASE_NUM + 17)
/** Maximum connection count exceeded. */
#define NRF_ERROR_CONN_COUNT                  (NRF_ERROR_BASE_NUM + 18)
/** Not enough resources for operation */
#define NRF_ERROR_RESOURCES                   (NRF_ERROR_BASE_NUM + 19)

#ifdef __cplusplus
}
#endif
#endif /* NRF_ERROR_H__ */

#endif /* CONFIG_SOFTDEVICE */

/**
 * @}
 */
