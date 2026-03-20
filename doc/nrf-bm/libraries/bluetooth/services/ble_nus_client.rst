.. _lib_ble_service_nus_client:

Nordic UART Service (NUS) Client
################################

.. contents::
   :local:
   :depth: 2

The Nordic UART Service is a simple GATT-based service with TX and RX characteristics.

Overview
********

The service is used to send and receive ASCII text strings to and from the peer.
Data is received from the peer through notifications, and data is sent directly using the API.

Configuration
*************

Set the :kconfig:option:`CONFIG_BLE_NUS_CLIENT` Kconfig option to enable the service.

Initialization
==============

The service instance is declared using the :c:macro:`BLE_NUS_CLIENT_DEF` macro, specifying the name of the instance.
The service is initialized by calling the :c:func:`ble_nus_client_init` function.
See the :c:struct:`ble_nus_client_config` structure for configuration details.

Usage
*****

Call the :c:func:`ble_nus_client_on_db_disc_evt` function on callback events from the Database Discovery library.
This function sends the :c:enumerator:`BLE_NUS_CLIENT_EVT_DISCOVERY_COMPLETE` event to the application if the Nordic UART Service is discovered in the peer's database.
The application must call the :c:func:`ble_nus_client_handles_assign` function to associate the link with the NUS Client instance.

Events from the service are forwarded through the event handler specified during initialization.
For a full list of events, see the :c:enum:`ble_nus_client_evt_type` enumeration.

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
