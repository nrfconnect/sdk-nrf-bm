/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @defgroup peer_database Peer Database
 * @ingroup peer_manager
 * @{
 * @brief An internal module of @ref peer_manager. A module for simple management of reading and
 *        writing of peer data into persistent storage.
 *
 */

#ifndef PEER_DATABASE_H__
#define PEER_DATABASE_H__

#include <stdint.h>
#include <bm/bluetooth/peer_manager/peer_manager_types.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include "peer_manager_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The size (in bytes) of each block in the internal buffer accessible via
 *        @ref pdb_write_buf_get.
 */
#define PDB_WRITE_BUF_SIZE (sizeof(struct pm_peer_data_bonding))

/**
 * @brief Function for creating a peer ID value from a connection handle.
 *
 * Use this function with pdb_write_buf_get() or pdb_write_buf_store(). It allows to create a
 * write buffer even if you do not yet know the proper peer ID the data will be stored for.
 *
 * @param[in]  conn_handle  The connection handle.
 * @param[out] peer_id      Temporary peer ID.
 *
 * @retval NRF_SUCCESS              Temporary peer ID written to @p peer_id.
 * @retval NRF_ERROR_INVALID_PARAM  Connection handle was invalid.
 */
static inline uint32_t pdb_temp_peer_id_get(uint16_t conn_handle, uint16_t *peer_id)
{
	const int idx = nrf_sdh_ble_idx_get(conn_handle);

	if (idx < 0) {
		return NRF_ERROR_INVALID_PARAM;
	}

	*peer_id = PM_PEER_ID_N_AVAILABLE_IDS + idx;
	return NRF_SUCCESS;
}

/**
 * @brief Function for initializing the module.
 *
 * @retval NRF_SUCCESS          If initialization was successful.
 * @retval NRF_ERROR_INTERNAL   An unexpected error happened.
 */
uint32_t pdb_init(void);

/**
 * @brief Function for freeing a peer's persistent bond storage.
 *
 * @note This function will call @ref pdb_write_buf_release on the data for this peer.
 *
 * @param[in] peer_id  ID to be freed.
 *
 * @retval NRF_SUCCESS              Peer ID was released and clear operation was initiated
 * successfully.
 * @retval NRF_ERROR_INVALID_PARAM  Peer ID was invalid.
 */
uint32_t pdb_peer_free(uint16_t peer_id);

/**
 * @brief Function for retrieving pointers to a write buffer for peer data.
 *
 * @details This function will provide pointers to a buffer of the data. The data buffer will not be
 *          written to persistent storage until @ref pdb_write_buf_store is called. The buffer is
 *          released by calling either @ref pdb_write_buf_release, @ref pdb_write_buf_store, or
 *          @ref pdb_peer_free.
 *
 *          When the data_id refers to a variable length data type, the available size is written
 *          to the data, both the top-level, and any internal length fields.
 *
 * @note Calling this function on a peer_id/data_id pair that already has a buffer created will
 *       give the same buffer, not create a new one. If n_bufs was increased since last time, the
 *       buffer might be relocated to be able to provide additional room. In this case, the data
 *       will be copied. If n_bufs was increased since last time, this function might return @ref
 *       NRF_ERROR_BUSY. In that case, the buffer is automatically released.
 *
 * @param[in]  peer_id      ID of the peer to get a write buffer for. If @p peer_id is larger than
 *                          @ref PM_PEER_ID_N_AVAILABLE_IDS, it is interpreted as pertaining to
 *                          the connection that have been assigned idx (peer_id -
 * PM_PEER_ID_N_AVAILABLE_IDS) using @ref nrf_sdh_ble_idx_get. See @ref pdb_temp_peer_id_get.
 * @param[in]  data_id      The piece of data to get.
 * @param[in]  n_bufs       Number of contiguous buffers needed.
 * @param[out] p_peer_data  Pointers to mutable peer data.
 *
 * @retval NRF_SUCCESS              Data retrieved successfully.
 * @retval NRF_ERROR_INVALID_PARAM  @p data_id was invalid, or @p n_bufs was 0 or more than the
 * total available buffers.
 * @retval NRF_ERROR_FORBIDDEN      n_bufs was higher or lower than the existing buffer. If needed,
 *                                  release the existing buffer with @ref pdb_write_buf_release, and
 *                                  call this function again.
 * @retval NRF_ERROR_NULL           p_peer_data was NULL.
 * @retval NRF_ERROR_BUSY           Not enough buffer(s) available.
 * @retval NRF_ERROR_INTERNAL       Unexpected internal error.
 */
uint32_t pdb_write_buf_get(uint16_t peer_id, enum pm_peer_data_id data_id, uint32_t n_bufs,
			   struct pm_peer_data *p_peer_data);

/**
 * @brief Function for freeing a write buffer allocated with @ref pdb_write_buf_get.
 *
 * @note This function will not write peer data to persistent memory. Data in released buffer will
 *       be lost.
 *
 * @param[in]  peer_id  ID of peer to release buffer for.
 * @param[in]  data_id  Which piece of data to release buffer for.
 *
 * @retval NRF_SUCCESS              Successfully released buffer.
 * @retval NRF_ERROR_NOT_FOUND      No buffer was allocated for this peer ID/data ID pair.
 */
uint32_t pdb_write_buf_release(uint16_t peer_id, enum pm_peer_data_id data_id);

/**
 * @brief Function for writing data into persistent storage. Writing happens asynchronously.
 *
 * @note This will unlock the data after it has been written.
 *
 * @param[in]  peer_id      The ID used to address the write buffer.
 * @param[in]  data_id      The piece of data to store.
 * @param[in]  new_peer_id  The ID to put in flash. This is usually the same as peer_id, but
 *                          must be valid, i.e. allocated (and smaller than @ref
 * PM_PEER_ID_N_AVAILABLE_IDS).
 *
 * @retval NRF_SUCCESS              Data storing was successfully started.
 * @retval NRF_ERROR_RESOURCES   No space available in persistent storage. Please clear some
 *                                  space, the operation will be reattempted after the next compress
 *                                  procedure.
 * @retval NRF_ERROR_INVALID_PARAM  @p data_id or @p new_peer_id was invalid.
 * @retval NRF_ERROR_NOT_FOUND      No buffer has been allocated for this peer ID/data ID pair.
 * @retval NRF_ERROR_INTERNAL       Unexpected internal error.
 */
uint32_t pdb_write_buf_store(uint16_t peer_id, enum pm_peer_data_id data_id, uint16_t new_peer_id);

#ifdef __cplusplus
}
#endif

#endif /* PEER_DATABASE_H__ */

/** @} */
