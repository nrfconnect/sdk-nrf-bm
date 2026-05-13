/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 *
 * @defgroup bm_spi_mngr SPI transaction manager
 * @{
 *
 * @brief SPI master transaction queue on top of @ref nrfx_spim.
 *
 * Transactions wait in a FIFO queue and run one after another on the bus.
 * Each transaction is one or more TX and RX steps in order.
 * You can hook @c begin_callback and  @c end_callback, and optionally pass a different
 * @ref nrfx_spim_config_t per transaction.
 * That lets you change pins between jobs, for example a different software chip select line.
 * Pins must still be valid for this SPIM instance and on the same GPIO port when your SoC or
 * board wiring requires it.
 *
 * @ref bm_spi_mngr_schedule adds a transaction and returns right away in a nonblocking way.
 * When it finishes, @c end_callback runs from the SPIM interrupt. @ref bm_spi_mngr_perform does
 * the same work but waits until it is done in a blocking way. Only use @c idle_fn from normal
 * code, not from an interrupt handler.
 *
 * Connect and enable the SPIM interrupt, for example @ref BM_IRQ_DIRECT_CONNECT, before
 * @ref bm_spi_mngr_init.
 */

#ifndef BM_SPI_MNGR_H__
#define BM_SPI_MNGR_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nrfx_spim.h>
#include <zephyr/sys/ring_buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macro for creating a simple SPI transfer initializer.
 *
 * @param[in] _tx_data Pointer to data to send, or NULL if @a _tx_length is zero.
 * @param[in] _tx_length Number of bytes to send, must fit in @c uint8_t.
 * @param[in] _rx_data Pointer to buffer for received data, or NULL if @a _rx_length is zero.
 * @param[in] _rx_length Number of bytes to receive, must fit in @c uint8_t.
 */
#define BM_SPI_MNGR_TRANSFER(_tx_data, _tx_length, _rx_data, _rx_length)                           \
	{                                                                                          \
		.tx_data = (const uint8_t *)(_tx_data),                                            \
		.tx_length = (uint8_t)(_tx_length),                                                \
		.rx_data = (uint8_t *)(_rx_data),                                                  \
		.rx_length = (uint8_t)(_rx_length),                                                \
	}

/**
 * @brief Transaction end callback.
 *
 * @param[in] result @c 0 on success. On failure, a negative error code from nrfx SPIM or from this
 *                   module (for example @c -EIO).
 * @param[in] user_data Pointer passed through from @ref bm_spi_mngr_transaction member
 *                      @c user_data.
 */
typedef void (*bm_spi_mngr_callback_end_t)(int result, void *user_data);

/**
 * @brief Transaction begin callback, runs from the SPIM event handler context.
 *
 * @param[in] user_data Pointer passed through from @ref bm_spi_mngr_transaction member
 *                      @c user_data.
 */
typedef void (*bm_spi_mngr_callback_begin_t)(void *user_data);

/**
 * @brief SPI transfer descriptor, one segment of a transaction.
 */
struct bm_spi_mngr_transfer {
	/**
	 * @brief Pointer to data to send.
	 */
	const uint8_t *tx_data;
	/**
	 * @brief Number of bytes to send.
	 */
	uint8_t tx_length;
	/**
	 * @brief Pointer to buffer for received data.
	 */
	uint8_t *rx_data;
	/**
	 * @brief Number of bytes to receive.
	 */
	uint8_t rx_length;
};

/**
 * @brief SPI transaction descriptor.
 *
 * @note If @ref required_spim_cfg is non NULL it must remain valid until the transaction
 *       completes. If it is NULL, the default configuration passed to @ref bm_spi_mngr_init is
 *       used.
 *
 * @note If @ref required_spim_cfg differs from the configuration currently in use, the module
 *       initializes the SPIM instance again before starting the transaction.
 */
struct bm_spi_mngr_transaction {
	/**
	 * @brief User function invoked before the first transfer of the transaction starts.
	 */
	bm_spi_mngr_callback_begin_t begin_callback;
	/**
	 * @brief User function invoked when the transaction completes or aborts after an error.
	 */
	bm_spi_mngr_callback_end_t end_callback;
	/**
	 * @brief Opaque pointer passed to the optional begin and end callbacks.
	 */
	void *user_data;
	/**
	 * @brief Array of transfers that make up the transaction.
	 */
	const struct bm_spi_mngr_transfer *transfers;
	/**
	 * @brief Number of entries in @ref transfers.
	 */
	uint8_t number_of_transfers;
	/**
	 * @brief Optional SPIM configuration for this transaction; NULL selects the default.
	 */
	const nrfx_spim_config_t *required_spim_cfg;
};

/**
 * @brief SPI manager control block (writable runtime state).
 */
struct bm_spi_mngr_cb {
	/**
	 * @brief Transaction currently being executed (NULL when idle).
	 */
	const struct bm_spi_mngr_transaction *volatile current_transaction;
	/**
	 * @brief Default SPIM configuration (copy of the argument to @ref bm_spi_mngr_init).
	 */
	nrfx_spim_config_t default_configuration;
	/**
	 * @brief Pointer to the SPIM configuration currently applied to the instance.
	 */
	const nrfx_spim_config_t *current_configuration;
	/**
	 * @brief Index of the active transfer within @ref current_transaction.
	 */
	uint8_t volatile current_transfer_idx;
};

/**
 * @brief Transaction queue (backing store + Zephyr @ref ring_buf).
 *
 * @ref bm_spi_mngr::queue points here. Member @c size is the maximum number of pending transactions
 * (not counting the transaction currently in progress).
 * Member @c byte_storage must provide @c size contiguous pointer-sized slots,
 * each @c sizeof(const bm_spi_mngr_transaction *), in bytes.
 */
struct bm_spi_mngr_queue {
	/**
	 * @brief Maximum number of pending transactions (not counting the one in progress).
	 */
	size_t size;
	/**
	 * @brief Zephyr @c ring_buf used as a FIFO of transaction pointers.
	 */
	struct ring_buf ring;
	/**
	 * @brief Backing memory for @c ring, at least @c size transaction pointers.
	 */
	uint8_t *byte_storage;
};

/**
 * @brief SPI transaction manager instance.
 *
 * @note Instantiate with @ref BM_SPI_MNGR_DEF. Do not modify fields directly.
 */
struct bm_spi_mngr {
	/**
	 * @brief Control block for this instance.
	 */
	struct bm_spi_mngr_cb *cb;
	/**
	 * @brief Pending transaction queue.
	 */
	struct bm_spi_mngr_queue *queue;
	/**
	 * @brief nrfx SPIM driver instance.
	 */
	nrfx_spim_t *spim;
};

/**
 * @brief Macro for defining an SPI transaction manager instance.
 *
 * This macro allocates a static buffer for the transaction queue.
 * Therefore, use it in only one place in the code for a given instance name.
 *
 * @note The queue size is the maximum number of pending transactions not counting the one that is
 *       running. For an empty queue with size of for example 4 elements, it is possible to schedule
 *       up to 5 transactions.
 *
 * @param[in] _name Name of the instance to be created.
 * @param[in] _queue_size Size of the transaction queue (maximum number of pending transactions).
 * @param[in] _spim_inst Index of the SPIM hardware instance to be used.
 */
#define BM_SPI_MNGR_DEF(_name, _queue_size, _spim_inst)                                            \
	static uint8_t _name##_queue_bytes[(_queue_size) *                                         \
					   sizeof(const struct bm_spi_mngr_transaction *)];        \
	static struct bm_spi_mngr_queue _name##_queue = {                                          \
		.size = (_queue_size),                                                             \
		.byte_storage = _name##_queue_bytes,                                               \
	};                                                                                         \
	static struct bm_spi_mngr_cb _name##_cb;                                                   \
	static nrfx_spim_t _name##_spim = NRFX_SPIM_INSTANCE(_spim_inst);                          \
	static const struct bm_spi_mngr _name = {                                                  \
		.cb = &_name##_cb,                                                                 \
		.queue = &_name##_queue,                                                           \
		.spim = &_name##_spim,                                                             \
	}

/**
 * @brief Initialize the SPI manager and the underlying SPIM driver.
 *
 * Initializes the transaction queue and calls @ref nrfx_spim_init with the internal event handler.
 * On success, clears the current transaction pointer and stores this SPIM configuration.
 *
 * @param[in] mgr Manager instance to initialize.
 * @param[in] default_spim_cfg  Pointer to the SPIM driver configuration. This configuration
 *                              will be used whenever the scheduled transaction has
 *                              @c required_spim_cfg set to NULL.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p mgr, @p default_spim_cfg, the queue, or its backing storage is NULL.
 * @retval -EINVAL If the queue size is zero.
 * @return Negative error code from @ref nrfx_spim_init on failure.
 */
int bm_spi_mngr_init(const struct bm_spi_mngr *mgr, const nrfx_spim_config_t *default_spim_cfg);

/**
 * @brief Uninitialize the SPI manager and SPIM.
 *
 * @param[in] mgr Manager instance.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p mgr is NULL.
 */
int bm_spi_mngr_uninit(const struct bm_spi_mngr *mgr);

/**
 * @brief Schedule an SPI transaction.
 *
 * The transaction is enqueued and started as soon as the SPI bus is available,
 * thus when all previously scheduled transactions have been finished (possibly immediately).
 *
 * If @ref bm_spi_mngr_transaction::required_spim_cfg is set to a non-NULL value,
 * the module compares it with the current configuration and reinitializes the SPIM instance with
 * the new parameters if any differences are found.
 * If @ref bm_spi_mngr_transaction::required_spim_cfg is NULL,
 * the default configuration passed to @ref bm_spi_mngr_init is used instead.
 *
 * @param[in] mgr SPI transaction manager instance.
 * @param[in] transaction Descriptor of the transaction to be scheduled.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p mgr, @p transaction, or its transfers array is NULL.
 * @retval -EINVAL If the transaction has zero transfers.
 * @retval -ENOMEM If the queue is full.
 */
int bm_spi_mngr_schedule(const struct bm_spi_mngr *mgr,
			 const struct bm_spi_mngr_transaction *transaction);

/**
 * @brief Schedule a transaction and wait until it is finished.
 *
 * This function schedules a transaction that consists of one or more transfers and waits until it
 * is finished.
 *
 * @param[in] mgr SPI transaction manager instance.
 * @param[in] config SPIM configuration for this transaction. If NULL, the default configuration
 *                   passed to @ref bm_spi_mngr_init is used.
 * @param[in] transfers Array of transfers to be performed.
 * @param[in] number_of_transfers Number of transfers to be performed.
 * @param[in] idle_fn User function called repeatedly while waiting, or NULL if not needed.
 *
 * @note This function blocks until the transaction completes and must not be called from ISR
 *       context.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p mgr or @p transfers is NULL.
 * @retval -EINVAL If @p number_of_transfers is zero.
 * @retval -ENOMEM If the queue is full.
 * @return Negative error code from the @ref nrfx_spim driver or from this module during the
 *         transaction.
 */
int bm_spi_mngr_perform(const struct bm_spi_mngr *mgr, const nrfx_spim_config_t *config,
			const struct bm_spi_mngr_transfer *transfers,
			uint8_t number_of_transfers, void (*idle_fn)(void));

/**
 * @brief Get the current state of an SPI transaction manager instance.
 *
 * @param[in] mgr SPI transaction manager instance.
 *
 * @retval true If all scheduled transactions have been finished.
 * @retval false Otherwise.
 */
static inline bool bm_spi_mngr_is_idle(const struct bm_spi_mngr *mgr)
{
	return mgr->cb->current_transaction == NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* BM_SPI_MNGR_H__ */

/** @} */
