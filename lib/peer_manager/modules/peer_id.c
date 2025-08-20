/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */


#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <zephyr/sys/atomic.h>
#include <nrf_bitmask.h>
#include <nrf_error.h>
#include <bluetooth/peer_manager/peer_manager_types.h>
#include <modules/peer_id.h>

#define FLAGS_PER_ELEMENT (sizeof(atomic_t) * CHAR_BIT)
#define FLAGS_ARRAY_LEN(flag_count) (((flag_count - 1) / FLAGS_PER_ELEMENT) + 1)
#define ATOMIC_BITMAP(name) atomic_t name[FLAGS_ARRAY_LEN((PM_PEER_ID_N_AVAILABLE_IDS))]

typedef struct {
	/** Bitmap designating which peer IDs are in use. */
	ATOMIC_BITMAP(used_peer_ids);
	/** Bitmap designating which peer IDs are marked for deletion. */
	ATOMIC_BITMAP(deleted_peer_ids);
} pi_t;

static pi_t m_pi = {{0}, {0}};

static void internal_state_reset(pi_t *p_pi)
{
	memset(p_pi, 0, sizeof(pi_t));
}

void peer_id_init(void)
{
	internal_state_reset(&m_pi);
}

static uint32_t find_and_set_flag(atomic_t *p_flags, uint32_t flag_count)
{
	for (uint32_t i = 0; i < FLAGS_ARRAY_LEN(flag_count); i++) {
		uint32_t word = atomic_get(&p_flags[i]);
		uint32_t inverted = ~word;

		while (inverted) {
			/* Find lowest zero bit */
			uint32_t first_zero = NRF_CTZ(inverted);
			uint32_t first_zero_global = first_zero + (i * 32);

			if (first_zero_global >= flag_count) {
				break;
			}
			if (!atomic_test_and_set_bit(p_flags, first_zero_global)) {
				return first_zero_global;
			}
			inverted &= ~(1u << first_zero);
		}
	}
	return flag_count;
}

static pm_peer_id_t claim(pm_peer_id_t peer_id, atomic_t *p_peer_id_flags)
{
	pm_peer_id_t allocated_peer_id = PM_PEER_ID_INVALID;

	if (peer_id == PM_PEER_ID_INVALID) {
		allocated_peer_id =
			find_and_set_flag(p_peer_id_flags, PM_PEER_ID_N_AVAILABLE_IDS);
		if (allocated_peer_id == PM_PEER_ID_N_AVAILABLE_IDS) {
			allocated_peer_id = PM_PEER_ID_INVALID;
		}
	} else if (peer_id < PM_PEER_ID_N_AVAILABLE_IDS) {
		bool lock_success = !atomic_test_and_set_bit(p_peer_id_flags, peer_id);

		allocated_peer_id = lock_success ? peer_id : PM_PEER_ID_INVALID;
	}
	return allocated_peer_id;
}

static void release(pm_peer_id_t peer_id, atomic_t *p_peer_id_flags)
{
	if (peer_id < PM_PEER_ID_N_AVAILABLE_IDS) {
		atomic_clear_bit(p_peer_id_flags, peer_id);
	}
}

pm_peer_id_t peer_id_allocate(pm_peer_id_t peer_id)
{
	return claim(peer_id, m_pi.used_peer_ids);
}

bool peer_id_delete(pm_peer_id_t peer_id)
{
	pm_peer_id_t deleted_peer_id;

	if (peer_id == PM_PEER_ID_INVALID) {
		return false;
	}

	deleted_peer_id = claim(peer_id, m_pi.deleted_peer_ids);

	return (deleted_peer_id == peer_id);
}

void peer_id_free(pm_peer_id_t peer_id)
{
	release(peer_id, m_pi.used_peer_ids);
	release(peer_id, m_pi.deleted_peer_ids);
}

bool peer_id_is_allocated(pm_peer_id_t peer_id)
{
	if (peer_id < PM_PEER_ID_N_AVAILABLE_IDS) {
		return atomic_test_bit(m_pi.used_peer_ids, peer_id);
	}
	return false;
}

bool peer_id_is_deleted(pm_peer_id_t peer_id)
{
	if (peer_id < PM_PEER_ID_N_AVAILABLE_IDS) {
		return atomic_test_bit(m_pi.deleted_peer_ids, peer_id);
	}
	return false;
}

pm_peer_id_t next_id_get(pm_peer_id_t prev_peer_id, atomic_t *p_peer_id_flags)
{
	pm_peer_id_t i = (prev_peer_id == PM_PEER_ID_INVALID) ? 0 : (prev_peer_id + 1);

	for (; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		if (atomic_test_bit(p_peer_id_flags, i)) {
			return i;
		}
	}

	return PM_PEER_ID_INVALID;
}

pm_peer_id_t peer_id_get_next_used(pm_peer_id_t peer_id)
{
	peer_id = next_id_get(peer_id, m_pi.used_peer_ids);

	while (peer_id != PM_PEER_ID_INVALID) {
		if (!peer_id_is_deleted(peer_id)) {
			return peer_id;
		}

		peer_id = next_id_get(peer_id, m_pi.used_peer_ids);
	}

	return peer_id;
}

pm_peer_id_t peer_id_get_next_deleted(pm_peer_id_t prev_peer_id)
{
	return next_id_get(prev_peer_id, m_pi.deleted_peer_ids);
}

uint32_t peer_id_n_ids(void)
{
	uint32_t n_ids = 0;

	for (pm_peer_id_t i = 0; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		n_ids += atomic_test_bit(m_pi.used_peer_ids, i);
	}

	return n_ids;
}
