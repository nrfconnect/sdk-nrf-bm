/*
 * Copyright (c) 2015 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */


/** @file
 * @defgroup nrf_mtx nRF Mutex
 * @{
 * @ingroup app_common
 * @brief Mutex used for protecting resources.
 *
 * This module provides a mutex that can be used to ensure only one context may enter a critical
 * section holding the lock.
 */
#ifndef NRF_MTX_H__
#define NRF_MTX_H__

#include <stdint.h>
#include <stdbool.h>
#include "nrfx.h"

#define NRF_MTX_LOCKED	 1
#define NRF_MTX_UNLOCKED 0

/**
 * @brief Mutex data type.
 *
 * All fields in this struct are internal, and should never be modified outside of the nrf_mtx_*
 * functions.
 */
typedef nrfx_atomic_t nrf_mtx_t;

/**
 * @brief Initialize mutex.
 *
 * This function _must_ be called before nrf_mtx_trylock() and nrf_mtx_unlock() functions.
 *
 * @param[in, out] p_mtx The mutex to be initialized.
 */
__STATIC_INLINE void nrf_mtx_init(nrf_mtx_t *p_mtx);

/**
 * @brief Destroy mutex.
 *
 * This function can be used in abort scenarios or when the mutex is no longer to be used.
 *
 * @param[in] p_mtx The mutex to be destroy.
 */
__STATIC_INLINE void nrf_mtx_destroy(nrf_mtx_t *p_mtx);

/**
 * @brief Try to lock a mutex.
 *
 * If the mutex is already held by another context, this function will return immediately.
 *
 * @param[in, out] p_mtx The mutex to be locked.
 * @return true if lock was acquired, false if not
 */
__STATIC_INLINE bool nrf_mtx_trylock(nrf_mtx_t *p_mtx);

/**
 * @brief Unlock a mutex.
 *
 * This function _must_ only be called when holding the lock. Unlocking a mutex which you do not
 * hold will give undefined behavior.
 *
 * @note Unlock must happen from the same context as the one used to lock the mutex.
 *
 * @param[in, out] p_mtx The mutex to be unlocked.
 */
__STATIC_INLINE void nrf_mtx_unlock(nrf_mtx_t *p_mtx);

#ifndef SUPPRESS_INLINE_IMPLEMENTATION

__STATIC_INLINE void nrf_mtx_init(nrf_mtx_t *p_mtx)
{
	NRFX_ASSERT(p_mtx != NULL);

	*p_mtx = NRF_MTX_UNLOCKED;
	__DMB();
}

__STATIC_INLINE void nrf_mtx_destroy(nrf_mtx_t *p_mtx)
{
	NRFX_ASSERT(p_mtx != NULL);

	/* Add memory barrier to ensure that any memory operations protected by the mutex complete
	 * before the mutex is destroyed.
	 */
	__DMB();

	*p_mtx = NRF_MTX_UNLOCKED;
}

__STATIC_INLINE bool nrf_mtx_trylock(nrf_mtx_t *p_mtx)
{
	NRFX_ASSERT(p_mtx != NULL);

	uint32_t old_val = NRFX_ATOMIC_FETCH_STORE((void *)p_mtx, NRF_MTX_LOCKED);

	/* Add memory barrier to ensure that the mutex is locked before any memory operations
	 * protected by the mutex are started.
	 */
	__DMB();

	return (old_val == NRF_MTX_UNLOCKED);
}

__STATIC_INLINE void nrf_mtx_unlock(nrf_mtx_t *p_mtx)
{
	NRFX_ASSERT(p_mtx != NULL);
	NRFX_ASSERT(*p_mtx == NRF_MTX_LOCKED);

	/* Add memory barrier to ensure that any memory operations protected by the mutex complete
	 * before the mutex is unlocked.
	 */
	__DMB();

	*p_mtx = NRF_MTX_UNLOCKED;
}

#endif /* SUPPRESS_INLINE_IMPLEMENTATION */

#endif /* NRF_MTX_H__ */
/** @} */
