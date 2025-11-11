.. _lib_ble_db_discovery:

Bluetooth: Database Discovery library
##############################

.. contents::
   :local:
   :depth: 2

This module contains the APIs and types exposed by the Database Discovery module. These APIs and types can be used by the application to perform discovery of a service and its characteristics at the peer server. This module can also be used to discover the desired services in multiple remote devices.

Overview
********

The library allows registration of callbacks that trigger upon discoveering selected services and characteristics in the peer device.

.. _lib_ble_db_discovery_configure:

Configuration
*************

The library is enabled and configured using the Kconfig system:

* :kconfig:option:`CONFIG_BLE_DB_DISCOVERY` - Enables the database discovery library.
* :kconfig:option:`CONFIG_BLE_GATT_DB_MAX_CHARS` - Sets the maximum number of database characteristics.
* :kconfig:option:`CONFIG_BLE_DB_DISCOVERY_MAX_SRV`- Sets the maximum number of services that can be discovered.
* :kconfig:option:`SRV_DISC_START_HANDLE`- Sets the start value used durning discovery.


Initialization
==============

The library is initialized by calling the :c:func:`ble_db_discovery_init` function.
See the :c:struct:`ble_db_discovery_config` struct for configuration details.

Usage
*****

After initialization, use :c:func:`ble_db_discovery_evt_register` to register a callback that triggers when a service is discovered with a specific UUID.
To start discovering from a peer, you must use :c:func:`ble_db_discovery_start` with the connection handle of the peer device.
For example, you can call :c:func:`ble_db_discovery_start` when the ble event :c:enum:`BLE_GAP_EVT_CONNECTED` is triggered and use the connection handle from the event as the second argument in :c:func:`ble_db_discovery_start`.


Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/bm/bluetooth/ble_db_discovery.h`
| Source files: :file:`lib/bluetooth/ble_db_discovery/`

:ref:`Bluetooth LE Database Discovery library API reference <api_ble_db_discovery>`
