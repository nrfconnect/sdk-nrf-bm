/**
 * Copyright (c) 2016-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup bm_storage NCS Bare Metal Storage library
 * @{
 *
 * @brief Library that provides abstractions for operations such as read, write, and erase
 *        on non-volatile memory.
 */

#ifndef BM_STORAGE_H__
#define BM_STORAGE_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Event IDs.
 */
enum bm_storage_evt_type {
	/* Event for @ref bm_storage_write. */
	BM_STORAGE_EVT_WRITE_RESULT,
	/* Event for @ref bm_storage_erase. */
	BM_STORAGE_EVT_ERASE_RESULT
};

/**
 * @brief Event dispatch types.
 */
enum bm_storage_evt_dispatch_type {
	/* The event was dispatched synchronously. */
	BM_STORAGE_EVT_DISPATCH_SYNC,
	/* The event was dispatched asynchronously. */
	BM_STORAGE_EVT_DISPATCH_ASYNC
};

/**
 * @brief Storage event.
 */
struct bm_storage_evt {
	/* Event ID. */
	enum bm_storage_evt_type id;
	/* Specifies if the operation was performed synchronously or asynchronously. */
	enum bm_storage_evt_dispatch_type dispatch_type;
	/* Result of the operation.
	 * NRF_SUCCESS on success.
	 * A positive NRF error otherwise.
	 */
	uint32_t result;
	/* Destination address where the operation was performed. */
	uint32_t addr;
	/* Pointer to the data that was written to non-volatile memory.
	 * Used by @ref BM_STORAGE_EVT_WRITE_RESULT events.
	 */
	void const *src;
	/* Length (in bytes) of the operation that was performed. */
	size_t len;
	/* Pointer to user data, passed to the implementation-specific API function call. */
	void *ctx;
};

/**
 * @brief Storage event handler type.
 */
typedef void (*bm_storage_evt_handler_t)(struct bm_storage_evt *evt);

/**
 * @brief Information about the implementation-specific non-volatile memory.
 */
struct bm_storage_info {
	/* Size of a page (in bytes). A page is the smallest unit that can be erased. */
	uint32_t erase_unit;
	/* Value used by the implementation-specific backend to represent erased memory. */
	uint32_t erase_value;
	/* Size of the smallest programmable unit (in bytes). */
	uint32_t program_unit;
	/* Specifies if the implementation-specific backend does not need erase. */
	bool no_explicit_erase;
};

/**
 * @brief Storage instance.
 *
 * An instance is bound to an API implementation and contains information about the
 * non-volatile memory, such as the program and erase units, as well as the implementation-specific
 * functionality.
 */
struct bm_storage {
	/**
	 * @brief Tells whether the instance is initialized.
	 *
	 * @note This field must not be set manually.
	 */
	bool initialized;
	/**
	 * @brief Information about the implementation-specific functionality and the non-volatile
	 *        memory peripheral.
	 *
	 * @note This field must not be set manually.
	 */
	const struct bm_storage_info *nvm_info;
	/**
	 * @brief The event handler function.
	 *
	 * @note If set to NULL, no events will be sent.
	 *       This field must be set manually.
	 */
	bm_storage_evt_handler_t evt_handler;
	/**
	 * @brief The beginning of the non-volatile memory region where this storage instance
	 *        can operate.
	 *        All non-volatile memory operations must be within the boundary delimited by this
	 *        field and @ref end_addr.
	 *
	 * @note This field must be set manually.
	 */
	uint32_t start_addr;
	/**
	 * @brief The last address (exclusive) of non-volatile memory where this storage instance
	 *        can operate.
	 *        All non-volatile memory operations must be within the boundary delimited by this
	 *        field and @ref start_addr.
	 *
	 * @note This field must be set manually.
	 */
	uint32_t end_addr;
};

/**
 * @brief Initialize a storage instance.
 *
 * @note This function can be called multiple times on different storage instances in order to
 *       configure each of them separately for initialization.
 *
 * @param[in] storage Storage instance to initialize.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_NULL If @p storage is @c NULL.
 * @retval NRF_ERROR_BUSY If the implementation-specific resource is busy.
 * @retval NRF_ERROR_INTERNAL If an implementation-specific internal error occurred.
 */
uint32_t bm_storage_init(struct bm_storage *storage);

/**
 * @brief Uninitialize a storage instance.
 *
 * @note This function can be called multiple times on different storage instances in order to
 *       configure each of them separately for uninitialization.
 *
 * @param[in] storage Storage instance to uninitialize.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_NULL If @p storage is @c NULL.
 * @retval NRF_ERROR_INVALID_STATE If @p storage is in an invalid state.
 * @retval NRF_ERROR_BUSY If the implementation-specific backend is busy with an ongoing operation.
 * @retval NRF_ERROR_NOT_SUPPORTED If the backend does not support uninitialization.
 */
uint32_t bm_storage_uninit(struct bm_storage *storage);

/**
 * @brief Read data from a storage instance.
 *
 * @param[in] storage Storage instance to read data from.
 * @param[in] src Address in non-volatile memory where to read data from.
 * @param[out] dest Destination where the data will be copied to.
 * @param[in] len Length of the data to copy (in bytes).
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_NULL If @p storage is @c NULL.
 * @retval NRF_ERROR_INVALID_STATE If @p storage is in an invalid state.
 * @retval NRF_ERROR_INVALID_LENGTH If @p len is zero or not a multiple of
 *                                  @ref bm_storage_info.program_unit.
 * @retval NRF_ERROR_INVALID_ADDR If @p dest or @p src are not 32-bit word aligned, or if they are
 *                                outside the bounds of the memory region configured in @p storage.
 * @retval NRF_ERROR_FORBIDDEN If the implementation-specific backend has not been initialized.
 */
uint32_t bm_storage_read(const struct bm_storage *storage, uint32_t src, void *dest, uint32_t len);

/**
 * @brief Write data to a storage instance.
 *
 * @param[in] storage Storage instance to write data to.
 * @param[in] dest Address in non-volatile memory where to write the data to.
 * @param[in] src Data to be written.
 * @param[in] len Length of the data to be written (in bytes).
 * @param[in] ctx Pointer to user data, passed to the implementation-specific API function call.
 *                Can be NULL.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_NULL If @p storage is @c NULL.
 * @retval NRF_ERROR_INVALID_STATE If @p storage is in an invalid state.
 * @retval NRF_ERROR_INVALID_LENGTH If @p len is zero or not a multiple of
 *                                  @ref bm_storage_info.program_unit.
 * @retval NRF_ERROR_INVALID_ADDR If @p dest or @p src are not 32-bit word aligned, or if they are
 *                                outside the bounds of the memory region configured in @p storage.
 * @retval NRF_ERROR_FORBIDDEN If the implementation-specific backend has not been initialized.
 * @retval NRF_ERROR_BUSY If the implementation-specific backend is busy with an ongoing operation.
 * @retval NRF_ERROR_INTERNAL If an implementation-specific internal error occurred.
 */
uint32_t bm_storage_write(const struct bm_storage *storage, uint32_t dest, const void *src,
			  uint32_t len, void *ctx);

/**
 * @brief Erase data in a storage instance.
 *
 * @param[in] storage Storage instance to erase data in.
 * @param[in] ctx Pointer to user data, passed to the implementation-specific API function call.
 *                Can be NULL.
 *
 * @retval NRF_SUCCESS on success.
 * @retval NRF_ERROR_NULL If @p storage is @c NULL.
 * @retval NRF_ERROR_INVALID_STATE If @p storage is in an invalid state.
 * @retval NRF_ERROR_INVALID_LENGTH If @p len is zero or not a multiple of
 *                                  @ref bm_storage_info.erase_unit.
 * @retval NRF_ERROR_INVALID_ADDR If @p addr is outside the bounds of the memory region configured
 *                                in @p storage.
 * @retval NRF_ERROR_FORBIDDEN If the implementation-specific backend has not been initialized.
 * @retval NRF_ERROR_BUSY If the implementation-specific backend is busy with an ongoing operation.
 * @retval NRF_ERROR_NOT_SUPPORTED If the implementation-specific backend does not implement this
 *                                 function.
 */
uint32_t bm_storage_erase(const struct bm_storage *storage, uint32_t addr, uint32_t len, void *ctx);

/**
 * @brief Query the status of a storage instance.
 *
 * @param[in] storage Storage instance to query the status of.
 *
 * @retval true If the storage instance is busy, or not initialized.
 * @retval false If the storage instance is not busy, or the operation is not supported.
 */
bool bm_storage_is_busy(const struct bm_storage *storage);

/**
 * @brief Singleton instance of the implementation-specific non-volatile memory information.
 */
extern const struct bm_storage_info bm_storage_info;

#ifdef __cplusplus
}
#endif

#endif /* BM_STORAGE_H__ */

/** @} */
