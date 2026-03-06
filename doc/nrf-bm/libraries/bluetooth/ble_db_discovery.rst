.. _lib_ble_db_discovery:

Bluetooth: Database Discovery library
#####################################

.. contents::
   :local:
   :depth: 2

This library handles the discovery of a connected peer's database.
You can use it to discover one or more services and their characteristics at a peer server.

Overview
********

Before starting a discovery, you need to register the UUID of one or more services you want to discover.
In addition, a GATT queue instance and a callback for handling discovery events must be set during initialization.

On discovery start, the library does automatically the necessary SoftDevice calls and handles the necessary events from the SoftDevice to discover the registered services, their characteristics and possible descriptors.
To automatically discover services, characteristics, and descriptors, the library calls the SoftDevice discovery functions in response to and based on the content of discovery response events received from the SoftDevice.
Discovery results for each service will be queued up.
Once discovery has been attempted for all registered services, the queued results are dispatched to the callback one at a time.

An event signaling that the instance is available and a new discovery can be started is dispatched when all discovery result events have been passed to the callback.

Configuration
*************

To enable and configure the library, use the following Kconfig options:

* :kconfig:option:`CONFIG_BLE_DB_DISCOVERY` - Enables the database discovery library.
* :kconfig:option:`CONFIG_BLE_DB_DISCOVERY_SRV_DISC_START_HANDLE`- Sets the start value used during discovery.
* :kconfig:option:`CONFIG_BLE_DB_DISCOVERY_MAX_SRV`- Sets the maximum number of service UUIDs that can be registered and subsequently discovered at a time and with one database discovery instance.
* :kconfig:option:`CONFIG_BLE_GATT_DB_MAX_CHARS` - Sets the maximum number of characteristics for each service that can be discovered.

.. note::

   * The size of the :c:struct:`ble_db_discovery` structure is affected by the choice of both :kconfig:option:`CONFIG_BLE_DB_DISCOVERY_MAX_SRV` and :kconfig:option:`CONFIG_BLE_GATT_DB_MAX_CHARS`.
   * The size of the :c:struct:`ble_gatt_db_srv` structure is affected by the choice of :kconfig:option:`CONFIG_BLE_GATT_DB_MAX_CHARS`.

Initialization
==============

The library is initialized by calling the :c:func:`ble_db_discovery_init` function.
See the :c:struct:`ble_db_discovery_config` structure for configuration details.

Usage
*****

After initialization, use the :c:func:`ble_db_discovery_service_register` function to register one or more service UUIDs for discovery.
A registered service UUID will stay registered even after a discovery completes.
This makes it possible to run discovery of the same services on different peers without having to register the services again.
You can use the :c:func:`ble_db_discovery_init` function to clear the registered service UUIDs.

To start discovering a peer's database, you must use the :c:func:`ble_db_discovery_start` function with the connection handle of the peer device.
For example, you can call the :c:func:`ble_db_discovery_start` function when the ble event :c:enum:`BLE_GAP_EVT_CONNECTED` event is triggered and use the connection handle from the event as the second argument in the :c:func:`ble_db_discovery_start` function.

When the discovery procedure completes, an event of either type :c:enumerator:`BLE_DB_DISCOVERY_COMPLETE` or :c:enumerator:`BLE_DB_DISCOVERY_SRV_NOT_FOUND` is raised for each registered service UUID.
An event of type :c:enumerator:`BLE_DB_DISCOVERY_COMPLETE` indicates that a service was found in the peer's database and the discovery result can be found by dereferencing the :c:member:`ble_db_discovery_evt.discovered_db` parameter.
The service UUID can be found in the discovery result.
An event of type :c:enumerator:`BLE_DB_DISCOVERY_SRV_NOT_FOUND` indicates that a service was not found in the peer's database.
The service UUID can be found from the :c:member:`ble_db_discovery_evt.srv_uuid` parameter.

When all discovery result events have been passed to the event handler, an additional event of type :c:enumerator:`BLE_DB_DISCOVERY_AVAILABLE` is passed to indicate that a new discovery can now be started using the :c:func:`ble_db_discovery_start` function.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`
* GATT queue - :kconfig:option:`CONFIG_BLE_GATT_QUEUE`

API documentation
*****************

| Header file: :file:`include/bm/bluetooth/ble_db_discovery.h`
| Source files: :file:`lib/bluetooth/ble_db_discovery/`

:ref:`Bluetooth LE Database Discovery library API reference <api_ble_db_discovery>`
