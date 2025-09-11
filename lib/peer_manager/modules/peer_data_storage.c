/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <sdk_macros.h>
#include <nrf_error.h>
#include <sdk_macros.h>
#include <bluetooth/peer_manager/peer_manager_types.h>
#include <bm_zms.h>
#include <modules/peer_manager_internal.h>
#include <modules/peer_id.h>
#include <modules/peer_data_storage.h>

#define STORAGE_NODE DT_NODELABEL(storage1_partition)
#define BM_ZMS_PARTITION_OFFSET DT_REG_ADDR(STORAGE_NODE)
#define BM_ZMS_PARTITION_SIZE DT_REG_SIZE(STORAGE_NODE)

#define CODE_DISABLED 0

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
extern void pdb_pds_evt_handler(pm_evt_t *evt);

/* Peer Data Storage events' handlers.
 * The number of elements in this array is PDS_EVENT_HANDLERS_CNT.
 */
static pm_evt_handler_internal_t const m_evt_handlers[] = {
	pdb_pds_evt_handler,
};

static bool m_module_initialized;
static volatile bool m_peer_delete_deferred;

static struct bm_zms_fs fs;

/* Function for dispatching events to all registered event handlers. */
static void pds_evt_send(pm_evt_t *p_event)
{
	p_event->conn_handle = BLE_CONN_HANDLE_INVALID;

	for (uint32_t i = 0; i < PDS_EVENT_HANDLERS_CNT; i++) {
		m_evt_handlers[i](p_event);
	}
}

static uint32_t peer_id_peer_data_id_to_entry_id(pm_peer_id_t peer_id,
						 pm_peer_data_id_t data_id)
{
	return (peer_id * PM_PEER_DATA_ID_LAST) + data_id;
}

static void entry_id_to_peer_id_peer_data_id(uint32_t entry_id, pm_peer_id_t *peer_id,
					     pm_peer_data_id_t *data_id)
{
	*data_id = entry_id % PM_PEER_DATA_ID_LAST;
	*peer_id = entry_id / PM_PEER_DATA_ID_LAST;
}

#if CODE_DISABLED /* todo: this does not apply anymore. Find an alternative. Used in evt handler. */
/* Function for checking whether a file ID is relevant for the Peer Manager. */
static bool file_id_within_pm_range(uint16_t file_id)
{
	return ((PDS_FIRST_RESERVED_FILE_ID <= file_id) && (file_id <= PDS_LAST_RESERVED_FILE_ID));
}

/* Function for checking whether a record key is relevant for the Peer Manager. */
static bool record_key_within_pm_range(uint16_t record_key)
{
	return ((PDS_FIRST_RESERVED_RECORD_KEY <= record_key) &&
		(record_key <= PDS_LAST_RESERVED_RECORD_KEY));
}
#endif

static bool peer_data_id_is_valid(pm_peer_data_id_t data_id)
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
static void send_unexpected_error(pm_peer_id_t peer_id, uint32_t err_code)
{
	pm_evt_t error_evt = {.evt_id = PM_EVT_ERROR_UNEXPECTED,
			      .peer_id = peer_id,
			      .params = {.error_unexpected = {
						 .error = err_code,
					 }}};
	pds_evt_send(&error_evt);
}

/* Function for deleting all data belonging to a peer.
 * These operations will be sent to FDS one at a time.
 */
static void peer_data_delete_process(void)
{
#if CODE_DISABLED
	uint32_t ret;
	pm_peer_id_t peer_id;
	uint16_t file_id;
	fds_record_desc_t desc;
	fds_find_token_t ftok;

	m_peer_delete_deferred = false;

	memset(&ftok, 0x00, sizeof(fds_find_token_t));
	peer_id = peer_id_get_next_deleted(PM_PEER_ID_INVALID);

	while ((peer_id != PM_PEER_ID_INVALID) &&
	       (fds_record_find_in_file(peer_id_to_file_id(peer_id), &desc, &ftok) ==
		FDS_ERR_NOT_FOUND)) {
		peer_id_free(peer_id);
		peer_id = peer_id_get_next_deleted(peer_id);
	}

	if (peer_id != PM_PEER_ID_INVALID) {
		file_id = peer_id_to_file_id(peer_id);
		ret = fds_file_delete(file_id);

		if (ret == FDS_ERR_NO_SPACE_IN_QUEUES) {
			m_peer_delete_deferred = true;
		} else if (ret != NRF_SUCCESS) {
			LOG_ERR("Could not delete peer data. fds_file_delete() returned 0x%x "
				"for peer_id: %d",
				ret, peer_id);
			send_unexpected_error(peer_id, ret);
		}
	}
#endif
}

static void peer_ids_load(void)
{
	pm_peer_id_t peer_id;
	pm_peer_id_t peer_id_iter;
	pm_peer_data_flash_t peer_data = { 0 };
	uint8_t peer_data_buffer[PM_PEER_DATA_MAX_SIZE] = { 0 };

	peer_data.p_all_data = peer_data_buffer;

	/* Search through existing bonds to look for a duplicate. */
	pds_peer_data_iterate_prepare(&peer_id_iter);

	/* @note This check is not thread safe since data is not copied while iterating. */
	/* todo: update comment above. */
	while (pds_peer_data_iterate(PM_PEER_DATA_ID_BONDING, &peer_id, &peer_data,
		&peer_id_iter)) {
		(void)peer_id_allocate(peer_id);
	}
}

static void bm_zms_evt_handler(bm_zms_evt_t const *p_evt)
{
#if CODE_DISABLED
	pm_evt_t pds_evt = {.peer_id = file_id_to_peer_id(p_fds_evt->write.file_id)};

	switch (p_fds_evt->id) {
	case FDS_EVT_WRITE:
	case FDS_EVT_UPDATE:
	case FDS_EVT_DEL_RECORD:
		if (file_id_within_pm_range(p_fds_evt->write.file_id) ||
		    record_key_within_pm_range(p_fds_evt->write.record_key)) {
			pds_evt.params.peer_data_update_succeeded.data_id =
				record_key_to_peer_data_id(p_fds_evt->write.record_key);
			pds_evt.params.peer_data_update_succeeded.action =
				(p_fds_evt->id == FDS_EVT_DEL_RECORD) ? PM_PEER_DATA_OP_DELETE
								      : PM_PEER_DATA_OP_UPDATE;
			pds_evt.params.peer_data_update_succeeded.token =
				p_fds_evt->write.record_id;

			if (p_fds_evt->result == NRF_SUCCESS) {
				pds_evt.evt_id = PM_EVT_PEER_DATA_UPDATE_SUCCEEDED;
				pds_evt.params.peer_data_update_succeeded.flash_changed = true;
			} else {
				pds_evt.evt_id = PM_EVT_PEER_DATA_UPDATE_FAILED;
				pds_evt.params.peer_data_update_failed.error = p_fds_evt->result;
			}

			pds_evt_send(&pds_evt);
		}
		break;

	case FDS_EVT_DEL_FILE:
		if (file_id_within_pm_range(p_fds_evt->del.file_id) &&
		    (p_fds_evt->del.record_key == FDS_RECORD_KEY_DIRTY)) {
			if (p_fds_evt->result == NRF_SUCCESS) {
				pds_evt.evt_id = PM_EVT_PEER_DELETE_SUCCEEDED;
				peer_id_free(pds_evt.peer_id);
			} else {
				pds_evt.evt_id = PM_EVT_PEER_DELETE_FAILED;
				pds_evt.params.peer_delete_failed.error = p_fds_evt->result;
			}

			/* Trigger remaining deletes. */
			m_peer_delete_deferred = true;

			pds_evt_send(&pds_evt);
		}
		break;

	case FDS_EVT_GC:
		if (p_fds_evt->result == NRF_SUCCESS) {
			pds_evt.evt_id = PM_EVT_FLASH_GARBAGE_COLLECTED;
		} else {
			pds_evt.evt_id = PM_EVT_FLASH_GARBAGE_COLLECTION_FAILED;
			pds_evt.params.garbage_collection_failed.error = p_fds_evt->result;
		}
		pds_evt.peer_id = PM_PEER_ID_INVALID;
		pds_evt_send(&pds_evt);
		break;

	default:
		/* No action. */
		break;
	}

	if (m_peer_delete_deferred) {
		peer_data_delete_process();
	}
#endif
	pm_peer_id_t peer_id;
	pm_peer_data_id_t data_id;

	entry_id_to_peer_id_peer_data_id(p_evt->id, &peer_id, &data_id);

	pm_evt_t pds_evt = { .peer_id = peer_id };

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
			pds_evt.evt_id = PM_EVT_PEER_DATA_UPDATE_FAILED;
			pds_evt.params.peer_data_update_failed.error = p_evt->result;
		}

		pds_evt_send(&pds_evt);
		break;
	case BM_ZMS_EVT_DELETE:
		pds_evt.params.peer_data_update_succeeded.data_id = data_id;
		pds_evt.params.peer_data_update_succeeded.action = PM_PEER_DATA_OP_DELETE;
		pds_evt.params.peer_data_update_succeeded.token = p_evt->id;

		if (p_evt->result == 0) {
			pds_evt.evt_id = PM_EVT_PEER_DATA_UPDATE_SUCCEEDED;
			pds_evt.params.peer_data_update_succeeded.flash_changed = true;
		} else {
			pds_evt.evt_id = PM_EVT_PEER_DATA_UPDATE_FAILED;
			pds_evt.params.peer_data_update_failed.error = p_evt->result;
		}

		pds_evt_send(&pds_evt);
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
#if defined(CONFIG_SOFTDEVICE)
		/* Wait for an event. */
		__WFE();

		/* Clear Event Register */
		__SEV();
		__WFE();
#endif
	}
}

void pds_peer_data_iterate_prepare(pm_peer_id_t *p_peer_id_iter)
{
	*p_peer_id_iter = 0;
}

bool pds_peer_data_iterate(pm_peer_data_id_t data_id, pm_peer_id_t *const p_peer_id,
			   pm_peer_data_flash_t *const p_data, pm_peer_id_t *p_peer_id_iter)
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

		ret = bm_zms_read(&fs, entry_id, temp_buf, PM_PEER_DATA_MAX_SIZE);
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

	fs.offset = BM_ZMS_PARTITION_OFFSET;
	fs.sector_size = CONFIG_PM_BM_ZMS_SECTOR_SIZE;
	fs.sector_count = (BM_ZMS_PARTITION_SIZE / CONFIG_PM_BM_ZMS_SECTOR_SIZE);

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

uint32_t pds_peer_data_read(pm_peer_id_t peer_id, pm_peer_data_id_t data_id,
			      pm_peer_data_t *const p_data, uint32_t const *const p_buf_len)
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

uint32_t pds_peer_data_store(pm_peer_id_t peer_id, pm_peer_data_const_t const *p_peer_data,
			       pm_store_token_t *p_store_token)
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

	return NRF_SUCCESS;
}

uint32_t pds_peer_data_delete(pm_peer_id_t peer_id, pm_peer_data_id_t data_id)
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

pm_peer_id_t pds_peer_id_allocate(void)
{
	NRF_PM_DEBUG_CHECK(m_module_initialized);
	return peer_id_allocate(PM_PEER_ID_INVALID);
}

uint32_t pds_peer_id_free(pm_peer_id_t peer_id)
{
	NRF_PM_DEBUG_CHECK(m_module_initialized);
	VERIFY_PEER_ID_IN_RANGE(peer_id);

	(void)peer_id_delete(peer_id);
	peer_data_delete_process();

	return NRF_SUCCESS;
}

bool pds_peer_id_is_allocated(pm_peer_id_t peer_id)
{
	NRF_PM_DEBUG_CHECK(m_module_initialized);
	return peer_id_is_allocated(peer_id);
}

bool pds_peer_id_is_deleted(pm_peer_id_t peer_id)
{
	NRF_PM_DEBUG_CHECK(m_module_initialized);
	return peer_id_is_deleted(peer_id);
}

pm_peer_id_t pds_next_peer_id_get(pm_peer_id_t prev_peer_id)
{
	NRF_PM_DEBUG_CHECK(m_module_initialized);
	return peer_id_get_next_used(prev_peer_id);
}

pm_peer_id_t pds_next_deleted_peer_id_get(pm_peer_id_t prev_peer_id)
{
	NRF_PM_DEBUG_CHECK(m_module_initialized);
	return peer_id_get_next_deleted(prev_peer_id);
}

uint32_t pds_peer_count_get(void)
{
	NRF_PM_DEBUG_CHECK(m_module_initialized);
	return peer_id_n_ids();
}
