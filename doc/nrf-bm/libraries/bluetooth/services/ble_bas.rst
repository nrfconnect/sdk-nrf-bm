.. _lib_ble_service_bas:

Battery Service (BAS)
#####################

.. contents::
   :local:
   :depth: 2

This module implements the Battery Service with the Battery Level characteristic.

Overview
********

During initialization, the module adds the Battery Service and Battery Level characteristic to the Bluetooth LE stack database.
Optionally, it can also add a Report Reference descriptor to the Battery Level characteristic (used when including the Battery Service in the HID service).

If specified, the module will support notification of the Battery Level characteristic through the :c:func:`ble_bas_battery_level_update` function.
If an event handler is supplied by the application, the Battery Service will generate Battery Service events to the application.

Configuration
*************

Set the :kconfig:option:`CONFIG_BLE_BAS` Kconfig option to enable the service.

Initialization
==============

The service instance is declared using the :c:macro:`BLE_BAS_DEF` macro, specifying the name of the instance.
The service is initialized by calling the :c:func:`ble_bas_init` function.
See the :c:struct:`ble_bas_config` struct for configuration details, in addition to the BAS specification.

Usage
*****

Events from the service are forwarded through the event handler specified during initialization.
For a full list of events see the :c:enum:`ble_bas_evt_type` enum.

The application can update the battery level by calling the :c:func:`ble_bas_battery_level_update` function.
To notify the battery level, the application can call the :c:func:`ble_bas_battery_level_notify` function.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/bm/bluetooth/services/ble_bas.h`
| Source files: :file:`subsys/bluetooth/services/ble_bas/`

:ref:`Battery Service API reference <api_ble_bas>`
