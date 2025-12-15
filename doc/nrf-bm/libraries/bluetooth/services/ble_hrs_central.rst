.. _lib_ble_service_hrs_client:

Heart Rate Service (HRS) Client
###############################

.. contents::
   :local:
   :depth: 2

This module implements the Heart Rate Service Client to retrieve Heart Rate Measurements and Body Sensor Location from a peripheral device with the :ref:`lib_ble_service_hrs`.

Overview
********

The HRS Client retrieves information such as the sensor location from a device that provides the Heart Rate Service.

Configuration
*************

Set the :kconfig:option:`CONFIG_BLE_HRS_CENTRAL` Kconfig option to enable the service.

Set the maximum number of RR intervals for each HRM notification using the :kconfig:option:`CONFIG_BLE_HRS_CENTRAL_RR_INTERVALS_MAX_COUNT` Kconfig option.

Initialization
==============

The service instance is declared using the :c:macro:`BLE_HRS_CENTRAL_DEF` macro, specifying the name of the instance.
To initialize the service, call the :c:func:`ble_hrs_central_init` function.
See the :c:struct:`ble_hrs_central_config` structure for configuration details, in addition to the HRS specification.

Usage
*****

Applications can use the :ref:`lib_ble_scan` library for detecting advertising devices that support the Heart Rate Service.

Upon connection, the application can use the :ref:`lib_ble_db_discovery` library to discover the peer's database.
Call the :c:func:`ble_hrs_on_db_disc_evt` function on callback events from the Database Discovery library.
This function sends the :c:enumerator:`BLE_HRS_CENTRAL_EVT_DISCOVERY_COMPLETE` event to the application if the Heart Rate Service is discovered in the peer's database.
The application must call the :c:func:`ble_hrs_central_handles_assign` function to associate the link with the HRS Client instance.

Upon Heart Rate Service discovery, the application can call the :c:func:`ble_hrs_central_hrm_notif_enable` function to request the peer to start sending Heart Rate Measurement notifications.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* :ref:`lib_nrf_sdh` - :kconfig:option:`CONFIG_NRF_SDH`
* :ref:`lib_ble_db_discovery` - :kconfig:option:`CONFIG_BLE_DB_DISCOVERY`

The library is expected to be used with the :ref:`lib_ble_scan` |BMshort| library - :kconfig:option:`CONFIG_BLE_SCAN`.

API documentation
*****************

| Header file: :file:`include/bm/bluetooth/services/ble_hrs_central.h`
| Source files: :file:`subsys/bluetooth/services/ble_hrs_central/`

:ref:`Heart Rate Service API reference <api_ble_hrs_central>`
