/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @defgroup pm_buffer Buffer
 * @ingroup peer_manager
 * @{
 * @brief An internal module of @ref peer_manager. This module provides a simple buffer.
 */

#ifndef BUFFER_H__
#define BUFFER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Invalid buffer block ID. */
#define PM_BUFFER_INVALID_ID 0xFF

/**
 * @brief Convenience macro for declaring memory and initializing a buffer instance.
 *
 * @param[out] p_buffer    The buffer instance to initialize.
 * @param[in]  n_blocks    The desired number of blocks in the buffer.
 * @param[in]  block_size  The desired block size of the buffer.
 * @param[out] err_code    The return code from @ref pm_buffer_init.
 */
#define PM_BUFFER_INIT(p_buffer, n_blocks, block_size, err_code)                                   \
	do {                                                                                       \
		__ALIGN(4) static uint8_t buffer_memory[(n_blocks) * (block_size)];                \
		static atomic_t mutex_memory[(n_blocks - 1) / (sizeof(atomic_t) * 8) + 1];         \
		err_code = pm_buffer_init((p_buffer), buffer_memory, (n_blocks) * (block_size),    \
					  mutex_memory, (n_blocks), (block_size));                 \
	} while (0)

typedef struct {
	/**
	 * @brief The storage for all buffer entries. The size of the buffer must be
	 *        n_blocks*block_size.
	 */
	uint8_t *p_memory;
	/** @brief A mutex group with one mutex for each buffer entry. */
	atomic_t *p_mutex;
	/** @brief The number of allocatable blocks in the buffer. */
	uint32_t n_blocks;
	/** @brief The size of each block in the buffer. */
	uint32_t block_size;
} pm_buffer_t;

/**
 * @brief Function for initializing a buffer instance.
 *
 * @param[out] p_buffer            The buffer instance to initialize.
 * @param[in]  p_buffer_memory     The memory this buffer will use.
 * @param[in]  buffer_memory_size  The size of p_buffer_memory. This must be at least
 *                                 n_blocks*block_size.
 * @param[in]  p_mutex_memory      The memory for the mutexes. This must be at least
 *                                 @ref NRF_ATFLAGS_ARRAY_LEN(n_blocks).
 * @param[in]  n_blocks            The number of blocks in the buffer.
 * @param[in]  block_size          The size of each block.
 *
 * @retval NRF_SUCCESS              Successfully initialized buffer instance.
 * @retval NRF_ERROR_INVALID_PARAM  A parameter was 0 or NULL or a size was too small.
 */
uint32_t pm_buffer_init(pm_buffer_t *p_buffer, uint8_t *p_buffer_memory,
			  uint32_t buffer_memory_size, atomic_t *p_mutex_memory,
			  uint32_t n_blocks, uint32_t block_size);

/**
 * @brief Function for acquiring a buffer block in a buffer.
 *
 * @param[in]  p_buffer  The buffer instance acquire from.
 * @param[in]  n_blocks  The number of contiguous blocks to acquire.
 *
 * @return The id of the acquired block, if successful.
 * @retval PM_BUFFER_INVALID_ID  If unsuccessful.
 */
uint8_t pm_buffer_block_acquire(pm_buffer_t *p_buffer, uint32_t n_blocks);

/**
 * @brief Function for getting a pointer to a specific buffer block.
 *
 * @param[in]  p_buffer  The buffer instance get from.
 * @param[in]  id        The id of the buffer to get the pointer for.
 *
 * @return A pointer to the buffer for the specified id, if the id is valid.
 * @retval NULL  If the id is invalid.
 */
uint8_t *pm_buffer_ptr_get(pm_buffer_t *p_buffer, uint8_t id);

/**
 * @brief Function for releasing a buffer block.
 *
 * @param[in]  p_buffer  The buffer instance containing the block to release.
 * @param[in]  id        The id of the block to release.
 */
void pm_buffer_release(pm_buffer_t *p_buffer, uint8_t id);

#ifdef __cplusplus
}
#endif

#endif /* BUFFER_H__ */

/** @} */
