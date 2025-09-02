.. _lib_ble_radio_notification:

Bluetooth: Radio Notification
#############################

.. contents::
   :local:
   :depth: 2

The Bluetooth Low Energy® Radio Notification library allows the user to subscribe to the Radio Notification signal to receive notification prior to and after radio events.

Overview
********

The library allows the user to register a handler to receive the radio notifications and configure the distance in microseconds between the active Radio Notification signal and the radio event.

Configuration
*************

The library is enabled and configured entirely using the Kconfig system.
Set the :kconfig:option:`CONFIG_BLE_RADIO_NOTIFICATION` Kconfig option to enable the library.

The library uses the ``SWI02`` IRQ.
Use the :kconfig:option:`CONFIG_BLE_RADIO_NOTIFICATION_IRQ_PRIO` Kconfig option to set the IRQ priority.
The library can be configured to receive notifications before the radio is active, after the radio is active or both before and after.
This can be chosen by setting the :kconfig:option:`CONFIG_BLE_RADIO_NOTIFICATION_ON_ACTIVE`, :kconfig:option:`CONFIG_BLE_RADIO_NOTIFICATION_ON_INACTIVE` or :kconfig:option:`CONFIG_BLE_RADIO_NOTIFICATION_ON_BOTH` Kconfig option, respectively.

Initialization
==============

The library is initialized by calling the :c:func:`ble_radio_notification_init` function.

Usage
*****

Initialize the library by calling the :c:func:`ble_radio_notification_init` function, specifying the event handler to receive the Radio Notification signal.
You can specify the distance in microseconds between the Radio Notification signal and the start of the radio event.
This event has ``active_state`` set to ``true``.
A new Radio Notification signal will be raised when the radio event completes with ``active_state`` set to ``false``.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/ble_radio_notification.h`
| Source files: :file:`lib/ble_radio_notification/`

   .. doxygengroup:: ble_radio_notification
