/**
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BLE_SERVICES_UUID_H__
#define BLE_SERVICES_UUID_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup UUID_SERVICES Service UUID definitions
 * @{ */
/** Alert Notification service UUID. */
#define BLE_UUID_ALERT_NOTIFICATION_SERVICE 0x1811
/** Battery service UUID. */
#define BLE_UUID_BATTERY_SERVICE 0x180F
/** Blood Pressure service UUID. */
#define BLE_UUID_BLOOD_PRESSURE_SERVICE 0x1810
/** Current Time service UUID. */
#define BLE_UUID_CURRENT_TIME_SERVICE 0x1805
/** Cycling Speed and Cadence service UUID. */
#define BLE_UUID_CYCLING_SPEED_AND_CADENCE 0x1816
/** Location and Navigation service UUID. */
#define BLE_UUID_LOCATION_AND_NAVIGATION_SERVICE 0x1819
/** Device Information service UUID. */
#define BLE_UUID_DEVICE_INFORMATION_SERVICE 0x180A
/** Glucose service UUID. */
#define BLE_UUID_GLUCOSE_SERVICE 0x1808
/** Health Thermometer service UUID. */
#define BLE_UUID_HEALTH_THERMOMETER_SERVICE 0x1809
/** Heart Rate service UUID. */
#define BLE_UUID_HEART_RATE_SERVICE 0x180D
/** Human Interface Device service UUID. */
#define BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE 0x1812
/** Immediate Alert service UUID. */
#define BLE_UUID_IMMEDIATE_ALERT_SERVICE 0x1802
/** Link Loss service UUID. */
#define BLE_UUID_LINK_LOSS_SERVICE 0x1803
/** Next Dst Change service UUID. */
#define BLE_UUID_NEXT_DST_CHANGE_SERVICE 0x1807
/** Phone Alert Status service UUID. */
#define BLE_UUID_PHONE_ALERT_STATUS_SERVICE 0x180E
/** Reference Time Update service UUID. */
#define BLE_UUID_REFERENCE_TIME_UPDATE_SERVICE 0x1806
/** Running Speed and Cadence service UUID. */
#define BLE_UUID_RUNNING_SPEED_AND_CADENCE 0x1814
/** Scan Parameters service UUID. */
#define BLE_UUID_SCAN_PARAMETERS_SERVICE 0x1813
/** TX Power service UUID. */
#define BLE_UUID_TX_POWER_SERVICE 0x1804
/** Internet Protocol Support service UUID. */
#define BLE_UUID_IPSP_SERVICE 0x1820
/** BOND MANAGEMENT service UUID */
#define BLE_UUID_BMS_SERVICE 0x181E
/** Continuous Glucose Monitoring service UUID */
#define BLE_UUID_CGM_SERVICE 0x181F
/** Pulse Oximeter Service UUID */
#define BLE_UUID_PLX_SERVICE 0x1822
/** Object Transfer Service UUID */
#define BLE_UUID_OTS_SERVICE 0x1825

/** @} */

/** @defgroup UUID_CHARACTERISTICS Characteristic UUID definitions
 * @{ */
/** Removable characteristic UUID. */
#define BLE_UUID_REMOVABLE_CHAR 0x2A3A
/** Service Required characteristic UUID. */
#define BLE_UUID_SERVICE_REQUIRED_CHAR 0x2A3B
/** Alert Category Id characteristic UUID. */
#define BLE_UUID_ALERT_CATEGORY_ID_CHAR 0x2A43
/** Alert Category Id Bit Mask characteristic UUID. */
#define BLE_UUID_ALERT_CATEGORY_ID_BIT_MASK_CHAR 0x2A42
/** Alert Level characteristic UUID. */
#define BLE_UUID_ALERT_LEVEL_CHAR 0x2A06
/** Alert Notification Control Point characteristic UUID. */
#define BLE_UUID_ALERT_NOTIFICATION_CONTROL_POINT_CHAR 0x2A44
/** Alert Status characteristic UUID. */
#define BLE_UUID_ALERT_STATUS_CHAR 0x2A3F
/** Battery Level characteristic UUID. */
#define BLE_UUID_BATTERY_LEVEL_CHAR 0x2A19
/** Blood Pressure Feature characteristic UUID. */
#define BLE_UUID_BLOOD_PRESSURE_FEATURE_CHAR 0x2A49
/** Blood Pressure Measurement characteristic UUID. */
#define BLE_UUID_BLOOD_PRESSURE_MEASUREMENT_CHAR 0x2A35
/** Body Sensor Location characteristic UUID. */
#define BLE_UUID_BODY_SENSOR_LOCATION_CHAR 0x2A38
/** Boot Keyboard Input Report characteristic UUID. */
#define BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT_CHAR 0x2A22
/** Boot Keyboard Output Report characteristic UUID. */
#define BLE_UUID_BOOT_KEYBOARD_OUTPUT_REPORT_CHAR 0x2A32
/** Boot Mouse Input Report characteristic UUID. */
#define BLE_UUID_BOOT_MOUSE_INPUT_REPORT_CHAR 0x2A33
/** Current Time characteristic UUID. */
#define BLE_UUID_CURRENT_TIME_CHAR 0x2A2B
/** Date Time characteristic UUID. */
#define BLE_UUID_DATE_TIME_CHAR 0x2A08
/** Day Date Time characteristic UUID. */
#define BLE_UUID_DAY_DATE_TIME_CHAR 0x2A0A
/** Day Of Week characteristic UUID. */
#define BLE_UUID_DAY_OF_WEEK_CHAR 0x2A09
/** Dst Offset characteristic UUID. */
#define BLE_UUID_DST_OFFSET_CHAR 0x2A0D
/** Exact Time 256 characteristic UUID. */
#define BLE_UUID_EXACT_TIME_256_CHAR 0x2A0C
/** Firmware Revision String characteristic UUID. */
#define BLE_UUID_FIRMWARE_REVISION_STRING_CHAR 0x2A26
/** Glucose Feature characteristic UUID. */
#define BLE_UUID_GLUCOSE_FEATURE_CHAR 0x2A51
/** Glucose Measurement characteristic UUID. */
#define BLE_UUID_GLUCOSE_MEASUREMENT_CHAR 0x2A18
/** Glucose Measurement Context characteristic UUID. */
#define BLE_UUID_GLUCOSE_MEASUREMENT_CONTEXT_CHAR 0x2A34
/** Hardware Revision String characteristic UUID. */
#define BLE_UUID_HARDWARE_REVISION_STRING_CHAR 0x2A27
/** Heart Rate Control Point characteristic UUID. */
#define BLE_UUID_HEART_RATE_CONTROL_POINT_CHAR 0x2A39
/** Heart Rate Measurement characteristic UUID. */
#define BLE_UUID_HEART_RATE_MEASUREMENT_CHAR 0x2A37
/** Hid Control Point characteristic UUID. */
#define BLE_UUID_HID_CONTROL_POINT_CHAR 0x2A4C
/** Hid Information characteristic UUID. */
#define BLE_UUID_HID_INFORMATION_CHAR 0x2A4A
/** IEEE Regulatory Certification Data List characteristic UUID. */
#define BLE_UUID_IEEE_REGULATORY_CERTIFICATION_DATA_LIST_CHAR 0x2A2A
/** Intermediate Cuff Pressure characteristic UUID. */
#define BLE_UUID_INTERMEDIATE_CUFF_PRESSURE_CHAR 0x2A36
/** Intermediate Temperature characteristic UUID. */
#define BLE_UUID_INTERMEDIATE_TEMPERATURE_CHAR 0x2A1E
/** Local Time Information characteristic UUID. */
#define BLE_UUID_LOCAL_TIME_INFORMATION_CHAR 0x2A0F
/** Manufacturer Name String characteristic UUID. */
#define BLE_UUID_MANUFACTURER_NAME_STRING_CHAR 0x2A29
/** Measurement Interval characteristic UUID. */
#define BLE_UUID_MEASUREMENT_INTERVAL_CHAR 0x2A21
/** Model Number String characteristic UUID. */
#define BLE_UUID_MODEL_NUMBER_STRING_CHAR 0x2A24
/** Unread Alert characteristic UUID. */
#define BLE_UUID_UNREAD_ALERT_CHAR 0x2A45
/** New Alert characteristic UUID. */
#define BLE_UUID_NEW_ALERT_CHAR 0x2A46
/** PNP Id characteristic UUID. */
#define BLE_UUID_PNP_ID_CHAR 0x2A50
/** Protocol Mode characteristic UUID. */
#define BLE_UUID_PROTOCOL_MODE_CHAR 0x2A4E
/** Record Access Control Point characteristic UUID. */
#define BLE_UUID_RECORD_ACCESS_CONTROL_POINT_CHAR 0x2A52
/** Reference Time Information characteristic UUID. */
#define BLE_UUID_REFERENCE_TIME_INFORMATION_CHAR 0x2A14
/** Report characteristic UUID. */
#define BLE_UUID_REPORT_CHAR 0x2A4D
/** Report Map characteristic UUID. */
#define BLE_UUID_REPORT_MAP_CHAR 0x2A4B
/** Ringer Control Point characteristic UUID. */
#define BLE_UUID_RINGER_CONTROL_POINT_CHAR 0x2A40
/** Ringer Setting characteristic UUID. */
#define BLE_UUID_RINGER_SETTING_CHAR 0x2A41
/** Scan Interval Window characteristic UUID. */
#define BLE_UUID_SCAN_INTERVAL_WINDOW_CHAR 0x2A4F
/** Scan Refresh characteristic UUID. */
#define BLE_UUID_SCAN_REFRESH_CHAR 0x2A31
/** Serial Number String characteristic UUID. */
#define BLE_UUID_SERIAL_NUMBER_STRING_CHAR 0x2A25
/** Software Revision String characteristic UUID. */
#define BLE_UUID_SOFTWARE_REVISION_STRING_CHAR 0x2A28
/** Supported New Alert Category characteristic UUID. */
#define BLE_UUID_SUPPORTED_NEW_ALERT_CATEGORY_CHAR 0x2A47
/** Supported Unread Alert Category characteristic UUID. */
#define BLE_UUID_SUPPORTED_UNREAD_ALERT_CATEGORY_CHAR 0x2A48
/** System Id characteristic UUID. */
#define BLE_UUID_SYSTEM_ID_CHAR 0x2A23
/** Temperature Measurement characteristic UUID. */
#define BLE_UUID_TEMPERATURE_MEASUREMENT_CHAR 0x2A1C
/** Temperature Type characteristic UUID. */
#define BLE_UUID_TEMPERATURE_TYPE_CHAR 0x2A1D
/** Time Accuracy characteristic UUID. */
#define BLE_UUID_TIME_ACCURACY_CHAR 0x2A12
/** Time Source characteristic UUID. */
#define BLE_UUID_TIME_SOURCE_CHAR 0x2A13
/** Time Update Control Point characteristic UUID. */
#define BLE_UUID_TIME_UPDATE_CONTROL_POINT_CHAR 0x2A16
/** Time Update State characteristic UUID. */
#define BLE_UUID_TIME_UPDATE_STATE_CHAR 0x2A17
/** Time With Dst characteristic UUID. */
#define BLE_UUID_TIME_WITH_DST_CHAR 0x2A11
/** Time Zone characteristic UUID. */
#define BLE_UUID_TIME_ZONE_CHAR 0x2A0E
/** TX Power Level characteristic UUID. */
#define BLE_UUID_TX_POWER_LEVEL_CHAR 0x2A07
/** Cycling Speed and Cadence Feature characteristic UUID. */
#define BLE_UUID_CSC_FEATURE_CHAR 0x2A5C
/** Cycling Speed and Cadence Measurement characteristic UUID. */
#define BLE_UUID_CSC_MEASUREMENT_CHAR 0x2A5B
/** Running Speed and Cadence Feature characteristic UUID. */
#define BLE_UUID_RSC_FEATURE_CHAR 0x2A54
/** Speed and Cadence Control Point UUID. */
#define BLE_UUID_SC_CTRLPT_CHAR 0x2A55
/** Running Speed and Cadence Measurement characteristic UUID. */
#define BLE_UUID_RSC_MEASUREMENT_CHAR 0x2A53
/** Sensor Location characteristic UUID. */
#define BLE_UUID_SENSOR_LOCATION_CHAR 0x2A5D
/** External Report Reference descriptor UUID. */
#define BLE_UUID_EXTERNAL_REPORT_REF_DESCR 0x2907
/** Report Reference descriptor UUID. */
#define BLE_UUID_REPORT_REF_DESCR 0x2908
/** Location Navigation Service, Feature characteristic UUID. */
#define BLE_UUID_LN_FEATURE_CHAR 0x2A6A
/** Location Navigation Service, Position quality UUID. */
#define BLE_UUID_LN_POSITION_QUALITY_CHAR 0x2A69
/** Location Navigation Service, Location and Speed characteristic UUID. */
#define BLE_UUID_LN_LOCATION_AND_SPEED_CHAR 0x2A67
/** Location Navigation Service, Navigation characteristic UUID. */
#define BLE_UUID_LN_NAVIGATION_CHAR 0x2A68
/** Location Navigation Service, Control point characteristic UUID. */
#define BLE_UUID_LN_CONTROL_POINT_CHAR 0x2A6B
/** BMS Control Point characteristic UUID. */
#define BLE_UUID_BMS_CTRLPT 0x2AA4
/** BMS Feature characteristic UUID. */
#define BLE_UUID_BMS_FEATURE 0x2AA5
/** CGM Service, Measurement characteristic UUID */
#define BLE_UUID_CGM_MEASUREMENT 0x2AA7
/** CGM Service, Feature characteristic UUID */
#define BLE_UUID_CGM_FEATURE 0x2AA8
/** CGM Service, Status characteristic UUID */
#define BLE_UUID_CGM_STATUS 0x2AA9
/** CGM Service, session start time characteristic UUID */
#define BLE_UUID_CGM_SESSION_START_TIME 0x2AAA
/** CGM Service, session run time characteristic UUID */
#define BLE_UUID_CGM_SESSION_RUN_TIME 0x2AAB
/** CGM Service, specific ops ctrlpt characteristic UUID */
#define BLE_UUID_CGM_SPECIFIC_OPS_CTRLPT 0x2AAC
/** PLX Service, spot check measurement characteristic UUID */
#define BLE_UUID_PLX_SPOT_CHECK_MEAS 0x2A5E
/** PLX Service, continuous measurement characteristic UUID */
#define BLE_UUID_PLX_CONTINUOUS_MEAS 0x2A5F
/** PLX Service, feature characteristic UUID */
#define BLE_UUID_PLX_FEATURES 0x2A60
/** OTS Service, feature characteristic UUID */
#define BLE_UUID_OTS_FEATURES 0x2ABD
/** OTS Service, Object Name characteristic UUID */
#define BLE_UUID_OTS_OBJECT_NAME 0x2ABE
/** OTS Service, Object Type characteristic UUID */
#define BLE_UUID_OTS_OBJECT_TYPE 0x2ABF
/** OTS Service, Object Size characteristic UUID */
#define BLE_UUID_OTS_OBJECT_SIZE 0x2AC0
/** OTS Service, Object First Created characteristic UUID */
#define BLE_UUID_OTS_OBJECT_FIRST_CREATED 0x2AC1
/** OTS Service, Object Last Modified characteristic UUID */
#define BLE_UUID_OTS_OBJECT_LAST_MODIFIED 0x2AC2
/** OTS Service, Object ID characteristic UUID */
#define BLE_UUID_OTS_OBJECT_ID 0x2AC3
/** OTS Service, Object Properties characteristic UUID */
#define BLE_UUID_OTS_OBJECT_PROPERTIES 0x2AC4
/** OTS Service, Object Action Control Point characteristic UUID */
#define BLE_UUID_OTS_OACP 0x2AC5
/** OTS Service, Object List Control Point characteristic UUID */
#define BLE_UUID_OTS_OLCP 0x2AC6
/** OTS Service, Object List Filter characteristic UUID */
#define BLE_UUID_OTS_LF 0x2AC7
/** OTS Service, Object Changed characteristic UUID */
#define BLE_UUID_OTS_OBJECT_CHANGED 0x2AC8

/** @} */

/** No Alert. */
#define BLE_CHAR_ALERT_LEVEL_NO_ALERT 0x00
/** Mild Alert. */
#define BLE_CHAR_ALERT_LEVEL_MILD_ALERT 0x01
/** High Alert. */
#define BLE_CHAR_ALERT_LEVEL_HIGH_ALERT 0x02

/** The length of an encoded Report Reference Descriptor. */
#define BLE_SRV_ENCODED_REPORT_REF_LEN 2
/** The length of a CCCD value. */
#define BLE_CCCD_VALUE_LEN 2

#ifdef __cplusplus
}
#endif

#endif /* BLE_SERVICES_UUID_H__ */
