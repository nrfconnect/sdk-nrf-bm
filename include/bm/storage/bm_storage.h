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
#include <zephyr/toolchain.h>

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
 * @brief Event dispatch modes.
 */
enum bm_storage_evt_dispatch_mode {
	/* The event was dispatched synchronously. */
	BM_STORAGE_EVT_DISPATCH_MODE_SYNC,
	/* The event was dispatched asynchronously. */
	BM_STORAGE_EVT_DISPATCH_MODE_ASYNC
};

/**
 * @brief Storage event.
 */
struct bm_storage_evt {
	/* Event ID. */
	enum bm_storage_evt_type id;
	/* Specifies if the operation was performed synchronously or asynchronously. */
	enum bm_storage_evt_dispatch_mode dispatch_mode;
	/* Result of the operation.
	 * 0 on success.
	 * A negative errno otherwise.
	 */
	int result;
	/* Destination address where the operation was performed. */
	uint32_t addr;
	/* Pointer to the data that was written to non-volatile memory.
	 * Used by @ref BM_STORAGE_EVT_WRITE_RESULT events.
	 */
	const void *src;
	/* Length (in bytes) of the operation that was performed. */
	size_t len;
	/* User-defined context */
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
	/**
	 * @brief Size of a page (in bytes). A page is the smallest unit that can be erased.
	 */
	uint32_t erase_unit;
	/**
	 * @brief Value used by the implementation-specific backend to represent erased memory.
	 */
	uint32_t erase_value;
	/**
	 * @brief Size of the smallest programmable unit (in bytes).
	 */
	uint32_t program_unit;
	/**
	 * @brief Specifies if the implementation-specific backend does not need erase.
	 */
	bool no_explicit_erase;
};

struct bm_storage;
struct bm_storage_config;

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
 * An instance is bound to an API implementation and contains information about the
 * non-volatile memory, such as the program and erase units, as well as the implementation-specific
 * functionality.
 */
struct bm_storage {
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
	 * @brief The starting address of this instance's partition.
	 */
	uint32_t addr;
	/**
	 * @brief The size of this instance's partition.
	 */
	uint32_t size;
	/**
	 * @brief Instance flags.
	 */
	struct {
		/**
		 * @brief The instance has been initialized.
		 */
		uint8_t is_initialized : 1;
		/**
		 * @brief The instance uses absolute addressing.
		 *
		 * When false, the instance uses relative addressing.
		 */
		uint8_t has_absolute_addressing : 1;
		/**
		 * @brief Automatically pad write operations.
		 */
		uint8_t pad_write_operations : 1;
	} flags;
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
	 * @brief The starting address of this instance's partition.
	 */
	uint32_t addr;
	/**
	 * @brief The size of this instance's partition.
	 */
	uint32_t size;
	/**
	 * @brief The beginning of the partition where this instance can operate.
	 *
	 * Setting this field alongside @ref end_addr implies absolute addressing in the API.
	 *
	 * @deprecated Set bm_storage_config::addr instead.
	 */
	__deprecated uint32_t start_addr;
	/**
	 * @brief The last address (exclusive) of the partition where this instance can operate.
	 *
	 * Setting this field alongside @ref end_addr implies absolute addressing in the API.
	 *
	 * @deprecated Set bm_storage_config::size instead.
	 */
	__deprecated uint32_t end_addr;
	/**
	 * @brief Configuration flags.
	 */
	struct {
		/**
		 * @brief Automatically pad write operations up to the program unit.
		 *
		 * The padding value is the same the contents of the NVM address being written to.
		 */
		uint8_t pad_write_operations : 1;
	} flags;
};

/**
 * @brief Initialize a storage instance.
 *
 * @note This function can be called multiple times on different storage instances in order to
 *       configure each of them separately for initialization.
 *
 * @param[in] storage Storage instance to initialize.
 * @param[in] config Configuration for the storage instance initialization.
 *
 * @retval 0 on success.
 * @retval -EFAULT The storage instance @p storage, @p config or the API are @c NULL.
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
 * @note This function can be called multiple times on different storage instances in order to
 *       configure each of them separately for uninitialization.
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
 * @brief Read data from a storage instance.
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
 * @brief Write data to a storage instance.
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
 * @retval -EBUSY If the implementation-specific backend is busy with an ongoing operation.
 * @retval -EIO If an implementation-specific internal error occurred.
 */
int bm_storage_write(const struct bm_storage *storage, uint32_t dest, const void *src,
		     uint32_t len, void *ctx);

/**
 * @brief Erase data in a storage instance.
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
 * @retval -EBUSY If the implementation-specific backend is busy with an ongoing operation.
 * @retval -EIO If an implementation-specific internal error occurred.
 */
int bm_storage_erase(const struct bm_storage *storage, uint32_t addr, uint32_t len, void *ctx);

/**
 * @brief Query the status of a storage instance.
 *
 * @param[in] storage Storage instance to query the status of.
 *
 * @retval true If the storage instance is busy, or not initialized.
 * @retval false If the storage instance is not busy, or the operation is not supported.
 */
bool bm_storage_is_busy(const struct bm_storage *storage);

#ifdef __cplusplus
}
#endif

#endif /* BM_STORAGE_H__ */

/** @} */
