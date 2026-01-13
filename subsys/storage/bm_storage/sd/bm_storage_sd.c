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
#include <zephyr/sys/util.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/ring_buffer.h>

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

struct {
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
	/* Number of times an operation has been retried on timeout */
	uint8_t retries;
	/* Whether the SoftDevice is enabled */
	uint8_t softdevice_is_enabled;
	struct bm_storage_sd_op current_operation;
} bm_storage_sd;

void bm_storage_sd_on_soc_evt(uint32_t evt, void *ctx);

RING_BUF_DECLARE(sd_fifo, CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE *
		 sizeof(struct bm_storage_sd_op));

static inline bool is_aligned32(uint32_t addr)
{
	return !(addr & 0x03);
}

static void event_send(const struct bm_storage_sd_op *op, bool is_sync, uint32_t result)
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
			.result = result,
			.ctx = op->ctx,
			.addr = op->erase.addr,
			.len = op->erase.len,
		};
		break;
	}

	evt.dispatch_type = (is_sync) ? BM_STORAGE_EVT_DISPATCH_SYNC :
					BM_STORAGE_EVT_DISPATCH_ASYNC;

	op->storage->evt_handler(&evt);
}

static uint32_t write_execute(const struct bm_storage_sd_op *op)
{
	uint32_t *dest;
	uint32_t *src;
	uint32_t chunk_len;

	dest = (uint32_t *)(op->write.dest + op->write.offset);
	src  = (uint32_t *)((uintptr_t)op->write.src + op->write.offset);

	/* Limit by _MAX_WRITE_SIZE */
	chunk_len =
		MIN(op->write.len - op->write.offset, CONFIG_BM_STORAGE_BACKEND_SD_MAX_WRITE_SIZE);

	/* Calculate number of 32-bit words for sd_flash_write() */
	chunk_len = MAX(1, chunk_len / bm_storage_info.program_unit);

	return sd_flash_write(dest, src, chunk_len);
}

static uint32_t erase_execute(struct bm_storage_sd_op *op)
{
	uint32_t *addr = UINT_TO_POINTER(op->erase.addr + op->erase.offset);

	return sd_flash_write(addr, &bm_storage_info.erase_value,
			      op->erase.len / bm_storage_info.erase_unit);
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

	__ASSERT_NO_MSG(bm_storage_sd.queue_state == QUEUE_RUNNING ||
			bm_storage_sd.queue_state == QUEUE_WAITING);

	if (bm_storage_sd.operation_state == OP_NONE) {
		if (!queue_load_next()) {
			bm_storage_sd.queue_state = QUEUE_IDLE;
			return;
		}
	}

	ret = 0;
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
		 * If the SoftDevice is enabled, wait for a system event.
		 * Otherwise, the SoftDevice call is synchronous and will not send an event so we
		 * simulate it.
		 */
		if (!bm_storage_sd.softdevice_is_enabled) {
			bool is_sync = true;

			bm_storage_sd_on_soc_evt(NRF_EVT_FLASH_OPERATION_SUCCESS, &is_sync);
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
		/* An error has occurred. We cannot proceed further with this operation. */
		event_send(&bm_storage_sd.current_operation, true, -EIO);
		bm_storage_sd.queue_state = QUEUE_IDLE;
		/* Decision: do not retry this operation again when the queue is processed */
		bm_storage_sd.operation_state = OP_NONE;
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
		op->write.offset += MIN(op->write.len - op->write.offset,
					CONFIG_BM_STORAGE_BACKEND_SD_MAX_WRITE_SIZE);

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

	return 0;
}

int bm_storage_backend_uninit(struct bm_storage *storage)
{
	if ((bm_storage_sd.current_operation.storage == storage) &&
	    (bm_storage_sd.queue_state != QUEUE_IDLE)) {
		return -EBUSY;
	}

	memset(storage, 0x00, sizeof(*storage));

	return 0;
}

int bm_storage_backend_read(const struct bm_storage *storage, uint32_t src, void *dest,
			    uint32_t len)
{
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
	bool queued;
	const struct bm_storage_sd_op op = {
		.storage = storage,
		.id = WRITE,
		.ctx = ctx,
		.write.src = src,
		.write.len = len,
		.write.dest = dest,
	};

	/* SoftDevice expects this alignment. */
	if (!is_aligned32((uint32_t)src) || !is_aligned32(dest)) {
		return -EFAULT;
	}

	queued = queue_store(&op);
	if (!queued) {
		return -EIO;
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

	/* SoftDevice expects this alignment. */
	if ((addr % bm_storage_info.erase_unit) || (len % bm_storage_info.erase_unit)) {
		return -EFAULT;
	}

	queued = queue_store(&op);
	if (!queued) {
		return -EIO;
	}

	queue_start();
	return 0;
}

bool bm_storage_backend_is_busy(const struct bm_storage *storage)
{
	return (bm_storage_sd.queue_state != QUEUE_IDLE);
}

int bm_storage_sd_on_state_evt(enum nrf_sdh_state_evt evt, void *ctx)
{
	/* Are we ready to change state? */
	bool is_busy = false;

	switch (evt) {
	case NRF_SDH_STATE_EVT_ENABLE_PREPARE:
	case NRF_SDH_STATE_EVT_DISABLE_PREPARE: {
		is_busy = (bm_storage_sd.queue_state == QUEUE_RUNNING);
		/* Pause queue */
		bm_storage_sd.queue_state = QUEUE_PAUSED;
		return is_busy;
	}

	case NRF_SDH_STATE_EVT_ENABLED:
	case NRF_SDH_STATE_EVT_DISABLED:
		__ASSERT_NO_MSG(
			bm_storage_sd.queue_state == QUEUE_IDLE ||
			bm_storage_sd.queue_state == QUEUE_PAUSED);

		bm_storage_sd.softdevice_is_enabled = (evt == NRF_SDH_STATE_EVT_ENABLED);

		/* Continue executing any operation still in the queue */
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

		/* We pass a pointer only when we call it manually for the synchronous
		 * processing.
		 */
		bool is_sync = (ctx != NULL);

		event_send(&bm_storage_sd.current_operation, is_sync,
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
	.program_unit = 4,
	.no_explicit_erase = true,
	.erase_unit = 4,
	.erase_value = 0xFFFFFFFF,
};
