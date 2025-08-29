/*
 * Copyright (c) 2015 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <ble_conn_state.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ble.h>
#include <ble_gap.h>
#include <nrf_sdh_ble.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_bms, CONFIG_BLE_CONN_STATE_LOG_LEVEL);

/** The number of flags kept for each connection, including user flags. */
#define TOTAL_FLAG_COLLECTION_COUNT (CONFIG_BLE_CONN_STATE_DEFAULT_FLAG_COLLECTION_COUNT + \
				     CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT)

/** Connection handles in the same order as they are indexed in the atomics */
struct ble_conn_state_conn_handle_list connections;

/**
 * @brief Structure containing all the flag collections maintained by the Connection State module.
 */
struct ble_conn_state_flag_collections {
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
	 *  invalidated, otherwise, the user is wholly responsible for the flag states.
	 */
	atomic_t user_flags[CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT];
};

/** @brief Structure containing the internal state of the Connection State module. */
struct ble_conn_state {
	/** Bitmap for keeping track of which user flags have been acquired. */
	atomic_t acquired_flags;
	union {
		/** Flag collection kept by the Connections State module. */
		struct ble_conn_state_flag_collections flags;
		/** Flag collections as array to allow iterating over all flag collections. */
		atomic_t flag_array[TOTAL_FLAG_COLLECTION_COUNT];
	};
};

/** Instantiation of the internal state. */
static struct ble_conn_state bcs = {0};

static int bcs_atomic_find_and_set_flag(atomic_t *flags)
{
	for (int i = 0; i < (sizeof(atomic_t) * 8); i++) {
		if (atomic_test_and_set_bit(flags, i) == false) {
			/* Bit was not set before */
			return i;
		}
	}

	return -1;
}

/** @brief Function for resetting all internal memory to the values it had at initialization. */
static inline void bcs_internal_state_reset(void)
{
	memset(&bcs, 0, sizeof(struct ble_conn_state));
}

static struct ble_conn_state_conn_handle_list conn_handle_list_get(atomic_t flags)
{
	struct ble_conn_state_conn_handle_list conn_handle_list = {
		.len = 0
	};

	if (flags) {
		for (uint32_t i = 0; i < BLE_CONN_STATE_MAX_CONNECTIONS; i++) {
			if (atomic_test_bit(&flags, i)) {
				conn_handle_list.conn_handles[conn_handle_list.len++] = i;
			}
		}
	}

	return conn_handle_list;
}

static uint32_t active_flag_count(atomic_t flags)
{
	uint32_t set_flag_count = 0;

	for (uint32_t i = 0; i < BLE_CONN_STATE_MAX_CONNECTIONS; i++) {
		if (atomic_test_bit(&flags, i)) {
			set_flag_count += 1;
		}
	}

	return set_flag_count;
}

/**
 * @brief Function for activating a connection record.
 *
 * @param conn_handle  The connection handle to copy into the record.
 *
 * @return whether the record was activated successfully.
 */
static bool record_activate(uint16_t conn_handle)
{
	if (conn_handle >= BLE_CONN_STATE_MAX_CONNECTIONS) {
		return false;
	}

	atomic_set_bit(&bcs.flags.connected_flags, conn_handle);
	atomic_set_bit(&bcs.flags.valid_flags, conn_handle);
	return true;
}

/**
 * @brief Function for marking a connection record as invalid and resetting the values.
 *
 * @param conn_handle  The connection to invalidate.
 */
static void record_invalidate(uint16_t conn_handle)
{
	for (uint32_t i = 0; i < TOTAL_FLAG_COLLECTION_COUNT; i++) {
		atomic_clear_bit(&bcs.flag_array[i], conn_handle);
	}
}

/**
 * @brief Function for marking a connection as disconnected. See @ref BLE_CONN_STATUS_DISCONNECTED.
 *
 * @param conn_handle The connection to set as disconnected.
 */
static void record_set_disconnected(uint16_t conn_handle)
{
	atomic_clear_bit(&bcs.flags.connected_flags, conn_handle);
}

/**
 * @brief Function for invalidating records with a @ref BLE_CONN_STATUS_DISCONNECTED
 *        connection status
 */
static void record_purge_disconnected(void)
{
	atomic_t disconnected_flags = ~bcs.flags.connected_flags;
	struct ble_conn_state_conn_handle_list disconnected_list;

	(void)atomic_and(&disconnected_flags, bcs.flags.valid_flags);
	disconnected_list = conn_handle_list_get(disconnected_flags);

	for (uint32_t i = 0; i < disconnected_list.len; i++) {
		record_invalidate(disconnected_list.conn_handles[i]);
	}
}

/**
 * @brief Function for checking if a user flag has been acquired.
 *
 * @param[in] flag_index Which flag to check.
 *
 * @return  Whether the flag has been acquired.
 */
static bool user_flag_is_acquired(uint16_t flag_index)
{
	return atomic_test_bit(&bcs.acquired_flags, flag_index);
}

void ble_conn_state_init(void)
{
	bcs_internal_state_reset();
}

static void flag_toggle(atomic_t *flags, uint16_t conn_handle, bool value)
{
	if (value) {
		atomic_set_bit(flags, conn_handle);
	} else {
		atomic_clear_bit(flags, conn_handle);
	}
}

int conn_id_get(uint8_t conn_idx)
{
	return connections.conn_handles[conn_idx];
}

int conn_idx_get(uint32_t conn_id)
{
	for (int i = 0; i < connections.len; i++) {
		if (connections.conn_handles[i] == conn_id) {
			return i;
		}
	}

	/* conn_id not found in list */

	for (int i = 0; i < BLE_CONN_STATE_MAX_CONNECTIONS; i++) {
		if (atomic_test_bit(&bcs.flags.connected_flags, i) == false) {
			record_invalidate(i);
			connections.conn_handles[i] = conn_id;
			return i;
		}
	}

	return -1;
}

/**
 * @brief Function for handling BLE events.
 *
 * @param[in] ble_evt Event received from the BLE stack.
 * @param[in] ctx Context.
 */
static void ble_evt_handler(ble_evt_t const *ble_evt, void *ctx)
{
	uint16_t conn_handle = ble_evt->evt.gap_evt.conn_handle;

	switch (ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		record_purge_disconnected();

		if (!record_activate(conn_handle)) {
			/* No more records available. Should not happen. */
			LOG_ERR("No more records available");
			__ASSERT(false, "No more records available");
#ifdef CONFIG_BLE_GAP_ROLE_CENTRAL
		} else if (ble_evt->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_CENTRAL) {
			/* Central */
			atomic_set_bit(&bcs.flags.central_flags, conn_handle);
#endif
		} else {
			/* No implementation required. */
		}

		break;
	case BLE_GAP_EVT_DISCONNECTED:
		record_set_disconnected(conn_handle);
		break;
	case BLE_GAP_EVT_CONN_SEC_UPDATE:
		uint8_t sec_lv = ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.lv;

		/* Set/unset flags based on security level. */
		flag_toggle(&bcs.flags.lesc_flags, conn_handle, sec_lv >= 4);
		flag_toggle(&bcs.flags.mitm_protected_flags, conn_handle, sec_lv >= 3);
		flag_toggle(&bcs.flags.encrypted_flags, conn_handle, sec_lv >= 2);
		break;
	case BLE_GAP_EVT_AUTH_STATUS:
		if (ble_evt->evt.gap_evt.params.auth_status.auth_status ==
		    BLE_GAP_SEC_STATUS_SUCCESS) {
			bool lesc = ble_evt->evt.gap_evt.params.auth_status.lesc;

			flag_toggle(&bcs.flags.lesc_flags, conn_handle, lesc);
		}

		break;
	}
}

NRF_SDH_BLE_OBSERVER(ble_evt_observer, ble_evt_handler, NULL, BLE_CONN_STATE_BLE_OBSERVER_PRIO);

bool ble_conn_state_valid(uint16_t conn_handle)
{
	if (conn_handle >= BLE_CONN_STATE_MAX_CONNECTIONS) {
		return false;
	}

	return atomic_test_bit(&bcs.flags.valid_flags, conn_handle);
}

uint8_t ble_conn_state_role(uint16_t conn_handle)
{
	uint8_t role = BLE_GAP_ROLE_INVALID;

	if (ble_conn_state_valid(conn_handle)) {
#if defined(CONFIG_SOFTDEVICE_PERIPHERAL) && defined(CONFIG_SOFTDEVICE_CENTRAL)
		bool central;

		central = atomic_test_bit(&bcs.flags.central_flags, conn_handle);
		role = central ? BLE_GAP_ROLE_CENTRAL : BLE_GAP_ROLE_PERIPH;
#elif defined(CONFIG_SOFTDEVICE_CENTRAL)
		role = BLE_GAP_ROLE_CENTRAL;
#else
		role = BLE_GAP_ROLE_PERIPH;
#endif
	}

	return role;
}

enum ble_conn_state_status ble_conn_state_status(uint16_t conn_handle)
{
	bool connected;
	enum ble_conn_state_status conn_status = BLE_CONN_STATUS_INVALID;

	if (ble_conn_state_valid(conn_handle)) {
		connected = atomic_test_bit(&bcs.flags.connected_flags, conn_handle);
		conn_status = connected ? BLE_CONN_STATUS_CONNECTED : BLE_CONN_STATUS_DISCONNECTED;
	}

	return conn_status;
}

bool ble_conn_state_encrypted(uint16_t conn_handle)
{
	if (ble_conn_state_valid(conn_handle)) {
		return atomic_test_bit(&bcs.flags.encrypted_flags, conn_handle);
	}

	return false;
}

bool ble_conn_state_mitm_protected(uint16_t conn_handle)
{
	if (ble_conn_state_valid(conn_handle)) {
		return atomic_test_bit(&bcs.flags.mitm_protected_flags, conn_handle);
	}

	return false;
}

bool ble_conn_state_lesc(uint16_t conn_handle)
{
	if (ble_conn_state_valid(conn_handle)) {
		return atomic_test_bit(&bcs.flags.lesc_flags, conn_handle);
	}

	return false;
}

uint32_t ble_conn_state_conn_count(void)
{
	return active_flag_count(bcs.flags.connected_flags);
}

uint32_t ble_conn_state_central_conn_count(void)
{
	atomic_t central_conn_flags;

	central_conn_flags = bcs.flags.central_flags;
	(void)atomic_and(&central_conn_flags, bcs.flags.connected_flags);

	return active_flag_count(central_conn_flags);
}

uint32_t ble_conn_state_peripheral_conn_count(void)
{
	atomic_t peripheral_conn_flags;

	peripheral_conn_flags = ~bcs.flags.central_flags;
	(void)atomic_and(&peripheral_conn_flags, bcs.flags.connected_flags);

	return active_flag_count(peripheral_conn_flags);
}

struct ble_conn_state_conn_handle_list ble_conn_state_conn_handles(void)
{
	return conn_handle_list_get(bcs.flags.valid_flags);
}

struct ble_conn_state_conn_handle_list ble_conn_state_central_handles(void)
{
	atomic_t central_conn_flags;

	central_conn_flags = bcs.flags.central_flags;
	(void)atomic_and(&central_conn_flags, bcs.flags.connected_flags);

	return conn_handle_list_get(central_conn_flags);
}

struct ble_conn_state_conn_handle_list ble_conn_state_periph_handles(void)
{
	atomic_t peripheral_conn_flags;

	peripheral_conn_flags = ~bcs.flags.central_flags;
	(void)atomic_and(&peripheral_conn_flags, bcs.flags.connected_flags);

	return conn_handle_list_get(peripheral_conn_flags);
}

uint16_t ble_conn_state_conn_idx(uint16_t conn_handle)
{
	if (ble_conn_state_valid(conn_handle)) {
		return conn_handle;
	} else {
		return BLE_CONN_STATE_MAX_CONNECTIONS;
	}
}

int ble_conn_state_user_flag_acquire(void)
{
	return bcs_atomic_find_and_set_flag(&bcs.acquired_flags);
}

bool ble_conn_state_user_flag_get(uint16_t conn_handle, uint16_t flag_index)
{
	if (user_flag_is_acquired(flag_index) && ble_conn_state_valid(conn_handle)) {
		return atomic_test_bit(&bcs.flags.user_flags[flag_index], conn_handle);
	} else {
		return false;
	}
}


void ble_conn_state_user_flag_set(uint16_t conn_handle, uint16_t flag_index, bool value)
{
	if (user_flag_is_acquired(flag_index) && ble_conn_state_valid(conn_handle)) {
		flag_toggle(&bcs.flags.user_flags[flag_index], conn_handle, value);
	}
}

static uint32_t for_each_set_flag(atomic_t flags,
				  ble_conn_state_user_function_t user_function, void *ctx)
{
	uint32_t call_count = 0;

	if (!user_function) {
		return 0;
	}

	if (flags) {
		for (uint32_t i = 0; i < BLE_CONN_STATE_MAX_CONNECTIONS; i++) {
			if (atomic_test_bit(&flags, i)) {
				user_function(i, ctx);
				call_count += 1;
			}
		}
	}

	return call_count;
}

uint32_t ble_conn_state_for_each_connected(ble_conn_state_user_function_t user_function, void *ctx)
{
	return for_each_set_flag(bcs.flags.connected_flags, user_function, ctx);
}


uint32_t ble_conn_state_for_each_set_user_flag(uint16_t flag_index,
					       ble_conn_state_user_function_t user_function,
					       void *ctx)
{
	if (!user_flag_is_acquired(flag_index)) {
		return 0;
	}

	return for_each_set_flag(bcs.flags.user_flags[flag_index], user_function, ctx);
}
