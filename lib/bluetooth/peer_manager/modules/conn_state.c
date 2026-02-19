/*
 * Copyright (c) 2015 - 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ble.h>
#include <ble_gap.h>
#include <modules/conn_state.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(peer_manager, CONFIG_PEER_MANAGER_LOG_LEVEL);

#define PM_CONN_STATE_DEFAULT_FLAG_COLLECTION_COUNT \
	((sizeof(struct pm_conn_state_flag_collections) / sizeof(atomic_t)) - \
	 CONFIG_PM_CONN_STATE_USER_FLAG_COUNT)

/** The number of flags kept for each connection, including user flags. */
#define TOTAL_FLAG_COLLECTION_COUNT (PM_CONN_STATE_DEFAULT_FLAG_COLLECTION_COUNT + \
				     CONFIG_PM_CONN_STATE_USER_FLAG_COUNT)

/**
 * @brief Structure containing all the flag collections maintained by the Connection State module.
 */
struct pm_conn_state_flag_collections {
	/** Flags indicating which connection handles are valid. */
	atomic_t valid_flags;
	/** Flags indicating which connections are connected, since disconnected connection handles
	 *  will not immediately be invalidated.
	 */
	atomic_t connected_flags;
	/** Flags indicating in which connections the local device is the central. */
	atomic_t central_flags;
	/** Flags indicating which connections are encrypted. */
	atomic_t encrypted_flags;
	/** Flags indicating which connections have encryption with protection from
	 *  man-in-the-middle attacks.
	 */
	atomic_t mitm_protected_flags;
	/** Flags indicating which connections have bonded using LE Secure Connections (LESC). */
	atomic_t lesc_flags;
	/** Flags that can be reserved by the user. The flags will be cleared when a connection is
	 *  invalidated, otherwise, the user is entirely responsible for the flag states.
	 */
	atomic_t user_flags[CONFIG_PM_CONN_STATE_USER_FLAG_COUNT];
};

/** @brief Structure containing the internal state of the Connection State module. */
struct pm_conn_state {
	/** Bitmap for keeping track of which user flags have been acquired. */
	atomic_t acquired_flags;
	union {
		/** Flag collection kept by the Connections State module. */
		struct pm_conn_state_flag_collections flags;
		/** Flag collections as array to allow iterating over all flag collections. */
		atomic_t flag_array[TOTAL_FLAG_COLLECTION_COUNT];
	};
};

/** Instantiation of the internal state. */
static struct pm_conn_state bcs = {0};

static bool pm_conn_state_valid_idx(int idx)
{
	if (idx < 0) {
		return false;
	}

	return atomic_test_bit(&bcs.flags.valid_flags, idx);
}

static void flag_toggle(atomic_t *flags, int idx, bool value)
{
	if (value) {
		atomic_set_bit(flags, idx);
	} else {
		atomic_clear_bit(flags, idx);
	}
}

static bool record_activate(int idx)
{
	if (idx < 0) {
		return false;
	}

	atomic_set_bit(&bcs.flags.connected_flags, idx);
	atomic_set_bit(&bcs.flags.valid_flags, idx);
	return true;
}

static void record_set_disconnected(int idx)
{
	atomic_clear_bit(&bcs.flags.connected_flags, idx);
}

static struct pm_conn_state_conn_handle_list conn_handle_list_get(atomic_t flags);

static void record_purge_disconnected(void)
{
	atomic_t disconnected_flags = ~bcs.flags.connected_flags;
	struct pm_conn_state_conn_handle_list disconnected_list;

	(void)atomic_and(&disconnected_flags, bcs.flags.valid_flags);
	disconnected_list = conn_handle_list_get(disconnected_flags);

	for (uint32_t i = 0; i < disconnected_list.len; i++) {
		/* Invalidate record */
		for (uint32_t j = 0; j < TOTAL_FLAG_COLLECTION_COUNT; j++) {
			atomic_clear_bit(&bcs.flag_array[j],
					 nrf_sdh_ble_idx_get(disconnected_list.conn_handles[i]));
		}
	}
}

static bool user_flag_is_acquired(uint32_t flag_index)
{
	return atomic_test_bit(&bcs.acquired_flags, flag_index);
}

static uint32_t for_each_set_flag(atomic_t flags, pm_conn_state_user_function_t user_function,
				  void *ctx)
{
	uint32_t call_count = 0;

	if (!user_function || !flags) {
		return 0;
	}

	for (uint32_t idx = 0; idx < PM_CONN_STATE_MAX_CONNECTIONS; idx++) {
		if (atomic_test_bit(&flags, idx)) {
			user_function(nrf_sdh_ble_conn_handle_get(idx), ctx);
			call_count += 1;
		}
	}

	return call_count;
}

static struct pm_conn_state_conn_handle_list conn_handle_list_get(atomic_t flags)
{
	uint16_t conn_handle;
	struct pm_conn_state_conn_handle_list conn_handle_list = {
		.len = 0
	};

	if (!flags) {
		/* return empty list */
		return conn_handle_list;
	}

	for (uint32_t i = 0; i < PM_CONN_STATE_MAX_CONNECTIONS; i++) {
		if (atomic_test_bit(&flags, i)) {
			conn_handle = nrf_sdh_ble_conn_handle_get(i);
			conn_handle_list.conn_handles[conn_handle_list.len++] = conn_handle;
		}
	}

	return conn_handle_list;
}

void pm_conn_state_init(void)
{
	memset(&bcs, 0, sizeof(struct pm_conn_state));
}

bool pm_conn_state_valid(uint16_t conn_handle)
{
	int idx = nrf_sdh_ble_idx_get(conn_handle);

	return pm_conn_state_valid_idx(idx);
}

uint8_t pm_conn_state_role(uint16_t conn_handle)
{
	int idx = nrf_sdh_ble_idx_get(conn_handle);
	uint8_t role = BLE_GAP_ROLE_INVALID;

	if (!pm_conn_state_valid_idx(idx)) {
		return BLE_GAP_ROLE_INVALID;
	}

#if defined(CONFIG_SOFTDEVICE_PERIPHERAL) && defined(CONFIG_SOFTDEVICE_CENTRAL)
	bool central;

	central = atomic_test_bit(&bcs.flags.central_flags, idx);
	role = central ? BLE_GAP_ROLE_CENTRAL : BLE_GAP_ROLE_PERIPH;
#elif defined(CONFIG_SOFTDEVICE_CENTRAL)
	role = BLE_GAP_ROLE_CENTRAL;
#else
	role = BLE_GAP_ROLE_PERIPH;
#endif

	return role;
}

enum pm_conn_state_status pm_conn_state_status(uint16_t conn_handle)
{
	int idx = nrf_sdh_ble_idx_get(conn_handle);
	bool connected;
	enum pm_conn_state_status conn_status = PM_CONN_STATUS_INVALID;

	if (!pm_conn_state_valid_idx(idx)) {
		return PM_CONN_STATUS_INVALID;
	}

	connected = atomic_test_bit(&bcs.flags.connected_flags, idx);
	conn_status = connected ? PM_CONN_STATUS_CONNECTED : PM_CONN_STATUS_DISCONNECTED;

	return conn_status;
}

bool pm_conn_state_encrypted(uint16_t conn_handle)
{
	int idx = nrf_sdh_ble_idx_get(conn_handle);

	if (!pm_conn_state_valid_idx(idx)) {
		return false;
	}

	return atomic_test_bit(&bcs.flags.encrypted_flags, idx);
}

bool pm_conn_state_mitm_protected(uint16_t conn_handle)
{
	int idx = nrf_sdh_ble_idx_get(conn_handle);

	if (!pm_conn_state_valid_idx(idx)) {
		return false;
	}

	return atomic_test_bit(&bcs.flags.mitm_protected_flags, idx);
}

bool pm_conn_state_lesc(uint16_t conn_handle)
{
	int idx = nrf_sdh_ble_idx_get(conn_handle);

	if (!pm_conn_state_valid_idx(idx)) {
		return false;
	}

	return atomic_test_bit(&bcs.flags.lesc_flags, idx);
}

struct pm_conn_state_conn_handle_list pm_conn_state_conn_handles(void)
{
	return conn_handle_list_get(bcs.flags.valid_flags);
}

int pm_conn_state_user_flag_acquire(void)
{
	for (int i = 0; i < CONFIG_PM_CONN_STATE_USER_FLAG_COUNT; i++) {
		if (atomic_test_and_set_bit(&bcs.acquired_flags, i) == false) {
			/* Bit was not set before */
			return i;
		}
	}

	return PM_CONN_STATE_USER_FLAG_INVALID;
}

bool pm_conn_state_user_flag_get(uint16_t conn_handle, uint16_t flag_index)
{
	int idx = nrf_sdh_ble_idx_get(conn_handle);

	if (!user_flag_is_acquired(flag_index) || !pm_conn_state_valid_idx(idx)) {
		return false;
	}

	return atomic_test_bit(&bcs.flags.user_flags[flag_index], idx);
}

void pm_conn_state_user_flag_set(uint16_t conn_handle, uint16_t flag_index, bool value)
{
	int idx = nrf_sdh_ble_idx_get(conn_handle);

	if (!user_flag_is_acquired(flag_index) || !pm_conn_state_valid_idx(idx)) {
		return;
	}

	flag_toggle(&bcs.flags.user_flags[flag_index], idx, value);
}

uint32_t pm_conn_state_for_each_set_user_flag(uint16_t flag_index,
					       pm_conn_state_user_function_t user_function,
					       void *ctx)
{
	if (!user_flag_is_acquired(flag_index)) {
		return 0;
	}

	return for_each_set_flag(bcs.flags.user_flags[flag_index], user_function, ctx);
}

static void ble_evt_handler(const ble_evt_t *ble_evt, void *ctx)
{
	int idx = nrf_sdh_ble_idx_get(ble_evt->evt.gap_evt.conn_handle);

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		record_purge_disconnected();

		if (!record_activate(idx)) {
			/* No more records available. Should not happen. */
			LOG_ERR("No more records available");
			__ASSERT(false, "No more records available");
#ifdef BLE_GAP_ROLE_CENTRAL
		} else if (ble_evt->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_CENTRAL) {
			/* Central */
			atomic_set_bit(&bcs.flags.central_flags, idx);
#endif
		} else {
			/* No implementation required. */
		}

		break;
	case BLE_GAP_EVT_DISCONNECTED:
		record_set_disconnected(idx);
		break;
	case BLE_GAP_EVT_CONN_SEC_UPDATE:
		uint8_t sec_lv = ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.lv;

		/* Set/unset flags based on security level. */
		flag_toggle(&bcs.flags.lesc_flags, idx, sec_lv >= 4);
		flag_toggle(&bcs.flags.mitm_protected_flags, idx, sec_lv >= 3);
		flag_toggle(&bcs.flags.encrypted_flags, idx, sec_lv >= 2);
		break;
	case BLE_GAP_EVT_AUTH_STATUS:
		if (ble_evt->evt.gap_evt.params.auth_status.auth_status ==
		    BLE_GAP_SEC_STATUS_SUCCESS) {
			bool lesc = ble_evt->evt.gap_evt.params.auth_status.lesc;

			flag_toggle(&bcs.flags.lesc_flags, idx, lesc);
		}

		break;
	}
}

NRF_SDH_BLE_OBSERVER(ble_evt_observer, ble_evt_handler, NULL, HIGHEST);
