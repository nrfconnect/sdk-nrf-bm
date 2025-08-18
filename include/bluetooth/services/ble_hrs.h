/*
 * Copyright (c) 2012 - 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/** @file
 *
 * @defgroup ble_hrs Heart Rate Service
 * @{
 * @brief Heart Rate Service.
 */
#ifndef BLE_HRS_H__
#define BLE_HRS_H__

#include <stdint.h>
#include <stdbool.h>
#include <ble.h>
#include <nrf_sdh_ble.h>
#include <ble_conn_params.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Define a Heart rate service instance.
 *
 * Define a heart rate service instance and register it as a Bluetooth event observer.
 */
#define BLE_HRS_DEF(_name)                                                                         \
	static struct ble_hrs _name;                                                               \
	extern void ble_hrs_on_ble_evt(const ble_evt_t *ble_evt, void *ctx);                       \
	NRF_SDH_BLE_OBSERVER(_name##_obs, ble_hrs_on_ble_evt, &_name, 0)

/**
 * @defgroup BLE_HRS_BODY_SENSOR_LOCATION HRS Body sensor location
 * @{
 */
#define BLE_HRS_BODY_SENSOR_LOCATION_OTHER 0
#define BLE_HRS_BODY_SENSOR_LOCATION_CHEST 1
#define BLE_HRS_BODY_SENSOR_LOCATION_WRIST 2
#define BLE_HRS_BODY_SENSOR_LOCATION_FINGER 3
#define BLE_HRS_BODY_SENSOR_LOCATION_HAND 4
#define BLE_HRS_BODY_SENSOR_LOCATION_EAR_LOBE 5
#define BLE_HRS_BODY_SENSOR_LOCATION_FOOT 6
/** @} */

/**
 * @brief Heart rate service event types.
 */
enum ble_hrs_evt_type {
	/**
	 * @brief Heart rate value notification enabled.
	 */
	BLE_HRS_EVT_NOTIFICATION_ENABLED,
	/**
	 * @brief Heart rate value notifcation disabled.
	 */
	BLE_HRS_EVT_NOTIFICATION_DISABLED
};

/**
 * @brief Heart rate service event.
 */
struct ble_hrs_evt {
	/**
	 * @brief Event type.
	 */
	enum ble_hrs_evt_type evt_type;
};

/* Forward declaration */
struct ble_hrs;

/**
 * @brief Heart rate service event handler type.
 */
typedef void (*ble_hrs_evt_handler_t)(struct ble_hrs *hrs, const struct ble_hrs_evt *evt);

/**
 * @brief Heart rate service configuration.
 */
struct ble_hrs_config {
	/**
	 * @brief Heart rate service event handler.
	 */
	ble_hrs_evt_handler_t evt_handler;
	/**
	 * @brief If sensor contact detection is to be supported or not.
	 */
	bool is_sensor_contact_supported;
	/**
	 * @brief Initial value of the Body Sensor Location characteristic, if not @c NULL.
	 */
	uint8_t *body_sensor_location;
	/**
	 * @brief Security requirement for writing the heart rate monitor characteristic CCCD.
	 */
	ble_gap_conn_sec_mode_t hrm_cccd_wr_sec;
	/**
	 * @brief Security requirement for reading the body sensor location characteristic value.
	 */
	ble_gap_conn_sec_mode_t bsl_rd_sec;
};

/**
 * @brief Heart rate service structure.
 */
struct ble_hrs {
	/**
	 * @brief Heart rate service event handler.
	 */
	ble_hrs_evt_handler_t evt_handler;
	/**
	 * @brief Heart rate service handle.
	 */
	uint16_t service_handle;
	/**
	 * @brief Handle of the current connection.
	 *
	 * Provided by the BLE stack. Is BLE_CONN_HANDLE_INVALID if not in a connection.
	 */
	uint16_t conn_handle;
	/**
	 * @brief Handles related to the heart rate measurement characteristic.
	 */
	ble_gatts_char_handles_t hrm_handles;
	/**
	 * @brief Handles related to the body sensor location characteristic.
	 */
	ble_gatts_char_handles_t bsl_handles;
	/**
	 * @brief Set of RR interval measurements since the last heart rate measurement
	 *        transmission.
	 */
	uint16_t rr_interval[CONFIG_BLE_HRS_MAX_BUFFERED_RR_INTERVALS];
	/**
	 * @brief Number of RR interval measurements since the last heart rate measurement
	 *        transmission.
	 */
	uint16_t rr_interval_count;
	/**
	 * @brief Current maximum heart rate measurement length.
	 *
	 * Adjusted according to the current ATT MTU.
	 */
	uint8_t max_hrm_len;
	/**
	 * @brief Whether sensor contact detection is supported or not.
	 */
	bool is_sensor_contact_supported;
	/**
	 * @brief Whether sensor contact has been detected.
	 */
	bool is_sensor_contact_detected;
};

/**
 * @brief Initialize the heart rate service.
 *
 * @param hrs Heart rate service.
 * @param hrs_config Heart rate service configuration.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p hrs or @p hrs_config are @c NULL.
 * @retval -EINVAL Invalid parameters.
 */
int ble_hrs_init(struct ble_hrs *hrs, const struct ble_hrs_config *hrs_config);

/**
 * @brief Connection parameters event handler.
 *
 * @details Handles all events from the conn params library of interest to the heart rate service.
 *
 * @param hrs Heart rate service.
 * @param conn_params_evt Event received from the conn params library.
 */
void ble_hrs_conn_params_evt(struct ble_hrs *hrs, const struct ble_conn_params_evt *conn_param_evt);

/**
 * @brief Function for sending heart rate measurement if notification has been enabled.
 *
 * @details The application calls this function after having performed a heart rate measurement.
 *          If notification has been enabled, the heart rate measurement data is encoded and sent to
 *          the client.
 *
 * @param hrs Heart rate service.
 * @param heart_rate heart rate Measurement.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p hrs is @c NULL.
 * @retval -EINVAL Failed to send notification.
 * @retval -ENOTCONN Invalid connection handle.
 * @retval -EPIPE Notifications not enabled in the CCCD.
 */
int ble_hrs_heart_rate_measurement_send(struct ble_hrs *hrs, uint16_t heart_rate);

/**
 * @brief Function for adding a RR Interval measurement to the RR Interval buffer.
 *
 * @details All buffered RR Interval measurements will be included in the next heart rate
 *          measurement message, up to the maximum number of measurements that will fit into the
 *          message. If the buffer is full, the oldest measurement in the buffer will be deleted.
 *
 * @param hrs Heart rate service.
 * @param rr_interval New RR interval measurement.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p hrs is @c NULL.
 */
int ble_hrs_rr_interval_add(struct ble_hrs *hrs, uint16_t rr_interval);

/**
 * @brief Function for checking if RR Interval buffer is full.
 *
 * @param hrs Heart rate service.
 *
 * @return Returns @c true if RR Interval buffer is full, @c false otherwise.
 */
bool ble_hrs_rr_interval_buffer_is_full(struct ble_hrs *hrs);

/**
 * @brief Function for setting the state of the sensor contact supported bit.
 *
 * @note Changing the sensor contact supported bit is not allowed when in a connection.
 *
 * @param hrs Heart rate service.
 * @param is_sensor_contact_supported New state of the sensor contact support bit.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p hrs is @c NULL.
 * @retval -EISCONN If in a connection.
 */
int ble_hrs_sensor_contact_supported_set(struct ble_hrs *hrs, bool is_sensor_contact_supported);

/**
 * @brief Function for setting the state of the sensor contact detected bit.
 *
 * @param hrs Heart rate service.
 * @param is_sensor_contact_detected New state of the sensor contact detected bit.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p hrs is @c NULL.
 */
int ble_hrs_sensor_contact_detected_update(struct ble_hrs *hrs, bool is_sensor_contact_detected);

/**
 * @brief Function for setting the Body Sensor Location.
 *
 * @details Sets a new value of the Body Sensor Location characteristic. The new value will be sent
 *          to the client the next time the client reads the Body Sensor Location characteristic.
 *
 * @param hrs Heart rate service.
 * @param body_sensor_location New body sensor location.
 *
 * @retval 0 On success.
 * @retval -EFAULT If @p hrs is @c NULL.
 * @retval -EINVAL Failed to set new value.
 */
int ble_hrs_body_sensor_location_set(struct ble_hrs *hrs, uint8_t body_sensor_location);

#ifdef __cplusplus
}
#endif

#endif /* BLE_HRS_H__ */

/** @} */
