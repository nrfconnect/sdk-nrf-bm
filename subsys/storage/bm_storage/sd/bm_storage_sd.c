/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <nrf_soc.h>
#include <nrf_sdm.h>
#include <nrf_error.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/storage/bm_storage.h>
#include <bm/storage/bm_storage_backend.h>
#include <zephyr/irq.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/ring_buffer.h>

/* 128-bit word line. This is the optimal size to fully utilize RRAM 128-bit word line with ECC
 * (error correction code) and minimize ECC updates overhead, due to these updates happening
 * per-line.
 */
#define SD_WRITE_BLOCK_SIZE 16

struct bm_storage_sd_op {
	/* The bm_storage instance that requested the operation. */
	const struct bm_storage *storage;
	/* User-defined parameter passed to the event handler. */
	void *ctx;
	/* Data to be written to non-volatile memory. */
	const void *src;
	/* Destination of the data in non-volatile memory. */
	uint32_t dest;
	/* Length of the data to be written (in bytes). */
	uint32_t len;
	/* Write offset. */
	uint32_t offset;
};

enum bm_storage_sd_state_type {
	/* No operations requested to the SoftDevice. */
	BM_STORAGE_SD_STATE_IDLE,
	/* A non-storage operation is pending. */
	BM_STORAGE_SD_STATE_OP_PENDING,
	/* An storage operation is executing. */
	BM_STORAGE_SD_STATE_OP_EXECUTING,
};

static struct {
	/* The module is initialized. */
	bool is_init;
	/* Ensures atomic access to various states. */
	atomic_t operation_ongoing;
	/* Internal storage state. */
	enum bm_storage_sd_state_type type;
	/* Number of times an operation has been retried on timeout. */
	uint32_t retries;
	/* The SoftDevice is enabled. */
	bool sd_enabled;
	/* A SoftDevice state change is impending. */
	bool paused;
	struct bm_storage_sd_op current_operation;
} bm_storage_sd;

#ifndef CONFIG_UNITY
static
#endif
void bm_storage_sd_on_soc_evt(uint32_t evt, void *ctx);

RING_BUF_DECLARE(sd_fifo, CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE *
		 sizeof(struct bm_storage_sd_op));

static inline bool is_aligned32(uint32_t addr)
{
	return !(addr & 0x03);
}

static void event_send(const struct bm_storage_sd_op *op, bool is_sync, uint32_t result)
{
	if (op->storage->evt_handler == NULL) {
		/* Do nothing. */
		return;
	}

	struct bm_storage_evt evt = {
		.id = BM_STORAGE_EVT_WRITE_RESULT,
		.dispatch_type = (is_sync) ? BM_STORAGE_EVT_DISPATCH_SYNC :
					BM_STORAGE_EVT_DISPATCH_ASYNC,
		.result = result,
		.addr = op->dest,
		.src = op->src,
		.len = op->len,
		.ctx = op->ctx
	};

	op->storage->evt_handler(&evt);
}

static uint32_t write_execute(const struct bm_storage_sd_op *op)
{
	__ASSERT(op->len % bm_storage_info.program_unit == 0,
		 "Data length is expected to be a multiple of the program unit.");

	__ASSERT(op->offset % bm_storage_info.program_unit == 0,
		 "Offset is expected to be a multiple of the program unit.");

	/* Calculate number of 32-bit words for sd_flash_write(). */
	uint32_t chunk_len_words = (op->len - op->offset) / sizeof(uint32_t);

	/* src and dest are word-aligned. */
	uint32_t *dest = (uint32_t *)(op->dest + op->offset);
	const uint32_t *src  = (const uint32_t *)((uint32_t)op->src + op->offset);

	return sd_flash_write(dest, src, chunk_len_words);
}

static void queue_process(void)
{
	uint32_t ret;
	unsigned int key;

	if (bm_storage_sd.type == BM_STORAGE_SD_STATE_IDLE) {
		key = irq_lock();
		ret = ring_buf_get(&sd_fifo, (uint8_t *)&bm_storage_sd.current_operation,
				   sizeof(struct bm_storage_sd_op));
		irq_unlock(key);
		if (ret != sizeof(struct bm_storage_sd_op)) {
			/* No more operations left to be processed, unlock the resource. */
			atomic_set(&bm_storage_sd.operation_ongoing, 0);
			return;
		}
	}

	bm_storage_sd.type = BM_STORAGE_SD_STATE_OP_EXECUTING;

	ret = write_execute(&bm_storage_sd.current_operation);

	switch (ret) {
	case NRF_SUCCESS:
		/* The operation was accepted by the SoftDevice.
		 * If the SoftDevice is enabled, wait for a system event.
		 * Otherwise, the SoftDevice call is synchronous and will not send an event so we
		 * simulate it.
		 */
		if (!bm_storage_sd.sd_enabled) {
			bool is_sync = true;

			bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, &is_sync);
		}
		break;
	case NRF_ERROR_BUSY:
		/* The SoftDevice is executing a non-volatile memory operation that was not
		 * requested by the storage logic.
		 * Stop processing the queue until a system event is received.
		 */
		bm_storage_sd.type = BM_STORAGE_SD_STATE_OP_PENDING;
		break;
	default:
		/* An error has occurred. We cannot proceed further with this operation. */
		event_send(&bm_storage_sd.current_operation, true, -EIO);
		/* Reset the internal state so we can accept other operations. */
		bm_storage_sd.type = BM_STORAGE_SD_STATE_IDLE;
		atomic_set(&bm_storage_sd.operation_ongoing, 0);
		break;
	}
}

static void queue_start(void)
{
	if ((atomic_cas(&bm_storage_sd.operation_ongoing, 0, 1)) &&
	    (!bm_storage_sd.paused)) {
		queue_process();
	}
}

/* Write operation success callback. Keeps track of the progress of an operation. */
static bool on_operation_success(struct bm_storage_sd_op *op)
{
	/* Reset the retry counter on success. */
	bm_storage_sd.retries = 0;

	__ASSERT(op->len % bm_storage_info.program_unit == 0,
		 "Data length is expected to be a multiple of the program unit.");

	__ASSERT(op->offset % bm_storage_info.program_unit == 0,
		 "Offset is expected to be a multiple of the program unit.");

	op->offset += op->len - op->offset;

	/* Avoid missing the last chunk by rounding */
	if (op->offset >= op->len) {
		return true;
	}

	return false;
}

/* Write operation failure callback. */
static bool on_operation_failure(const struct bm_storage_sd_op *op)
{
	bm_storage_sd.retries++;

	if (bm_storage_sd.retries > CONFIG_BM_STORAGE_BACKEND_SD_MAX_RETRIES) {
		/* Maximum amount of retries reached. Give up. */
		bm_storage_sd.retries = 0;
		return true;
	}

	return false;
}

int bm_storage_backend_init(struct bm_storage *storage)
{
	/* If it's already initialized, return early successfully.
	 * This is to support more than one client initialization.
	 */
	if (bm_storage_sd.is_init) {
		return 0;
	}

	/* Initialize the SoftDevice storage backend from one context only. */
	if (!atomic_cas(&bm_storage_sd.operation_ongoing, 0, 1)) {
		return -EBUSY;
	}

	sd_softdevice_is_enabled((uint8_t *)&bm_storage_sd.sd_enabled);

	bm_storage_sd.type = BM_STORAGE_SD_STATE_IDLE;

	bm_storage_sd.is_init = true;

	atomic_set(&bm_storage_sd.operation_ongoing, 0);

	return 0;
}

int bm_storage_backend_uninit(struct bm_storage *storage)
{
	/* Do not touch the internal state.
	 * Let queued operations complete.
	 */
	return 0;
}

int bm_storage_backend_read(const struct bm_storage *storage, uint32_t src, void *dest,
			    uint32_t len)
{
	if (!bm_storage_sd.is_init) {
		return -EPERM;
	}

	/* SoftDevice expects this alignment. */
	if (!is_aligned32(src)) {
		return -EFAULT;
	}

	/* src is expected to be 32-bit word-aligned. */
	memcpy(dest, (uint32_t *)src, len);

	return 0;
}

int bm_storage_backend_write(const struct bm_storage *storage, uint32_t dest,
			     const void *src, uint32_t len, void *ctx)
{
	uint32_t written;
	unsigned int key;

	if (!bm_storage_sd.is_init) {
		return -EPERM;
	}

	/* SoftDevice expects this alignment. */
	if (!is_aligned32((uint32_t)src) || !is_aligned32(dest)) {
		return -EFAULT;
	}

	struct bm_storage_sd_op op = {
		.storage = storage,
		.ctx = ctx,
		.src = src,
		.len = len,
		.dest = dest,
	};

	key = irq_lock();
	written = ring_buf_put(&sd_fifo, (void *)&op, sizeof(struct bm_storage_sd_op));
	irq_unlock(key);

	if (written != sizeof(struct bm_storage_sd_op)) {
		return -EIO;
	}

	queue_start();

	return 0;
}

bool bm_storage_backend_is_busy(const struct bm_storage *storage)
{
	return (bm_storage_sd.type != BM_STORAGE_SD_STATE_IDLE);
}

#ifndef CONFIG_UNITY
static
#endif
int bm_storage_sd_on_state_evt(enum nrf_sdh_state_evt evt, void *ctx)
{
	switch (evt) {
	case NRF_SDH_STATE_EVT_ENABLE_PREPARE:
	case NRF_SDH_STATE_EVT_DISABLE_PREPARE:
		/* Only allow changing state when idle */
		bm_storage_sd.paused = true;
		return (bm_storage_sd.type != BM_STORAGE_SD_STATE_IDLE);

	case NRF_SDH_STATE_EVT_ENABLED:
	case NRF_SDH_STATE_EVT_DISABLED:
		/* Continue executing any operation still in the queue */
		bm_storage_sd.paused = false;
		bm_storage_sd.sd_enabled = (evt == NRF_SDH_STATE_EVT_ENABLED);
		queue_process();
		return 0;

	case NRF_SDH_STATE_EVT_BLE_ENABLED:
	default:
		/* Not interesting */
		return 0;
	}
}
NRF_SDH_STATE_EVT_OBSERVER(sdh_state_evt, bm_storage_sd_on_state_evt, NULL, HIGH);

#ifndef CONFIG_UNITY
static
#endif
void bm_storage_sd_on_soc_evt(uint32_t evt, void *ctx)
{
	if ((evt != NRF_EVT_FLASH_OPERATION_SUCCESS) &&
	    (evt != NRF_EVT_FLASH_OPERATION_ERROR)) {
		return;
	}

	switch (bm_storage_sd.type) {
	case BM_STORAGE_SD_STATE_IDLE:
		return;
	case BM_STORAGE_SD_STATE_OP_PENDING:
		break;
	case BM_STORAGE_SD_STATE_OP_EXECUTING:
		bool operation_finished = false;

		switch (evt) {
		case NRF_EVT_FLASH_OPERATION_SUCCESS:
			operation_finished = on_operation_success(&bm_storage_sd.current_operation);
			break;
		case NRF_EVT_FLASH_OPERATION_ERROR:
			operation_finished = on_operation_failure(&bm_storage_sd.current_operation);
			break;
		default:
			break;
		}

		if (operation_finished) {
			bm_storage_sd.type = BM_STORAGE_SD_STATE_IDLE;

			/* We pass a pointer only when we call it manually for the synchronous
			 * processing.
			 */
			bool is_sync = (ctx != NULL);

			event_send(&bm_storage_sd.current_operation, is_sync,
				   (evt == NRF_EVT_FLASH_OPERATION_SUCCESS) ? 0 : -ETIMEDOUT);
		}
		break;
	default:
		break;
	}

	if (!bm_storage_sd.paused) {
		queue_process();
	} else {
		nrf_sdh_observer_ready(&sdh_state_evt);
	}
}
NRF_SDH_SOC_OBSERVER(sdh_soc, bm_storage_sd_on_soc_evt, NULL, HIGH);

const struct bm_storage_info bm_storage_info = {
	.program_unit = SD_WRITE_BLOCK_SIZE,
	.no_explicit_erase = true
};
