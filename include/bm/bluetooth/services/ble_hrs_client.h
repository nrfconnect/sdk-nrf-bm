/*
 * Copyright (c) 2012 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/**
 * @file
 *
 * @defgroup ble_hrs_client Heart Rate Service Client
 * @{
 * @brief    Heart Rate Service Client.
 *
 * @details  This library contains the APIs and types exposed by the Heart Rate Service Client
 *           library. The application can use these APIs and types to perform the discovery of
 *           Heart Rate Service at the peer and to interact with it.
 *
 * @warning  Currently, this library only supports the Heart Rate Measurement characteristic. This
 *           means that it is able to enable notification of the characteristic at the peer and
 *           is able to receive Heart Rate Measurement notifications from the peer. It does not
 *           support the Body Sensor Location and the Heart Rate Control Point characteristics.
 *           When a Heart Rate Measurement is received, this library decodes only the
 *           Heart Rate Measurement value field (both 8-bit and 16-bit) and provides it to
 *           the application.
 */

#ifndef BLE_HRS_CLIENT_H__
#define BLE_HRS_CLIENT_H__

#include <stdint.h>
#include <ble.h>
#include <bm/bluetooth/ble_db_discovery.h>
#include <bm/bluetooth/ble_gq.h>
#include <bm/softdevice_handler/nrf_sdh_ble.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macro for defining a ble_hrs_client instance.
 *
 * @param _name Name of the instance.
 * @hideinitializer
 */
#define BLE_HRS_CLIENT_DEF(_name)                                                                 \
	static struct ble_hrs_client _name;                                                       \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_hrs_client_on_ble_evt, &_name, USER_LOW)

/**
 * @brief HRS Client event type.
 */
enum ble_hrs_client_evt_type {
	/** Event indicating that the Heart Rate Service was discovered at the peer. */
	BLE_HRS_CLIENT_EVT_DISCOVERY_COMPLETE,
	/** Event indicating that a notification of the Heart Rate Measurement characteristic was
	 *  received from the peer.
	 */
	BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION,
	/** Error. */
	BLE_HRS_CLIENT_EVT_ERROR,
};

/**
 * @brief Heart Rate Measurement received from the peer.
 */
struct ble_hrm {
	/** Heart Rate Value. */
	uint16_t hr_value;
	/** Number of RR intervals. */
	uint8_t rr_intervals_cnt;
	/** RR intervals. */
	uint16_t rr_intervals[CONFIG_BLE_HRS_CLIENT_RR_INTERVALS_MAX_COUNT];
};

/**
 * @brief Database for handles related to the Heart Rate Service found on the peer.
 */
struct hrs_db {
	/** Handle of the CCCD of the Heart Rate Measurement characteristic. */
	uint16_t hrm_cccd_handle;
	/** Handle of the Heart Rate Measurement characteristic, as provided by the SoftDevice. */
	uint16_t hrm_handle;
};

/**
 * @brief Heart Rate Event.
 */
struct ble_hrs_client_evt {
	/** Type of the event. */
	enum ble_hrs_client_evt_type evt_type;
	/** Connection handle on which the Heart Rate service was discovered on the peer device. */
	uint16_t conn_handle;
	union {
		/** Handles related to the Heart Rate, found on the peer device.
		 *  This is filled if the evt_type is @ref BLE_HRS_CLIENT_EVT_DISCOVERY_COMPLETE.
		 */
		struct hrs_db peer_db;
		/** Heart Rate Measurement received. This is filled if the evt_type
		 *  is @ref BLE_HRS_CLIENT_EVT_HRM_NOTIFICATION.
		 */
		struct ble_hrm hrm;
		/** Error event. This is filled if the evt_type
		 *  is @ref BLE_HRS_CLIENT_EVT_ERROR.
		 */
		struct {
			/** Error reason */
			uint32_t reason;
		} error;
	} params;
};

/** Forward declaration. */
struct ble_hrs_client;

/**
 * @brief Event handler type.
 *
 * @details This is the type of the event handler that is to be provided by the application
 *          of this module to receive events.
 */
typedef void (*ble_hrs_client_evt_handler_t)(struct ble_hrs_client *ble_hrs_client,
					      struct ble_hrs_client_evt *evt);

/**
 * @brief Heart Rate Client.
 */
struct ble_hrs_client {
	/** Connection handle, as provided by the SoftDevice. */
	uint16_t conn_handle;
	/** Handles related to HRS on the peer. */
	struct hrs_db peer_hrs_db;
	/** Application event handler to be called when there
	 *  is an event related to the Heart Rate Service.
	 */
	ble_hrs_client_evt_handler_t evt_handler;
	/** Bluetooth LE GATT Queue instance. */
	const struct ble_gq *gatt_queue;
};

/**
 * @brief Heart Rate Client configuration structure.
 */
struct ble_hrs_client_config {
	/** Event handler to be called by the Heart Rate Client module when
	 *  there is an event related to the Heart Rate Service.
	 */
	ble_hrs_client_evt_handler_t evt_handler;
	/** Bluetooth LE GATT Queue instance. */
	const struct ble_gq *gatt_queue;
	/** Database discovery instance. */
	struct ble_db_discovery *db_discovery;
};

/**
 * @brief Initialize the Heart Rate Client module.
 *
 * @details This function registers with the Database Discovery module for the Heart Rate Service.
 *          The module looks for the presence of a Heart Rate Service instance at the peer
 *          when a discovery is started.
 *
 * @param[in] ble_hrs_client Heart Rate Client structure.
 * @param[in] ble_hrs_client_config Heart rate service client configuration.
 *
 * @retval NRF_SUCCESS On successful initialization.
 * @return Otherwise, this function propagates the error code returned by the
 *         Database Discovery module API @ref ble_db_discovery_service_register.
 */
uint32_t ble_hrs_client_init(struct ble_hrs_client *ble_hrs_client,
			      struct ble_hrs_client_config *ble_hrs_client_config);

/**
 * @brief Handle Bluetooth LE events from the SoftDevice.
 *
 * @details This function handles the Bluetooth LE events received from the SoftDevice.
 *          If an event is relevant to the Heart Rate Client module, the function uses
 *          the event's data to update interval variables and, if necessary, send events to the
 *          application.
 *
 * @param[in] ble_evt Bluetooth LE event.
 * @param[in] ctx Heart Rate Client structure.
 */
void ble_hrs_client_on_ble_evt(const ble_evt_t *ble_evt, void *ctx);

/**
 * @brief Request the peer to start sending notification of Heart Rate Measurement.
 *
 * @details This function enables notification of the Heart Rate Measurement at the peer
 *          by writing to the CCCD of the Heart Rate Measurement characteristic.
 *
 * @param ble_hrs_client Heart Rate Client structure.
 *
 * @retval NRF_SUCCESS If the SoftDevice is requested to write to the CCCD of the peer.
 * @return Error code returned  by the SoftDevice API @ref sd_ble_gattc_write.
 */
uint32_t ble_hrs_client_hrm_notif_enable(struct ble_hrs_client *ble_hrs_client);

/**
 * @brief Request the peer to stop sending notification of Heart Rate Measurement.
 *
 * @details This function disables notification of the Heart Rate Measurement at the peer
 *          by writing to the CCCD of the Heart Rate Measurement characteristic.
 *
 * @param ble_hrs_client Heart Rate Client structure.
 *
 * @retval NRF_SUCCESS If the SoftDevice is requested to write to the CCCD of the peer.
 * @return Error code returned  by the SoftDevice API @ref sd_ble_gattc_write.
 */
uint32_t ble_hrs_client_hrm_notif_disable(struct ble_hrs_client *ble_hrs_client);

/**
 * @brief Handle events from the Database Discovery module.
 *
 * @details Call this function when you get a callback event from the Database Discovery
 *          module. This function handles an event from the Database Discovery module and determines
 *          whether it relates to the discovery of Heart Rate Service at the peer. If it
 *          does, the function calls the application's event handler to indicate that the Heart Rate
 *          Service was discovered at the peer. The function also populates the event with
 *          service-related information before providing it to the application.
 *
 * @param[in] ble_hrs_client Heart Rate Client structure instance for
 * associating the link.
 * @param[in] evt Event received from the Database Discovery module.
 *
 */
void ble_hrs_on_db_disc_evt(struct ble_hrs_client *ble_hrs_client,
			    const struct ble_db_discovery_evt *evt);

/**
 * @brief Assign handles to an instance of hrs_c.
 *
 * @details Call this function when a link has been established with a peer to
 *          associate the link to this instance of the module. This association makes it
 *          possible to handle several links and associate each link to a particular
 *          instance of this module. The connection handle and attribute handles are
 *          provided from the discovery event @ref BLE_HRS_CLIENT_EVT_DISCOVERY_COMPLETE.
 *
 * @param[in] ble_hrs_client Heart Rate Client structure instance for
 * associating the link.
 * @param[in] conn_handle Connection handle to associate with the given Heart Rate
 * Client Instance.
 * @param[in] peer_hrs_handles Attribute handles for the HRS server you want this HRS_C
 * client to interact with.
 */
uint32_t ble_hrs_client_handles_assign(struct ble_hrs_client *ble_hrs_client,
					uint16_t conn_handle,
					const struct hrs_db *peer_hrs_handles);

#ifdef __cplusplus
}
#endif

#endif /* BLE_HRS_CLIENT_H__ */

/** @} */
