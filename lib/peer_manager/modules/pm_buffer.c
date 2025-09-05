/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */


#include <stdbool.h>
#include <string.h>
#include <zephyr/sys/atomic.h>
#include <nrf_error.h>
#include <modules/pm_buffer.h>

#define BUFFER_IS_VALID(p_buffer)                                                                  \
	((p_buffer != NULL) && (p_buffer->p_memory != NULL) && (p_buffer->p_mutex != NULL))

static bool mutex_lock(atomic_t *mutex, int index)
{
	return !atomic_test_and_set_bit(mutex, index);
}

static void mutex_unlock(atomic_t *mutex, int index)
{
	atomic_clear_bit(mutex, index);
}

static bool mutex_lock_status_get(atomic_t *mutex, int index)
{
	return atomic_test_bit(mutex, index);
}

uint32_t pm_buffer_init(pm_buffer_t *p_buffer, uint8_t *p_buffer_memory,
			uint32_t buffer_memory_size, atomic_t *p_mutex_memory,
			uint32_t n_blocks, uint32_t block_size)
{
	if ((p_buffer != NULL) && (p_buffer_memory != NULL) && (p_mutex_memory != NULL) &&
	    (buffer_memory_size >= (n_blocks * block_size)) && (n_blocks != 0) &&
	    (block_size != 0)) {
		p_buffer->p_memory = p_buffer_memory;
		p_buffer->p_mutex = p_mutex_memory;
		p_buffer->n_blocks = n_blocks;
		p_buffer->block_size = block_size;

		return NRF_SUCCESS;
	} else {
		return NRF_ERROR_INVALID_PARAM;
	}
}

uint8_t pm_buffer_block_acquire(pm_buffer_t *p_buffer, uint32_t n_blocks)
{
	if (!BUFFER_IS_VALID(p_buffer)) {
		return PM_BUFFER_INVALID_ID;
	}

	uint8_t first_locked_mutex = PM_BUFFER_INVALID_ID;

	for (uint8_t i = 0; i < p_buffer->n_blocks; i++) {
		if (mutex_lock(p_buffer->p_mutex, i)) {
			if (first_locked_mutex == PM_BUFFER_INVALID_ID) {
				first_locked_mutex = i;
			}
			if ((i - first_locked_mutex + 1U) == n_blocks) {
				return first_locked_mutex;
			}
		} else if (first_locked_mutex != PM_BUFFER_INVALID_ID) {
			for (uint8_t j = first_locked_mutex; j < i; j++) {
				pm_buffer_release(p_buffer, j);
			}
			first_locked_mutex = PM_BUFFER_INVALID_ID;
		}
	}

	return (PM_BUFFER_INVALID_ID);
}

uint8_t *pm_buffer_ptr_get(pm_buffer_t *p_buffer, uint8_t id)
{
	if (!BUFFER_IS_VALID(p_buffer)) {
		return NULL;
	}

	if ((id != PM_BUFFER_INVALID_ID) && mutex_lock_status_get(p_buffer->p_mutex, id)) {
		return &p_buffer->p_memory[id * p_buffer->block_size];
	} else {
		return NULL;
	}
}

void pm_buffer_release(pm_buffer_t *p_buffer, uint8_t id)
{
	if (BUFFER_IS_VALID(p_buffer) && (id != PM_BUFFER_INVALID_ID) &&
	    mutex_lock_status_get(p_buffer->p_mutex, id)) {
		mutex_unlock(p_buffer->p_mutex, id);
	}
}
