/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <nrf_ble_qwr.h>
#include <ble.h>
#include <zephyr/sys/byteorder.h>

/* Non-zero value used to make sure the given structure has been initialized by the module. */
#define NRF_BLE_QWR_INITIALIZED 0xAABBCCDD

/**
 * @brief Function for decoding a uint16 value.
 *
 * @param[in] encoded_data Buffer where the encoded data is stored.
 *
 * @return Decoded value.
 */
static inline uint16_t uint16_decode(const uint8_t *encoded_data)
{
	return sys_get_le16(encoded_data);
}

int nrf_ble_qwr_init(struct nrf_ble_qwr *qwr, struct nrf_ble_qwr_init const *qwr_init)
{
	if (!qwr || !qwr_init) {
		return -EFAULT;
	}

	if (qwr->initialized == NRF_BLE_QWR_INITIALIZED) {
		return -EPERM;
	}

	qwr->error_handler = qwr_init->error_handler;
	qwr->initialized = NRF_BLE_QWR_INITIALIZED;
	qwr->conn_handle = BLE_CONN_HANDLE_INVALID;
#if (CONFIG_NRF_BLE_QWR_MAX_ATTR > 0)
	memset(qwr->attr_handles, 0, sizeof(qwr->attr_handles));
	qwr->nb_registered_attr = 0;
	qwr->is_user_mem_reply_pending = false;
	qwr->mem_buffer = qwr_init->mem_buffer;
	qwr->callback = qwr_init->callback;
	qwr->nb_written_handles = 0;
#endif
	return 0;
}

#if (CONFIG_NRF_BLE_QWR_MAX_ATTR > 0)
int nrf_ble_qwr_attr_register(struct nrf_ble_qwr *qwr, uint16_t attr_handle)
{
	if (!qwr) {
		return -EFAULT;
	}

	if (qwr->initialized != NRF_BLE_QWR_INITIALIZED) {
		return -EPERM;
	}

	if ((qwr->nb_registered_attr == CONFIG_NRF_BLE_QWR_MAX_ATTR) ||
	    (qwr->mem_buffer.p_mem == NULL) ||
	    (qwr->mem_buffer.len == 0))	{
		return -ENOMEM;
	}

	if (attr_handle == BLE_GATT_HANDLE_INVALID) {
		return -EINVAL;
	}

	qwr->attr_handles[qwr->nb_registered_attr] = attr_handle;
	qwr->nb_registered_attr++;

	return 0;
}

int nrf_ble_qwr_value_get(
	struct nrf_ble_qwr *qwr, uint16_t attr_handle, uint8_t *mem, uint16_t *len)
{
	uint16_t i = 0;
	uint16_t handle = BLE_GATT_HANDLE_INVALID;
	uint16_t val_len = 0;
	uint16_t val_offset = 0;
	uint32_t cur_len = 0;

	if (!qwr || !mem || !len) {
		return -EFAULT;
	}

	if (qwr->initialized != NRF_BLE_QWR_INITIALIZED) {
		return -EPERM;
	}

	do {
		handle = uint16_decode(&(qwr->mem_buffer.p_mem[i]));
		if (handle == BLE_GATT_HANDLE_INVALID) {
			break;
		}

		i += sizeof(uint16_t);
		val_offset = uint16_decode(&(qwr->mem_buffer.p_mem[i]));
		i += sizeof(uint16_t);
		val_len = uint16_decode(&(qwr->mem_buffer.p_mem[i]));
		i += sizeof(uint16_t);

		if (handle == attr_handle) {
			cur_len = val_offset + val_len;
			if (cur_len <= *len) {
				memcpy((mem + val_offset), &(qwr->mem_buffer.p_mem[i]), val_len);
			} else {
				return -ENOMEM;
			}
		}

		i += val_len;
	} while (i < qwr->mem_buffer.len);

	*len = cur_len;
	return 0;
}
#endif

int nrf_ble_qwr_conn_handle_assign(struct nrf_ble_qwr *qwr, uint16_t conn_handle)
{
	if (!qwr) {
		return -EFAULT;
	}

	if (qwr->initialized != NRF_BLE_QWR_INITIALIZED) {
		return -EPERM;
	}

	qwr->conn_handle = conn_handle;

	return 0;
}

/**
 * @brief Checks if a user_mem_reply is pending, if so attempts to send it.
 *
 * @param[in] qwr QWR structure.
 */
static void user_mem_reply(struct nrf_ble_qwr *qwr)
{
	int err;

	if (qwr->is_user_mem_reply_pending) {
#if (CONFIG_NRF_BLE_QWR_MAX_ATTR == 0)
		err = sd_ble_user_mem_reply(qwr->conn_handle, NULL);
#else
		err = sd_ble_user_mem_reply(qwr->conn_handle,
					    (ble_user_mem_block_t const *)&qwr->mem_buffer);
#endif
		if (err == NRF_SUCCESS) {
			qwr->is_user_mem_reply_pending = false;
		} else if (err == NRF_ERROR_BUSY) {
			qwr->is_user_mem_reply_pending = true;
		} else {
			qwr->error_handler(err);
		}
	}
}

/**
 * @brief Handle a user memory request event.
 *
 * @param[in] qwr QWR structure.
 * @param[in] evt User_mem_request event to be handled.
 */
static void on_user_mem_request(struct nrf_ble_qwr *qwr, ble_common_evt_t const *evt)
{
	if ((evt->params.user_mem_request.type == BLE_USER_MEM_TYPE_GATTS_QUEUED_WRITES) &&
	    (evt->conn_handle == qwr->conn_handle)) {
		qwr->is_user_mem_reply_pending = true;
		user_mem_reply(qwr);
	}
}

/**
 * @brief Handle a user memory release event.
 *
 * @param[in] qwr QWR structure.
 * @param[in] evt User_mem_release event to be handled.
 */
static void on_user_mem_release(struct nrf_ble_qwr *qwr, ble_common_evt_t const *evt)
{
#if (CONFIG_NRF_BLE_QWR_MAX_ATTR > 0)
	if ((evt->params.user_mem_release.type == BLE_USER_MEM_TYPE_GATTS_QUEUED_WRITES) &&
	    (evt->conn_handle == qwr->conn_handle)) {
		/* Cancel the current operation. */
		qwr->nb_written_handles = 0;
	}
#endif
}

#if (CONFIG_NRF_BLE_QWR_MAX_ATTR > 0)
/**
 * @brief Handle a prepare write event.
 *
 * @param[in] qwr QWR structure.
 * @param[in] evt WRITE event to be handled.
 */
static void on_prepare_write(struct nrf_ble_qwr *qwr, ble_gatts_evt_write_t const *evt)
{
	int err;
	ble_gatts_rw_authorize_reply_params_t auth_reply = {};

	auth_reply.params.write.gatt_status = NRF_BLE_QWR_REJ_REQUEST_ERR_CODE;
	auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;

	uint32_t i;

	for (i = 0; i < qwr->nb_written_handles; i++) {
		if (qwr->written_attr_handles[i] == evt->handle) {
			auth_reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
			break;
		}
	}

	if (auth_reply.params.write.gatt_status != BLE_GATT_STATUS_SUCCESS) {
		for (i = 0; i < qwr->nb_registered_attr; i++) {
			if (qwr->attr_handles[i] == evt->handle) {
				auth_reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
				qwr->written_attr_handles[qwr->nb_written_handles++] =
					evt->handle;
				break;
			}
		}
	}

	err = sd_ble_gatts_rw_authorize_reply(qwr->conn_handle, &auth_reply);
	if (err != NRF_SUCCESS) {
		/* Cancel the current operation. */
		qwr->nb_written_handles = 0;

		/* Report error to application. */
		qwr->error_handler(err);
	}
}

/**
 * @brief Handle an execute write event.
 *
 * @param[in] qwr QWR structure.
 * @param[in] evt EXEC WRITE event to be handled.
 */
static void on_execute_write(struct nrf_ble_qwr *qwr, ble_gatts_evt_write_t const *write_evt)
{
	uint32_t err;
	uint16_t ret_val;
	ble_gatts_rw_authorize_reply_params_t auth_reply = {};
	struct nrf_ble_qwr_evt evt;

	auth_reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
	auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;

	if (qwr->nb_written_handles == 0) {
		auth_reply.params.write.gatt_status = NRF_BLE_QWR_REJ_REQUEST_ERR_CODE;
		err = sd_ble_gatts_rw_authorize_reply(qwr->conn_handle, &auth_reply);
		if (err != NRF_SUCCESS) {
			/* Report error to application. */
			qwr->error_handler(err);
		}

		return;
	}

	for (uint16_t i = 0; i < qwr->nb_written_handles; i++) {
		evt.evt_type = NRF_BLE_QWR_EVT_AUTH_REQUEST;
		evt.attr_handle = qwr->written_attr_handles[i];
		ret_val = qwr->callback(qwr, &evt);
		if (ret_val != BLE_GATT_STATUS_SUCCESS) {
			auth_reply.params.write.gatt_status = ret_val;
		}
	}

	err = sd_ble_gatts_rw_authorize_reply(qwr->conn_handle, &auth_reply);
	if (err != NRF_SUCCESS) {
		/* Report error to application. */
		qwr->error_handler(err);
	}

	/* If the execute has not been rejected by any of the registered applications,
	 * propagate execute write event to all written handles.
	 */
	if (auth_reply.params.write.gatt_status == BLE_GATT_STATUS_SUCCESS) {
		for (uint16_t i = 0; i < qwr->nb_written_handles; i++) {
			evt.evt_type = NRF_BLE_QWR_EVT_EXECUTE_WRITE;
			evt.attr_handle = qwr->written_attr_handles[i];

			(void)qwr->callback(qwr, &evt);

			auth_reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
		}
	}
	qwr->nb_written_handles = 0;
}

/**
 * @brief Handle a cancel write event.
 *
 * @param[in] qwr QWR structure.
 * @param[in] evt EXEC WRITE event to be handled.
 */
static void on_cancel_write(struct nrf_ble_qwr *qwr, ble_gatts_evt_write_t const *write_evt)
{
	uint32_t err;
	ble_gatts_rw_authorize_reply_params_t auth_reply = {};

	auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
	auth_reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;

	err = sd_ble_gatts_rw_authorize_reply(qwr->conn_handle, &auth_reply);
	if (err != NRF_SUCCESS) {
		/* Report error to application. */
		qwr->error_handler(err);
	}
	qwr->nb_written_handles = 0;
}
#endif

/**
 * @brief Handle a rw_authorize_request event.
 *
 * @param[in] qwr QWR structure.
 * @param[in] evt RW_authorize_request event to be handled.
 */
static void on_rw_authorize_request(struct nrf_ble_qwr *qwr, ble_gatts_evt_t const *evt)
{
#if (CONFIG_NRF_BLE_QWR_MAX_ATTR == 0)
	uint32_t err;
	ble_gatts_rw_authorize_reply_params_t auth_reply = {0};
#endif

	if (evt->conn_handle != qwr->conn_handle) {
		return;
	}

	const ble_gatts_evt_rw_authorize_request_t *p_auth_req = &evt->params.authorize_request;

	if (p_auth_req->type != BLE_GATTS_AUTHORIZE_TYPE_WRITE) {
		return;
	}

#if (CONFIG_NRF_BLE_QWR_MAX_ATTR == 0)
	/* Handle only queued write related operations. */
	if ((p_auth_req->request.write.op != BLE_GATTS_OP_PREP_WRITE_REQ) &&
	    (p_auth_req->request.write.op != BLE_GATTS_OP_EXEC_WRITE_REQ_NOW) &&
	    (p_auth_req->request.write.op != BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL)) {
		return;
	}

	/* Prepare the response. */
	auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
	auth_reply.params.write.gatt_status = NRF_BLE_QWR_REJ_REQUEST_ERR_CODE;

	if (p_auth_req->request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL) {
		auth_reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
	}

	err = sd_ble_gatts_rw_authorize_reply(evt->conn_handle, &auth_reply);
	if (err != NRF_SUCCESS) {
		/* Report error to application. */
		qwr->error_handler(err);
	}
#else
	switch (p_auth_req->request.write.op) {
	case BLE_GATTS_OP_PREP_WRITE_REQ:
		on_prepare_write(qwr, &p_auth_req->request.write);
		break;
	case BLE_GATTS_OP_EXEC_WRITE_REQ_NOW:
		on_execute_write(qwr, &p_auth_req->request.write);
		break;
	case BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL:
		on_cancel_write(qwr, &p_auth_req->request.write);
		break;
	default:
		/* No implementation needed. */
		break;
	}
#endif
}

void nrf_ble_qwr_on_ble_evt(ble_evt_t const *ble_evt, void *context)
{
	struct nrf_ble_qwr *qwr;

	if (!context || !ble_evt) {
		return;
	}

	qwr = (struct nrf_ble_qwr *)context;

	if (qwr->initialized != NRF_BLE_QWR_INITIALIZED) {
		return;
	}

	if (ble_evt->evt.common_evt.conn_handle == qwr->conn_handle) {
		user_mem_reply(qwr);
	}

	switch (ble_evt->header.evt_id) {
	case BLE_EVT_USER_MEM_REQUEST:
		on_user_mem_request(qwr, &ble_evt->evt.common_evt);
		break;
	case BLE_EVT_USER_MEM_RELEASE:
		on_user_mem_release(qwr, &ble_evt->evt.common_evt);
		break;
	case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
		on_rw_authorize_request(qwr, &ble_evt->evt.gatts_evt);
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		if (ble_evt->evt.gap_evt.conn_handle == qwr->conn_handle) {
			qwr->conn_handle = BLE_CONN_HANDLE_INVALID;
#if (CONFIG_NRF_BLE_QWR_MAX_ATTR > 0)
			qwr->nb_written_handles = 0;
#endif
		}
		break;
	default:
		break;
	}
}
