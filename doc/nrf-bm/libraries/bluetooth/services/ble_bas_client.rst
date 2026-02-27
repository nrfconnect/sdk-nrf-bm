.. _lib_ble_service_bas_client:

Battery Service (BAS) Client
############################

.. contents::
   :local:
   :depth: 2

This module implements the Battery Service (BAS) Client that retrieves information about the battery level from a peer device that implements the Battery Service (BAS) server.

Overview
********

The BAS Client retrieves battery level information from a BAS server.

Configuration
*************

Set the :kconfig:option:`CONFIG_BLE_BAS_CLIENT` Kconfig option to enable the service.

Initialization
==============

The service instance is declared using the :c:macro:`BLE_BAS_CLIENT_DEF` macro, specifying the name of the instance.
The service is initialized by calling the :c:func:`ble_bas_client_init` function.
See the :c:struct:`ble_bas_client_config` structure for configuration details, in addition to the BAS specification.

Usage
*****

Applications can use the :ref:`lib_ble_scan` library for detecting advertising devices that support the Battery Service.

Upon connection, the application can use the :ref:`lib_ble_db_discovery` library to discover the peer's database.
Call the :c:func:`ble_bas_on_db_disc_evt` function on callback events from the Database Discovery library.
This function sends the :c:enumerator:`BLE_BAS_CLIENT_EVT_DISCOVERY_COMPLETE` event to the application if the Battery Service is discovered in the peer's database.
The application must call the :c:func:`ble_bas_client_handles_assign` function to associate the link with the BAS Client instance.
Upon Battery Service discovery, the application can call the :c:func:`ble_bas_client_bl_notif_enable` function to request the peer to start sending Battery Level Measurement notifications.

Events from the service are forwarded through the event handler specified during initialization.
For a full list of events see the :c:enum:`ble_bas_client_evt_type` enumeration.

The application can read the battery level of the connected device by calling the :c:func:`ble_bas_client_bl_read` function.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* :ref:`lib_nrf_sdh` - :kconfig:option:`CONFIG_NRF_SDH`
* :ref:`lib_ble_db_discovery` - :kconfig:option:`CONFIG_BLE_DB_DISCOVERY`

The library is expected to be used with the :ref:`lib_ble_scan` |BMshort| library - :kconfig:option:`CONFIG_BLE_SCAN`.

API documentation
*****************

| Header file: :file:`include/bm/bluetooth/services/ble_bas_client.h`
| Source files: :file:`subsys/bluetooth/services/ble_bas_client/`

:ref:`Battery Service Client API reference <api_ble_bas_client>`
