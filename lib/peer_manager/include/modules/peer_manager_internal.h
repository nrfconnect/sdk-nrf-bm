/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file peer_manager_types.h
 *
 * @addtogroup peer_manager
 * @brief File containing definitions used solely inside the Peer Manager's modules.
 * @{
 */

#ifndef PEER_MANAGER_INTERNAL_H__
#define PEER_MANAGER_INTERNAL_H__

#include <stdint.h>
#include <ble.h>
#include <ble_gap.h>
#include <bluetooth/peer_manager/peer_manager_types.h>
#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief One piece of data associated with a peer, together with its type.
 *
 * @note This type is deprecated.
 */
typedef struct {
	/** @brief The length of the data in words. */
	uint16_t length_words;
	/**
	 * @brief ID that specifies the type of data (defines which member of the union is
	 *        used).
	 */
	pm_peer_data_id_t data_id;
	/** @brief The data. */
	union {
		/** @brief The exchanged bond information in addition to metadata of the bonding. */
		pm_peer_data_bonding_t *p_bonding_data;
		/**
		 * @brief A value locally assigned to this peer. Its
		 *        interpretation is up to the user. The rank is not set
		 *        automatically by the Peer Manager, but it is assigned by
		 *        the user using either @ref pm_peer_rank_highest or a @ref
		 *        PM_PEER_DATA_FUNCTIONS function.
		 */
		uint32_t *p_peer_rank;
		/** @brief Value of peer's Central Address Resolution characteristic. */
		uint32_t *p_central_addr_res;
		/** @brief Whether a service changed indication should be sent to the peer. */
		bool *p_service_changed_pending;
		/** @brief Persistent information pertaining to a peer GATT client. */
		pm_peer_data_local_gatt_db_t *p_local_gatt_db;
		/** @brief Persistent information pertaining to a peer GATT server. */
		ble_gatt_db_srv_t *p_remote_gatt_db;
		/**
		 * @brief Arbitrary data to associate with the peer. This data can be freely used
		 *        by the application.
		 */
		uint8_t *p_application_data;
		/**
		 * @brief Generic access pointer to the data. It is used only to
		 *        handle the data without regard to type.
		 */
		void *p_all_data;
	};
} pm_peer_data_t;

/**
 * @brief Immutable version of @ref pm_peer_data_t.
 *
 * @note This type is deprecated.
 */
typedef struct {
	/** @brief The length of the data in words. */
	uint16_t length_words;
	/**
	 * @brief ID that specifies the type of data (defines which member of the union is
	 *        used).
	 */
	pm_peer_data_id_t data_id;
	/** @brief The data. */
	union {
		/** @brief Immutable @ref pm_peer_data_t::p_bonding_data. */
		pm_peer_data_bonding_t const *p_bonding_data;
		/** @brief Immutable @ref pm_peer_data_t::p_peer_rank. */
		uint32_t const *p_peer_rank;
		/** @brief Immutable @ref pm_peer_data_t::p_central_addr_res. */
		uint32_t const *p_central_addr_res;
		/** @brief Immutable @ref pm_peer_data_t::p_service_changed_pending. */
		bool const *p_service_changed_pending;
		/** @brief Immutable @ref pm_peer_data_t::p_local_gatt_db. */
		pm_peer_data_local_gatt_db_t const *p_local_gatt_db;
		/** @brief Immutable @ref pm_peer_data_t::p_remote_gatt_db. */
		ble_gatt_db_srv_t const *p_remote_gatt_db;
		/** @brief Immutable @ref pm_peer_data_t::p_application_data. */
		uint8_t const *p_application_data;
		/** @brief Immutable @ref pm_peer_data_t::p_all_data. */
		void const *p_all_data;
	};
} pm_peer_data_const_t;

/**
 * @brief Version of @ref pm_peer_data_t that reflects the structure of peer data in flash.
 *
 * @note This type is deprecated.
 */
typedef pm_peer_data_const_t pm_peer_data_flash_t;

/**
 * @brief Event handler for events from the @ref peer_manager module.
 *
 * @sa pm_register
 *
 * @param[in]  p_event  The event that has occurred.
 */
typedef void (*pm_evt_handler_internal_t)(pm_evt_t *p_event);

/** @brief Macro for showing that a variable is unused. */
#define UNUSED_VARIABLE(X) ((void)(X))

/** @brief The number of bytes in a word. */
#define BYTES_PER_WORD (4)

/**
 * @brief Macro for calculating the number of words that are needed to hold a number of bytes.
 *
 * @param[in]  n_bytes  The number of bytes.
 *
 * @returns The number of words needed to hold @p n_bytes bytes.
 */
#define BYTES_TO_WORDS(n_bytes) DIV_ROUND_UP((n_bytes), BYTES_PER_WORD)

/**
 * @brief Macro for calculating the flash size of bonding data.
 *
 * @return The number of words that the data takes in flash.
 */
#define PM_BONDING_DATA_N_WORDS() BYTES_TO_WORDS(sizeof(pm_peer_data_bonding_t))

/**
 * @brief Macro for calculating the flash size of service changed pending state.
 *
 * @return The number of words that the data takes in flash.
 */
#define PM_SC_STATE_N_WORDS() BYTES_TO_WORDS(sizeof(bool))

/**
 * @brief Macro for calculating the flash size of local GATT database data.
 *
 * @param[in]  local_db_len  The length, in bytes, of the database as reported by the SoftDevice.
 *
 * @return The number of words that the data takes in flash.
 */
#define PM_LOCAL_DB_N_WORDS(local_db_len)                                                          \
	BYTES_TO_WORDS((local_db_len) + PM_LOCAL_DB_LEN_OVERHEAD_BYTES)

/**
 * @brief Macro for calculating the length of a local GATT database attribute array.
 *
 * @param[in]  n_words  The number of words that the data takes in flash.
 *
 * @return The length of the database attribute array.
 */
#define PM_LOCAL_DB_LEN(n_words) (((n_words)*BYTES_PER_WORD) - PM_LOCAL_DB_LEN_OVERHEAD_BYTES)

/**
 * @brief Macro for calculating the flash size of remote GATT database data.
 *
 * @param[in]  service_count  The number of services in the service array.
 *
 * @return The number of words that the data takes in flash.
 */
#define PM_REMOTE_DB_N_WORDS(service_count)                                                        \
	BYTES_TO_WORDS(sizeof(ble_gatt_db_srv_t) * (service_count))

/**
 * @brief Macro for calculating the flash size of remote GATT database data.
 *
 * @param[in]  n_words  The length in number of words.
 *
 * @return The number of words that the data takes in flash.
 */
#define PM_REMOTE_DB_N_SERVICES(n_words) (((n_words)*BYTES_PER_WORD) / sizeof(ble_gatt_db_srv_t))

/**
 * @brief Function for calculating the flash size of the usage index.
 *
 * @return The number of words that the data takes in flash.
 */
#define PM_USAGE_INDEX_N_WORDS() BYTES_TO_WORDS(sizeof(uint32_t))

#ifdef NRF_PM_DEBUG

#define NRF_PM_DEBUG_CHECK(condition)                                                              \
	if (!(condition)) {                                                                        \
		__asm("bkpt #0");                                                                  \
	}

#else

/* Prevent "variable set but never used" compiler warnings. */
#define NRF_PM_DEBUG_CHECK(condition) (void)(condition)

#endif

#ifdef __cplusplus
}
#endif

#endif /* PEER_MANAGER_INTERNAL_H__ */

/** @} */
