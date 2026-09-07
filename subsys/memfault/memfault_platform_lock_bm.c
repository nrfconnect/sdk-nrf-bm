/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>

#include "memfault/core/platform/overrides.h"

/*
 * Bare-metal Memfault locking uses irq_lock()/irq_unlock() for memfault lock.
 */

static atomic_t bm_memfault_lock_nesting;
static unsigned int bm_memfault_lock_irq_key;

void memfault_lock(void)
{
	/*
	 * atomic_inc() returns the *previous* value, so a previous value of 0
	 * means this call is the outermost lock and must actually take irq_lock()
	 */
	if (atomic_inc(&bm_memfault_lock_nesting) == 0) {
		bm_memfault_lock_irq_key = irq_lock();
	}
}

void memfault_unlock(void)
{
	__ASSERT(atomic_get(&bm_memfault_lock_nesting) > 0, "Unbalanced memfault_unlock()");

	/*
	 * atomic_dec() returns the *previous* value, so a previous value of 1
	 * means the nesting count just dropped to 0 and irq_unlock() must run
	 */
	if (atomic_dec(&bm_memfault_lock_nesting) == 1) {
		irq_unlock(bm_memfault_lock_irq_key);
	}
}
