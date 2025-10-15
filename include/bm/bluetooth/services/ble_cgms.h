/*
 * Copyright (c) 2016 - 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
/** @file
 *
 * @defgroup ble_cgms Continuous Glucose Monitoring Service
 * @{
 * @ingroup ble_sdk_srv
 * @brief Continuous Glucose Monitoring Service (CGMS) module.
 *
 * @details This module implements a sensor for the Continuous Glucose Monitoring Service.
 *          The sensor is a GATT Server that sends CGM measurements to a connected CGMS
 *          Collector. The CGMS sensor stores records that can be accessed with the
 *          Record Access Control Point (RACP). The collector can access the features and status
 *          of the sensor. Session Run Time and Session Start Time can be used to convey timing
 *          information between the sensor and the collector. The Specific Ops Control Point
 *          is used to stop and start monitoring sessions, among other things.
 *
 * @note    The application must register this module as BLE event observer using the
 *          NRF_SDH_BLE_OBSERVER macro. Example:
 *          @code
 *              struct ble_cgms instance;
 *              NRF_SDH_BLE_OBSERVER(anything, BLE_CGMS_BLE_OBSERVER_PRIO,
 *                                   ble_cgms_on_ble_evt, &instance);
 *          @endcode
 */

#ifndef BLE_CGMS_H__
#define BLE_CGMS_H__

#include <stdint.h>

#include <bm/bluetooth/ble_gq.h>
#include <bm/bluetooth/ble_racp.h>
#include <bm/nrf_sdh_ble.h>
#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macro for defining a ble_cgms instance.
 *
 * @param _name Name of the instance.
 * @hideinitializer
 */
#define BLE_CGMS_DEF(_name)                                                                        \
	static struct ble_cgms _name;                                                              \
	NRF_SDH_BLE_OBSERVER(_name ## _obs,                                                        \
			     ble_cgms_on_ble_evt, &_name,                                          \
			     CONFIG_BLE_CGMS_BLE_OBSERVER_PRIO)

#define OPCODE_LENGTH 1
#define HANDLE_LENGTH 2

/**
 * @brief Macro for calculating maximum length of data (in bytes) that can be transmitted to
 *        the peer in one ATT packet, given the ATT MTU size.
 */
#define BLE_CGMS_DATA_MAX_LEN_CALC(mtu_size) ((mtu_size) - OPCODE_LENGTH - HANDLE_LENGTH)

/**
 * @name CGM Feature characteristic defines
 * @{
 */
/** Calibration supported. */
#define BLE_CGMS_FEAT_CALIBRATION_SUPPORTED BIT(0)
/** Patient High/Low Alerts supported. */
#define BLE_CGMS_FEAT_PATIENT_HIGH_LOW_ALERTS_SUPPORTED BIT(1)
/** Hypo Alerts supported. */
#define BLE_CGMS_FEAT_HYPO_ALERTS_SUPPORTED BIT(2)
/** Hyper Alerts supported. */
#define BLE_CGMS_FEAT_HYPER_ALERTS_SUPPORTED BIT(3)
/** Rate of Increase/Decrease Alerts supported. */
#define BLE_CGMS_FEAT_RATE_OF_INCREASE_DECREASE_ALERTS_SUPPORTED BIT(4)
/** Device Specific Alert supported. */
#define BLE_CGMS_FEAT_DEVICE_SPECIFIC_ALERT_SUPPORTED BIT(5)
/** Sensor Malfunction Detection supported. */
#define BLE_CGMS_FEAT_SENSOR_MALFUNCTION_DETECTION_SUPPORTED BIT(6)
/** Sensor Temperature High-Low Detection supported. */
#define BLE_CGMS_FEAT_SENSOR_TEMPERATURE_HIGH_LOW_DETECTION_SUPPORTED BIT(7)
/** Sensor Result High-Low Detection supported. */
#define BLE_CGMS_FEAT_SENSOR_RESULT_HIGH_LOW_DETECTION_SUPPORTED BIT(8)
/** Low Battery Detection supported. */
#define BLE_CGMS_FEAT_LOW_BATTERY_DETECTION_SUPPORTED BIT(9)
/** Sensor Type Error Detection supported. */
#define BLE_CGMS_FEAT_SENSOR_TYPE_ERROR_DETECTION_SUPPORTED BIT(10)
/** General Device Fault supported. */
#define BLE_CGMS_FEAT_GENERAL_DEVICE_FAULT_SUPPORTED BIT(11)
/** E2E-CRC supported. */
#define BLE_CGMS_FEAT_E2E_CRC_SUPPORTED BIT(12)
/** Multiple Bond supported. */
#define BLE_CGMS_FEAT_MULTIPLE_BOND_SUPPORTED BIT(13)
/** Multiple Sessions supported. */
#define BLE_CGMS_FEAT_MULTIPLE_SESSIONS_SUPPORTED BIT(14)
/** CGM Trend Information supported. */
#define BLE_CGMS_FEAT_CGM_TREND_INFORMATION_SUPPORTED BIT(15)
/** CGM Quality supported. */
#define BLE_CGMS_FEAT_CGM_QUALITY_SUPPORTED BIT(16)
/** @} */

/**
 * @name Continuous Glucose Monitoring type
 * @{
 */
/** Capillary Whole blood. */
#define BLE_CGMS_MEAS_TYPE_CAP_BLOOD 0x01
/** Capillary Plasma. */
#define BLE_CGMS_MEAS_TYPE_CAP_PLASMA 0x02
/** Venous Whole blood. */
#define BLE_CGMS_MEAS_TYPE_VEN_BLOOD 0x03
/** Venous Plasma. */
#define BLE_CGMS_MEAS_TYPE_VEN_PLASMA 0x04
/** Arterial Whole blood. */
#define BLE_CGMS_MEAS_TYPE_ART_BLOOD 0x05
/** Arterial Plasma. */
#define BLE_CGMS_MEAS_TYPE_ART_PLASMA 0x06
/** Undetermined Whole blood. */
#define BLE_CGMS_MEAS_TYPE_UNDET_BLOOD 0x07
/** Undetermined Plasma. */
#define BLE_CGMS_MEAS_TYPE_UNDET_PLASMA 0x08
/** Interstitial Fluid (ISF). */
#define BLE_CGMS_MEAS_TYPE_FLUID 0x09
/** Control Solution. */
#define BLE_CGMS_MEAS_TYPE_CONTROL 0x0A
/** @} */

/**
 * @name CGM sample location
 * @{
 */
/** Finger. */
#define BLE_CGMS_MEAS_LOC_FINGER 0x01
/** Alternate Site Test (AST). */
#define BLE_CGMS_MEAS_LOC_AST 0x02
/** Earlobe. */
#define BLE_CGMS_MEAS_LOC_EAR 0x03
/** Control solution. */
#define BLE_CGMS_MEAS_LOC_CONTROL 0x04
/** Subcutaneous tissue. */
#define BLE_CGMS_MEAS_LOC_SUB_TISSUE 0x05
/** Sample Location value not available. */
#define BLE_CGMS_MEAS_LOC_NOT_AVAIL 0x0F
/** @} */

/**
 * @name CGM Measurement Sensor Status Annunciation
 * @{
 */
 /** Status: Session Stopped. */
#define BLE_CGMS_STATUS_SESSION_STOPPED BIT(0)
 /** Status: Device Battery Low. */
#define BLE_CGMS_STATUS_DEVICE_BATTERY_LOW BIT(1)
 /** Status: Sensor type incorrect for device. */
#define BLE_CGMS_STATUS_SENSOR_TYPE_INCORRECT_FOR_DEVICE BIT(2)
 /** Status: Sensor malfunction. */
#define BLE_CGMS_STATUS_SENSOR_MALFUNCTION BIT(3)
 /** Status: Device Specific Alert. */
#define BLE_CGMS_STATUS_DEVICE_SPECIFIC_ALERT BIT(4)
 /** Status: General device fault has occurred in the sensor. */
#define BLE_CGMS_STATUS_GENERAL_DEVICE_FAULT BIT(5)
/** @} */

/**
 * @name CGM Measurement flags
 * @{
 */
/** CGM Trend Information present. */
#define BLE_CGMS_FLAG_TREND_INFO_PRESENT BIT(0)
/** CGM Quality present. */
#define BLE_CGMS_FLAGS_QUALITY_PRESENT BIT(1)
/** Sensor Status Annunciation Field, Warning-Octet present. */
#define BLE_CGMS_STATUS_FLAGS_WARNING_OCT_PRESENT BIT(5)
/** Sensor Status Annunciation Field, Cal/Temp-Octet present. */
#define BLE_CGMS_STATUS_FLAGS_CALTEMP_OCT_PRESENT BIT(6)
/** Sensor Status Annunciation Field, Status-Octet present. */
#define BLE_CGMS_STATUS_FLAGS_STATUS_OCT_PRESENT BIT(7)
/** @} */

/**
 * @name Byte length of various commands (used for validating, encoding, and decoding data).
 * @{
 */

/** Length of CRC Fields in bytes (if used). */
#define BLE_CGMS_CRC_LEN 2

/** Maximum size of a transmitted Glucose Measurement. */
#define BLE_CGMS_MEAS_LEN_MAX BLE_CGMS_DATA_MAX_LEN_CALC(CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE)

/** Maximum length of one measurement record.
 *  Size 1 byte, flags 1 byte, glucose concentration 2 bytes, offset 2 bytes,
 *  status 3 bytes, trend 2 bytes, quality 2 bytes, CRC 2 bytes.
 */
#define BLE_CGMS_MEAS_REC_LEN_MAX 15
/** Minimum length of one measurement record.
 *  Size 1 byte, flags 1 byte, glucose concentration 2 bytes, offset 2 bytes.
 */
#define BLE_CGMS_MEAS_REC_LEN_MIN 6

/** Maximum number of records per notification.
 *  We can send more than one measurement record per notification,
 *  but we do not want a single record split over two notifications.
 */
#define BLE_CGMS_MEAS_REC_PER_NOTIF_MAX (BLE_CGMS_MEAS_LEN_MAX / BLE_CGMS_MEAS_REC_LEN_MIN)

/** Length of a CGM Feature characteristic.
 *  Feature 3 bytes, Type 4 bits, Sample Location 4 bits, CRC 2 bytes.
 */
#define BLE_CGMS_FEATURE_LEN 6

/** Length of a CGM Status characteristic.
 *  Time Offset 2 bytes, Status 3 bytes, CRC 2 bytes.
 */
#define BLE_CGMS_STATUS_LEN 7

/** Length of the CGM Session Start Time characteristic.
 *  Session Start Time 7 bytes, Time Zone 1 byte, DST Offset 1 byte, CRC 2 bytes.
 */
#define BLE_CGMS_SST_LEN 11

/** Length of the CGM Session Run Time characteristic.
 *  CGM Session Run Time 2 bytes, CRC 2 bytes.
 */
#define BLE_CGMS_SRT_LEN 4

/** Maximum length of the CGM Specific Ops Control point (SOCP) operand. */
#define BLE_CGMS_SOCP_OPERAND_MAX 17

/** Length of the CGM Specific Ops Control point (SOCP) characteristic.
 *  Op code 1 byte, operand 17 bytes, CRC 2 bytes.
 */
#define BLE_CGMS_SOCP_LEN 20

/** Length of a Calibration Data Record.
 *  Concentration 2 bytes, time 2 bytes, calibration 4 bits, calibration sample location 4 bits,
 *  next calibration time 2 bytes, record number 2 bytes, calibration status 1 byte.
 */
#define BLE_CGMS_MAX_CALIB_LEN 10

/** Maximum number of calibration values that can be stored. */
#define BLE_CGMS_CALIBS_NB_MAX 5

/** Maximum number of pending Record Access Control Point operations. */
#define BLE_CGMS_RACP_PENDING_OPERANDS_MAX 2
/** @} */

/**
 * @defgroup ble_cgms_enums Enumerations
 * @{
 */

/** @brief CGM Service events. */
enum ble_cgms_evt_type {
	/** Error. */
	BLE_CGMS_EVT_ERROR,
	/** Glucose value notification enabled. */
	BLE_CGMS_EVT_NOTIFICATION_ENABLED,
	/** Glucose value notification disabled. */
	BLE_CGMS_EVT_NOTIFICATION_DISABLED,
	/** Glucose value notification start session. */
	BLE_CGMS_EVT_START_SESSION,
	/** Glucose value notification stop session. */
	BLE_CGMS_EVT_STOP_SESSION,
	/** Glucose value write communication interval. */
	BLE_CGMS_EVT_WRITE_COMM_INTERVAL,
};

/** @} */ /* ble_cgms_enums */

/**
 * @defgroup ble_cgms_structs Structures
 * @{
 */

/** @brief CGM Service event. */
struct ble_cgms_evt {
	/** Event type. */
	enum ble_cgms_evt_type evt_type;
	union {
		/** @ref BLE_CGMS_EVT_ERROR event data. */
		struct {
			/* Error reason */
			uint32_t reason;
		} error;
	};
};

/** @} */ /* ble_cgms_structs */

/**
 * @defgroup ble_cgms_types Types
 * @{
 */

/** @brief Forward declaration of the struct ble_cgms type. */
struct ble_cgms;

/** @brief CGM Service event handler type.
 *
 * @param cgms CGMS instance.
 * @param evt CGMS Event.
 */
typedef void (*ble_cgms_evt_handler_t)(struct ble_cgms *cgms,
					   const struct ble_cgms_evt *evt);

/** @} */ /* ble_cgms_types */

/**
 * @addtogroup ble_cgms_structs
 * @{
 */

/** @brief CGM Measurement Sensor Status Annunciation. */
struct ble_cgms_sensor_annunc {
	/** Warning annunciation. */
	uint8_t warning;
	/** Calibration and Temperature annunciation. */
	uint8_t calib_temp;
	/** Status annunciation. */
	uint8_t status;
};

/** @brief CGM measurement. */
struct ble_cgms_meas {
	/** Indicates the presence of optional fields and the Sensor Status Annunciation field. */
	uint8_t flags;
	/** Glucose concentration in mg/dL.
	 *  16-bit word comprising 4-bit exponent and signed 12-bit mantissa.
	 */
	uint16_t glucose_concentration;
	/** Relative time stamp (Time offset) in minutes since the Session Start Time (SST). */
	uint16_t time_offset;
	/** Sensor Status Annunciation.
	 *  Optional field that can contain 'Status', 'Cal/Temp', and/or 'Warning' octets.
	 */
	struct ble_cgms_sensor_annunc sensor_status_annunciation;
	/** Optional field that can include Trend Information. */
	uint16_t trend;
	/** Optional field that includes the Quality of the measurement. */
	uint16_t quality;
};

/** @brief CGM Measurement record. */
struct ble_cgms_rec {
	/** CGM measurement. */
	struct ble_cgms_meas meas;
};

/** @brief Features supported by the CGM Service. */
struct ble_cgms_feature {
	/** Information on supported features in the CGM Service. */
	uint32_t feature;
	/** Type. */
	uint8_t type;
	/** Sample location. */
	uint8_t sample_location;
};

/** @brief Status of the CGM measurement. */
struct ble_cgms_status {
	/** Time offset. */
	uint16_t time_offset;
	/** Status. */
	struct ble_cgms_sensor_annunc status;
};

/** @brief CGM Service initialization structure that contains all options and data needed for
 *         initializing the service.
 */
struct ble_cgms_config {
	/** Event handler to be called for handling events in the CGM Service. */
	ble_cgms_evt_handler_t evt_handler;
	/** Pointer to BLE GATT Queue instance. */
	const struct ble_gq *gatt_queue;
	/** Features supported by the service. */
	struct ble_cgms_feature feature;
	/** Sensor status. */
	struct ble_cgms_status initial_sensor_status;
	/** Run time. */
	uint16_t initial_run_time;
};

/** @brief Specific Operation Control Point response structure. */
struct ble_socp_rsp {
	/** Opcode describing the response. */
	uint8_t opcode;
	/** The original opcode for the request to which this response belongs. */
	uint8_t req_opcode;
	/** Response code. */
	uint8_t rsp_code;
	/** Array containing the response value. */
	uint8_t resp_val[BLE_CGMS_SOCP_OPERAND_MAX];
	/** Length of the response value. */
	uint8_t size_val;
};

/** @brief Calibration value. */
struct ble_cgms_calib {
	/** Array containing the calibration value. */
	uint8_t value[BLE_CGMS_MAX_CALIB_LEN];
};

/** @brief Record Access Control Point transaction data. */
struct ble_cgms_racp {
	/** Operator of the current request. */
	uint8_t racp_proc_operator;
	/** Current record index. */
	uint16_t racp_proc_record_idx;
	/** The last record to send,
	 *  can be used together with racp_proc_record_idx to determine a range of records to send.
	 *  (used by greater/less filters).
	 */
	uint16_t racp_proc_records_idx_last_to_send;
	/** Number of reported records. */
	uint16_t racp_proc_records_reported;
	/** RACP procedure that has been requested from the peer. */
	struct ble_racp_value racp_request;
	/** RACP response to be sent. */
	struct ble_racp_value pending_racp_response;
	/** RACP processing active. */
	bool racp_processing_active;
	/** Operand of the RACP response to be sent. */
	uint8_t pending_racp_response_operand[BLE_CGMS_RACP_PENDING_OPERANDS_MAX];
};

/** @brief Handles related to CGM characteristics. */
struct ble_cgms_char_handles {
	/** Handles related to the CGM Measurement characteristic. */
	ble_gatts_char_handles_t measurement;
	/** Handles related to the CGM Feature characteristic. */
	ble_gatts_char_handles_t feature;
	/** Handles related to the CGM Status characteristic. */
	ble_gatts_char_handles_t status;
	/** Handles related to the CGM Session Start Time characteristic. */
	ble_gatts_char_handles_t sst;
	/** Handles related to the CGM Session Run Time characteristic. */
	ble_gatts_char_handles_t srt;
	/** Handles related to the CGM Record Access Control Point characteristic. */
	ble_gatts_char_handles_t racp;
	/** Handles related to the CGM Specific Ops Control Point characteristic. */
	ble_gatts_char_handles_t socp;
};

/**@brief Status information for the CGM Service. */
struct ble_cgms {
	/** Event handler to be called for handling events in the CGM Service. */
	ble_cgms_evt_handler_t evt_handler;
	/** Pointer to BLE GATT Queue instance. */
	const struct ble_gq  *gatt_queue;
	/** Error handler to be called in case of an error from SoftDevice. */
	ble_gq_req_error_cb_t gatt_err_handler;
	/** Handle of the CGM Service (as provided by the BLE stack). */
	uint16_t service_handle;
	/** GATTS characteristic handles for the different characteristics in the service. */
	struct ble_cgms_char_handles char_handles;
	/** Handle of the current connection (as provided by the BLE stack;
	 *  @ref BLE_CONN_HANDLE_INVALID if not in a connection).
	 */
	uint16_t conn_handle;
	/** Structure to store the value of the feature characteristic. */
	struct ble_cgms_feature feature;
	/** Variable to keep track of the communication interval. */
	uint8_t comm_interval;
	/** Structure containing response data to be indicated to the peer device. */
	struct ble_socp_rsp socp_response;
	/** Calibration value. Can be read from and written to SOCP. (Feature not supported) */
	struct ble_cgms_calib calibration_val[BLE_CGMS_CALIBS_NB_MAX];
	/** Indicator if we are currently in a session. */
	bool is_session_started;
	/** Variable to keep track of the number of sessions that were run. */
	uint8_t nb_run_session;
	/** Variable to store the expected run time of a session. */
	uint16_t session_run_time;
	/** Structure to keep track of the sensor status. */
	struct ble_cgms_status sensor_status;
	/** Structure to manage Record Access requests. */
	struct ble_cgms_racp racp_data;
};

/** @} */

/**
 * @defgroup ble_cgms_functions Functions
 * @{
 */

/**
 * @brief Initialize a CGM Service instance.
 *
 * @param[out] cgms CGM Service structure. This structure must be supplied by
 *                    the application. It is initialized by this function and will later
 *                    be used to identify this particular service instance.
 * @param[in] cgms_init Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS If the service was initialized successfully.
 * @retval NRF_ERROR_NULL If any of the input parameters are NULL.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error are propagated.
 */
uint32_t ble_cgms_init(struct ble_cgms *cgms, const struct ble_cgms_config *cgms_init);

/**
 * @brief Function for handling the application's BLE stack events.
 *
 * @details Handles all events from the BLE stack that are of interest to the CGM Service.
 *
 * @param[in] ble_evt Event received from the BLE stack.
 * @param[in] context Instance of the CGM Service.
 */
void ble_cgms_on_ble_evt(const ble_evt_t *ble_evt, void *context);

/**
 * @brief Report a new glucose measurement to the CGM Service module.
 *
 * @details The application calls this function after having performed a new glucose measurement.
 *          The new measurement is recorded in the RACP database.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] rec Pointer to the glucose record (measurement plus context).
 *
 * @retval NRF_SUCCESS If a measurement was successfully created.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error are propagated.
 */
uint32_t ble_cgms_meas_create(struct ble_cgms *cgms, struct ble_cgms_rec *rec);

/**
 * @brief Assign a connection handle to a CGM Service instance.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] conn_handle Connection Handle to use for this instance of the CGM Service.
 *
 * @retval NRF_SUCCESS If the connection handle was successfully stored in the CGM Service instance.
 * @retval NRF_ERROR_NULL If any of the input parameters are NULL.
 */
uint32_t ble_cgms_conn_handle_assign(struct ble_cgms *cgms, uint16_t conn_handle);

/**
 * @brief Update the CGM status characteristic value.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] status New status.
 *
 * @retval NRF_SUCCESS If the status was updated successfully.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error are propagated.
 */
uint32_t ble_cgms_update_status(struct ble_cgms *cgms, struct ble_cgms_status *status);

/**
 * @brief Set the Session Run Time characteristic value.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] run_time Run Time that will be displayed in the Session Run Time
 *                     characteristic value.
 *
 * @retval NRF_SUCCESS If the Session Run Time characteristic value was set successfully.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error are propagated.
 */
uint32_t ble_cgms_srt_set(struct ble_cgms *cgms, uint16_t run_time);

/** @} */ /* ble_cgms_functions */

#ifdef __cplusplus
}
#endif

#endif /* BLE_CGMS_H__ */

/** @} */
