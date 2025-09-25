.. _lib_ble_service_hrs:

Heart Rate Service (HRS)
########################

.. contents::
   :local:
   :depth: 2

This module implements the Heart Rate Service with the Heart Rate Measurement, Body Sensor Location, and Heart Rate Control Point characteristics.

Overview
********

During initialization, the module adds the Heart Rate Service and Heart Rate Measurement characteristic to the Bluetooth LE stack database.
Optionally, it also adds the Body Sensor Location and Heart Rate Control Point characteristics.

If enabled, a notification of the Heart Rate Measurement characteristic is sent when the application calls the :c:func:`ble_hrs_heart_rate_measurement_send` function.

The Heart Rate Service also provides a set of functions for manipulating the various fields in the Heart Rate Measurement characteristic, as well as setting the Body Sensor Location characteristic value.

If an event handler is supplied by the application, the Heart Rate Service will generate Heart Rate Service events to the application.

Configuration
*************

Set the :kconfig:option:`CONFIG_BLE_HRS` Kconfig option to enable the service.

You can use the :kconfig:option:`CONFIG_BLE_HRS_MAX_BUFFERED_RR_INTERVALS` Kconfig option to set the size of RR Interval buffers.

Initialization
==============

The service instance is declared using the :c:macro:`BLE_HRS_DEF` macro, specifying the name of the instance.
The service is initialized by calling the :c:func:`ble_hrs_init` function.
See the :c:struct:`ble_hrs_config` struct for configuration details, in addition to the HRS specification.

Usage
*****

Events from the service are forwarded through the event handler specified during initialization.
For a full list of events, see the :c:enum:`ble_hrs_evt_type` enum.

The :c:func:`ble_hrs_conn_params_evt` function must be called on :ref:`lib_ble_conn_params` events to forward the event to the HRS service.
The application can send heart rate measurements by calling the :c:func:`ble_hrs_heart_rate_measurement_send` function.
This requires notifications to be enabled.

The :c:func:`ble_hrs_rr_interval_add` function can be called to add a RR Interval measurement to the RR Interval buffer.
Use the :c:func:`ble_hrs_rr_interval_buffer_is_full` function to check if the RR Interval buffer is full.

The state of the sensor contact supported bit and sensor contact detected bit can be set by calling the :c:func:`ble_hrs_sensor_contact_supported_set` and :c:func:`ble_hrs_sensor_contact_detected_update` functions, respectively.
To set the body sensor location, call the :c:func:`ble_hrs_body_sensor_location_set` function.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/bm/bluetooth/services/ble_hrs.h`
| Source files: :file:`subsys/bluetooth/services/ble_hrs/`

:ref:`Heart Rate Service API reference <api_ble_hrs>`
