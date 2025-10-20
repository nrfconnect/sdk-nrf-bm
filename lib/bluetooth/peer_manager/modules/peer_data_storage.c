/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/atomic.h>
#include <sdk_macros.h>
#include <bm/fs/bm_zms.h>
#include <bm/bluetooth/peer_manager/peer_manager_types.h>
#include <modules/peer_manager_internal.h>
#include <modules/peer_id.h>
#include <modules/peer_data_storage.h>

#define PEER_MANAGER_NODE DT_NODELABEL(peer_manager_partition)
#define PEER_MANAGER_PARTITION_OFFSET DT_REG_ADDR(PEER_MANAGER_NODE)
#define PEER_MANAGER_PARTITION_SIZE DT_REG_SIZE(PEER_MANAGER_NODE)

LOG_MODULE_DECLARE(peer_manager, CONFIG_PEER_MANAGER_LOG_LEVEL);

/* Macro for verifying that the peer id is within a valid range. */
#define VERIFY_PEER_ID_IN_RANGE(id)                                                                \
	VERIFY_FALSE((id >= PM_PEER_ID_N_AVAILABLE_IDS), NRF_ERROR_INVALID_PARAM)

/* Macro for verifying that the peer data id is within a valid range. */
#define VERIFY_PEER_DATA_ID_IN_RANGE(id)                                                           \
	VERIFY_TRUE(peer_data_id_is_valid(id), NRF_ERROR_INVALID_PARAM)

/* The number of registered event handlers. */
#define PDS_EVENT_HANDLERS_CNT ARRAY_SIZE(m_evt_handlers)

/* Peer Data Storage event handler in Peer Database. */
extern void pdb_pds_evt_handler(struct pm_evt *evt);

/* Peer Data Storage events' handlers.
 * The number of elements in this array is PDS_EVENT_HANDLERS_CNT.
 */
static const pm_evt_handler_internal_t m_evt_handlers[] = {
	pdb_pds_evt_handler,
};

static bool m_module_initialized;
static volatile bool m_peer_delete_deferred;

static struct bm_zms_fs fs;

/* Keeps track of the number of peers currently under delete processing. */
static atomic_t delete_counter;

/* Function for dispatching events to all registered event handlers. */
static void pds_evt_send(struct pm_evt *p_event)
{
	p_event->conn_handle = BLE_CONN_HANDLE_INVALID;

	for (uint32_t i = 0; i < PDS_EVENT_HANDLERS_CNT; i++) {
		m_evt_handlers[i](p_event);
	}
}

#define ENTRY_ID_PEER_ID_OFFSET_BITS 16
#define ENTRY_ID_DATA_ID_MASK        ((1 << ENTRY_ID_PEER_ID_OFFSET_BITS) - 1)

/**
 * @brief Pack the given peer_id and data_id into a single 32 bit entry_id.
 *
 * @p peer_id is stored in the most significant 16 bits.
 * @p data_id is stored in the least significant 16 bits.
 */
static uint32_t peer_id_peer_data_id_to_entry_id(uint16_t peer_id, enum pm_peer_data_id data_id)
{
	return (peer_id << ENTRY_ID_PEER_ID_OFFSET_BITS) | (data_id & ENTRY_ID_DATA_ID_MASK);
}

/**
 * @brief Unpack the given entry_id into a peer_id and data_id.
 */
static void entry_id_to_peer_id_peer_data_id(uint32_t entry_id, uint16_t *peer_id,
					     enum pm_peer_data_id *data_id)
{
	*data_id = entry_id & ENTRY_ID_DATA_ID_MASK;
	*peer_id = entry_id >> ENTRY_ID_PEER_ID_OFFSET_BITS;
}

static bool peer_data_id_is_valid(enum pm_peer_data_id data_id)
{
	return ((data_id == PM_PEER_DATA_ID_BONDING) ||
		(data_id == PM_PEER_DATA_ID_SERVICE_CHANGED_PENDING) ||
		(data_id == PM_PEER_DATA_ID_GATT_LOCAL) ||
		(data_id == PM_PEER_DATA_ID_GATT_REMOTE) ||
		(data_id == PM_PEER_DATA_ID_PEER_RANK) ||
		(data_id == PM_PEER_DATA_ID_CENTRAL_ADDR_RES) ||
		(data_id == PM_PEER_DATA_ID_APPLICATION));
}

/**
 * @brief Function for sending a PM_EVT_ERROR_UNEXPECTED event.
 *
 * @param[in]  peer_id    The peer the event pertains to.
 * @param[in]  err_code   The unexpected error that occurred.
 */
static void send_unexpected_error(uint16_t peer_id, uint32_t err_code)
{
	struct pm_evt error_evt = {
		.evt_id = PM_EVT_ERROR_UNEXPECTED,
		.peer_id = peer_id,
		.params.error_unexpected = {
			.error = err_code,
		},
	};
	pds_evt_send(&error_evt);
}

/* Returns the next data entry or a negative errno. */
static uint32_t find_next_data_entry_in_peer(uint16_t peer_id, uint32_t *next_entry_id)
{
	ssize_t ret;
	uint8_t temp_buf[PM_PEER_DATA_MAX_SIZE] = { 0 };

	for (enum pm_peer_data_id i = 0; i < PM_PEER_DATA_ID_LAST; i++) {
		uint32_t entry_id = peer_id_peer_data_id_to_entry_id(peer_id, i);

		ret = bm_zms_read(&fs, entry_id, temp_buf, sizeof(temp_buf));
		/* Unexpected error. */
		if (ret < 0 && ret != -ENOENT) {
			LOG_ERR("Could not read entry %d from NVM. bm_zms_read() returned %d. "
				"peer_id: %d, data_id: %d", entry_id, ret, peer_id, i);
			return NRF_ERROR_INTERNAL;
		}

		/* Some peer data has been found. */
		if (ret > 0) {
			*next_entry_id = entry_id;
			return NRF_SUCCESS;
		}
	}

	/* Every data read for the peer has returned `-ENOENT`. */
	return NRF_ERROR_NOT_FOUND;
}

/* Function for deleting all data belonging to a peer.
 * These operations will be sent to FDS one at a time.
 */
static void peer_data_delete_process(void)
{
	int err;
	uint16_t peer_id;
	uint32_t entry_id;

	m_peer_delete_deferred = false;

	peer_id = peer_id_get_next_deleted(PM_PEER_ID_INVALID);

	/* PM_PEER_ID_INVALID is used in `peer_id` to inform that there are no more next deleted. */
	while ((peer_id != PM_PEER_ID_INVALID) &&
		(find_next_data_entry_in_peer(peer_id, &entry_id) == NRF_ERROR_NOT_FOUND)) {
		peer_id_free(peer_id);
		peer_id = peer_id_get_next_deleted(peer_id);
	}

	if (peer_id != PM_PEER_ID_INVALID) {
		err = bm_zms_delete(&fs, entry_id);
		if (err == -ENOMEM) {
			m_peer_delete_deferred = true;
		} else if (err < 0) {
			LOG_ERR("Could not delete peer data. bm_zms_delete() returned %d "
				"for peer_id: %d", err, peer_id);
			/* Send generic internal error since ZMS returns errnos. */
			send_unexpected_error(peer_id, NRF_ERROR_INTERNAL);
		}
	}
}

static void peer_ids_load(void)
{
	uint16_t peer_id;
	uint16_t peer_id_iter;
	struct pm_peer_data_const peer_data = { 0 };
	uint8_t peer_data_buffer[PM_PEER_DATA_MAX_SIZE] = { 0 };

	peer_data.p_all_data = peer_data_buffer;

	/* Search through existing bonds to look for a duplicate. */
	pds_peer_data_iterate_prepare(&peer_id_iter);

	while (pds_peer_data_iterate(PM_PEER_DATA_ID_BONDING, &peer_id, &peer_data,
		&peer_id_iter)) {
		(void)peer_id_allocate(peer_id);
	}
}

static void bm_zms_evt_handler(const bm_zms_evt_t *p_evt)
{
	uint16_t peer_id;
	enum pm_peer_data_id data_id;

	entry_id_to_peer_id_peer_data_id(p_evt->id, &peer_id, &data_id);

	struct pm_evt pds_evt = { .peer_id = peer_id };

	switch (p_evt->evt_id) {
	case BM_ZMS_EVT_INIT:
		if (p_evt->result) {
			LOG_ERR("BM_ZMS initialization failed with error %d", p_evt->result);
		}
		break;
	case BM_ZMS_EVT_WRITE:
		pds_evt.params.peer_data_update_succeeded.data_id = data_id;
		pds_evt.params.peer_data_update_succeeded.action = PM_PEER_DATA_OP_UPDATE;
		pds_evt.params.peer_data_update_succeeded.token = p_evt->id;

		if (p_evt->result == 0) {
			pds_evt.evt_id = PM_EVT_PEER_DATA_UPDATE_SUCCEEDED;
			pds_evt.params.peer_data_update_succeeded.flash_changed = true;
		} else {
			LOG_ERR("BM_ZMS write failed with error %d", p_evt->result);
			pds_evt.evt_id = PM_EVT_PEER_DATA_UPDATE_FAILED;
			pds_evt.params.peer_data_update_failed.error = NRF_ERROR_INTERNAL;
		}

		pds_evt_send(&pds_evt);
		break;
	case BM_ZMS_EVT_DELETE:
		if (!peer_id_is_deleted(peer_id)) {
			pds_evt.params.peer_data_update_succeeded.data_id = data_id;
			pds_evt.params.peer_data_update_succeeded.action = PM_PEER_DATA_OP_DELETE;
			pds_evt.params.peer_data_update_succeeded.token = p_evt->id;

			if (p_evt->result == 0) {
				pds_evt.evt_id = PM_EVT_PEER_DATA_UPDATE_SUCCEEDED;
				pds_evt.params.peer_data_update_succeeded.flash_changed = true;
			} else {
				LOG_ERR("BM_ZMS delete failed with error %d", p_evt->result);
				pds_evt.evt_id = PM_EVT_PEER_DATA_UPDATE_FAILED;
				pds_evt.params.peer_data_update_failed.error = NRF_ERROR_INTERNAL;
			}

			pds_evt_send(&pds_evt);
		} else {
			if (p_evt->result == -ENOMEM) {
				m_peer_delete_deferred = true;
			} else if (p_evt->result < 0) {
				/* Unrecoverable error. */
				LOG_ERR("BM_ZMS delete failed with error %d", p_evt->result);

				atomic_dec(&delete_counter);

				pds_evt.evt_id = PM_EVT_PEER_DELETE_FAILED;
				pds_evt.params.peer_delete_failed.error = NRF_ERROR_INTERNAL;
				pds_evt_send(&pds_evt);
			} else {
				uint32_t next_entry_id;
				uint32_t ret;

				ret = find_next_data_entry_in_peer(peer_id, &next_entry_id);
				if (ret == NRF_SUCCESS) {
					/* Process the next entry for the peer. */
					m_peer_delete_deferred = true;
				} else if (ret == NRF_ERROR_NOT_FOUND) {
					atomic_dec(&delete_counter);

					/* Process the next deleted peers, if any are present. */
					m_peer_delete_deferred = true;

					pds_evt.evt_id = PM_EVT_PEER_DELETE_SUCCEEDED;
					peer_id_free(pds_evt.peer_id);
					pds_evt_send(&pds_evt);
				}
			}
		}

		break;
	default:
		/* No action. */
		break;
	}

	if (m_peer_delete_deferred) {
		peer_data_delete_process();
	}
}

static void wait_for_init(void)
{
	while (!fs.init_flags.initialized) {
		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();
	}
}

void pds_peer_data_iterate_prepare(uint16_t *p_peer_id_iter)
{
	*p_peer_id_iter = 0;
}

bool pds_peer_data_iterate(enum pm_peer_data_id data_id, uint16_t *const p_peer_id,
			   struct pm_peer_data_const *const p_data, uint16_t *p_peer_id_iter)
{
	ssize_t ret;
	uint8_t temp_buf[PM_PEER_DATA_MAX_SIZE] = { 0 };

	if (*p_peer_id_iter >= PM_PEER_ID_N_AVAILABLE_IDS) {
		return false;
	}

	/* Exits the loop when `ret > 0` (it found data), it reached the end of the available peers,
	 * or the read had a catastrophical failure.
	 */
	do {
		uint32_t entry_id = peer_id_peer_data_id_to_entry_id(*p_peer_id_iter, data_id);

		ret = bm_zms_read(&fs, entry_id, temp_buf, sizeof(temp_buf));
		if (ret < 0 && ret != -ENOENT) {
			LOG_ERR("Could not read data from NVM. bm_zms_read() returned %d. "
				"peer_id: %d",
				ret, *p_peer_id_iter);
			return false;
		}

		(*p_peer_id_iter)++;
	} while ((ret == -ENOENT) && (*p_peer_id_iter < PM_PEER_ID_N_AVAILABLE_IDS));

	if ((ret == -ENOENT) && (*p_peer_id_iter == PM_PEER_ID_N_AVAILABLE_IDS)) {
		return false;
	}

	/* We found a suitable Peer ID. */

	/* `p_peer_id_iter` counts the iterations, so the Peer ID is iterations minus one. */
	*p_peer_id = (*p_peer_id_iter) - 1;

	/* `ret` is equal the exact amount of data contained in the entry, so copy that amount
	 * safely.
	 */
	memcpy((void *)p_data->p_all_data, temp_buf, ret);

	return true;
}

uint32_t pds_init(void)
{
	int err;

	/* Check for re-initialization if debugging. */
	NRF_PM_DEBUG_CHECK(!m_module_initialized);

	err = bm_zms_register(&fs, bm_zms_evt_handler);
	if (err) {
		LOG_ERR("Could not initialize NVM storage. bm_zms_register() returned %d.", err);
		return NRF_ERROR_INTERNAL;
	}

	fs.offset = PEER_MANAGER_PARTITION_OFFSET;
	fs.sector_size = CONFIG_PM_BM_ZMS_SECTOR_SIZE;
	fs.sector_count = (PEER_MANAGER_PARTITION_SIZE / CONFIG_PM_BM_ZMS_SECTOR_SIZE);

	err = bm_zms_mount(&fs);
	if (err) {
		LOG_ERR("Could not initialize NVM storage. bm_zms_mount() returned %d.", err);
		return NRF_ERROR_RESOURCES;
	}
	wait_for_init();

	peer_id_init();
	peer_ids_load();

	m_module_initialized = true;

	return NRF_SUCCESS;
}

uint32_t pds_peer_data_read(uint16_t peer_id, enum pm_peer_data_id data_id,
			    struct pm_peer_data *const p_data, const uint32_t *const p_buf_len)
{
	ssize_t ret;
	uint8_t temp_buf[PM_PEER_DATA_MAX_SIZE] = { 0 };

	NRF_PM_DEBUG_CHECK(m_module_initialized);
	NRF_PM_DEBUG_CHECK(p_data != NULL);
	NRF_PM_DEBUG_CHECK(p_buf_len != NULL);

	VERIFY_PEER_ID_IN_RANGE(peer_id);
	VERIFY_PEER_DATA_ID_IN_RANGE(data_id);

	uint32_t entry_id = peer_id_peer_data_id_to_entry_id(peer_id, data_id);

	ret = bm_zms_read(&fs, entry_id, temp_buf, sizeof(temp_buf));
	if (ret == -ENOENT) {
		LOG_DBG("Could not read entry %d. bm_zms_read() returned %d. "
			"peer_id: %d, data_id: %d", entry_id,
			ret, peer_id, data_id);
		return NRF_ERROR_NOT_FOUND;
	} else if (ret < 0) {
		LOG_ERR("Could not read data from NVM. bm_zms_read() returned %d. "
			"peer_id: %d",
			ret, peer_id);
		return NRF_ERROR_INTERNAL;
	}

	memcpy(p_data->p_all_data, temp_buf, MIN(*p_buf_len, ret));

	if (*p_buf_len < ret) {
		return NRF_ERROR_DATA_SIZE;
	}

	return NRF_SUCCESS;
}

uint32_t pds_peer_data_store(uint16_t peer_id, const struct pm_peer_data_const *p_peer_data,
			     uint32_t *p_store_token)
{
	ssize_t ret;

	NRF_PM_DEBUG_CHECK(m_module_initialized);
	NRF_PM_DEBUG_CHECK(p_peer_data != NULL);

	VERIFY_PEER_ID_IN_RANGE(peer_id);
	VERIFY_PEER_DATA_ID_IN_RANGE(p_peer_data->data_id);

	uint32_t entry_id = peer_id_peer_data_id_to_entry_id(peer_id, p_peer_data->data_id);

	ret = bm_zms_write(&fs, entry_id, p_peer_data->p_all_data,
			   p_peer_data->length_words * BYTES_PER_WORD);
	if (ret < 0) {
		LOG_ERR("Could not write data to NVM. bm_zms_write() returned %d. "
			"peer_id: %d",
			ret, peer_id);
		return NRF_ERROR_INTERNAL;
	}

	if (p_store_token != NULL) {
		/* Update the store token. */
		*p_store_token = entry_id;
	}

	return NRF_SUCCESS;
}

uint32_t pds_peer_data_delete(uint16_t peer_id, enum pm_peer_data_id data_id)
{
	int err;

	NRF_PM_DEBUG_CHECK(m_module_initialized);

	VERIFY_PEER_ID_IN_RANGE(peer_id);
	VERIFY_PEER_DATA_ID_IN_RANGE(data_id);

	uint32_t entry_id = peer_id_peer_data_id_to_entry_id(peer_id, data_id);

	err = bm_zms_delete(&fs, entry_id);
	if (err) {
		LOG_ERR("Could not delete peer data. bm_zms_delete() returned %d. peer_id: %d, "
			"data_id: %d.",
			err, peer_id, data_id);
		return NRF_ERROR_INTERNAL;
	}

	return NRF_SUCCESS;
}

uint16_t pds_peer_id_allocate(void)
{
	NRF_PM_DEBUG_CHECK(m_module_initialized);
	return peer_id_allocate(PM_PEER_ID_INVALID);
}

uint32_t pds_peer_id_free(uint16_t peer_id)
{
	NRF_PM_DEBUG_CHECK(m_module_initialized);
	VERIFY_PEER_ID_IN_RANGE(peer_id);

	(void)peer_id_delete(peer_id);

	/* Only start processing on the first delete request.
	 * `peer_data_delete_process` will iteratively take care of processing all the peers marked
	 * for deletion.
	 */
	if (atomic_inc(&delete_counter) == 0) {
		peer_data_delete_process();
	}

	return NRF_SUCCESS;
}

bool pds_peer_id_is_allocated(uint16_t peer_id)
{
	NRF_PM_DEBUG_CHECK(m_module_initialized);
	return peer_id_is_allocated(peer_id);
}

bool pds_peer_id_is_deleted(uint16_t peer_id)
{
	NRF_PM_DEBUG_CHECK(m_module_initialized);
	return peer_id_is_deleted(peer_id);
}

uint16_t pds_next_peer_id_get(uint16_t prev_peer_id)
{
	NRF_PM_DEBUG_CHECK(m_module_initialized);
	return peer_id_get_next_used(prev_peer_id);
}

uint16_t pds_next_deleted_peer_id_get(uint16_t prev_peer_id)
{
	NRF_PM_DEBUG_CHECK(m_module_initialized);
	return peer_id_get_next_deleted(prev_peer_id);
}

uint32_t pds_peer_count_get(void)
{
	NRF_PM_DEBUG_CHECK(m_module_initialized);
	return peer_id_n_ids();
}
