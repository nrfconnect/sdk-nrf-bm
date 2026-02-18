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
#include <stddef.h>
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
 * @brief Storage event.
 */
struct bm_storage_evt {
	/**
	 * @brief Event identifier.
	 */
	enum bm_storage_evt_type id;
	/**
	 * @brief Whether the event was dispatched asynchronously.
	 */
	bool is_async;
	/**
	 * @brief Result of the operation.
	 *
	 * Zero on success, a negative errno otherwise.
	 */
	int result;
	/**
	 * @brief Address in memory where the operation was performed.
	 *
	 */
	uint32_t addr;
	/**
	 * @brief Pointer to the data written to memory.
	 *
	 * Valid when the event is @ref BM_STORAGE_EVT_WRITE_RESULT.
	 */
	const void *src;
	/**
	 * @brief Length of the operation.
	 */
	size_t len;
	/**
	 * @brief User-defined context.
	 */
	void *ctx;
};

/**
 * @brief Storage event handler type.
 */
typedef void (*bm_storage_evt_handler_t)(struct bm_storage_evt *evt);

/**
 * @brief Information about the non-volatile memory.
 */
struct bm_storage_info {
	/**
	 * @brief Size of the smallest unit of memory that can be programmed, in bytes.
	 */
	uint32_t program_unit;
	/**
	 * @brief Size of the smallest unit of memory that can be erased, in bytes.
	 */
	uint32_t erase_unit;
	/**
	 * @brief Value used to represent erased memory.
	 */
	uint8_t erase_value;
	/**
	 * @brief Whether the hardware requires memory to be erased, before it can be written.
	 */
	bool no_explicit_erase;
};

struct bm_storage;
struct bm_storage_config;

/**
 * @brief Backend API.
 *
 * Provides function pointers for a storage backend implementation.
 * An API instance is assigned during initialization via
 * @ref bm_storage_config.api.
 *
 * @see bm_storage_backends.h for available backend API instances.
 */
struct bm_storage_api {
	int (*init)(struct bm_storage *storage, const struct bm_storage_config *config);
	int (*uninit)(struct bm_storage *storage);
	int (*read)(const struct bm_storage *storage, uint32_t src, void *dest, uint32_t len);
	int (*write)(const struct bm_storage *storage, uint32_t dest, const void *src, uint32_t len,
		     void *ctx);
	int (*erase)(const struct bm_storage *storage, uint32_t addr, uint32_t len, void *ctx);
	bool (*is_busy)(const struct bm_storage *storage);
};

/**
 * @brief Storage instance.
 *
 * An instance is bound to an API implementation (backend) and the partition on which it operates.
 */
struct bm_storage {
	/**
	 * @brief Tells whether the instance is initialized.
	 */
	bool initialized;
	/**
	 * @brief API implementation.
	 */
	const struct bm_storage_api *api;
	/**
	 * @brief Information about the implementation-specific functionality and the non-volatile
	 *        memory peripheral.
	 */
	const struct bm_storage_info *nvm_info;
	/**
	 * @brief The event handler function.
	 */
	bm_storage_evt_handler_t evt_handler;
	/**
	 * @brief The beginning of the non-volatile memory region where this storage instance
	 *        can operate.
	 *        All non-volatile memory operations must be within the boundary delimited by this
	 *        field and @ref end_addr.
	 */
	uint32_t start_addr;
	/**
	 * @brief The last address (exclusive) of non-volatile memory where this storage instance
	 *        can operate.
	 *        All non-volatile memory operations must be within the boundary delimited by this
	 *        field and @ref start_addr.
	 */
	uint32_t end_addr;
};

/**
 * @brief Configuration for storage instance initialization.
 */
struct bm_storage_config {
	/**
	 * @brief The event handler function.
	 *
	 * @note If set to NULL, no events will be sent.
	 */
	bm_storage_evt_handler_t evt_handler;
	/**
	 * @brief API implementation.
	 */
	const struct bm_storage_api *api;
	/**
	 * @brief The beginning of the non-volatile memory region where this storage instance
	 *        can operate.
	 *        All non-volatile memory operations must be within the boundary delimited by this
	 *        field and @ref end_addr.
	 */
	uint32_t start_addr;
	/**
	 * @brief The last address (exclusive) of non-volatile memory where this storage instance
	 *        can operate.
	 *        All non-volatile memory operations must be within the boundary delimited by this
	 *        field and @ref start_addr.
	 */
	uint32_t end_addr;
};

/**
 * @brief Initialize a storage instance.
 *
 * @param[in] storage Storage instance to initialize.
 * @param[in] config Configuration for the storage instance initialization.
 *
 * @retval 0 on success.
 * @retval -EFAULT The storage instance @p storage or @p config is @c NULL.
 * @retval -EIO If an implementation-specific internal error occurred.
 */
int bm_storage_init(struct bm_storage *storage, const struct bm_storage_config *config);

/**
 * @brief Uninitialize a storage instance.
 *
 * Uninitialization prevents an instance from accepting new operations until it is re-initialized.
 * If this instance has any outstanding operations, these will complete as normal and an
 * event will be sent to the instance's event handler.
 *
 * @param[in] storage Storage instance to uninitialize.
 *
 * @retval 0 on success.
 * @retval -EFAULT The storage instance @p storage is @c NULL.
 * @retval -EPERM If @p storage is in an invalid state.
 * @retval -EBUSY If the implementation-specific backend is busy with an ongoing operation.
 * @retval -ENOTSUP If the backend does not support uninitialization.
 */
int bm_storage_uninit(struct bm_storage *storage);

/**
 * @brief Read data from storage.
 *
 * @param[in] storage Storage instance to read data from.
 * @param[in] src Address in non-volatile memory where to read data from.
 * @param[out] dest Destination where the data will be copied to.
 * @param[in] len Length of the data to copy (in bytes).
 *
 * @retval 0 on success.
 * @retval -EFAULT The storage instance @p storage or @p dest is @c NULL.
 * @retval -EPERM The storage instance @p storage is not initialized.
 * @retval -EINVAL If @p len is zero.
 */
int bm_storage_read(const struct bm_storage *storage, uint32_t src, void *dest, uint32_t len);

/**
 * @brief Write data to storage.
 *
 * The write address and length must be a multiple of the backend's program unit.
 *
 * @param[in] storage Storage instance to write data to.
 * @param[in] dest Address in non-volatile memory where to write the data to.
 * @param[in] src Data to be written.
 * @param[in] len Length of the data to be written (in bytes).
 * @param[in] ctx User-defined context sent to the event handler.
 *
 * @retval 0 on success.
 * @retval -EFAULT The storage instance @p storage or @p src is @c NULL.
 * @retval -EPERM The storage instance @p storage is not initialized.
 * @retval -EINVAL The @p dest or @p len parameters are unaligned.
 * @retval -ENOMEM Out of memory to perform the requested operation.
 * @retval -EBUSY The operation could not be accepted at this time.
 * @retval -EIO An internal error has occurred.
 */
int bm_storage_write(const struct bm_storage *storage, uint32_t dest, const void *src,
		     uint32_t len, void *ctx);

/**
 * @brief Erase data from storage.
 *
 * The erase address and length must be a multiple of the backend's erase unit.
 *
 * @param[in] storage Storage instance to erase data in.
 * @param[in] addr Address in non-volatile memory where to erase the data.
 * @param[in] len Length of the data to be erased (in bytes).
 * @param[in] ctx User-defined context sent to the event handler.
 *
 * @retval 0 on success.
 * @retval -EFAULT The storage instance @p storage is @c NULL.
 * @retval -EPERM The storage instance @p storage is not initialized.
 * @retval -EINVAL The @p addr or @p len parameters are unaligned.
 * @retval -ENOMEM Out of memory to perform the requested operation.
 * @retval -EBUSY The operation could not be accepted at this time.
 * @retval -EIO An internal error has occurred.
 */
int bm_storage_erase(const struct bm_storage *storage, uint32_t addr, uint32_t len, void *ctx);

/**
 * @brief Query the status of a storage instance.
 *
 * @param[in] storage Storage instance to query the status of.
 *
 * @retval true The storage instance is busy.
 * @retval false The storage instance is not busy, or it is uninitialized or @c NULL.
 */
bool bm_storage_is_busy(const struct bm_storage *storage);

/**
 * @brief Retrieve NVM storage information.
 *
 * @param[in] storage The storage instance.
 *
 * @return Pointer to the NVM information, or @c NULL if @p storage is
 *         @c NULL or not initialized.
 */
const struct bm_storage_info *bm_storage_nvm_info_get(const struct bm_storage *storage);

#ifdef __cplusplus
}
#endif

#include <bm/storage/bm_storage_backends.h>

#endif /* BM_STORAGE_H__ */

/** @} */
