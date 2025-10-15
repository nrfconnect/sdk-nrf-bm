/**
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup bm_storage_backend NCS Bare Metal Storage library backend
 * @{
 *
 * @brief Backend API for the NCS Bare Metal Storage library.
 */

#ifndef BM_STORAGE_BACKEND_H__
#define BM_STORAGE_BACKEND_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the storage peripheral.
 *
 * @note This function must be defined by the backend.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_BUSY If the implementation-specific resource is busy.
 * @retval NRF_ERROR_INTERNAL If an implementation-specific internal error occurred.
 */
uint32_t bm_storage_backend_init(struct bm_storage *storage);
/**
 * @brief Uninitialize the storage peripheral.
 *
 * @note This function is optional. If not defined in the backend, a weak implementation will return
 * NRF_ERROR_NOT_SUPPORTED.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_BUSY If the implementation-specific backend is busy with an ongoing operation.
 * @retval NRF_ERROR_NOT_SUPPORTED If the backend does not support uninitialization.
 */
uint32_t bm_storage_backend_uninit(struct bm_storage *storage);
/**
 * @brief Read data from non-volatile memory.
 *
 * @note This function must be defined by the backend.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_FORBIDDEN If the implementation-specific backend has not been initialized.
 */
uint32_t bm_storage_backend_read(const struct bm_storage *storage, uint32_t src, void *dest,
				 uint32_t len);
/**
 * @brief Write bytes to non-volatile memory.
 *
 * @note This function must be defined by the backend.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_FORBIDDEN If the implementation-specific backend has not been initialized.
 * @retval NRF_ERROR_BUSY If the implementation-specific backend is busy with an ongoing operation.
 * @retval NRF_ERROR_INTERNAL If an implementation-specific internal error occurred.
 */
uint32_t bm_storage_backend_write(const struct bm_storage *storage, uint32_t dest, const void *src,
				  uint32_t len, void *ctx);
/**
 * @brief Erase the non-volatile memory.
 *
 * @note This function is optional. If not defined in the backend, a weak implementation will return
 * NRF_ERROR_NOT_SUPPORTED.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_FORBIDDEN If the implementation-specific backend has not been initialized.
 * @retval NRF_ERROR_BUSY If the implementation-specific backend is busy with an ongoing operation.
 * @retval NRF_ERROR_NOT_SUPPORTED If the backend does not support erase.
 */
uint32_t bm_storage_backend_erase(const struct bm_storage *storage, uint32_t addr, uint32_t len,
				  void *ctx);
/**
 * @brief Check if there are any pending operations.
 *
 * @note This function is optional. If not defined in the backend, a weak implementation will return
 * false.
 *
 * @retval true If the storage instance is busy.
 * @retval false If the storage instance is not busy, or the operation is not supported.
 */
bool bm_storage_backend_is_busy(const struct bm_storage *storage);

#ifdef __cplusplus
}
#endif

#endif /* BM_STORAGE_BACKEND_H__ */

/** @} */
