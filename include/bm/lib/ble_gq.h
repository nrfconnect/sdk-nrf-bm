/*
 * Copyright (c) 2018-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @defgroup ble_gq GATT Queue
 * @{
 * @brief  Queue for the BLE GATT requests.
 *
 * @details The BLE GATT Queue (BGQ) module can be used to queue BLE GATT requests if the
 *          SoftDevice is not able to handle them at the moment. In this case, processing of
 *          the queued request is postponed. Later on, when the corresponding BLE event indicates
 *          that the SoftDevice may be free, the request is retried.
 */

#ifndef BLE_GQ_H__
#define BLE_GQ_H__

#include <stddef.h>
#include <stdint.h>

#include <bm/sdh/nrf_sdh_ble.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/slist.h>
#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macro for defining a BLE GATT queue instance with default parameters from Kconfig.
 *
 * @param _name Name of the instance.
 */
#define BLE_GQ_DEF(_name)                                                                          \
	BLE_GQ_CUSTOM_DEF(_name, CONFIG_BLE_GQ_MAX_CONNECTIONS, CONFIG_BLE_GQ_HEAP_SIZE,           \
			  (CONFIG_BLE_GQ_MAX_CONNECTIONS * CONFIG_BLE_GQ_QUEUE_SIZE))

/**
 * @brief Macro for defining a BLE GATT queue instance.
 *
 * @param _name            Name of the instance.
 * @param _max_conns       Maximum number of connection handles that can be registered.
 * @param _heap_size       Size of heap used for storing additional data for
 *                         write, notify and indicate operations.
 * @param _max_req_blocks  Maximum number of requests that can be held at any point in time.
 */
#define BLE_GQ_CUSTOM_DEF(_name, _max_conns, _heap_size, _max_req_blocks)                          \
	static uint16_t CONCAT(_name, _conn_handles_arr)[] = {                                     \
		LISTIFY(_max_conns, BLE_GQ_CONN_HANDLE_INIT, (,)),                                 \
	};                                                                                         \
	BUILD_ASSERT(ARRAY_SIZE(CONCAT(_name, _conn_handles_arr)) == (_max_conns));                \
	static uint16_t CONCAT(_name, _purge_arr)[] = {                                            \
		LISTIFY(_max_conns, BLE_GQ_PURGE_ARRAY_INIT, (,), (_max_conns)),                   \
	};                                                                                         \
	BUILD_ASSERT(ARRAY_SIZE(CONCAT(_name, _purge_arr)) == (_max_conns));                       \
	static sys_slist_t CONCAT(_name, _req_queues)[] = {                                        \
		LISTIFY(_max_conns, BLE_GQ_REQ_QUEUE_INIT, (,), _name),                            \
	};                                                                                         \
	BUILD_ASSERT(ARRAY_SIZE(CONCAT(_name, _req_queues)) == (_max_conns));                      \
	K_MEM_SLAB_DEFINE_STATIC(_name##_req_blocks, sizeof(struct ble_gq_req), (_max_req_blocks), \
				 sizeof(void *));                                                  \
	static K_HEAP_DEFINE(_name##_heap, (_heap_size));                                          \
	static const struct ble_gq _name = {                                                       \
		.max_conns = (_max_conns),                                                         \
		.conn_handles = CONCAT(_name, _conn_handles_arr),                                  \
		.purge_list = CONCAT(_name, _purge_arr),                                           \
		.req_queue = (sys_slist_t *)&CONCAT(_name, _req_queues),                           \
		.req_blocks = &CONCAT(_name, _req_blocks),                                         \
		.data_pool = &CONCAT(_name, _heap),                                                \
	};                                                                                         \
	NRF_SDH_BLE_OBSERVER(CONCAT(_name, _obs), ble_gq_on_ble_evt, (void *)&_name,               \
			     CONFIG_BLE_GQ_OBSERVER_PRIO)

/**
 * @brief Helper macro for initializing connection handle array. Used in @ref BLE_GQ_CUSTOM_DEF.
 */
#define BLE_GQ_CONN_HANDLE_INIT(i, _) BLE_CONN_HANDLE_INVALID

/**
 * @brief Helper macro for initializing purge array. Used in @ref BLE_GQ_CUSTOM_DEF.
 */
#define BLE_GQ_PURGE_ARRAY_INIT(i, _max_conns) (_max_conns)

/**
 * @brief Helper macro for initializing request queues. Used in @ref BLE_GQ_CUSTOM_DEF.
 */
#define BLE_GQ_REQ_QUEUE_INIT(i, _name) SYS_SLIST_STATIC_INIT(&((_name)[i]))

/**
 * @brief BLE GATT request types.
 */
enum ble_gq_req_type {
	/**
	 * @brief GATTC Read Request. See @ref sd_ble_gattc_read.
	 */
	BLE_GQ_REQ_GATTC_READ,
	/**
	 * @brief GATTC Write Request. See @ref sd_ble_gattc_write.
	 */
	BLE_GQ_REQ_GATTC_WRITE,
	/**
	 * @brief GATTC Service Discovery Request. See @ref sd_ble_gattc_primary_services_discover.
	 */
	BLE_GQ_REQ_SRV_DISCOVERY,
	/**
	 * @brief GATTC Characteristic Discovery Request.
	 *        See @ref sd_ble_gattc_characteristics_discover.
	 */
	BLE_GQ_REQ_CHAR_DISCOVERY,
	/**
	 * @brief GATTC Characteristic Descriptor Discovery Request.
	 *        See @ref sd_ble_gattc_descriptors_discover.
	 */
	BLE_GQ_REQ_DESC_DISCOVERY,
	/**
	 * @brief GATTS Handle Value Notification or Indication. See @ref ble_gatts_hvx_params_t.
	 */
	BLE_GQ_REQ_GATTS_HVX,
	/**
	 * @brief Total number of different GATT Request types.
	 */
	BLE_GQ_REQ_NUM,
};

/**
 * @brief Error handler type.
 */
typedef void (*ble_gq_req_error_cb_t)(uint16_t conn_handle, uint32_t nrf_error, void *context);

/**
 * @brief Structure used to handle SoftDevice error.
 */
struct ble_gq_req_error_handler {
	/**
	 * @brief Error handler to be called in case of an error from SoftDevice.
	 */
	ble_gq_req_error_cb_t cb;
	/**
	 * @brief Parameter passed to the error handler;
	 */
	void *ctx;
};

/**
 * @brief Structure to hold a BLE GATT request.
 */
struct ble_gq_req {
	/**
	 * @brief Data for storing the request in a singly-linked list.
	 */
	sys_snode_t node;
	/**
	 * @brief Type of request.
	 */
	enum ble_gq_req_type type;
	/**
	 * @brief Extra payload data that cannot be contained in the request queue.
	 *
	 * Used internally by the GATT queue to manage additional memory allocations.
	 */
	uint8_t *data;
	/**
	 * @brief Error handler structure.
	 */
	struct ble_gq_req_error_handler error_handler;
	/**
	 * @brief Request type specific parameters.
	 */
	union {
		/**
		 * @brief GATTC read parameters.
		 *        Type @ref BLE_GQ_REQ_GATTC_READ.
		 */
		struct {
			uint16_t handle;
			uint16_t offset;
		} gattc_read;
		/**
		 * @brief GATTC write parameters.
		 *        Type @ref BLE_GQ_REQ_GATTC_WRITE.
		 */
		ble_gattc_write_params_t gattc_write;
		/**
		 * @brief GATTC service discovery parameters.
		 *        Type @ref BLE_GQ_REQ_SRV_DISCOVERY.
		 */
		struct {
			uint16_t start_handle;
			ble_uuid_t srvc_uuid;
		} gattc_srv_disc;
		/**
		 * @brief GATTC characteristic discovery parameters.
		 *        Type @ref NRF_BLE_GQ_REQ_CHAR_DISCOVERY.
		 */
		ble_gattc_handle_range_t gattc_char_disc;
		/**
		 * @brief GATTC characteristic descriptor discovery parameters.
		 *        Type @ref NRF_BLE_GQ_REQ_DESC_DISCOVERY.
		 */
		ble_gattc_handle_range_t gattc_desc_disc;
		/**
		 * @brief GATTS handle value notification or indication parameters.
		 *        Type @ref NRF_BLE_GQ_REQ_GATTS_HVX.
		 */
		ble_gatts_hvx_params_t gatts_hvx;
	};
};

/**
 * @brief BLE GATT Queue.
 */
struct ble_gq {
	/**
	 * @brief Maximum number of connection handles that can be registered.
	 */
	const uint16_t max_conns;
	/**
	 * @brief Pointer to array with registered connection handles.
	 */
	uint16_t *const conn_handles;
	/**
	 * @brief Pointer to array used to hold indices of request queues to purge.
	 */
	uint16_t *const purge_list;
	/**
	 * @brief Pointer to array of lists used to hold pending requests.
	 */
	sys_slist_t *const req_queue;
	/**
	 * @brief Pointer to memory slabs used to hold GATT requests.
	 */
	struct k_mem_slab *const req_blocks;
	/**
	 * @brief Heap for allocating memory for write, notification, and indication request values.
	 */
	struct k_heap *const data_pool;
};

/**
 * @brief Add a GATT request to the GATT queue instance.
 *
 * @details This function adds a request to the BGQ instance and allocates necessary memory
 *          for data that can be held within the request descriptor. If the SoftDevice is free,
 *          this request will be processed immediately. Otherwise, the request remains in the
 *          queue and is processed later.
 *
 * @param[in] gatt_queue   Pointer to the @ref ble_gq instance.
 * @param[in] req          Pointer to the request.
 * @param[in] conn_handle  Connection handle associated with the request.
 *
 * @retval 0        Request was added successfully.
 * @retval -EFAULT  Any parameter was NULL.
 * @retval -EINVAL  If @p conn_handle is not registered or type of request @p req is not valid.
 * @retval -ENOMEM  There was no room in the queue or in the data pool.
 */
int ble_gq_item_add(const struct ble_gq *gatt_queue, struct ble_gq_req *req, uint16_t conn_handle);

/**
 * @brief Register connection handle in the GATT queue instance.
 *
 * @details This function is used for registering connection handle in the BGQ instance.
 *          From this point, the BGQ instance can handle GATT requests associated with the
 *          handle until connection is no longer valid (disconnect event occurs).
 *
 * @param[in] gatt_queue   Pointer to the @ref ble_gq instance.
 * @param[in] conn_handle  Connection handle.
 *
 * @retval 0        Connection handle was successful registered.
 * @retval -EFAULT  If @p gatt_queue was NULL.
 * @retval -ENOMEM  No space for another connection handle.
 */
int ble_gq_conn_handle_register(const struct ble_gq *gatt_queue, uint16_t conn_handle);

/**
 * @brief Handle BLE events from the SoftDevice.
 *
 * @details This function handles the BLE events received from the SoftDevice. If a BLE
 *          event is relevant to the BGQ module, it is used to update internal variables,
 *          process queued GATT requests and, if necessary, send errors to the application.
 *
 * @param[in] ble_evt     Pointer to the BLE event.
 * @param[in] gatt_queue  Pointer to the @ref ble_gq instance.
 */
void ble_gq_on_ble_evt(const ble_evt_t *ble_evt, void *gatt_queue);

#ifdef __cplusplus
}
#endif

#endif /* BLE_GQ_H__ */

/** @} */
