/**
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup bm_storage_backend NCS Bare Metal Storage library Backend
 * @{
 *
 * @brief Backend for the NCS Bare Metal Storage library.
 */

#ifndef BM_STORAGE_BACKEND_H__
#define BM_STORAGE_BACKEND_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the storage peripheral.
 *
 * This function must be defined by the backend.
 */
uint32_t bm_storage_backend_init(struct bm_storage *storage);
/**
 * @brief Uninitialize the storage peripheral.
 *
 * This function is optional. If not defined in the backend, a weak implementation will return
 * NRF_ERROR_NOT_SUPPORTED.
 */
uint32_t bm_storage_backend_uninit(struct bm_storage *storage);
/**
 * @brief Read data from non-volatile memory.
 *
 * This function must be defined by the backend.
 */
uint32_t bm_storage_backend_read(const struct bm_storage *storage, uint32_t src, void *dest,
				 uint32_t len);
/**
 * @brief Write bytes to non-volatile memory.
 *
 * This function must be defined by the backend.
 */
uint32_t bm_storage_backend_write(const struct bm_storage *storage, uint32_t dest, const void *src,
				  uint32_t len, void *ctx);
/**
 * @brief Erase the non-volatile memory.
 *
 * This function is optional. If not defined in the backend, a weak implementation will return
 * NRF_ERROR_NOT_SUPPORTED.
 */
uint32_t bm_storage_backend_erase(const struct bm_storage *storage, uint32_t addr, uint32_t len,
				  void *ctx);
/**
 * @brief Check if there are any pending operations.
 *
 * This function is optional. If not defined in the backend, a weak implementation will return
 * true.
 */
bool bm_storage_backend_is_busy(const struct bm_storage *storage);

#ifdef __cplusplus
}
#endif

#endif /* BM_STORAGE_BACKEND_H__ */

/** @} */
