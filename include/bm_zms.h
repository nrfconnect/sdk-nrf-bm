/* Copyright (c) 2018 Laczen
 * Copyright (c) 2024 BayLibre SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * BM_ZMS: Bare Metal Zephyr Memory Storage
 */
#ifndef ZEPHYR_INCLUDE_FS_BM_ZMS_H_
#define ZEPHYR_INCLUDE_FS_BM_ZMS_H_

#include <sys/types.h>
#include <zephyr/sys/atomic.h>
#include <stdbool.h>
#include <bm_storage.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup bm_zms Bare Metal Zephyr Memory Storage (ZMS)
 * @ingroup file_system_storage
 * @{
 * @}
 */

/**
 * @defgroup bm_zms_data_structures BM_ZMS data structures
 * @ingroup bm_zms
 * @{
 */

/** BM_ZMS event IDs. */
typedef enum {
	BM_ZMS_EVT_NONE,  /* Event if an internal error happened before queuing an operation. */
	BM_ZMS_EVT_INIT,  /* Event for @ref bm_zms_init. */
	BM_ZMS_EVT_WRITE, /* Event for @ref bm_zms_write. */
	BM_ZMS_EVT_CLEAR, /* Event for @ref bm_zms_clear. */
} bm_zms_evt_id_t;

/**@brief A BM_ZMS event. */
typedef struct {
	bm_zms_evt_id_t evt_id; /* The event ID. See @ref bm_zms_evt_id_t. */
	uint32_t result;        /* The result of the operation related to this event. */
	uint32_t id;            /* The ID of the entry as specified in the corresponding
				 * write/delete operation.
				 */
} bm_zms_evt_t;

/* Init flags. */
struct bm_zms_init_flags {
	volatile bool initialized;  /* true when the storage is initialized. */
	volatile bool initializing; /* true when initialization is ongoing. */
	bool cb_registred;	    /* true when the user callback is registred. */
} __packed;

/** Zephyr Memory Storage file system structure */
struct bm_zms_fs {
	/** File system offset in flash. */
	off_t offset;
	/** Allocation Table Entry (ATE) write address.
	 * Addresses are stored as `uint64_t`:
	 * - high 4 bytes correspond to the sector.
	 * - low 4 bytes are the offset in the sector.
	 */
	uint64_t ate_wra;
	/** Data write address */
	uint64_t data_wra;
	/** Storage system is split into sectors. The sector size must be a multiple of
	 *  `erase-block-size` if the device has erase capabilities.
	 */
	uint32_t sector_size;
	/** Number of sectors in the file system. */
	uint32_t sector_count;
	/** Current cycle counter of the active sector (pointed to by `ate_wra`). */
	uint8_t sector_cycle;
	/** Flags indicating if the file system is initialized. */
	struct bm_zms_init_flags init_flags;
	/** Size of an Allocation Table Entry. */
	size_t ate_size;
	/** BM Storage instance for asynchronous writes. */
	struct bm_storage zms_bm_storage;
	/** Number of writes currently handled by the storage system. */
	atomic_t ongoing_writes;
	/** The user number that identifies the callback for an event. */
	uint32_t user_num;
#if CONFIG_BM_ZMS_LOOKUP_CACHE
	/** Lookup table used to cache ATE addresses of written IDs. */
	uint64_t lookup_cache[CONFIG_BM_ZMS_LOOKUP_CACHE_SIZE];
#endif
};

/**
 * @}
 */

/**
 * @defgroup bm_zms_high_level_api Bare Metal ZMS API
 * @ingroup bm_zms
 * @{
 */

/**
 *@brief Bare Metal ZMS event handler function prototype.
 *
 * @param p_evt The event.
 */
typedef void (*bm_zms_cb_t)(bm_zms_evt_t const *p_evt);

/**
 * @brief Register a callback to BM_ZMS for handling events.
 *
 * @param cb Pointer to the event handler callback.
 * @param fs Pointer to the file system structure.
 *
 * @retval 0 on success.
 * @retval -ENOMEM if no more callback slots are available.
 * @retval -EINVAL if @p fs or @p cb are NULL.
 */
int bm_zms_register(struct bm_zms_fs *fs, bm_zms_cb_t cb);

/**
 * @brief Mount a BM_ZMS file system.
 *
 * @param fs Pointer to the file system.
 *
 * @retval 0 If the initialization is queued successfully.
 * @retval -ENOMEM if the internal fifo is full.
 * @retval -EBUSY if an initialization is already executing.
 * @retval -EINVAL if any of the sector layout is invalid.
 * @retval -EIO if the backend storage initialization failed.
 */
int bm_zms_mount(struct bm_zms_fs *fs);

/**
 * @brief Clear the BM_ZMS file system from device. The BM_ZMS file system must be re-mounted after
 * this operation.
 *
 * @param fs Pointer to the file system.
 *
 * @retval 0 if the clear operation is queued successfully.
 * @retval -EACCES if @p fs is not mounted.
 * @retval -EIO if there is an internal error.
 */
int bm_zms_clear(struct bm_zms_fs *fs);

/**
 * @brief Write an entry to the file system.
 *
 * @note  When the `len` parameter is equal to `0` the entry is effectively removed (it is
 * equivalent to calling @ref bm_zms_delete()). It is not possible to distinguish between a
 * deleted entry and an entry with data of length 0.
 *
 * @param fs Pointer to the file system.
 * @param id ID of the entry to be written.
 * @param data Pointer to the data to be written.
 * @param len Number of bytes to be written (maximum 64 KiB).
 *
 * @return Number of bytes queued for write. On success, it will be equal to the number of bytes
 *         requested to be written or 0.
 *         On error, returns negative value of error codes defined in `errno.h`.
 * @retval -EACCES if BM_ZMS is still not initialized.
 * @retval -EIO if there is an internal error.
 * @retval -EINVAL if @p len is invalid.
 */
ssize_t bm_zms_write(struct bm_zms_fs *fs, uint32_t id, const void *data, size_t len);

/**
 * @brief Delete an entry from the file system.
 *
 * @param fs Pointer to the file system.
 * @param id ID of the entry to be deleted.
 *
 * @retval 0 if the delete operation is queued.
 * @retval -EACCES if BM_ZMS is still not initialized.
 * @retval -EIO if there is an internal error.
 */
int bm_zms_delete(struct bm_zms_fs *fs, uint32_t id);

/**
 * @brief Read an entry from the file system.
 *
 * @param fs Pointer to the file system.
 * @param id ID of the entry to be read.
 * @param data Pointer to data buffer.
 * @param len Number of bytes to read at most.
 *
 * @return Number of bytes read. On success, it will be equal to the number of bytes requested
 *         to be read or less than that if the stored data has a smaller size than the requested
 *         one.
 *         On error, returns negative value of error codes defined in `errno.h`.
 * @retval Number of bytes read (> 0) on success.
 * @retval -EACCES if BM_ZMS is still not initialized.
 * @retval -EIO if there is a memory read/write error.
 * @retval -ENOENT if there is no entry with the given @p id.
 */
ssize_t bm_zms_read(struct bm_zms_fs *fs, uint32_t id, void *data, size_t len);

/**
 * @brief Read a history entry from the file system.
 *
 * @param fs Pointer to the file system.
 * @param id ID of the entry to be read.
 * @param data Pointer to data buffer.
 * @param len Number of bytes to be read.
 * @param cnt History counter: 0: latest entry, 1: one before latest ...
 *
 * @return Number of bytes read. On success, it will be equal to the number of bytes requested
 *         to be read. When the return value is larger than the number of bytes requested to read
 *         this indicates not all bytes were read, and more data is available.
 *         On error, returns negative value of error codes defined in `errno.h`.
 * @retval Number of bytes read (> 0) on success.
 * @retval -EACCES if BM_ZMS is still not initialized.
 * @retval -EIO if there is a memory read/write error.
 * @retval -ENOENT if there is no entry with the given @p id and history counter.
 */
ssize_t bm_zms_read_hist(struct bm_zms_fs *fs, uint32_t id, void *data, size_t len, uint32_t cnt);

/**
 * @brief Gets the length of the data that is stored in an entry with a given `id`
 *
 * @param fs Pointer to the file system.
 * @param id ID of the entry whose data length to retrieve.
 *
 * @return Data length contained in the ATE. On success, it will be equal to the number of bytes
 *         in the ATE. On error, returns negative value of error codes defined in `errno.h`.
 * @retval Length of the entry with the given @p id (> 0) on success.
 * @retval -EACCES if BM_ZMS is still not initialized.
 * @retval -EIO if there is a memory read/write error.
 * @retval -ENOENT if there is no entry with the given @p id and history counter.
 */
ssize_t bm_zms_get_data_length(struct bm_zms_fs *fs, uint32_t id);

/**
 * @brief Calculate the available free space in the file system.
 *
 * @param fs Pointer to the file system.
 *
 * @return Number of free bytes. On success, it will be equal to the number of bytes that can
 *         still be written to the file system.
 *         Calculating the free space is a time-consuming operation, especially on SPI flash.
 *         On error, returns negative value of error codes defined in `errno.h`.
 * @retval Number of free bytes (>= 0) on success.
 * @retval -EACCES if BM_ZMS is still not initialized.
 * @retval -EIO if there is a memory read/write error.
 */
ssize_t bm_zms_calc_free_space(struct bm_zms_fs *fs);

/**
 * @brief Tells how much contiguous free space remains in the currently active BM_ZMS sector.
 *
 * @param fs Pointer to the file system.
 *
 * @retval Number of free bytes (>= 0) in the current active sector.
 * @retval -EACCES if BM_ZMS is still not initialized.
 */
ssize_t bm_zms_active_sector_free_space(struct bm_zms_fs *fs);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_FS_BM_ZMS_H_ */
