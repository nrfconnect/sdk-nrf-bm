/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup bm_lpuarte NCS Bare Metal Low Power UART with EasyDMA driver
 * @{
 */

#ifndef LPUARTE_H__
#define LPUARTE_H__

#include <stdint.h>
#include <zephyr/sys/util.h>
#include <nrfx_gpiote.h>
#include <nrfx_uarte.h>
#include <bm/bm_timer.h>

#ifdef __cplusplus
extern "C" {
#endif

/* States. */
enum bm_lpuarte_rx_state {
	/* RX is disabled. */
	RX_OFF,
	/* RX is in low power, idle state with pin detection armed. */
	RX_IDLE,
	/* RX request is pending, receiver is in preparation. */
	RX_PREPARE,
	/* RX is in active state, receiver is running. */
	RX_ACTIVE,
	/* RX is transitioning to from active idle state. */
	RX_TO_IDLE,
	/* RX is transitioning to off state. */
	RX_TO_OFF,
};

/* Low power uart structure. */
struct bm_lpuarte {
	/* Physical UART device instance */
	nrfx_uarte_t *uarte_inst;
	/* Request pin. */
	nrfx_gpiote_pin_t req_pin;
	/* Response pin. */
	nrfx_gpiote_pin_t rdy_pin;
	/* GPIOTE channel used by rdy pin. */
	uint8_t rdy_ch;
	/* Timer used for TX timeout. */
	struct bm_timer tx_timer;
	/* Current TX buffer. */
	const uint8_t *tx_buf;
	/* Length of TX data. */
	size_t tx_len;
	/* Set to true if physical transfer is started. */
	bool tx_active;
	/* Application callback. */
	nrfx_uarte_event_handler_t callback;
	/* RX state */
	enum bm_lpuarte_rx_state rx_state;
};

/* Configuration structured. */
struct bm_lpuarte_config {
	/* Uarte instance. */
	nrfx_uarte_t *uarte_inst;
	/* Uarte instance configuration. */
	nrfx_uarte_config_t uarte_cfg;
	/* Request pin number. */
	nrfx_gpiote_pin_t req_pin;
	/* Ready pin number. */
	nrfx_gpiote_pin_t rdy_pin;
};

/**
 * @brief Initialize LPUARTE driver instance.
 *
 * @param[in] lpu Low Power UARTE driver instance structure.
 * @param[in] lpu_cfg Low Power UARTE driver instance configuration structure.
 * @param[in] event_handler Event handler provided by the user.
 *
 * @return 0 on success. Otherwise a negative errno.
 */
int bm_lpuarte_init(struct bm_lpuarte *lpu, struct bm_lpuarte_config *lpu_cfg,
		    nrfx_uarte_event_handler_t  event_handler);

/**
 * @brief Deinitialize LPUARTE driver instance.
 *
 * @param[in] lpu Low Power UARTE driver instance structure.
 */
void bm_lpuarte_uninit(struct bm_lpuarte *lpu);

/**
 * @brief Send data over LPUARTE.
 *
 * @param[in] lpu Low Power UARTE driver instance structure.
 * @param[in] data Data to transfer.
 * @param[in] length Size of data to transfer.
 * @param[in] timeout TX timeout in milliseconds.
 *
 * @retval 0       Initialization of transmission was successful.
 * @retval -EBUSY  When transfer is already in progress.
 * @retval -EFAULT If @p lpu or @p data is NULL.
 * @retval -EINVAL Length is zero.
 */
int bm_lpuarte_tx(struct bm_lpuarte *lpu, const uint8_t *data, size_t length, int32_t timeout);

/**
 * @brief Check if TX is in progress.
 *
 * @param[in] lpu Low Power UARTE driver instance structure.
 *
 * @retval true  if transfer in progress.
 * @retval false if no transfer in progress.
 */
bool bm_lpuarte_tx_in_progress(struct bm_lpuarte *lpu);

/**
 * @brief Abort transmission.
 *
 * @param[in] lpu  Low Power UARTE driver instance structure.
 * @param[in] sync If true, transmition is aborted synchronously.
 *
 * @retval 0            Successfully initiated abort.
 * @retval -EFAULT      If @p lpu is NULL.
 * @retval -EINPROGRESS If there is no pending transfer.
 */
int bm_lpuarte_tx_abort(struct bm_lpuarte *lpu, bool sync);

/**
 * @brief Enable the receiver.
 *
 * The event handler will be called from the caller context with
 * the @ref NRFX_UARTE_EVT_RX_BUF_REQUEST event. The user may respond and provide a buffer
 * using @ref bm_lpuarte_rx_buffer_set. An error is returned if buffer is not provided. After that,
 * the receiver is started and another @ref NRFX_UARTE_EVT_RX_BUF_REQUEST is generated.
 * If a new buffer is not provided, then the receiver is disabled once the first buffer
 * becomes full. If a new buffer is provided, then the receiver will seamlessly switch to
 * a new buffer (using a hardware shortcut).
 *
 * @param[in] lpu Low Power UARTE driver instance structure.
 *
 * @retval 0      Receiver successfully enabled.
 * @retval -EBUSY When receiver is already enabled.
 */
int bm_lpuarte_rx_enable(struct bm_lpuarte *lpu);

/**
 * @brief Provide reception buffer.
 *
 * The function should be called as a response to the @ref NRFX_UARTE_EVT_RX_BUF_REQUEST event.
 * If the function is called before enabling the receiver, the first buffer is configured.
 * If the function is called and there is no active buffer but the receiver is enabled
 * but not started, it starts reception.
 *
 * @param[in] lpu    Low Power UARTE driver instance structure.
 * @param[in] data   Pointer to a buffer.
 * @param[in] length Buffer length.
 *
 * @retval 0            Buffer successfully set.
 * @retval -EINPROGRESS Buffer provided without pending request.
 * @retval -EACCES      No cache buffer provided or blocking mode is used,
 *                      transfer cannot be handled.
 * @retval -EBUSY       Previous buffer is still in use.
 * @retval -EPERM       Provided uncached buffer after providing cached one.
 */
int bm_lpuarte_rx_buffer_set(struct bm_lpuarte *lpu, uint8_t *data, size_t length);

/**
 * @brief Abort any ongoing reception.
 *
 * @note @ref NRFX_UARTE_EVT_RX_DONE event will be generated in non-blocking mode.
 *       It will contain number of bytes received until the abort was called. The event
 *       handler will be called from the UARTE interrupt context.
 *
 * @warning When the double-buffering feature is used and the UARTE interrupt
 *          is processed with a delay (for example, due to a higher priority
 *          interrupt) long enough for the first buffer to be filled completely,
 *          the event handler will be supplied with the pointer to the first
 *          buffer and the number of bytes received in the second buffer.
 *          This is because from hardware perspective it is impossible to deduce
 *          the reception of which buffer has been aborted.
 *          To prevent this from happening, keep the UARTE interrupt latency low
 *          or use large enough reception buffers.
 *
 * @param[in] lpu          Low Power UARTE driver instance structure.
 * @param[in] sync         If true, receiver is disabled synchronously.
 *
 * @retval 0            Successfully initiate disabling or disabled (synchronous mode).
 * @retval -EINPROGRESS Receiver was not enabled.
 */
int bm_lpuarte_rx_abort(struct bm_lpuarte *lpu, bool sync);

#ifdef __cplusplus
}
#endif

#endif /* LPUARTE_H__ */

/** @} */
