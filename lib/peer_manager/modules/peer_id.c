/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */


#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <zephyr/sys/atomic.h>
#include <nrf_error.h>
#include <bluetooth/peer_manager/peer_manager_types.h>
#include <modules/peer_id.h>

#define BITS_SIZEOF(type) (sizeof(type) * CHAR_BIT)
#define ATOMIC_BITMAP(name)                                                                        \
	atomic_t name[(PM_PEER_ID_N_AVAILABLE_IDS - 1) / BITS_SIZEOF(atomic_t) + 1]                \

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

static pm_peer_id_t claim(pm_peer_id_t peer_id, atomic_t *p_peer_id_flags)
{
	pm_peer_id_t allocated_peer_id = PM_PEER_ID_INVALID;

	if (peer_id == PM_PEER_ID_INVALID) {
		allocated_peer_id =
//todo->			nrf_atflags_find_and_set_flag(p_peer_id_flags, PM_PEER_ID_N_AVAILABLE_IDS);
		if (allocated_peer_id == PM_PEER_ID_N_AVAILABLE_IDS) {
			allocated_peer_id = PM_PEER_ID_INVALID;
		}
	} else if (peer_id < PM_PEER_ID_N_AVAILABLE_IDS) {
		bool lock_success = !nrf_atflags_fetch_set(p_peer_id_flags, peer_id);

		allocated_peer_id = lock_success ? peer_id : PM_PEER_ID_INVALID;
	}
	return allocated_peer_id;
}

static void release(pm_peer_id_t peer_id, atomic_t *p_peer_id_flags)
{
	if (peer_id < PM_PEER_ID_N_AVAILABLE_IDS) {
		nrf_atflags_clear(p_peer_id_flags, peer_id);
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
		return nrf_atflags_get(m_pi.used_peer_ids, peer_id);
	}
	return false;
}

bool peer_id_is_deleted(pm_peer_id_t peer_id)
{
	if (peer_id < PM_PEER_ID_N_AVAILABLE_IDS) {
		return nrf_atflags_get(m_pi.deleted_peer_ids, peer_id);
	}
	return false;
}

pm_peer_id_t next_id_get(pm_peer_id_t prev_peer_id, atomic_t *p_peer_id_flags)
{
	pm_peer_id_t i = (prev_peer_id == PM_PEER_ID_INVALID) ? 0 : (prev_peer_id + 1);

	for (; i < PM_PEER_ID_N_AVAILABLE_IDS; i++) {
		if (nrf_atflags_get(p_peer_id_flags, i)) {
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
		n_ids += nrf_atflags_get(m_pi.used_peer_ids, i);
	}

	return n_ids;
}
