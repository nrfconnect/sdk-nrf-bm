/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm/bm_spi_mngr.h>

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/sys/__assert.h>

/* Size of one slot in the transaction queue, each slot holds a single transaction pointer. */
#define TXN_SLOT_SIZE ((uint32_t)sizeof(struct bm_spi_mngr_transaction *))

/* Add one transaction pointer to the back of the queue. IRQs are locked here internally because
 * the queue is accessed both from the caller of bm_spi_mngr_schedule() and from the SPIM interrupt
 * handler that starts the next queued transaction when one finishes.
 */
static int queue_push(struct bm_spi_mngr *bm_spi_mngr, const void *src)
{
	struct ring_buf *rb = &bm_spi_mngr->queue.ring_buf;
	int err;
	unsigned int key = irq_lock();

	if (ring_buf_space_get(rb) < TXN_SLOT_SIZE) {
		err = -ENOMEM;
	} else {
		uint32_t written = ring_buf_put(rb, (const uint8_t *)src, TXN_SLOT_SIZE);

		err = (written == TXN_SLOT_SIZE) ? 0 : -ENOMEM;
	}

	irq_unlock(key);
	return err;
}

/* Take one transaction pointer from the front of the queue. IRQs are locked here internally
 * because the queue is accessed both from start_pending_transaction() and from the SPIM interrupt
 * handler that starts the next queued transaction when one finishes.
 */
static int queue_pop(struct bm_spi_mngr *bm_spi_mngr, void *element)
{
	struct ring_buf *rb = &bm_spi_mngr->queue.ring_buf;
	int err;

	unsigned int key = irq_lock();

	if (ring_buf_size_get(rb) < TXN_SLOT_SIZE) {
		err = -ENOENT;
	} else {
		uint32_t bytes_read = ring_buf_get(rb, (uint8_t *)element, TXN_SLOT_SIZE);

		err = (bytes_read == TXN_SLOT_SIZE) ? 0 : -ENOENT;
	}

	irq_unlock(key);

	return err;
}

/* Start the active segment of the current transaction via nrfx_spim_xfer(). */
static int start_transfer(const struct bm_spi_mngr *bm_spi_mngr)
{
	__ASSERT_NO_MSG(bm_spi_mngr != NULL);

	/* Use a local copy so we do not read two volatile fields in one expression. */
	const uint8_t curr_transfer_idx = bm_spi_mngr->cb.current_transfer_idx;
	const struct bm_spi_mngr_transaction *txn = bm_spi_mngr->cb.current_transaction;
	const struct bm_spi_mngr_transfer *transfer = &txn->transfers[curr_transfer_idx];

	nrfx_spim_xfer_desc_t xfer = NRFX_SPIM_XFER_TRX(
		transfer->tx_data, transfer->tx_length, transfer->rx_data,
		transfer->rx_length);

	return nrfx_spim_xfer(bm_spi_mngr->spim, &xfer, 0);
}

/* If the transaction defines begin_callback, call it before the first transfer starts. */
static void transaction_begin_signal(const struct bm_spi_mngr *bm_spi_mngr)
{
	__ASSERT_NO_MSG(bm_spi_mngr != NULL);

	const struct bm_spi_mngr_transaction *current_transaction =
		bm_spi_mngr->cb.current_transaction;

	if (current_transaction->begin_callback != NULL) {
		void *user_data = current_transaction->user_data;

		current_transaction->begin_callback(user_data);
	}
}

/* If end_callback is set, call it with the transaction result (success or error code). */
static void transaction_end_signal(const struct bm_spi_mngr *bm_spi_mngr, int result)
{
	__ASSERT_NO_MSG(bm_spi_mngr != NULL);

	const struct bm_spi_mngr_transaction *current_transaction =
		bm_spi_mngr->cb.current_transaction;

	if (current_transaction->end_callback != NULL) {
		void *user_data = current_transaction->user_data;

		current_transaction->end_callback(result, user_data);
	}
}

static void spim_event_handler(const nrfx_spim_event_t *event, void *context);

/* Start the next transaction from the queue. Called from the scheduling path when the bus is
 * idle, and from the SPIM interrupt handler after a transaction finishes to force a switch to
 * the next one. The current transaction pointer is only cleared when the queue is empty, so
 * back-to-back transactions never look idle in between.
 */
static void start_pending_transaction(struct bm_spi_mngr *bm_spi_mngr, bool switch_transaction)
{
	__ASSERT_NO_MSG(bm_spi_mngr != NULL);

	int err;

	while (1) {
		bool start_transaction = false;

		/* IRQs are locked while touching the queue and the current transaction pointer
		 * because both are also accessed from the SPIM interrupt handler.
		 */
		unsigned int key = irq_lock();

		if (switch_transaction || bm_spi_mngr_is_idle(bm_spi_mngr)) {
			err = queue_pop(bm_spi_mngr, (void *)&bm_spi_mngr->cb.current_transaction);
			if (err) {
				/* Queue is empty, mark the manager as idle. */
				bm_spi_mngr->cb.current_transaction = NULL;
			} else {
				start_transaction = true;
			}
		}

		irq_unlock(key);

		if (!start_transaction) {
			return;
		}

		/* A transaction can carry its own SPIM configuration, otherwise the default
		 * configuration from bm_spi_mngr_init() is used.
		 */
		const nrfx_spim_config_t *instance_cfg;

		if (bm_spi_mngr->cb.current_transaction->required_spim_cfg == NULL) {
			instance_cfg = &bm_spi_mngr->cb.default_configuration;
		} else {
			instance_cfg = bm_spi_mngr->cb.current_transaction->required_spim_cfg;
		}

		/* Reinitialize the SPIM instance only when this transaction needs a different
		 * configuration than the one currently active (different pins, frequency, mode,
		 * and so on).
		 */
		if (memcmp(bm_spi_mngr->cb.current_configuration, instance_cfg,
			   sizeof(*instance_cfg)) != 0) {
			nrfx_spim_uninit(bm_spi_mngr->spim);
			err = nrfx_spim_init(bm_spi_mngr->spim,
					     instance_cfg,
					     spim_event_handler,
					     (void *)bm_spi_mngr);
			__ASSERT_NO_MSG(err == 0);
			bm_spi_mngr->cb.current_configuration = instance_cfg;
		}

		/* Try to start first transfer for this new transaction. */
		bm_spi_mngr->cb.current_transfer_idx = 0;

		/* Execute user code if available before starting transaction. */
		transaction_begin_signal(bm_spi_mngr);
		err = start_transfer(bm_spi_mngr);

		if (err) {
			/* Transfer failed to start. Notify the user that this transaction cannot be
			 * started and try with the next one in the next iteration of the loop.
			 */
			transaction_end_signal(bm_spi_mngr, err);
			switch_transaction = true;
			continue;
		}

		/* Transaction started successfully, nothing more to do here now. */
		return;
	}
}

/* Handle SPIM events. Called from the SPIM interrupt when a transfer finishes. If there are more
 * transfers in the current transaction, start the next one. Otherwise, notify the user and start
 * the next queued transaction (if any).
 */
static void spim_event_handler(const nrfx_spim_event_t *event, void *context)
{
	__ASSERT_NO_MSG(event != NULL);
	__ASSERT_NO_MSG(context != NULL);

	int err;
	struct bm_spi_mngr *bm_spi_mngr = (struct bm_spi_mngr *)context;

	/* This callback should be called only during a transaction. */
	__ASSERT_NO_MSG(bm_spi_mngr->cb.current_transaction != NULL);

	if (event->type != NRFX_SPIM_EVENT_DONE) {
		/* The transfer failed. Finish the transaction with an I/O error result and start
		 * the next queued transaction (if there is any).
		 */
		transaction_end_signal(bm_spi_mngr, -EIO);
		start_pending_transaction(bm_spi_mngr, true);
		return;
	}

	/* Transfer finished successfully.
	 * If there is another one to be performed in the current transaction, start it now.
	 * Use a local variable to avoid using two volatile variables in one expression.
	 */
	uint8_t curr_transfer_idx = bm_spi_mngr->cb.current_transfer_idx;

	++curr_transfer_idx;
	if (curr_transfer_idx < bm_spi_mngr->cb.current_transaction->number_of_transfers) {
		bm_spi_mngr->cb.current_transfer_idx = curr_transfer_idx;

		err = start_transfer(bm_spi_mngr);

		if (err) {
			/* The next transfer could not be started due to some error.
			 * Finish the transaction with this error code as the result.
			 */
			transaction_end_signal(bm_spi_mngr, err);
			start_pending_transaction(bm_spi_mngr, true);
			return;
		}

		/* The current transaction is running and its next transfer has been
		 * successfully started. There is nothing more to do.
		 */
		return;
	}

	/* The current transaction has been completed. Notify the user and start the next one
	 * (if there is any). Switch transactions here so that current_transaction is set to NULL
	 * only if there is nothing more to do, in order to not generate a spurious idle status
	 * (even for a moment).
	 */
	transaction_end_signal(bm_spi_mngr, 0);
	start_pending_transaction(bm_spi_mngr, true);
}

int bm_spi_mngr_init(struct bm_spi_mngr *bm_spi_mngr,
		     const nrfx_spim_config_t *default_spim_config)
{
	int err;

	if (bm_spi_mngr == NULL || default_spim_config == NULL) {
		return -EFAULT;
	}
	if (bm_spi_mngr->queue.byte_storage == NULL) {
		return -EFAULT;
	}
	if (bm_spi_mngr->queue.size == 0) {
		return -EINVAL;
	}

	/* Initialize the ring buffer that holds the queued transactions. Each slot stores one
	 * pointer to a transaction descriptor, so the total size in bytes is the number of queue
	 * slots multiplied by the size of one slot.
	 */
	const uint32_t queue_bytes = (uint32_t)bm_spi_mngr->queue.size * TXN_SLOT_SIZE;

	ring_buf_init(&bm_spi_mngr->queue.ring_buf, queue_bytes, bm_spi_mngr->queue.byte_storage);

	err = nrfx_spim_init(bm_spi_mngr->spim,
			     default_spim_config,
			     spim_event_handler,
			     (void *)bm_spi_mngr);

	if (err) {
		return err;
	}

	bm_spi_mngr->cb.current_transaction = NULL;
	bm_spi_mngr->cb.default_configuration = *default_spim_config;
	bm_spi_mngr->cb.current_configuration = &bm_spi_mngr->cb.default_configuration;

	return 0;
}

int bm_spi_mngr_uninit(struct bm_spi_mngr *bm_spi_mngr)
{
	if (bm_spi_mngr == NULL) {
		return -EFAULT;
	}

	nrfx_spim_uninit(bm_spi_mngr->spim);

	bm_spi_mngr->cb.current_transaction = NULL;

	return 0;
}

int bm_spi_mngr_schedule(struct bm_spi_mngr *bm_spi_mngr,
			 const struct bm_spi_mngr_transaction *transaction)
{
	int err;

	if (bm_spi_mngr == NULL || transaction == NULL || transaction->transfers == NULL) {
		return -EFAULT;
	}
	if (transaction->number_of_transfers == 0) {
		return -EINVAL;
	}

	err = queue_push(bm_spi_mngr, &transaction);

	if (err) {
		return err;
	}

	/* New transaction has been successfully added to queue,
	 * so if we are currently idle it's time to start the job.
	 */
	start_pending_transaction(bm_spi_mngr, false);

	return 0;
}

bool bm_spi_mngr_is_idle(const struct bm_spi_mngr *bm_spi_mngr)
{
	return bm_spi_mngr->cb.current_transaction == NULL;
}
