/*
 * Copyright (c) 2018-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_error.h>
#include <stdbool.h>
#include <stdint.h>
#include <bm/bluetooth/ble_gq.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/slist.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(ble_gatt_queue, CONFIG_BLE_GQ_LOG_LEVEL);

/* Function prototype for preparing a request for storage.
 *
 * Functions of this type should:
 * 1. Allocate memory for additional request data.
 * 2. Copy request to the storage buffer.
 *
 * data_pool  Memory pool for storing additional request data.
 * req        Request to be stored.
 * req_buf    Request storage buffer.
 *
 * Should return:
 * NRF_SUCCESS if successful.
 * NRF_ERROR_NO_MEM if there are no room in the data pool for a new allocation.
 */
typedef uint32_t (*req_data_store_t)(struct k_heap *data_pool, const struct ble_gq_req *req,
				     struct ble_gq_req *req_buf);

/* Prepare a GATTC write request for storage. */
static uint32_t gattc_write_store(struct k_heap *data_pool, const struct ble_gq_req *req,
				  struct ble_gq_req *req_buf)
{
	const ble_gattc_write_params_t *const gattc_write = &req->gattc_write;
	uint8_t *data;

	/* Allocate additional memory for GATTC write request data. */
	data = k_heap_aligned_alloc(data_pool, sizeof(void *), gattc_write->len, K_NO_WAIT);
	if (data == NULL) {
		return NRF_ERROR_NO_MEM;
	}

	LOG_DBG("Allocated heap memory with addr: %#lx", (uintptr_t)data);

	/* Copy relevant data to the allocated heap space. */
	memcpy(data, (void *)gattc_write->p_value, gattc_write->len);

	/* Copy request to storage. */
	*req_buf = *req;

	/* Update pointers to point onto allocated data. */
	req_buf->data = data;
	req_buf->gattc_write.p_value = data;

	return NRF_SUCCESS;
}

/* Prepare a GATTS notification or indication request for storage. */
static uint32_t gatts_hvx_store(struct k_heap *data_pool, const struct ble_gq_req *req,
				struct ble_gq_req *req_buf)
{
	const ble_gatts_hvx_params_t *const gatts_hvx = &req->gatts_hvx;
	uint8_t *data;

	/* Allocate additional memory for GATTS notification or indication request data. */
	data = k_heap_aligned_alloc(data_pool, sizeof(void *), *gatts_hvx->p_len + sizeof(uint16_t),
				    K_NO_WAIT);
	if (data == NULL) {
		return NRF_ERROR_NO_MEM;
	}

	LOG_DBG("Allocated heap memory with addr: %#lx", (uintptr_t)data);

	/* Copy relevant data to the allocated heap space. */
	memcpy(&data[0], (void *)gatts_hvx->p_len, sizeof(uint16_t));
	memcpy(&data[sizeof(uint16_t)], (void *)gatts_hvx->p_data, *gatts_hvx->p_len);

	/* Copy request to storage. */
	*req_buf = *req;

	/* Update pointers to point onto allocated data. */
	req_buf->data = data;
	req_buf->gatts_hvx.p_len = (uint16_t *)&data[0];
	req_buf->gatts_hvx.p_data = &data[sizeof(uint16_t)];

	return NRF_SUCCESS;
}

/* Array of memory store functions for different GATT request types. */
static const req_data_store_t req_data_store[BLE_GQ_REQ_NUM] = {
	[BLE_GQ_REQ_GATTC_READ] = NULL,
	[BLE_GQ_REQ_GATTC_WRITE] = gattc_write_store,
	[BLE_GQ_REQ_SRV_DISCOVERY] = NULL,
	[BLE_GQ_REQ_CHAR_DISCOVERY] = NULL,
	[BLE_GQ_REQ_DESC_DISCOVERY] = NULL,
	[BLE_GQ_REQ_GATTS_HVX] = gatts_hvx_store,
};

static void request_error_handle(const struct ble_gq_req *req, uint16_t conn_handle,
				 uint32_t nrf_err)
{
	if (nrf_err == NRF_SUCCESS) {
		LOG_DBG("SD GATT procedure (%d) succeeded on connection handle: %d.", req->type,
			conn_handle);
	} else {
		LOG_DBG("SD GATT procedure (%d) failed on connection handle %d with nrf_error %#x",
			req->type, conn_handle, nrf_err);
		if (req->error_handler.cb != NULL) {
			req->error_handler.cb(conn_handle, nrf_err, req->error_handler.ctx);
		}
	}
}

/* Process a single GATT request. */
static bool request_process(const struct ble_gq_req *req, uint16_t conn_handle)
{
	uint32_t nrf_err;
	uint16_t len;

	switch (req->type) {
	case BLE_GQ_REQ_GATTC_READ:
		LOG_DBG("GATTC read request");
		nrf_err = sd_ble_gattc_read(conn_handle, req->gattc_read.handle,
					    req->gattc_read.offset);
		break;
	case BLE_GQ_REQ_GATTC_WRITE:
		LOG_DBG("GATTC write request");
		nrf_err = sd_ble_gattc_write(conn_handle, &req->gattc_write);
		break;
	case BLE_GQ_REQ_SRV_DISCOVERY:
		LOG_DBG("GATTC primary services discovery request");
		nrf_err = sd_ble_gattc_primary_services_discover(conn_handle,
								 req->gattc_srv_disc.start_handle,
								 &req->gattc_srv_disc.srvc_uuid);
		break;
	case BLE_GQ_REQ_CHAR_DISCOVERY:
		LOG_DBG("GATTC characteristics discovery request");
		nrf_err = sd_ble_gattc_characteristics_discover(conn_handle,
								&req->gattc_char_disc);
		break;
	case BLE_GQ_REQ_DESC_DISCOVERY:
		LOG_DBG("GATTC characteristic descriptors discovery request");
		nrf_err = sd_ble_gattc_descriptors_discover(conn_handle, &req->gattc_desc_disc);
		break;
	case BLE_GQ_REQ_GATTS_HVX:
		LOG_DBG("GATTS notification or indication");
		if (req->gatts_hvx.p_len == NULL) {
			LOG_DBG("GATTS HVX request p_len is NULL");
			nrf_err = NRF_ERROR_INVALID_PARAM;
			break;
		}
		len = *(req->gatts_hvx.p_len);
		nrf_err = sd_ble_gatts_hvx(conn_handle, &req->gatts_hvx);
		if (nrf_err == NRF_SUCCESS && len != *(req->gatts_hvx.p_len)) {
			nrf_err = NRF_ERROR_DATA_SIZE;
		}
		break;
	default:
		nrf_err = NRF_ERROR_NOT_SUPPORTED;
		LOG_WRN("Unimplemented GATT request with type: %d", req->type);
		break;
	}

	if (nrf_err == NRF_ERROR_BUSY) {
		LOG_DBG("SD is currently busy. The GATT procedure will be attempted again later.");

		/* SoftDevice was busy. */
		return false;
	}

	request_error_handle(req, conn_handle, nrf_err);

	/* Request was accepted by SoftDevice. */
	return true;
}

/* Process a request from the GATT queue instance. */
static void queue_process(const struct ble_gq *gq, uint16_t conn_handle, uint16_t conn_id)
{
	sys_snode_t *elem;
	struct ble_gq_req *req;

	elem = sys_slist_peek_head(&gq->req_queue[conn_id]);
	if (elem == NULL) {
		/* Queue is empty */
		return;
	}

	req = CONTAINER_OF(elem, struct ble_gq_req, node);

	const bool req_processed = request_process(req, conn_handle);

	if (!req_processed) {
		return;
	}

	/* Peeking was successful above. Queue should have at least one element.
	 * The first element is already known. Dequeue it.
	 */
	(void)sys_slist_get_not_empty(&gq->req_queue[conn_id]);

	/* Clear any additional data associated with the request. */
	if (req->type >= BLE_GQ_REQ_NUM || req_data_store[req->type] != NULL) {
		LOG_DBG("Freeing heap memory with addr %#lx", (uintptr_t)req->data);
		k_heap_free(gq->data_pool, req->data);
	}

	/* Release the memory block back to its associated memory slab. */
	k_mem_slab_free(gq->req_blocks, req);
}

/* Clear all requests from the queue identified by the conn_id. */
static void req_queue_clear(const struct ble_gq *gq, uint16_t conn_id)
{
	sys_snode_t *elem;
	struct ble_gq_req *req;

	while (true) {
		elem = sys_slist_get(&gq->req_queue[conn_id]);
		if (elem == NULL) {
			/* Clearing of request queue is done. Request queue is empty. */
			break;
		}

		/* Based on offset, get pointer to the structure containing the list element. */
		req = CONTAINER_OF(elem, struct ble_gq_req, node);

		/* Clear any additional data associated with the request. */
		if (req_data_store[req->type] != NULL) {
			LOG_DBG("Freeing heap memory with addr %#lx", (uintptr_t)req->data);
			k_heap_free(gq->data_pool, req->data);
		}

		/* Release the memory block back to its associated memory slab. */
		k_mem_slab_free(gq->req_blocks, req);
	}
}

/* Clear all requests from all queues marked for purging. */
static void req_queues_purge(const struct ble_gq *gq)
{
	uint16_t conn_id;

	__ASSERT_NO_MSG(gq != NULL);

	for (uint32_t i = 0; i < gq->max_conns; i++) {
		conn_id = gq->purge_list[i];
		if (conn_id >= gq->max_conns) {
			continue;
		}

		LOG_DBG("Purging request queue with id: %d", conn_id);

		req_queue_clear(gq, conn_id);
		gq->purge_list[i] = gq->max_conns;
	}
}

/* Mark request queue identified with conn_id for purging.
 * All pending requests in marked queues will be freed when req_queues_purge is invoked.
 */
static void req_queue_purge_schedule(const struct ble_gq *gq, uint16_t conn_id)
{
	uint16_t idx;

	for (idx = 0; idx < gq->max_conns; idx++) {
		if (gq->purge_list[idx] >= gq->max_conns) {
			gq->purge_list[idx] = conn_id;
			break;
		}
	}

	/* Assert if there was no space in array to schedule purge of a request queue. */
	__ASSERT_NO_MSG(idx == gq->max_conns);
}

/* Find connection ID for the provided connection handle within the GATT queue instance registery.
 *
 * Returns the connection ID, or gq->max_conns if a connection ID matching the connection handle
 * was not found.
 */
static uint16_t conn_handle_id_find(const struct ble_gq *gq, uint16_t conn_handle)
{
	for (uint16_t id = 0; id < gq->max_conns; id++) {
		if (conn_handle == gq->conn_handles[id]) {
			return id;
		}
	}

	return gq->max_conns;
}

/* Register the provided connection handle within the GATT queue instance registery. */
static uint32_t conn_handle_register(const struct ble_gq *gq, uint16_t conn_handle)
{
	uint16_t unused_id = gq->max_conns;

	for (uint16_t id = 0; id < gq->max_conns; id++) {
		if (gq->conn_handles[id] == conn_handle) {
			return NRF_SUCCESS;
		}
		if (gq->conn_handles[id] == BLE_CONN_HANDLE_INVALID && unused_id == gq->max_conns) {
			unused_id = id;
		}
	}

	if (unused_id == gq->max_conns) {
		return NRF_ERROR_NO_MEM;
	}

	gq->conn_handles[unused_id] = conn_handle;
	return NRF_SUCCESS;
}

uint32_t ble_gq_item_add(const struct ble_gq *gq, struct ble_gq_req *req, uint16_t conn_handle)
{
	int err;
	uint32_t nrf_err;
	uint16_t conn_id;
	struct ble_gq_req *buffered_req;

	if (gq == NULL || req == NULL) {
		return NRF_ERROR_NULL;
	}

	/* Purge queues that are no longer used by any connection. */
	req_queues_purge(gq);

	/* Check if connection handle is registered and if GATT request is valid. */
	conn_id = conn_handle_id_find(gq, conn_handle);
	if (req->type >= BLE_GQ_REQ_NUM || conn_id >= gq->max_conns) {
		return NRF_ERROR_INVALID_PARAM;
	}

	/* Try processing a request without buffering. */
	if (sys_slist_is_empty(&gq->req_queue[conn_id])) {
		const bool req_processed = request_process(req, conn_handle);

		if (req_processed) {
			return NRF_SUCCESS;
		}
	}

	/* Request must be buffered for later processing. */

	/* Allocate memory for holding the request. */
	__ASSERT_NO_MSG(gq->req_blocks != NULL);
	err = k_mem_slab_alloc(gq->req_blocks, (void **)&buffered_req, K_NO_WAIT);
	if (err) {
		return NRF_ERROR_NO_MEM;
	}

	/* Allocate extra memory if required by the request type. */
	if (req_data_store[req->type] != NULL) {
		__ASSERT_NO_MSG(gq->data_pool != NULL);
		nrf_err = req_data_store[req->type](gq->data_pool, req, buffered_req);
		if (nrf_err) {
			k_mem_slab_free(gq->req_blocks, buffered_req);
			return nrf_err;
		}
	} else {
		/* Copy request. No extra memory needed. */
		*buffered_req = *req;
	}

	/* Queue request for later processing when SoftDevice is ready (not busy). */
	sys_slist_append(&gq->req_queue[conn_id], &buffered_req->node);

	/* Check if SoftDevice is still busy. */
	queue_process(gq, conn_handle, conn_id);
	return NRF_SUCCESS;
}

uint32_t ble_gq_conn_handle_register(const struct ble_gq *gq, uint16_t conn_handle)
{
	uint32_t nrf_err;

	if (gq == NULL) {
		return NRF_ERROR_NULL;
	}

	/* Purge the queues that are no longer used by any connection. */
	req_queues_purge(gq);

	/* Find a free spot in the connection handle registery and register the connection. */
	nrf_err = conn_handle_register(gq, conn_handle);
	if (nrf_err) {
		LOG_DBG("Failed to register connection handle 0x%04x", conn_handle);
		return nrf_err;
	}

	LOG_DBG("Registered connection handle 0x%04x", conn_handle);
	return NRF_SUCCESS;
}

void ble_gq_on_ble_evt(const ble_evt_t *ble_evt, void *gatt_queue)
{
	const struct ble_gq *const gq = (struct ble_gq *)gatt_queue;
	uint16_t conn_handle;
	uint16_t conn_id;

	if (ble_evt == NULL || gq == NULL) {
		return;
	}

	/* Obtain connection handle and filter out events that do not trigger queue processing. */
	if (ble_evt->header.evt_id == BLE_GAP_EVT_DISCONNECTED) {
		conn_handle = ble_evt->evt.gap_evt.conn_handle;
	} else if (IN_RANGE(ble_evt->header.evt_id, BLE_GATTC_EVT_BASE, BLE_GATTC_EVT_LAST)) {
		conn_handle = ble_evt->evt.gattc_evt.conn_handle;
	} else if (IN_RANGE(ble_evt->header.evt_id, BLE_GATTS_EVT_BASE, BLE_GATTS_EVT_LAST)) {
		conn_handle = ble_evt->evt.gatts_evt.conn_handle;
	} else {
		/* Irrelevant event for this module. Do nothing. */
		return;
	}

	/* Check if connection handle is registered. */
	conn_id = conn_handle_id_find(gq, conn_handle);
	if (conn_id >= gq->max_conns) {
		return;
	}

	/* Perform operation on the queue. */
	if (ble_evt->header.evt_id == BLE_GAP_EVT_DISCONNECTED) {
		/* Remove connection from GATT queue registry on a disconnect event. */
		gq->conn_handles[conn_id] = BLE_CONN_HANDLE_INVALID;

		/* Signal a purge of the request queue on a disconnect event. */
		req_queue_purge_schedule(gq, conn_id);
	} else {
		/* Check if SoftDevice is still busy. */
		queue_process(gq, conn_handle, conn_id);
	}
}
