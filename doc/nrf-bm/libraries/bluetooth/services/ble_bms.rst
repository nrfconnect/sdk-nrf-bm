.. _lib_ble_service_bms:

Bond Management Service (BMS)
#############################

.. contents::
   :local:
   :depth: 2

Overview
********

This library implements the Bond Management Service with the corresponding set of characteristics defined in the `Bond Management Service Specification`_.

You can configure the service to support your desired feature set of bond management operations.
All the BMS features in the "LE transport only" mode are supported:

 * Delete the bond of the requesting device.
 * Delete all bonds on the server.
 * Delete all bonds on the server except the one of the requesting device.

You can enable each feature when initializing the library.

Authorization
=============

You can require authorization to access each BMS feature.

When required, the client's request to execute a bond management operation must contain the authorization code.
The server compares the code with its local version and accepts the request only if the codes match.

If you use at least one BMS feature that requires authorization, you need to provide a callback with comparison logic for the authorization codes.
You can set this callback when initializing the library.

Deleting the bonds
==================

The server deletes bonding information on client's request right away when there is no active Bluetooth® Low Energy connection associated with a bond.
Otherwise, the server removes the bond for a given peer when it disconnects.

Configuration
*************

Set the :kconfig:option:`CONFIG_BLE_BMS` Kconfig option to enable the service.

Initialization
==============

The service instance is declared using the :c:macro:`BLE_BMS_DEF` macro, specifying the name of the instance.
The service is initialized by calling the :c:func:`ble_bms_init` function.
See the :c:struct:`ble_bms_config` struct for configuration details, in addition to the BMS specification.

Usage
*****

Events from the service are forwarded through the event handler specified during initialization.
For a full list of events see the :c:enum:`ble_bms_evt_type` enum.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/bluetooth/services/ble_bms.h`
| Source files: :file:`subsys/bluetooth/services/ble_bms/`

:ref:`Bond Management Service API reference <api_ble_bms>`
