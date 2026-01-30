.. _lib_ble_service_nus_client:

Nordic UART Service (NUS) Client
################################

.. contents::
   :local:
   :depth: 2

The Nordic UART Service is a simple GATT-based service with TX and RX characteristics.

Overview
********
The responsibility of connecting to the NUS server is shared between the main program and the ble_nus_client module. The main program scans and reads the advertisement packets and connects to a device that delivers the NUS. After connecting, it starts service discovery. In this way, it is possible to scan for different services without needing to start unnecessary service discoveries.
The ble_nus_client module is responsible for parsing the service scan response and passing notifications back to the main program.

Configuration
*************

Set the :kconfig:option:`CONFIG_BLE_NUS_CLIENT` Kconfig option to enable the service.

Initialization
==============

The service instance is declared using the :c:macro:`BLE_NUS_CLIENT_DEF` macro, specifying the name of the instance.
The service is initialized by calling the :c:func:`ble_nus_client_init` function.
See the :c:struct:`ble_nus_client_config` struct for configuration details.

Usage
*****

Applications can use the :ref:`lib_ble_scan` library for detecting advertising devices that support the Nordic UART Service.

Upon connection, the application can use the :ref:`lib_ble_db_discovery` library to discover the peer's database.
Call the :c:func:`ble_nus_client_on_db_disc_evt` function on callback events from the Database Discovery library.
This function sends the :c:enumerator:`BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE` event to the application if the Nordic UART Service is discovered in the peer's database.
The application must call the :c:func:`ble_nus_client_handles_assign` function to associate the link with the NUS Client instance.

Events from the service are forwarded through the event handler specified during initialization.
For a full list of events, see the :c:enum:`ble_nus_client_evt_type` enum.

The application can send data to the peer by calling the :c:func:`ble_nus_client_string_send` function.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* :ref:`lib_nrf_sdh` - :kconfig:option:`CONFIG_NRF_SDH`
* :ref:`lib_ble_db_discovery` - :kconfig:option:`CONFIG_BLE_DB_DISCOVERY`

The library is expected to be used with the :ref:`lib_ble_scan` |BMshort| library - :kconfig:option:`CONFIG_BLE_SCAN`.

API documentation
*****************

| Header file: :file:`include/bm/bluetooth/services/ble_nus_client.h`
| Source files: :file:`subsys/bluetooth/services/ble_nus_client/`

:ref:`Nordic UART Service (NUS) API reference <api_ble_nus_client>`
