/*
 * Copyright (c) 2015-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */


#include <stdint.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <nrf_error.h>
#include <bluetooth/peer_manager/peer_manager_types.h>
#include <bm_zms.h>
#include <modules/peer_manager_internal.h>
#include <modules/peer_id.h>
#include <modules/peer_data_storage.h>

LOG_MODULE_REGISTER(peer_data_storage, CONFIG_PEER_MANAGER_LOG_LEVEL);

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

/* A token used for Flash Data Storage searches. */
static fds_find_token_t m_fds_ftok;

/* Function for dispatching events to all registered event handlers. */
static void pds_evt_send(pm_evt_t *p_event)
{
	p_event->conn_handle = BLE_CONN_HANDLE_INVALID;

	for (uint32_t i = 0; i < PDS_EVENT_HANDLERS_CNT; i++) {
		m_evt_handlers[i](p_event);
	}
}

/* Function to convert peer IDs to file IDs. */
static uint16_t peer_id_to_file_id(pm_peer_id_t peer_id)
{
	return (uint16_t)(peer_id + PEER_ID_TO_FILE_ID);
}

/* Function to convert peer data id to type id. */
static pm_peer_id_t file_id_to_peer_id(uint16_t file_id)
{
	return (pm_peer_id_t)(file_id + FILE_ID_TO_PEER_ID);
}

/* Function to convert peer data IDs to record keys. */
static uint16_t peer_data_id_to_record_key(pm_peer_data_id_t peer_data_id)
{
	return (uint16_t)(peer_data_id + DATA_ID_TO_RECORD_KEY);
}

/* Function to convert record keys to peer data IDs. */
static pm_peer_data_id_t record_key_to_peer_data_id(uint16_t record_key)
{
	return (pm_peer_data_id_t)(record_key + RECORD_KEY_TO_DATA_ID);
}

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

/* Function for deleting all data beloning to a peer.
 * These operations will be sent to FDS one at a time.
 */
static void peer_data_delete_process(void)
{
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
}

static uint32_t peer_data_find(pm_peer_id_t peer_id, pm_peer_data_id_t data_id,
				 fds_record_desc_t *const p_desc)
{
	uint32_t ret;
	fds_find_token_t ftok;

	NRF_PM_DEBUG_CHECK(peer_id < PM_PEER_ID_N_AVAILABLE_IDS);
	NRF_PM_DEBUG_CHECK(peer_data_id_is_valid(data_id));
	NRF_PM_DEBUG_CHECK(p_desc != NULL);

	memset(&ftok, 0x00, sizeof(fds_find_token_t));

	uint16_t file_id = peer_id_to_file_id(peer_id);
	uint16_t record_key = peer_data_id_to_record_key(data_id);

	ret = fds_record_find(file_id, record_key, p_desc, &ftok);

	if (ret != NRF_SUCCESS) {
		return NRF_ERROR_NOT_FOUND;
	}

	return NRF_SUCCESS;
}

static void peer_ids_load(void)
{
	fds_record_desc_t record_desc;
	fds_flash_record_t record;
	fds_find_token_t ftok;

	memset(&ftok, 0x00, sizeof(fds_find_token_t));

	uint16_t const record_key = peer_data_id_to_record_key(PM_PEER_DATA_ID_BONDING);

	while (fds_record_find_by_key(record_key, &record_desc, &ftok) == NRF_SUCCESS) {
		pm_peer_id_t peer_id;

		/* It is safe to ignore the return value since the descriptor was
		 * just obtained and also 'record' is different from NULL.
		 */
		(void)fds_record_open(&record_desc, &record);
		peer_id = file_id_to_peer_id(record.p_header->file_id);
		(void)fds_record_close(&record_desc);

		(void)peer_id_allocate(peer_id);
	}
}

static void fds_evt_handler(fds_evt_t const *const p_fds_evt)
{
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
}

uint32_t pds_init(void)
{
	uint32_t ret;

	/* Check for re-initialization if debugging. */
	NRF_PM_DEBUG_CHECK(!m_module_initialized);

	ret = fds_register(fds_evt_handler);
	if (ret != NRF_SUCCESS) {
		LOG_ERR("Could not initialize flash storage. fds_register() returned 0x%x.", ret);
		return NRF_ERROR_INTERNAL;
	}

	ret = fds_init();
	if (ret != NRF_SUCCESS) {
		LOG_ERR("Could not initialize flash storage. fds_init() returned 0x%x.", ret);
		return NRF_ERROR_RESOURCES;
	}

	peer_id_init();
	peer_ids_load();

	m_module_initialized = true;

	return NRF_SUCCESS;
}

uint32_t pds_peer_data_read(pm_peer_id_t peer_id, pm_peer_data_id_t data_id,
			      pm_peer_data_t *const p_data, uint32_t const *const p_buf_len)
{
	uint32_t ret;
	fds_record_desc_t rec_desc;
	fds_flash_record_t rec_flash;

	NRF_PM_DEBUG_CHECK(m_module_initialized);
	NRF_PM_DEBUG_CHECK(p_data != NULL);

	VERIFY_PEER_ID_IN_RANGE(peer_id);
	VERIFY_PEER_DATA_ID_IN_RANGE(data_id);

	ret = peer_data_find(peer_id, data_id, &rec_desc);

	if (ret != NRF_SUCCESS) {
		return NRF_ERROR_NOT_FOUND;
	}

	/* Shouldn't fail, unless the record was deleted in the meanwhile or the CRC check has
	 * failed.
	 */
	ret = fds_record_open(&rec_desc, &rec_flash);

	if (ret != NRF_SUCCESS) {
		return NRF_ERROR_NOT_FOUND;
	}

	p_data->data_id = data_id;
	p_data->length_words = rec_flash.p_header->length_words;

	/* If p_buf_len is NULL, provide a pointer to data in flash, otherwise,
	 * check that the buffer is large enough and copy the data in flash into the buffer.
	 */
	if (p_buf_len == NULL) {
		/* The cast is necessary because if no buffer is provided, we just copy the pointer,
		 * but in that case it should be considered a pointer to const data by the caller,
		 * since it is a pointer to data in flash.
		 */
		p_data->p_all_data = (void *)rec_flash.p_data;
	} else {
		uint32_t const data_len_bytes = (p_data->length_words * sizeof(uint32_t));
		uint32_t const copy_len_bytes =
			MIN((*p_buf_len), (p_data->length_words * sizeof(uint32_t)));

		memcpy(p_data->p_all_data, rec_flash.p_data, copy_len_bytes);

		if (copy_len_bytes < data_len_bytes) {
			return NRF_ERROR_DATA_SIZE;
		}
	}

	/* Shouldn't fail unless the record was already closed, in which case it can be ignored. */
	(void)fds_record_close(&rec_desc);

	return NRF_SUCCESS;
}

void pds_peer_data_iterate_prepare(void)
{
	memset(&m_fds_ftok, 0x00, sizeof(fds_find_token_t));
}

bool pds_peer_data_iterate(pm_peer_data_id_t data_id, pm_peer_id_t *const p_peer_id,
			   pm_peer_data_flash_t *const p_data)
{
	uint32_t ret;
	uint16_t rec_key;
	fds_record_desc_t rec_desc;
	fds_flash_record_t rec_flash;

	NRF_PM_DEBUG_CHECK(m_module_initialized);
	NRF_PM_DEBUG_CHECK(p_peer_id != NULL);
	NRF_PM_DEBUG_CHECK(p_data != NULL);

	VERIFY_PEER_DATA_ID_IN_RANGE(data_id);

	rec_key = peer_data_id_to_record_key(data_id);

	if (fds_record_find_by_key(rec_key, &rec_desc, &m_fds_ftok) != NRF_SUCCESS) {
		return false;
	}

	ret = fds_record_open(&rec_desc, &rec_flash);

	if (ret != NRF_SUCCESS) {
		/* It can only happen if the record was deleted after the call to
		 * fds_record_find_by_key(), before we could open it, or if CRC support was enabled
		 * in Flash Data Storage at compile time and the CRC check failed.
		 */
		return false;
	}

	p_data->data_id = data_id;
	p_data->length_words = rec_flash.p_header->length_words;
	p_data->p_all_data = rec_flash.p_data;

	*p_peer_id = file_id_to_peer_id(rec_flash.p_header->file_id);

	(void)fds_record_close(&rec_desc);

	return true;
}

uint32_t pds_peer_data_store(pm_peer_id_t peer_id, pm_peer_data_const_t const *p_peer_data,
			       pm_store_token_t *p_store_token)
{
	uint32_t ret;
	fds_record_t rec;
	fds_record_desc_t rec_desc;

	NRF_PM_DEBUG_CHECK(m_module_initialized);
	NRF_PM_DEBUG_CHECK(p_peer_data != NULL);

	VERIFY_PEER_ID_IN_RANGE(peer_id);
	VERIFY_PEER_DATA_ID_IN_RANGE(p_peer_data->data_id);

	/* Prepare the record to be stored in flash. */
	rec.file_id = peer_id_to_file_id(peer_id);
	rec.key = peer_data_id_to_record_key(p_peer_data->data_id);
	rec.data.p_data = (void *)p_peer_data->p_all_data;
	rec.data.length_words = p_peer_data->length_words;

	ret = peer_data_find(peer_id, p_peer_data->data_id, &rec_desc);

	if (ret == NRF_ERROR_NOT_FOUND) {
		ret = fds_record_write(&rec_desc, &rec);
	} else {
		/* Update existing record. */
		ret = fds_record_update(&rec_desc, &rec);
	}

	switch (ret) {
	case NRF_SUCCESS:
		if (p_store_token != NULL) {
			/* Update the store token. */
			(void)fds_record_id_from_desc(&rec_desc, (uint32_t *)p_store_token);
		}
		return NRF_SUCCESS;

	case FDS_ERR_BUSY:
	case FDS_ERR_NO_SPACE_IN_QUEUES:
		return NRF_ERROR_BUSY;

	case FDS_ERR_NO_SPACE_IN_FLASH:
		return NRF_ERROR_RESOURCES;

	case FDS_ERR_UNALIGNED_ADDR:
		return NRF_ERROR_INVALID_ADDR;

	default:
		LOG_ERR("Could not write data to flash. fds_record_{write|update}() returned 0x%x. "
			"peer_id: %d",
			ret, peer_id);
		return NRF_ERROR_INTERNAL;
	}
}

uint32_t pds_peer_data_delete(pm_peer_id_t peer_id, pm_peer_data_id_t data_id)
{
	uint32_t ret;
	fds_record_desc_t record_desc;

	NRF_PM_DEBUG_CHECK(m_module_initialized);

	VERIFY_PEER_ID_IN_RANGE(peer_id);
	VERIFY_PEER_DATA_ID_IN_RANGE(data_id);

	ret = peer_data_find(peer_id, data_id, &record_desc);

	if (ret != NRF_SUCCESS) {
		return NRF_ERROR_NOT_FOUND;
	}

	ret = fds_record_delete(&record_desc);

	switch (ret) {
	case NRF_SUCCESS:
		return NRF_SUCCESS;

	case FDS_ERR_NO_SPACE_IN_QUEUES:
		return NRF_ERROR_BUSY;

	default:
		LOG_ERR("Could not delete peer. fds_record_delete() returned 0x%x. peer_id: %d, "
			"data_id: %d.",
			ret, peer_id, data_id);
		return NRF_ERROR_INTERNAL;
	}
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
