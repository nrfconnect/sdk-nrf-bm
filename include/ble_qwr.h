/**
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup ble_qwr Queued Writes module
 * @{
 * @ingroup ble_sdk_lib
 * @brief Module for handling Queued Write operations.
 *
 * @details This module handles prepare write, execute write, and cancel write
 * commands. It also manages memory requests related to these operations.
 *
 * @note The application must propagate BLE stack events to this module by calling
 *       @ref ble_qwr_on_ble_evt().
 */

#ifndef NRF_BLE_QUEUED_WRITES_H__
#define NRF_BLE_QUEUED_WRITES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <ble.h>
#include <ble_gatts.h>

/**
 * @brief Macro for defining a ble_qwr instance.
 *
 * @param _name Name of the instance.
 * @hideinitializer
 */
#define BLE_QWR_DEF(_name)                                                                         \
	static struct ble_qwr _name;                                                               \
	NRF_SDH_BLE_OBSERVER(_name ## _obs,                                                        \
			     ble_qwr_on_ble_evt,                                                   \
			     &_name,                                                               \
			     CONFIG_BLE_QWR_BLE_OBSERVER_PRIO)

/* Error code used by the module to reject prepare write requests on non-registered attributes. */
#define BLE_QWR_REJ_REQUEST_ERR_CODE BLE_GATT_STATUS_ATTERR_APP_BEGIN + 0

/** @brief Queued Writes module event types. */
enum ble_qwr_evt_type {
	/** Error event */
	BLE_QWR_EVT_ERROR,
	/** Event that indicates that an execute write command was received for a registered handle
	 * and that the received data was actually written and is now ready.
	 */
	BLE_QWR_EVT_EXECUTE_WRITE,
	/** Event that indicates that an execute write command was received for a registered handle
	 * and that the write request must now be accepted or rejected.
	 */
	BLE_QWR_EVT_AUTH_REQUEST,
};

/** @brief Queued Writes module events. */
struct ble_qwr_evt {
	/** Type of the event. */
	enum ble_qwr_evt_type evt_type;
	union {
		/** @ref BLE_QWR_EVT_ERROR event data. */
		struct {
			int reason;
		} error;
		/** @ref BLE_QWR_EVT_EXECUTE_WRITE event data. */
		struct {
			/** Handle of the attribute to which the event relates. */
			uint16_t attr_handle;
		} exec_write;
		/** @ref BLE_QWR_EVT_AUTH_REQUEST event data. */
		struct {
			/** Handle of the attribute to which the event relates. */
			uint16_t attr_handle;
		} auth_req;
	};
};

/* Forward declaration of the struct ble_qwr. */
struct ble_qwr;

/**
 * @brief Queued Writes module event handler type.
 *
 * If the provided event is of type @ref BLE_QWR_EVT_AUTH_REQUEST,
 * this function must accept or reject the execute write request by returning
 * one of the @ref BLE_GATT_STATUS_CODES.
 */
typedef uint16_t (*ble_qwr_evt_handler_t)(struct ble_qwr *qwr, const struct ble_qwr_evt *evt);

/**
 * @brief Queued Writes structure.
 * @details This structure contains status information for the Queued Writes module.
 */
struct ble_qwr {
	/** Flag that indicates whether the module has been initialized. */
	uint32_t initialized;
	/** Event handler function that is called for events concerning the handles of all
	 *  registered attributes.
	 */
	ble_qwr_evt_handler_t evt_handler;
	/** Connection handle. */
	uint16_t conn_handle;
	/** Flag that indicates whether a mem_reply is pending
	 *  (because a previous attempt returned busy).
	 */
	bool is_user_mem_reply_pending;
#if (CONFIG_BLE_QWR_MAX_ATTR > 0)
	/** List of handles for registered attributes, for which the module accepts and handles
	 *  prepare write operations.
	 */
	uint16_t attr_handles[CONFIG_BLE_QWR_MAX_ATTR];
	/** Number of registered attributes. */
	uint8_t nb_registered_attr;
	/** List of attribute handles that have been written to during the current prepare write or
	 *  execute write operation.
	 */
	uint16_t written_attr_handles[CONFIG_BLE_QWR_MAX_ATTR];
	/** Number of attributes that have been written to during the current prepare write or
	 *  execute write operation.
	 */
	uint8_t nb_written_handles;
	/** Memory buffer that is provided to the SoftDevice on an ON_USER_MEM_REQUEST event. */
	ble_user_mem_block_t mem_buffer;
#endif
};

/**
 * @brief Queued Writes init structure.
 *
 * @details This structure contains all information needed to initialize the Queued Writes module.
 */
struct ble_qwr_config {
	/** Event handler function that is called for events concerning the handles of all
	 *  registered attributes.
	 */
	ble_qwr_evt_handler_t evt_handler;
#if (CONFIG_BLE_QWR_MAX_ATTR > 0)
	/** Memory buffer that is provided to the SoftDevice on an ON_USER_MEM_REQUEST event. */
	ble_user_mem_block_t mem_buffer;
#endif
};

/**
 * @brief Function for initializing the Queued Writes module.
 *
 * @details Call this function in the main entry of your application to
 * initialize the Queued Writes module. It must be called only once with a
 * given Queued Writes structure.
 *
 * @param[out] qwr Queued Writes structure. This structure must be supplied by the application. It
 *                 is initialized by this function and is later used to identify the particular
 *                 Queued Writes instance.
 * @param[in] qwr_init Initialization structure.
 *
 * @retval 0 If the Queued Writes module was initialized successfully.
 * @retval -EFAULT If @p qwr or @p qwr_init is @c NULL.
 * @retval -EPERM If the given @p qwr instance has already been initialized.
 */
int ble_qwr_init(struct ble_qwr *qwr, struct ble_qwr_config const *qwr_config);


/**
 * @brief Function for assigning a connection handle to an instance of the Queued Writes module.
 *
 * @details Call this function when a link with a peer has been established to associate this link
 *          to the instance of the module. This makes it possible to handle several links and
 *          associate each link to a particular instance of this module.
 *
 * @param[in] qwr Queued Writes structure.
 * @param[in] conn_handle Connection handle to be associated with the given Queued Writes instance.
 *
 * @retval 0 If the assignment was successful.
 * @retval -EFAULT If @p qwr is @c NULL.
 * @retval -EPERM If the given @p qwr instance has not been initialized.
 */
int ble_qwr_conn_handle_assign(struct ble_qwr *qwr, uint16_t conn_handle);

/**
 * @brief Function for handling BLE stack events.
 *
 * @details Handles all events from the BLE stack that are of interest to the Queued Writes module.
 *
 * @param[in] ble_evt Event received from the BLE stack.
 * @param[in] context Queued Writes structure.
 */
void ble_qwr_on_ble_evt(ble_evt_t const *ble_evt, void *context);

#if (CONFIG_BLE_QWR_MAX_ATTR > 0)
/**
 * @brief Function for registering an attribute with the Queued Writes module.
 *
 * @details Call this function for each attribute that you want to enable for Queued Writes
 *          (thus a series of prepare write and execute write operations).
 *
 * @param[in] qwr Queued Writes structure.
 * @param[in] attr_handle Handle of the attribute to register.
 *
 * @retval 0 If the registration was successful.
 * @retval -ENOMEM If no more memory is available to add this registration.
 * @retval -EFAULT If @p qwr is @c NULL.
 * @retval -EPERM If the given @p qwr instance has not been initialized.
 */
int ble_qwr_attr_register(struct ble_qwr *qwr, uint16_t attr_handle);


/**
 * @brief Function for retrieving the received data for a given attribute.
 *
 * @details Call this function after receiving an @ref BLE_QWR_EVT_AUTH_REQUEST
 * event to retrieve a linear copy of the data that was received for the given attribute.
 *
 * @param[in] qwr Queued Writes structure.
 * @param[in] attr_handle Handle of the attribute.
 * @param[out] mem Pointer to the application buffer where the received data will be copied.
 * @param[in,out] len Input: length of the input buffer. Output: length of the received data.
 *
 * @retval 0 If the data was retrieved and stored successfully.
 * @retval -ENOMEM If the provided buffer was smaller than the received data.
 * @retval -EFAULT If @p qwr, @p mem or @p len is @c NULL.
 * @retval -EPERM If the given @p qwr instance has not been initialized.
 */
int ble_qwr_value_get(
	struct ble_qwr *qwr, uint16_t attr_handle, uint8_t *mem, uint16_t *len);
#endif /* (CONFIG_BLE_QWR_MAX_ATTR > 0) */

#ifdef __cplusplus
}
#endif

#endif /* NRF_BLE_QUEUED_WRITES_H__ */

/** @} */
