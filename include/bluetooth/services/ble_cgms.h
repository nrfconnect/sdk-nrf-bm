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
 *          Collector. The CGMS Sensor stores records that can be accessed with the
 *          Record Access Control Point (RACP). The collector can access the features and status
 *          of the sensor. Session Run Time and Session Start Time can be used to convey timing
 *          information between the sensor and the collector. The Specific Operations Control Point
 *          is used to stop and start monitoring sessions, among other things.
 *
 * @note    The application must register this module as BLE event observer using the
 *          NRF_SDH_BLE_OBSERVER macro. Example:
 *          @code
 *              struct nrf_ble_cgms instance;
 *              NRF_SDH_BLE_OBSERVER(anything, NRF_BLE_CGMS_BLE_OBSERVER_PRIO,
 *                                   nrf_ble_cgms_on_ble_evt, &instance);
 *          @endcode
 */

#ifndef BLE_CGMS_H__
#define BLE_CGMS_H__

#include <ble_racp.h>
#include <nrf_sdh_ble.h>
#include <ble_gq.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macro for defining a nrf_ble_cgms instance.
 *
 * @param _name Name of the instance.
 * @hideinitializer
 */
#define NRF_BLE_CGMS_DEF(_name)                                                                    \
static struct nrf_ble_cgms _name;                                                                  \
NRF_SDH_BLE_OBSERVER(_name ## _obs,                                                                \
		     nrf_ble_cgms_on_ble_evt, &_name,                                              \
		     CONFIG_NRF_BLE_CGMS_BLE_OBSERVER_PRIO)

/**
 * @name CGM Feature characteristic defines
 * @{
 */
/** Calibration supported. */
#define NRF_BLE_CGMS_FEAT_CALIBRATION_SUPPORTED (0x01 << 0)
/** Patient High/Low Alerts supported. */
#define NRF_BLE_CGMS_FEAT_PATIENT_HIGH_LOW_ALERTS_SUPPORTED (0x01 << 1)
/** Hypo Alerts supported. */
#define NRF_BLE_CGMS_FEAT_HYPO_ALERTS_SUPPORTED (0x01 << 2)
/** Hyper Alerts supported. */
#define NRF_BLE_CGMS_FEAT_HYPER_ALERTS_SUPPORTED (0x01 << 3)
/** Rate of Increase/Decrease Alerts supported. */
#define NRF_BLE_CGMS_FEAT_RATE_OF_INCREASE_DECREASE_ALERTS_SUPPORTED (0x01 << 4)
/** Device Specific Alert supported. */
#define NRF_BLE_CGMS_FEAT_DEVICE_SPECIFIC_ALERT_SUPPORTED (0x01 << 5)
/** Sensor Malfunction Detection supported. */
#define NRF_BLE_CGMS_FEAT_SENSOR_MALFUNCTION_DETECTION_SUPPORTED (0x01 << 6)
/** Sensor Temperature High-Low Detection supported. */
#define NRF_BLE_CGMS_FEAT_SENSOR_TEMPERATURE_HIGH_LOW_DETECTION_SUPPORTED (0x01 << 7)
/** Sensor Result High-Low Detection supported. */
#define NRF_BLE_CGMS_FEAT_SENSOR_RESULT_HIGH_LOW_DETECTION_SUPPORTED (0x01 << 8)
/** Low Battery Detection supported. */
#define NRF_BLE_CGMS_FEAT_LOW_BATTERY_DETECTION_SUPPORTED (0x01 << 9)
/** Sensor Type Error Detection supported. */
#define NRF_BLE_CGMS_FEAT_SENSOR_TYPE_ERROR_DETECTION_SUPPORTED (0x01 << 10)
/** General Device Fault supported. */
#define NRF_BLE_CGMS_FEAT_GENERAL_DEVICE_FAULT_SUPPORTED (0x01 << 11)
/** E2E-CRC supported. */
#define NRF_BLE_CGMS_FEAT_E2E_CRC_SUPPORTED (0x01 << 12)
/** Multiple Bond supported. */
#define NRF_BLE_CGMS_FEAT_MULTIPLE_BOND_SUPPORTED (0x01 << 13)
/** Multiple Sessions supported. */
#define NRF_BLE_CGMS_FEAT_MULTIPLE_SESSIONS_SUPPORTED (0x01 << 14)
/** CGM Trend Information supported. */
#define NRF_BLE_CGMS_FEAT_CGM_TREND_INFORMATION_SUPPORTED (0x01 << 15)
/** CGM Quality supported. */
#define NRF_BLE_CGMS_FEAT_CGM_QUALITY_SUPPORTED (0x01 << 16)
/** @} */

/**
 * @name Continuous Glucose Monitoring type
 * @{
 */
/** Capillary Whole blood. */
#define NRF_BLE_CGMS_MEAS_TYPE_CAP_BLOOD 0x01
/** Capillary Plasma. */
#define NRF_BLE_CGMS_MEAS_TYPE_CAP_PLASMA 0x02
/** Venous Whole blood. */
#define NRF_BLE_CGMS_MEAS_TYPE_VEN_BLOOD 0x03
/** Venous Plasma. */
#define NRF_BLE_CGMS_MEAS_TYPE_VEN_PLASMA 0x04
/** Arterial Whole blood. */
#define NRF_BLE_CGMS_MEAS_TYPE_ART_BLOOD 0x05
/** Arterial Plasma. */
#define NRF_BLE_CGMS_MEAS_TYPE_ART_PLASMA 0x06
/** Undetermined Whole blood. */
#define NRF_BLE_CGMS_MEAS_TYPE_UNDET_BLOOD 0x07
/** Undetermined Plasma. */
#define NRF_BLE_CGMS_MEAS_TYPE_UNDET_PLASMA 0x08
/** Interstitial Fluid (ISF). */
#define NRF_BLE_CGMS_MEAS_TYPE_FLUID 0x09
/** Control Solution. */
#define NRF_BLE_CGMS_MEAS_TYPE_CONTROL 0x0A
/** @} */

/**
 * @name CGM sample location
 * @{
 */
/** Finger. */
#define NRF_BLE_CGMS_MEAS_LOC_FINGER 0x01
/** Alternate Site Test (AST). */
#define NRF_BLE_CGMS_MEAS_LOC_AST 0x02
/** Earlobe. */
#define NRF_BLE_CGMS_MEAS_LOC_EAR 0x03
/** Control solution. */
#define NRF_BLE_CGMS_MEAS_LOC_CONTROL 0x04
/** Subcutaneous tissue. */
#define NRF_BLE_CGMS_MEAS_LOC_SUB_TISSUE 0x05
/** Sample Location value not available. */
#define NRF_BLE_CGMS_MEAS_LOC_NOT_AVAIL 0x0F
/** @} */

/**
 * @name CGM Measurement Sensor Status Annunciation
 * @{
 */
 /** Status: Session Stopped. */
#define NRF_BLE_CGMS_STATUS_SESSION_STOPPED (0x01 << 0)
 /** Status: Device Battery Low. */
#define NRF_BLE_CGMS_STATUS_DEVICE_BATTERY_LOW (0x01 << 1)
 /** Status: Sensor type incorrect for device. */
#define NRF_BLE_CGMS_STATUS_SENSOR_TYPE_INCORRECT_FOR_DEVICE (0x01 << 2)
 /** Status: Sensor malfunction. */
#define NRF_BLE_CGMS_STATUS_SENSOR_MALFUNCTION (0x01 << 3)
 /** Status: Device Specific Alert. */
#define NRF_BLE_CGMS_STATUS_DEVICE_SPECIFIC_ALERT (0x01 << 4)
 /** Status: General device fault has occurred in the sensor. */
#define NRF_BLE_CGMS_STATUS_GENERAL_DEVICE_FAULT (0x01 << 5)
/** @} */

/**
 * @name CGM Measurement flags
 * @{
 */
/** CGM Trend Information Present. */
#define NRF_BLE_CGMS_FLAG_TREND_INFO_PRESENT 0x01
/** CGM Quality Present. */
#define NRF_BLE_CGMS_FLAGS_QUALITY_PRESENT 0x02
/** Sensor Status Annunciation Field, Warning-Octet present. */
#define NRF_BLE_CGMS_STATUS_FLAGS_WARNING_OCT_PRESENT 0x20
/** Sensor Status Annunciation Field, Cal/Temp-Octet present. */
#define NRF_BLE_CGMS_STATUS_FLAGS_CALTEMP_OCT_PRESENT 0x40
/** Sensor Status Annunciation Field, Status-Octet present. */
#define NRF_BLE_CGMS_STATUS_FLAGS_STATUS_OCT_PRESENT 0x80
/** @} */

/**
 * @name Byte length of various commands (used for validating, encoding, and decoding data).
 * @{
 */
/** Length of the opcode inside the Glucose Measurement packet. */
#define NRF_BLE_CGMS_MEAS_OP_LEN 1
/** Length of the handle inside the Glucose Measurement packet. */
#define NRF_BLE_CGMS_MEAS_HANDLE_LEN 2
/** Maximum size of a transmitted Glucose Measurement. */
#define NRF_BLE_CGMS_MEAS_LEN_MAX (BLE_GATT_ATT_MTU_DEFAULT -                                      \
				   NRF_BLE_CGMS_MEAS_OP_LEN -                                      \
				   NRF_BLE_CGMS_MEAS_HANDLE_LEN)
/** Maximum length of one measurement record.
 *  Size 1 byte, flags 1 byte, glucose concentration 2 bytes, offset 2 bytes,
 *  status 3 bytes, trend 2 bytes, quality 2 bytes, CRC 2 bytes.
 */
#define NRF_BLE_CGMS_MEAS_REC_LEN_MAX 15
/** Minimum length of one measurement record.
 *  Size 1 byte, flags 1 byte, glucose concentration 2 bytes, offset 2 bytes.
 */
#define NRF_BLE_CGMS_MEAS_REC_LEN_MIN 6
/** Maximum number of records per notification.
 *  We can send more than one measurement record per notification,
 *  but we do not want a a single record split over two notifications.
 */
#define NRF_BLE_CGMS_MEAS_REC_PER_NOTIF_MAX (NRF_BLE_CGMS_MEAS_LEN_MAX /                           \
					     NRF_BLE_CGMS_MEAS_REC_LEN_MIN)

/** Length of a response. Response code 1 byte, response value 1 byte. */
#define NRF_BLE_CGMS_SOCP_RESP_CODE_LEN 2
/** Length of a feature. Feature 3 bytes, type 4 bits, sample location 4 bits, CRC 2 bytes. */
#define NRF_BLE_CGMS_FEATURE_LEN 6
/** Length of a status. Offset 2 bytes, status 3 bytes, CRC 2 bytes. */
#define NRF_BLE_CGMS_STATUS_LEN 7
/** Length of a calibration record.
 *  Concentration 2 bytes, time 2 bytes, calibration 4 bits, calibration sample location 4 bits,
 *  next calibration time 2 bytes, record number 2 bytes, calibration status 1 byte.
 */
#define NRF_BLE_CGMS_MAX_CALIB_LEN 10
/** Maximum number of calibration values that can be stored. */
#define NRF_BLE_CGMS_CALIBS_NB_MAX 5
/** Length of the start time. Date time 7 bytes, time zone 1 byte, DST 1 byte. */
#define NRF_BLE_CGMS_SST_LEN 9
/** Length of the CRC bytes (if used). */
#define NRF_BLE_CGMS_CRC_LEN 2
/** Length of the Session Run Time attribute. */
#define NRF_BLE_CGMS_SRT_LEN 2
/** Max length of a SOCP response. */
#define NRF_BLE_CGMS_SOCP_RESP_LEN (NRF_BLE_CGMS_MEAS_LEN_MAX - NRF_BLE_CGMS_SOCP_RESP_CODE_LEN)

/** Maximum number of pending Record Access Control Point operations. */
#define NRF_BLE_CGMS_RACP_PENDING_OPERANDS_MAX 2
/** @} */

/**
 * @defgroup nrf_ble_cgms_enums Enumerations
 * @{
 */

/** @brief CGM Service events. */
enum nrf_ble_cgms_evt_type {
	/** Error. */
	NRF_BLE_CGMS_EVT_ERROR,
	/** Glucose value notification enabled. */
	NRF_BLE_CGMS_EVT_NOTIFICATION_ENABLED,
	/** Glucose value notification disabled. */
	NRF_BLE_CGMS_EVT_NOTIFICATION_DISABLED,
	/** Glucose value notification start session. */
	NRF_BLE_CGMS_EVT_START_SESSION,
	/** Glucose value notification stop session. */
	NRF_BLE_CGMS_EVT_STOP_SESSION,
	/** Glucose value write communication interval. */
	NRF_BLE_CGMS_EVT_WRITE_COMM_INTERVAL,
};

/** @} */ /* nrf_ble_cgms_enums */

/**
 * @defgroup nrf_ble_cgms_structs Structures
 * @{
 */

/** @brief CGM Service event. */
struct nrf_ble_cgms_evt {
	enum nrf_ble_cgms_evt_type evt_type; /** Type of event. */
	union {
		struct {
			/* Error reason */
			uint32_t reason;
		} error;
	};
};

/** @} */ /* nrf_ble_cgms_structs */

/**
 * @defgroup nrf_ble_cgms_types Types
 * @{
 */

/** @brief Forward declaration of the struct nrf_ble_cgms type. */
struct nrf_ble_cgms;

/** @brief CGM Service event handler type.
 *
 * @param cgms CGMS instance.
 * @param evt CGMS Event.
 */
typedef void (*nrf_ble_cgms_evt_handler_t)(struct nrf_ble_cgms *cgms,
					   struct nrf_ble_cgms_evt *evt);

/** @} */ /* nrf_ble_cgms_types */

/**
 * @addtogroup nrf_ble_cgms_structs
 * @{
 */

/** @brief CGM Measurement Sensor Status Annunciation. */
struct nrf_ble_cgms_sensor_annunc {
	/** Warning annunciation. */
	uint8_t warning;
	/** Calibration and Temperature annunciation. */
	uint8_t calib_temp;
	/** Status annunciation. */
	uint8_t status;
};

/** @brief CGM measurement. */
struct nrf_ble_cgms_meas {
	/** Indicates the presence of optional fields and the Sensor Status Annunciation field. */
	uint8_t flags;
	/** Glucose concentration.
	 *  16-bit word comprising 4-bit exponent and signed 12-bit mantissa.
	 */
	uint16_t glucose_concentration;
	/** Time offset.
	 *  Represents the time difference between measurements.
	 */
	uint16_t time_offset;
	/** Sensor Status Annunciation.
	 *  Variable length, can include Status, Cal/Temp, and Warning.
	 */
	struct nrf_ble_cgms_sensor_annunc sensor_status_annunciation;
	/** Optional field that can include Trend Information. */
	uint16_t trend;
	/** Optional field that includes the Quality of the measurement. */
	uint16_t quality;
};

/** @brief CGM Measurement record. */
struct ble_cgms_rec {
	/** CGM measurement. */
	struct nrf_ble_cgms_meas meas;
};

/** @brief Features supported by the CGM Service. */
struct nrf_ble_cgms_feature {
	/** Information on supported features in the CGM Service. */
	uint32_t feature;
	/** Type. */
	uint8_t type;
	/** Sample location. */
	uint8_t sample_location;
};

/** @brief Status of the CGM measurement. */
struct nrf_ble_cgm_status {
	/** Time offset. */
	uint16_t time_offset;
	/** Status. */
	struct nrf_ble_cgms_sensor_annunc status;
};

/** @brief CGM Service initialization structure that contains all options and data needed for
 *         initializing the service.
 */
struct nrf_ble_cgms_config {
	/** Event handler to be called for handling events in the CGM Service. */
	nrf_ble_cgms_evt_handler_t evt_handler;
	/** Pointer to BLE GATT Queue instance. */
	const struct ble_gq *gatt_queue;
	/** Features supported by the service. */
	struct nrf_ble_cgms_feature feature;
	/** Sensor status. */
	struct nrf_ble_cgm_status initial_sensor_status;
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
	uint8_t resp_val[NRF_BLE_CGMS_SOCP_RESP_LEN];
	/** Length of the response value. */
	uint8_t size_val;
};

/** @brief Calibration value. */
struct nrf_ble_cgms_calib {
	/** Array containing the calibration value. */
	uint8_t value[NRF_BLE_CGMS_MAX_CALIB_LEN];
};

/** @brief Record Access Control Point transaction data. */
struct nrf_ble_cgms_racp {
	/** Operator of the current request. */
	uint8_t racp_proc_operator;
	/** Current record index. */
	uint16_t racp_proc_record_ndx;
	/** The last record to send,
	 *  can be used together with racp_proc_record_ndx to determine a range of records to send.
	 *  (used by greater/less filters).
	 */
	uint16_t racp_proc_records_ndx_last_to_send;
	/** Number of reported records. */
	uint16_t racp_proc_records_reported;
	/** RACP procedure that has been requested from the peer. */
	struct ble_racp_value racp_request;
	/** RACP response to be sent. */
	struct ble_racp_value pending_racp_response;
	/** RACP processing active. */
	bool racp_processing_active;
	/** Operand of the RACP response to be sent. */
	uint8_t pending_racp_response_operand[NRF_BLE_CGMS_RACP_PENDING_OPERANDS_MAX];
};

/** @brief Handles related to CGM characteristics. */
struct nrf_ble_cgms_char_handler {
	/** Handles related to the CGM Measurement characteristic. */
	ble_gatts_char_handles_t measurment;
	/** Handles related to the CGM Feature characteristic. */
	ble_gatts_char_handles_t feature;
	/** Handles related to the CGM Session Start Time characteristic. */
	ble_gatts_char_handles_t sst;
	/** Handles related to the CGM Record Access Control Point characteristic. */
	ble_gatts_char_handles_t racp;
	/** Handles related to the CGM Session Run Time characteristic. */
	ble_gatts_char_handles_t srt;
	/** Handles related to the CGM Specific Operations Control Point characteristic. */
	ble_gatts_char_handles_t socp;
	/** Handles related to the CGM Status characteristic. */
	ble_gatts_char_handles_t status;
};

/**@brief Status information for the CGM Service. */
struct nrf_ble_cgms {
	/** Event handler to be called for handling events in the CGM Service. */
	nrf_ble_cgms_evt_handler_t evt_handler;
	/** Pointer to BLE GATT Queue instance. */
	const struct ble_gq  *gatt_queue;
	/** Error handler to be called in case of an error from SoftDevice. */
	ble_gq_req_error_cb_t gatt_err_handler;
	/** Handle of the CGM Service (as provided by the BLE stack). */
	uint16_t service_handle;
	/** GATTS characteristic handles for the different characteristics in the service. */
	struct nrf_ble_cgms_char_handler char_handles;
	/** Handle of the current connection (as provided by the BLE stack;
	 *  @ref BLE_CONN_HANDLE_INVALID if not in a connection).
	 */
	uint16_t conn_handle;
	/** Structure to store the value of the feature characteristic. */
	struct nrf_ble_cgms_feature feature;
	/** Variable to keep track of the communication interval. */
	uint8_t comm_interval;
	/** Structure containing response data to be indicated to the peer device. */
	struct ble_socp_rsp socp_response;
	/** Calibration value. Can be read from and written to SOCP. */
	struct nrf_ble_cgms_calib calibration_val[NRF_BLE_CGMS_CALIBS_NB_MAX];
	/** Indicator if we are currently in a session. */
	bool is_session_started;
	/** Variable to keep track of the number of sessions that were run. */
	uint8_t nb_run_session;
	/** Variable to store the expected run time of a session. */
	uint16_t session_run_time;
	/** Structure to keep track of the sensor status. */
	struct nrf_ble_cgm_status sensor_status;
	/** Structure to manage Record Access requests. */
	struct nrf_ble_cgms_racp racp_data;
};

/** @} */

/**
 * @defgroup nrf_ble_cgms_functions Functions
 * @{
 */

/**
 * @brief Function for updating the status.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] status New status.
 *
 * @retval NRF_SUCCESS If the status was updated successfully.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error are propagated.
 */
int nrf_ble_cgms_update_status(struct nrf_ble_cgms *cgms, struct nrf_ble_cgm_status *status);

/**
 * @brief Function for initializing the CGM Service.
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
int nrf_ble_cgms_init(struct nrf_ble_cgms *cgms, const struct nrf_ble_cgms_config *cgms_init);

/**
 * @brief Function for handling the application's BLE stack events.
 *
 * @details Handles all events from the BLE stack that are of interest to the CGM Service.
 *
 * @param[in] ble_evt Event received from the BLE stack.
 * @param[in] context Instance of the CGM Service.
 */
void nrf_ble_cgms_on_ble_evt(ble_evt_t const *ble_evt, void *context);

/**
 * @brief Function for reporting a new glucose measurement to the CGM Service module.
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
int nrf_ble_cgms_meas_create(struct nrf_ble_cgms *cgms, struct ble_cgms_rec *rec);

/**
 * @brief Function for assigning a connection handle to a CGM Service instance.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] conn_handle Connection Handle to use for this instance of the CGM Service.
 *
 * @retval NRF_SUCCESS If the connection handle was successfully stored in the CGM Service instance.
 * @retval NRF_ERROR_NULL If any of the input parameters are NULL.
 */
int nrf_ble_cgms_conn_handle_assign(struct nrf_ble_cgms *cgms, uint16_t conn_handle);

/**
 * @brief Function for setting the Session Run Time attribute value.
 *
 * @param[in] cgms Instance of the CGM Service.
 * @param[in] run_time Run Time that will be displayed in the Session Run Time
 *                     attribute value.
 *
 * @retval NRF_SUCCESS If the Session Run Time attribute value was set successfully.
 * @return If functions from other modules return errors to this function,
 *         the @ref nrf_error are propagated.
 */
int nrf_ble_cgms_srt_set(struct nrf_ble_cgms *cgms, uint16_t run_time);

/** @} */ /* nrf_ble_cgms_functions */

#ifdef __cplusplus
}
#endif

#endif /* BLE_CGMS_H__ */

/** @} */
