/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <string.h>
#include <nrf_soc.h>
#include <nrf_sdm.h>
#include <nrf_error.h>
#include <bm/softdevice_handler/nrf_sdh_soc.h>
#include <bm/softdevice_handler/nrf_sdh.h>
#include <bm/storage/bm_storage.h>
#include <bm/storage/bm_storage_backend.h>
#include <zephyr/irq.h>
#include <zephyr/sys/util.h>
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
	/* The operation ID (write, erase) */
	enum {
		WRITE,
		ERASE,
	} id;
	/* User-defined parameter passed to the event handler. */
	void *ctx;
	union {
		struct {
			/* Data to be written to non-volatile memory */
			const void *src;
			/* Destination of the data in non-volatile memory */
			uint32_t dest;
			/* Length of the data to be written (in bytes) */
			uint32_t len;
			/* Operation offset */
			uint32_t offset;
		} write;
		struct {
			/* The address to start erasing from */
			uint32_t addr;
			/* The number of bytes to erase */
			uint32_t len;
			/* Operation offset */
			uint32_t offset;
		} erase;
	};
};

static struct {
	/* Internal storage state. */
	union {
		enum {
			/* Queue is idle. */
			QUEUE_IDLE,
			/* An operation is executing. */
			QUEUE_RUNNING,
			/* Waiting for an external operation to complete. */
			QUEUE_WAITING,
			/* Queue processing is paused. */
			QUEUE_PAUSED,
		} queue_state;
		atomic_t atomic_state;
	};
	enum {
		OP_NONE,
		OP_EXECUTING,
	} operation_state;
	/* Number of times an operation has been retried on timeout. */
	uint8_t retries;
	/* Whether the SoftDevice is enabled */
	uint8_t softdevice_is_enabled;
	struct bm_storage_sd_op current_operation;
} bm_storage_sd;

#ifndef CONFIG_UNITY
static
#endif
void bm_storage_sd_on_soc_evt(uint32_t evt, void *ctx);

RING_BUF_DECLARE(sd_fifo, CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE *
		 sizeof(struct bm_storage_sd_op));

static void event_send(const struct bm_storage_sd_op *op, uint32_t result)
{
	struct bm_storage_evt evt;

	if (op->storage->evt_handler == NULL) {
		/* Do nothing. */
		return;
	}

	switch (op->id) {
	case WRITE:
		evt = (struct bm_storage_evt) {
			.id = BM_STORAGE_EVT_WRITE_RESULT,
			.is_async = bm_storage_sd.softdevice_is_enabled,
			.result = result,
			.ctx = op->ctx,
			.addr = op->write.dest,
			.src = op->write.src,
			.len = op->write.len,
		};
		break;
	case ERASE:
		evt = (struct bm_storage_evt) {
			.id = BM_STORAGE_EVT_ERASE_RESULT,
			.is_async = bm_storage_sd.softdevice_is_enabled,
			.result = result,
			.ctx = op->ctx,
			.addr = op->erase.addr,
			.len = op->erase.len,
		};
		break;
	}

	op->storage->evt_handler(&evt);
}

static uint32_t write_execute(const struct bm_storage_sd_op *op)
{
	__ASSERT(op->write.len % bm_storage_info.program_unit == 0,
		 "Data length is expected to be a multiple of the program unit.");

	__ASSERT(op->write.offset % bm_storage_info.program_unit == 0,
		 "Offset is expected to be a multiple of the program unit.");

	/* Calculate number of 32-bit words for sd_flash_write(). */
	uint32_t chunk_len_words = (op->write.len - op->write.offset) / sizeof(uint32_t);

	/* src and dest are word-aligned. */
	uint32_t *dest = (uint32_t *)(op->write.dest + op->write.offset);
	const uint32_t *src  = (const uint32_t *)((uint32_t)op->write.src + op->write.offset);

	return sd_flash_write(dest, src, chunk_len_words);
}

static uint8_t erase_buf[SD_WRITE_BLOCK_SIZE];

static uint32_t erase_execute(struct bm_storage_sd_op *op)
{
	uint32_t *addr = UINT_TO_POINTER(op->erase.addr + op->erase.offset);

	return sd_flash_write(addr, (const uint32_t *)erase_buf,
			      bm_storage_info.erase_unit / sizeof(uint32_t));
}

static bool queue_load_next(void)
{
	uint32_t bytes;
	unsigned int key;

	key = irq_lock();
	bytes = ring_buf_get(&sd_fifo, (uint8_t *)&bm_storage_sd.current_operation,
			     sizeof(struct bm_storage_sd_op));
	irq_unlock(key);

	return (bytes == sizeof(struct bm_storage_sd_op));
}

static bool queue_store(const struct bm_storage_sd_op *op)
{
	uint32_t bytes;
	unsigned int key;

	key = irq_lock();
	bytes = ring_buf_put(&sd_fifo, (uint8_t *)op, sizeof(*op));
	irq_unlock(key);

	return (bytes == sizeof(*op));
}

static void queue_process(void)
{
	uint32_t ret;

	if (bm_storage_sd.operation_state == OP_NONE) {
		if (!queue_load_next()) {
			bm_storage_sd.queue_state = QUEUE_IDLE;
			return;
		}
	}

	ret = 0;
	bm_storage_sd.queue_state = QUEUE_RUNNING;
	bm_storage_sd.operation_state = OP_EXECUTING;

	switch (bm_storage_sd.current_operation.id) {
	case WRITE:
		ret = write_execute(&bm_storage_sd.current_operation);
		break;
	case ERASE:
		ret = erase_execute(&bm_storage_sd.current_operation);
		break;
	}

	switch (ret) {
	case NRF_SUCCESS:
		/* The operation was accepted by the SoftDevice.
		 * If the SoftDevice is enabled, wait for a SoC event, otherwise simulate it.
		 */
		if (!bm_storage_sd.softdevice_is_enabled) {
			bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, NULL);
		}
		break;
	case NRF_ERROR_BUSY:
		/* The SoftDevice is executing a non-volatile memory operation that was not
		 * requested by the storage logic.
		 * Stop processing the queue until a system event is received.
		 */
		bm_storage_sd.queue_state = QUEUE_WAITING;
		break;
	default:
		/* An error has occurred and we cannot proceed further with this operation.
		 * Process the next operation in the queue.
		 */
		event_send(&bm_storage_sd.current_operation, -EIO);
		bm_storage_sd.operation_state = OP_NONE;
		queue_process();
		break;
	}
}

static void queue_start(void)
{
	if (atomic_cas(&bm_storage_sd.atomic_state, QUEUE_IDLE, QUEUE_RUNNING)) {
		queue_process();
	}
}

/* Write operation success callback. Keeps track of the progress of an operation. */
static bool on_operation_success(struct bm_storage_sd_op *op)
{
	/* Reset the retry counter on success. */
	bm_storage_sd.retries = 0;

	switch (op->id) {
	case WRITE:
		__ASSERT(op->len % bm_storage_info.program_unit == 0,
			 "Data length is expected to be a multiple of the program unit.");

		__ASSERT(op->offset % bm_storage_info.program_unit == 0,
			 "Offset is expected to be a multiple of the program unit.");

		op->write.offset += op->write.len - op->write.offset;
		/* Avoid missing the last chunk by rounding */
		if (op->write.offset >= op->write.len) {
			return true;
		}
		break;

	case ERASE:
		op->erase.offset += bm_storage_info.erase_unit;

		if (op->erase.len == op->erase.offset) {
			return true;
		}
		break;
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
	sd_softdevice_is_enabled(&bm_storage_sd.softdevice_is_enabled);

	(void)memset(erase_buf, (int)(bm_storage_info.erase_value & 0xFF), sizeof(erase_buf));

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
	memcpy(dest, UINT_TO_POINTER(src), len);

	return 0;
}

int bm_storage_backend_write(const struct bm_storage *storage, uint32_t dest,
			     const void *src, uint32_t len, void *ctx)
{
	bool queued;
	const struct bm_storage_sd_op op = {
		.storage = storage,
		.id = WRITE,
		.ctx = ctx,
		.write.src = src,
		.write.len = len,
		.write.dest = dest,
	};

	/* SoftDevice requires this alignment. */
	if (!IS_ALIGNED(src, sizeof(uint32_t))) {
		return -EINVAL;
	}

	queued = queue_store(&op);
	if (!queued) {
		return -ENOMEM;
	}

	queue_start();
	return 0;
}

int bm_storage_backend_erase(const struct bm_storage *storage, uint32_t addr, uint32_t len,
			     void *ctx)
{
	bool queued;
	const struct bm_storage_sd_op op = {
		.storage = storage,
		.id = ERASE,
		.ctx = ctx,
		.erase.addr = addr,
		.erase.len = len,
	};

	queued = queue_store(&op);
	if (!queued) {
		return -ENOMEM;
	}

	queue_start();
	return 0;
}

bool bm_storage_backend_is_busy(const struct bm_storage *storage)
{
	return (bm_storage_sd.queue_state != QUEUE_IDLE);
}

#ifndef CONFIG_UNITY
static
#endif
int bm_storage_sd_on_state_evt(enum nrf_sdh_state_evt evt, void *ctx)
{
	/* Are we ready to change state? */
	bool is_busy = false;

	switch (evt) {
	case NRF_SDH_STATE_EVT_ENABLE_PREPARE:
	case NRF_SDH_STATE_EVT_DISABLE_PREPARE: {
		/* Pause queue */
		is_busy = (bm_storage_sd.queue_state == QUEUE_RUNNING);
		bm_storage_sd.queue_state = QUEUE_PAUSED;
		return is_busy;
	}

	case NRF_SDH_STATE_EVT_ENABLED:
	case NRF_SDH_STATE_EVT_DISABLED:
		__ASSERT_NO_MSG(bm_storage_sd.queue_state == QUEUE_IDLE ||
				bm_storage_sd.queue_state == QUEUE_PAUSED);

		/* Continue executing any operation still in the queue */
		bm_storage_sd.softdevice_is_enabled = (evt == NRF_SDH_STATE_EVT_ENABLED);
		bm_storage_sd.queue_state = QUEUE_RUNNING;
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
	bool operation_finished;

	if ((evt != NRF_EVT_FLASH_OPERATION_SUCCESS) &&
	    (evt != NRF_EVT_FLASH_OPERATION_ERROR)) {
		/* This is not a FLASH event, return immediately */
		return;
	}

	if (bm_storage_sd.queue_state == QUEUE_IDLE) {
		/* We did not request any operation, ignore this event */
		return;
	}
	if (bm_storage_sd.queue_state == QUEUE_WAITING) {
		/* We attempted to schedule an operation, but SoftDevice was busy.
		 * Attempt to schedule the operation now.
		 */
		queue_process();
		return;
	}

	/* An operation has progressed.
	 * We need to send an event if it has completed.
	 * Then, if we are not paused we try to process the next operation,
	 * otherwise, we let the SoftDevice change state.
	 */
	operation_finished = false;

	switch (evt) {
	case NRF_EVT_FLASH_OPERATION_SUCCESS:
		operation_finished = on_operation_success(&bm_storage_sd.current_operation);
		break;
	case NRF_EVT_FLASH_OPERATION_ERROR:
		operation_finished = on_operation_failure(&bm_storage_sd.current_operation);
		break;
	}

	if (operation_finished) {
		/* Load a new operation next */
		bm_storage_sd.operation_state = OP_NONE;

		event_send(&bm_storage_sd.current_operation,
			   (evt == NRF_EVT_FLASH_OPERATION_SUCCESS) ? 0 : -ETIMEDOUT);
	}

	if (bm_storage_sd.queue_state == QUEUE_PAUSED) {
		/* Let SoftDevice state change happen now */
		nrf_sdh_observer_ready(&sdh_state_evt);
		return;
	}
	if (bm_storage_sd.queue_state == QUEUE_RUNNING) {
		queue_process();
		return;
	}
}
NRF_SDH_SOC_OBSERVER(sdh_soc, bm_storage_sd_on_soc_evt, NULL, HIGH);

const struct bm_storage_info bm_storage_info = {
	.program_unit = SD_WRITE_BLOCK_SIZE,
	.no_explicit_erase = true,
	.erase_unit = SD_WRITE_BLOCK_SIZE,
	.erase_value = 0xFF,
};
