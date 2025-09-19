.. _lib_ble_service_lbs:

Bluetooth: LED Button Service (LBS)
###################################

.. contents::
   :local:
   :depth: 2

Overview
********

This module implements a custom LED Button Service with an LED and Button Characteristics.
During initialization, the module adds the LED Button Service and Characteristics to the BLE stack database.

The application must supply an event handler for receiving LED Button Service events.
Using this handler, the service notifies the application when the LED value changes.

The service also provides a function for letting the application notify the state of the Button Characteristic to connected peers.

Configuration
*************

Set the :kconfig:option:`CONFIG_BLE_LBS` Kconfig option to enable the service.

Initialization
==============

The service instance is declared using the :c:macro:`BLE_LBS_DEF` macro, specifying the name of the instance.
The service is initialized by calling the :c:func:`ble_lbs_init` function.
See the :c:struct:`ble_lbs_config` struct for configuration details, in addition to the LBS specification.

Usage
*****

Events from the service are forwarded through the event handler specified during initialization.
For a full list of events see the :c:enum:`ble_lbs_evt_type` enum.

The application can send a button change notification by calling the :c:func:`ble_lbs_on_button_change` function.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/bluetooth/services/ble_lbs.h`
| Source files: :file:`subsys/bluetooth/services/ble_lbs/`

   .. doxygengroup:: ble_lbs
      :members:
