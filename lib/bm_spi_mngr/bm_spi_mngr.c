/* Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bm/bm_spi_mngr.h>

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/sys/__assert.h>

/* State shared between bm_spi_mngr_perform() and its internal end callback. The callback writes
 * the result and clears transaction_in_progress to release the caller from its wait loop.
 */
struct bm_spi_mngr_cb_data {
	bool transaction_in_progress;
	int transaction_result;
};

/* Add one transaction pointer to the back of the queue. IRQs are locked here internally because
 * the queue is accessed both from the caller of bm_spi_mngr_schedule() and from the SPIM interrupt
 * handler that starts the next queued transaction when one finishes.
 */
static int queue_push(const struct bm_spi_mngr *bm_spi_mngr, const void *src)
{
	struct ring_buf *rb = &bm_spi_mngr->queue->ring;
	const uint32_t rec = (uint32_t)sizeof(const struct bm_spi_mngr_transaction *);
	int ret;
	unsigned int key = irq_lock();

	if (ring_buf_space_get(rb) < rec) {
		ret = -ENOMEM;
	} else {
		uint32_t w = ring_buf_put(rb, (const uint8_t *)src, rec);

		ret = (w == rec) ? 0 : -ENOMEM;
	}

	irq_unlock(key);
	return ret;
}

/* Take one transaction pointer from the front of the queue. IRQs are locked here internally
 * because the queue is accessed both from start_pending_transaction() and from the SPIM interrupt
 * handler that starts the next queued transaction when one finishes.
 */
static int queue_pop(const struct bm_spi_mngr *bm_spi_mngr, void *element)
{
	struct ring_buf *rb = &bm_spi_mngr->queue->ring;
	const uint32_t rec = (uint32_t)sizeof(const struct bm_spi_mngr_transaction *);
	int ret;

	unsigned int key = irq_lock();

	if (ring_buf_size_get(rb) < rec) {
		ret = -ENOENT;
	} else {
		uint32_t r = ring_buf_get(rb, (uint8_t *)element, rec);

		ret = (r == rec) ? 0 : -ENOENT;
	}

	irq_unlock(key);

	return ret;
}

/* Start the active segment of the current transaction via nrfx_spim_xfer(). */
static int start_transfer(const struct bm_spi_mngr *bm_spi_mngr)
{
	__ASSERT_NO_MSG(bm_spi_mngr != NULL);

	/* Use a local copy so we do not read two volatile fields in one expression. */
	uint8_t curr_transfer_idx = bm_spi_mngr->cb->current_transfer_idx;
	struct bm_spi_mngr_cb *cb = bm_spi_mngr->cb;
	const struct bm_spi_mngr_transaction *txn = cb->current_transaction;
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
		bm_spi_mngr->cb->current_transaction;

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
		bm_spi_mngr->cb->current_transaction;

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
static void start_pending_transaction(const struct bm_spi_mngr *bm_spi_mngr,
				      bool switch_transaction)
{
	__ASSERT_NO_MSG(bm_spi_mngr != NULL);

	int err;

	while (1) {
		bool start_transaction = false;
		struct bm_spi_mngr_cb *cb = bm_spi_mngr->cb;

		/* IRQs are locked while touching the queue and the current transaction pointer
		 * because both are also accessed from the SPIM interrupt handler.
		 */
		unsigned int key = irq_lock();

		if (switch_transaction || bm_spi_mngr_is_idle(bm_spi_mngr)) {
			if (queue_pop(bm_spi_mngr,
				      (void *)(&cb->current_transaction)) == 0) {
				start_transaction = true;
			} else {
				/* Queue is empty, mark the manager as idle. */
				cb->current_transaction = NULL;
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

		if (cb->current_transaction->required_spim_cfg == NULL) {
			instance_cfg = &cb->default_configuration;
		} else {
			instance_cfg = cb->current_transaction->required_spim_cfg;
		}

		/* Reinitialize the SPIM instance only when this transaction needs a different
		 * configuration than the one currently active (different pins, frequency, mode,
		 * and so on).
		 */
		if (memcmp(cb->current_configuration, instance_cfg, sizeof(*instance_cfg)) != 0) {
			nrfx_spim_uninit(bm_spi_mngr->spim);
			err = nrfx_spim_init(bm_spi_mngr->spim,
					     instance_cfg,
					     spim_event_handler,
					     (void *)bm_spi_mngr);
			__ASSERT_NO_MSG(err == 0);
			cb->current_configuration = instance_cfg;
		}

		/* Try to start first transfer for this new transaction. */
		cb->current_transfer_idx = 0;

		/* Execute user code if available before starting transaction. */
		transaction_begin_signal(bm_spi_mngr);
		err = start_transfer(bm_spi_mngr);

		/* If transaction started successfully there is nothing more to do here now. */
		if (err == 0) {
			return;
		}

		/* Transfer failed to start. Notify the user that this transaction cannot be
		 * started and try with the next one in the next iteration of the loop.
		 */
		transaction_end_signal(bm_spi_mngr, err);

		switch_transaction = true;
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
	struct bm_spi_mngr_cb *cb = ((const struct bm_spi_mngr *)context)->cb;

	/* This callback should be called only during a transaction. */
	__ASSERT_NO_MSG(cb->current_transaction != NULL);

	if (event->type != NRFX_SPIM_EVENT_DONE) {
		/* The transfer failed. Finish the transaction with an I/O error result and start
		 * the next queued transaction (if there is any).
		 */
		transaction_end_signal(((const struct bm_spi_mngr *)context), -EIO);
		start_pending_transaction(((const struct bm_spi_mngr *)context), true);
		return;
	}

	/* Transfer finished successfully.
	 * If there is another one to be performed in the current transaction, start it now.
	 * Use a local variable to avoid using two volatile variables in one expression.
	 */
	err = 0;
	uint8_t curr_transfer_idx = cb->current_transfer_idx;

	++curr_transfer_idx;
	if (curr_transfer_idx < cb->current_transaction->number_of_transfers) {
		cb->current_transfer_idx = curr_transfer_idx;

		err = start_transfer((const struct bm_spi_mngr *)context);

		if (err == 0) {
			/* The current transaction is running and its next transfer has been
			 * successfully started. There is nothing more to do.
			 */
			return;
		}
		/* If the next transfer could not be started due to some error,
		 * finish the transaction with this error code as the result.
		 */
	}

	/* The current transaction has been completed (or its next transfer failed to start).
	 * Notify the user and start the next one (if there is any). Switch transactions here so
	 * that current_transaction is set to NULL only if there is nothing more to do, in order to
	 * not generate a spurious idle status (even for a moment).
	 */
	transaction_end_signal((const struct bm_spi_mngr *)context, err);
	start_pending_transaction((const struct bm_spi_mngr *)context, true);
}

int bm_spi_mngr_init(const struct bm_spi_mngr *bm_spi_mngr,
		     const nrfx_spim_config_t *default_spim_config)
{
	struct bm_spi_mngr_queue *queue;
	struct bm_spi_mngr_cb *cb;
	int err;

	if (bm_spi_mngr == NULL || default_spim_config == NULL) {
		return -EFAULT;
	}
	if (bm_spi_mngr->queue == NULL || bm_spi_mngr->queue->byte_storage == NULL) {
		return -EFAULT;
	}
	if (bm_spi_mngr->queue->size == 0) {
		return -EINVAL;
	}

	queue = bm_spi_mngr->queue;

	/* Initialize the ring buffer that holds the queued transactions. Each slot stores one
	 * pointer to a transaction descriptor, so the total size in bytes is the number of queue
	 * slots multiplied by the size of one pointer.
	 */
	ring_buf_init(&queue->ring,
		      (uint32_t)(queue->size * sizeof(const struct bm_spi_mngr_transaction *)),
		      queue->byte_storage);

	err = nrfx_spim_init(bm_spi_mngr->spim,
			     default_spim_config,
			     spim_event_handler,
			     (void *)bm_spi_mngr);

	if (err) {
		return err;
	}

	cb = bm_spi_mngr->cb;

	cb->current_transaction = NULL;
	cb->default_configuration = *default_spim_config;
	cb->current_configuration = &cb->default_configuration;

	return 0;
}

int bm_spi_mngr_uninit(const struct bm_spi_mngr *bm_spi_mngr)
{
	if (bm_spi_mngr == NULL) {
		return -EFAULT;
	}

	nrfx_spim_uninit(bm_spi_mngr->spim);

	ring_buf_reset(&bm_spi_mngr->queue->ring);
	bm_spi_mngr->cb->current_transaction = NULL;

	return 0;
}

int bm_spi_mngr_schedule(const struct bm_spi_mngr *bm_spi_mngr,
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

/* Internal end callback used by bm_spi_mngr_perform().
 * Stores the transaction result and clears the in-progress flag,
 * which releases the blocking caller from its wait loop.
 */
static void spi_internal_transaction_cb(int result, void *user_data)
{
	volatile struct bm_spi_mngr_cb_data *cb_data = user_data;

	cb_data->transaction_result = result;
	cb_data->transaction_in_progress = false;
}

int bm_spi_mngr_perform(const struct bm_spi_mngr *bm_spi_mngr,
			const nrfx_spim_config_t *config,
			const struct bm_spi_mngr_transfer *transfers,
			uint8_t number_of_transfers, void (*user_function)(void))
{
	int err;
	volatile struct bm_spi_mngr_cb_data cb_data = {
		.transaction_in_progress = true,
	};
	/* Wrap the transfers in an internal transaction so the same scheduling path can be reused.
	 * The internal end callback signals completion back to this function.
	 */
	struct bm_spi_mngr_transaction internal_transaction = {
		.begin_callback = NULL,
		.end_callback = spi_internal_transaction_cb,
		.user_data = (void *)&cb_data,
		.transfers = transfers,
		.number_of_transfers = number_of_transfers,
		.required_spim_cfg = config,
	};

	if (bm_spi_mngr == NULL || transfers == NULL) {
		return -EFAULT;
	}
	if (number_of_transfers == 0) {
		return -EINVAL;
	}

	/* This call blocks (busy-waits), so it can't run from ISR context. Guard against misuse. */
	__ASSERT_NO_MSG(!k_is_in_isr());

	err = bm_spi_mngr_schedule(bm_spi_mngr, &internal_transaction);

	if (err) {
		return err;
	}

	/* Block until the end callback runs from the SPIM interrupt and clears the flag. */
	while (cb_data.transaction_in_progress) {
		if (user_function) {
			user_function();
		}
	}

	return 0;
}
