.. _lib_ble_radio_notification:

Bluetooth: Radio Notification
#############################

.. contents::
   :local:
   :depth: 2

The Bluetooth Low Energy® Radio Notification library provides an interface for subscribing to the Radio Notification signal, allowing the application to receive notifications before and after radio events occur.

Overview
********

This library enables applications to register a handler for receiving radio notifications.
You can also use it to configure the interval in microseconds between the active Radio Notification signal and the radio event.

Configuration
*************

The library is enabled and configured using the Kconfig system.
Set the :kconfig:option:`CONFIG_BLE_RADIO_NOTIFICATION` Kconfig option to enable the library.

The library uses the ``SWI02`` IRQ.
Use the :kconfig:option:`CONFIG_BLE_RADIO_NOTIFICATION_IRQ_PRIO` Kconfig option to set the IRQ priority.

The library can be configured to receive notifications before the radio is active, after the radio is active, or both before and after.
You can select this using the :kconfig:option:`CONFIG_BLE_RADIO_NOTIFICATION_ON_ACTIVE`, the :kconfig:option:`CONFIG_BLE_RADIO_NOTIFICATION_ON_INACTIVE`, or the :kconfig:option:`CONFIG_BLE_RADIO_NOTIFICATION_ON_BOTH` Kconfig options, respectively.

Initialization
==============

Initialize the library by calling the :c:func:`ble_radio_notification_init` function.

Usage
*****

When initializing the library with the :c:func:`ble_radio_notification_init` function, make sure to specify the event handler to receive the Radio Notification signal.
You can specify the interval in microseconds between the Radio Notification signal and the start of the radio event.
The signal raised at the start of the radio event has ``active_state`` set to ``true``.
A new Radio Notification signal will be raised when the radio event completes with ``active_state`` set to ``false``.

Sample
******

The usage of this library is demonstrated in the :ref:`ble_radio_ntf_sample` sample.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/bm/bluetooth/ble_radio_notification.h`
| Source files: :file:`lib/bluetooth/ble_radio_notification/`

:ref:`Bluetooth LE Radio Notification library API reference <api_ble_radio_notif>`
