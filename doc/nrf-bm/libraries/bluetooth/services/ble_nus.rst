.. _lib_ble_service_nus:

Nordic UART Service (NUS)
#########################

.. contents::
   :local:
   :depth: 2

The Nordic UART Service is a simple GATT-based service with TX and RX characteristics.

Overview
********

Data received from the peer is passed to the application, and the data received from the application of this service is sent to the peer as Handle Value Notifications.
This module demonstrates how to implement a custom GATT-based service and characteristics using the SoftDevice.
The service is used by the application to send and receive ASCII text strings to and from the peer.

Configuration
*************

Set the :kconfig:option:`CONFIG_BLE_NUS` Kconfig option to enable the service.

Initialization
==============

The service instance is declared using the :c:macro:`BLE_NUS_DEF` macro, specifying the name of the instance.
The service is initialized by calling the :c:func:`ble_nus_init` function.
See the :c:struct:`ble_nus_config` struct for configuration details.

Usage
*****

Events from the service are forwarded through the event handler specified during initialization.
For a full list of events, see the :c:enum:`ble_nus_evt_type` enum.

The application can send data to the peer by calling the :c:func:`ble_nus_data_send` function.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/bm/bluetooth/services/ble_nus.h`
| Source files: :file:`subsys/bluetooth/services/ble_nus/`

:ref:`Nordic UART Service (NUS) API reference <api_ble_nus>`
