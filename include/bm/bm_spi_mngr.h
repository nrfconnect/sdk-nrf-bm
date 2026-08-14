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
 * @brief SPI controller transaction queue on top of @ref nrfx_spim.
 *
 * Transactions wait in a FIFO queue and run one after another on the bus.
 * Each transaction is one or more TX and RX steps in order.
 * You can hook @c begin_callback and  @c end_callback, and optionally pass a different
 * @ref nrfx_spim_config_t per transaction.
 * That lets you change pins between jobs, for example a different software chip select line.
 * Pins must still be valid for this SPIM instance and on the same GPIO port when your SoC or
 * board wiring requires it.
 *
 * @ref bm_spi_mngr_schedule adds a transaction.
 * When it finishes, @c end_callback runs from the SPIM interrupt.
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
 * @param[in] _tx_data Pointer to data to send, or @c NULL if @p _tx_length is zero.
 * @param[in] _tx_length Number of bytes to send, must fit in @c uint8_t.
 * @param[in] _rx_data Pointer to buffer for received data, or @c NULL if @p _rx_length is zero.
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
 *
 * Manager stores only a pointer to this descriptor,
 * so the whole struct must remain valid until the transaction completes.
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
 * The manager stores only a pointer to this descriptor, so the descriptor,
 * its @ref transfers array, and any referenced buffers must remain valid until the transaction
 * completes.
 *
 * If @ref required_spim_cfg is non-NULL it must remain valid until the transaction completes.
 * If it is @c NULL, the default configuration passed to @ref bm_spi_mngr_init is used.
 *
 * If @ref required_spim_cfg differs from the configuration currently in use,
 * the module initializes the SPIM instance again before starting the transaction.
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
	 *
	 * Must stay valid until the transaction completes, if either callback uses it.
	 */
	void *user_data;
	/**
	 * @brief Array of transfers that make up the transaction.
	 *
	 * Must stay valid until the transaction completes.
	 */
	const struct bm_spi_mngr_transfer *transfers;
	/**
	 * @brief Number of entries in @ref transfers.
	 */
	uint8_t number_of_transfers;
	/**
	 * @brief Optional SPIM configuration for this transaction, NULL selects the default.
	 *
	 * If non-NULL, must stay valid until the transaction completes.
	 */
	const nrfx_spim_config_t *required_spim_cfg;
};

/**
 * @brief SPI transaction manager instance.
 *
 * Instantiate with @ref BM_SPI_MNGR_DEF. Do not modify fields directly.
 */
struct bm_spi_mngr {
	/**
	 * @brief Control block (writable runtime state) for this instance.
	 */
	struct {
		/**
		 * @brief Transaction currently being executed (NULL when idle).
		 */
		const struct bm_spi_mngr_transaction *volatile current_transaction;
		/**
		 * @brief Default SPIM configuration
		 *        (copy of the argument to @ref bm_spi_mngr_init)
		 */
		nrfx_spim_config_t default_configuration;
		/**
		 * @brief Pointer to the SPIM configuration currently applied to the instance.
		 */
		const nrfx_spim_config_t *current_configuration;
		/**
		 * @brief Index of the active transfer within @c current_transaction.
		 */
		uint8_t volatile current_transfer_idx;
	} cb;
	/**
	 * @brief Pending transaction queue (buffer + Zephyr @ref ring_buf).
	 */
	struct {
		/**
		 * @brief Maximum number of pending transactions
		 *        (not counting the one in progress).
		 */
		size_t size;
		/**
		 * @brief Zephyr @c ring_buf used as a FIFO of transaction pointers.
		 */
		struct ring_buf ring_buf;
		/**
		 * @brief Buffer that @c ring_buf uses to store the queued transaction pointers.
		 *
		 * Must provide @c size contiguous pointer-sized slots, each
		 * @c sizeof(const bm_spi_mngr_transaction *) in bytes.
		 */
		uint8_t *byte_storage;
	} queue;
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
 * The queue size is the maximum number of pending transactions not counting the one that is
 * running. For an empty queue with size of for example 4 elements, it is possible to schedule
 * up to 5 transactions.
 *
 * @param[in] _name Name of the instance to be created.
 * @param[in] _queue_size Size of the transaction queue (maximum number of pending transactions).
 * @param[in] _spim_inst Index of the SPIM hardware instance to be used.
 */
#define BM_SPI_MNGR_DEF(_name, _queue_size, _spim_inst)                                            \
	static uint8_t _name##_queue_bytes[(_queue_size) *                                         \
					   sizeof(const struct bm_spi_mngr_transaction *)];        \
	static nrfx_spim_t _name##_spim = NRFX_SPIM_INSTANCE(_spim_inst);                          \
	static struct bm_spi_mngr _name = {                                                        \
		.queue = {                                                                         \
			.size = (_queue_size),                                                     \
			.byte_storage = _name##_queue_bytes,                                       \
		},                                                                                 \
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
 * @retval -EFAULT If @p mgr, @p default_spim_cfg, the queue, or its buffer is NULL.
 * @retval -EINVAL If the queue size is zero.
 * @return Negative error code from @ref nrfx_spim_init on failure.
 */
int bm_spi_mngr_init(struct bm_spi_mngr *mgr, const nrfx_spim_config_t *default_spim_cfg);

/**
 * @brief Uninitialize the SPI manager and SPIM.
 *
 * @param[in] mgr Manager instance.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p mgr is NULL.
 */
int bm_spi_mngr_uninit(struct bm_spi_mngr *mgr);

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
 * The queue stores only a pointer to @p transaction, not a copy.
 * The caller must keep @p transaction and its @c transfers array valid until the transaction
 * completes (until @c end_callback runs).
 *
 * @param[in] mgr SPI transaction manager instance.
 * @param[in] transaction Descriptor of the transaction to be scheduled.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p mgr, @p transaction, or its transfers array is NULL.
 * @retval -EINVAL If the transaction has zero transfers.
 * @retval -ENOMEM If the queue is full.
 */
int bm_spi_mngr_schedule(struct bm_spi_mngr *mgr,
			 const struct bm_spi_mngr_transaction *transaction);

/**
 * @brief Get the current state of an SPI transaction manager instance.
 *
 * @param[in] mgr SPI transaction manager instance.
 *
 * @retval true If all scheduled transactions have been finished.
 * @retval false Otherwise.
 */
bool bm_spi_mngr_is_idle(const struct bm_spi_mngr *mgr);

#ifdef __cplusplus
}
#endif

#endif /* BM_SPI_MNGR_H__ */

/** @} */
