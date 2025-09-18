/*
 * Copyright (c) 2018-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrf_error.h>
#include <nrf_strerror.h>
#include <bm_timer.h>
#include <modules/id_manager.h>
#include <modules/auth_status_tracker.h>

LOG_MODULE_DECLARE(peer_manager, CONFIG_PEER_MANAGER_LOG_LEVEL);

/* Assume that waiting interval doubles with each failed authentication. */
#define PAIR_REWARD_TICKS BM_TIMER_MS_TO_TICKS(CONFIG_PM_RA_PROTECTION_REWARD_PERIOD)
#define PENALITY_LVL_TO_PENALITY_MS(_lvl) (CONFIG_PM_RA_PROTECTION_MIN_WAIT_INTERVAL * (1 << _lvl))
#define PENALITY_LVL_TO_PENALITY_TICKS(_lvl) BM_TIMER_MS_TO_TICKS(PENALITY_LVL_TO_PENALITY_MS(_lvl))
#define PENALITY_LVL_NEXT_SET(_lvl)                                                                \
	_lvl = (PENALITY_LVL_TO_PENALITY_MS(_lvl) >= (CONFIG_PM_RA_PROTECTION_MAX_WAIT_INTERVAL))  \
		       ? (_lvl)                                                                    \
		       : (_lvl + 1)

/** @brief Tracked peer state. */
typedef struct {
	/** @brief BLE address, used to identify peer. */
	ble_gap_addr_t peer_addr;
	/**
	 * @brief Accumulated reward ticks, used to decrease penality level after achieving certain
	 *  threshold.
	 */
	uint32_t reward_ticks;
	/**
	 * @brief Accumulated penality ticks, used to determine remaining time
	 *        in which pairing attempts should be rejected.
	 */
	uint32_t penality_ticks;
	/**
	 * @brief Accumulated penality level, used to determine waiting interval
	 *        after failed authorization attempt.
	 */
	uint8_t penality_lvl;
	/**
	 * @brief Flag indicating that the waiting interval for this peer has not
	 *        passed yet.
	 */
	bool is_active;
	/** @brief Flag indicating that this entry is valid in the peer blacklist. */
	bool is_valid;
} blacklisted_peer_t;

static struct bm_timer m_pairing_attempt_timer;
static blacklisted_peer_t m_blacklisted_peers[CONFIG_PM_RA_PROTECTION_TRACKED_PEERS_NUM];
static uint64_t m_ticks_cnt;

/**
 * @brief Function for updating the state of blacklisted peers after timer has been stopped or
 *        timed out.
 *
 * @param[in]  ticks_passed  The number of ticks since the timer has started.
 */
static uint32_t blacklisted_peers_state_update(uint32_t ticks_passed)
{
	uint32_t minimal_ticks = UINT32_MAX;

	for (uint32_t id = 0; id < ARRAY_SIZE(m_blacklisted_peers); id++) {
		blacklisted_peer_t *p_bl_peer = &m_blacklisted_peers[id];

		if (p_bl_peer->is_valid) {
			if (p_bl_peer->is_active) {
				if (p_bl_peer->penality_ticks > ticks_passed) {
					p_bl_peer->penality_ticks -= ticks_passed;
					minimal_ticks =
						MIN(minimal_ticks, p_bl_peer->penality_ticks);
				} else {
					p_bl_peer->is_active = false;

					if (p_bl_peer->penality_lvl == 0) {
						p_bl_peer->is_valid = false;
						LOG_DBG("Peer has been removed from the "
							"blacklist, its address:");
						LOG_HEXDUMP_DBG(p_bl_peer->peer_addr.addr,
							sizeof(p_bl_peer->peer_addr.addr), "");
					} else {
						minimal_ticks =
							MIN(minimal_ticks, PAIR_REWARD_TICKS);
					}

					LOG_DBG("Pairing waiting interval has expired for:");
					LOG_HEXDUMP_DBG(p_bl_peer->peer_addr.addr,
						sizeof(p_bl_peer->peer_addr.addr), "");
				}
			} else {
				if (p_bl_peer->penality_lvl == 0) {
					p_bl_peer->is_valid = false;
					LOG_DBG("Peer has been removed from the blacklist, "
						"its address:");
					LOG_HEXDUMP_DBG(p_bl_peer->peer_addr.addr,
						sizeof(p_bl_peer->peer_addr.addr), "");
				} else {
					p_bl_peer->reward_ticks += ticks_passed;
					if (p_bl_peer->reward_ticks >= PAIR_REWARD_TICKS) {
						p_bl_peer->penality_lvl--;
						p_bl_peer->reward_ticks -= PAIR_REWARD_TICKS;
						LOG_DBG("Peer penality level has decreased "
							"to %d for device:",
							p_bl_peer->penality_lvl);
						LOG_HEXDUMP_DBG(p_bl_peer->peer_addr.addr,
							sizeof(p_bl_peer->peer_addr.addr), "");
					}

					minimal_ticks =
						MIN(minimal_ticks,
						    (PAIR_REWARD_TICKS - p_bl_peer->reward_ticks));
				}
			}
		}
	}

	return minimal_ticks;
}

/**
 * @brief Function for handling state transition of blacklisted peers.
 *
 * @param[in]  context  Context containing the number of ticks since the timer has started.
 */
static void blacklisted_peers_state_transition_handle(void *context)
{
	int err;
	uint32_t minimal_ticks;
	uint32_t ticks_passed = (uint32_t)context;

	minimal_ticks = blacklisted_peers_state_update(ticks_passed);
	m_ticks_cnt = k_uptime_ticks();

	if (minimal_ticks != UINT32_MAX) {
		err = bm_timer_start(&m_pairing_attempt_timer, minimal_ticks,
					   (void *)minimal_ticks);
		if (err) {
			LOG_WRN("bm_timer_start() returned %d", err);
		}
		LOG_DBG("Restarting the timer");
	}
}

uint32_t ast_init(void)
{
	int err_code = bm_timer_init(&m_pairing_attempt_timer, BM_TIMER_MODE_SINGLE_SHOT,
					blacklisted_peers_state_transition_handle);
	if (err_code) {
		return NRF_ERROR_INTERNAL;
	}

	return NRF_SUCCESS;
}

void ast_auth_error_notify(uint16_t conn_handle)
{
	int err;
	uint32_t err_code;
	ble_gap_addr_t peer_addr;
	uint32_t new_timeout;
	uint32_t free_id = ARRAY_SIZE(m_blacklisted_peers);
	bool new_bl_entry = true;

	/* Get the peer address associated with connection handle. */
	err_code = im_ble_addr_get(conn_handle, &peer_addr);
	if (err_code != NRF_SUCCESS) {
		LOG_WRN("im_ble_addr_get() returned %s. conn_handle: %d. "
			"Link was likely disconnected.",
			nrf_strerror_get(err_code), conn_handle);
		return;
	}

	/* Stop the timer and update the state of all blacklisted peers. */
	err = bm_timer_stop(&m_pairing_attempt_timer);
	if (err) {
		LOG_WRN("bm_timer_stop() returned %d", err);
		return;
	}

	new_timeout = blacklisted_peers_state_update((uint32_t)(k_uptime_ticks() - m_ticks_cnt));
	m_ticks_cnt = k_uptime_ticks();

	/* Check if authorization has failed for already blacklisted peer. */
	for (uint32_t id = 0; id < ARRAY_SIZE(m_blacklisted_peers); id++) {
		blacklisted_peer_t *p_bl_peer = &m_blacklisted_peers[id];

		if (p_bl_peer->is_valid) {
			if (memcmp(peer_addr.addr, p_bl_peer->peer_addr.addr, BLE_GAP_ADDR_LEN) ==
			    0) {
				uint8_t lvl = p_bl_peer->penality_lvl;

				PENALITY_LVL_NEXT_SET(lvl);
				p_bl_peer->penality_lvl = lvl;
				p_bl_peer->reward_ticks = 0;
				p_bl_peer->penality_ticks = PENALITY_LVL_TO_PENALITY_TICKS(lvl);

				new_timeout = MIN(new_timeout, p_bl_peer->penality_ticks);

				p_bl_peer->is_active = true;
				new_bl_entry = false;

				LOG_DBG("Pairing waiting interval has been renewed. "
					"Penality level: %d for device:",
					lvl);
				LOG_HEXDUMP_DBG(p_bl_peer->peer_addr.addr,
					sizeof(p_bl_peer->peer_addr.addr), "");
			}
		} else {
			free_id = id;
		}
	}

	/* Add a new peer to the blacklist. */
	if (new_bl_entry) {
		if (free_id < ARRAY_SIZE(m_blacklisted_peers)) {
			blacklisted_peer_t *p_bl_peer = &m_blacklisted_peers[free_id];

			memcpy(&p_bl_peer->peer_addr, &peer_addr, sizeof(peer_addr));

			p_bl_peer->penality_lvl = 0;
			p_bl_peer->reward_ticks = 0;
			p_bl_peer->penality_ticks =
				PENALITY_LVL_TO_PENALITY_TICKS(p_bl_peer->penality_lvl);

			new_timeout = MIN(new_timeout, p_bl_peer->penality_ticks);

			p_bl_peer->is_active = true;
			p_bl_peer->is_valid = true;
			LOG_DBG("New peer has been added to the blacklist:");
			LOG_HEXDUMP_DBG(p_bl_peer->peer_addr.addr,
				sizeof(p_bl_peer->peer_addr.addr), "");
		} else {
			LOG_WRN("No space to blacklist another peer ID");
		}
	}

	/* Restart the timer. */
	if (new_timeout != UINT32_MAX) {
		err = bm_timer_start(&m_pairing_attempt_timer, new_timeout, (void *)new_timeout);
		if (err) {
			LOG_WRN("bm_timer_start() returned %d", err);
		}
	}
}

bool ast_peer_blacklisted(uint16_t conn_handle)
{
	uint32_t err_code;
	ble_gap_addr_t peer_addr;

	err_code = im_ble_addr_get(conn_handle, &peer_addr);
	if (err_code != NRF_SUCCESS) {
		LOG_WRN("im_ble_addr_get() returned %s. conn_handle: %d. "
			"Link was likely disconnected.",
			nrf_strerror_get(err_code), conn_handle);
		return true;
	}

	for (uint32_t id = 0; id < ARRAY_SIZE(m_blacklisted_peers); id++) {
		blacklisted_peer_t *p_bl_peer = &m_blacklisted_peers[id];

		if (p_bl_peer->is_valid) {
			if ((memcmp(peer_addr.addr, p_bl_peer->peer_addr.addr, BLE_GAP_ADDR_LEN) ==
			     0) &&
			    (p_bl_peer->is_active)) {
				return true;
			}
		}
	}

	return false;
}
