.. _lib_ble_adv:

Bluetooth: Advertising library
##############################

.. contents::
   :local:
   :depth: 2

The Advertising library handles the BLE advertising of your application. You can use it for many typical connectable advertising scenarios.
If your application requires non-connectable advertising the library can be taken as a reference together with the :ref:`api_ble_adv_data_encoder` API.

Overview
********

The library covers the most common advertising behaviors and can be adapted by enabling or disabling specific advertising modes.
The library event handler makes it possible to extend the provided advertising functionality and, for example, add indications for specific advertising modes.

Advertising modes
=================

The library supports the following connectable advertising modes:

* ``Directed High Duty`` - After disconnecting, the application immediately attempts to reconnect to the peer that was connected most recently.
  This advertising mode is very useful to stay connected to one peer and seamlessly recover from accidental disconnects.
  This mode is only allowed very briefly since it has a high chance of blocking other wireless traffic.
  Directed High Duty will only work if extended advertising is disabled.
* ``Directed`` - Direct advertising to its last peer.
  This has a lower duty cycle compared to ``Directed High Duty``.
* ``Fast`` - Rapidly advertise to surrounding devices.
* ``Slow`` - Advertise to surrounding devices with a longer advertising interval than in fast advertising mode.
  This advertising mode conserves power and generates less traffic for other wireless devices that might be present.
  Finding a device and connecting to it might take more time in slow advertising mode.
* ``Idle`` (Advertising off) - The application stops advertising.

The advertising modes are enabled by Kconfig options.
See the :ref:`lib_ble_adv_configure` section below.

When advertising, the library transitions through the enabled advertising modes until a connection is made or advertising times out.
The flow of advertising is ``Direct High Duty`` -> ``Direct`` -> ``Fast`` -> ``Slow`` -> ``Idle``.
If you start advertising in direct mode, the library first attempts ``Direct`` advertising.
If no information about a previous connection is available or the previous peer is not available, the library starts ``Fast`` advertising.
If no peer connects before the ``Fast`` advertising times out, the application moves on to ``Slow`` advertising with a longer advertising interval.
If no peer connects before the configured time-out, advertising stops.

Disabled advertising modes are skipped. (Directed High Duty is always skipped if extended advertising is enabled).
You can configure the timeout thresholds and advertising intervals for ``Direct``, ``Fast``, and ``Slow`` advertising.
``Direct High Duty`` is a one-shot burst and its time-out thresholdsand advertising interval cannot be configured.

Whitelist
=========
Whitelist advertising affects the filtering parameters for ``Fast`` and ``Slow`` advertising modes.
The whitelist stores all devices that have been connected before.
If you enable use of the whitelist, the application specifically advertises to the devices that are on the whitelist.

You can enable or disable whitelist advertising in Kconfig.
After initialization, you can temporarily disable whitelist advertising for one connection using the :c:func:`ble_adv_restart_without_whitelist` function.
After the device disconnects the whitelist will take effect again.
To permanently disable whitelist advertising, you must disable it in Kconfig.

.. _lib_ble_adv_configure:

Configuration
*************

The library is enabled and configured using the Kconfig system.
Set the :kconfig:option:`CONFIG_BLE_ADV` Kconfig option to enable the library.

The library has the following Kconfig options:

* :kconfig:option:`CONFIG_BLE_ADV_NAME` - Sets the advertising name.
* :kconfig:option:`CONFIG_BLE_ADV_RESTART_ON_DISCONNECT`- Start advertising upon disconnect.
* :kconfig:option:`CONFIG_BLE_ADV_USE_WHITELIST` - Use whitelist.
* :kconfig:option:`CONFIG_BLE_ADV_EXTENDED_ADVERTISING` - Enables extended advertising.
* :kconfig:option:`CONFIG_BLE_ADV_DIRECTED_ADVERTISING` - Enables directed advertising.
* :kconfig:option:`CONFIG_BLE_ADV_DIRECTED_ADVERTISING_HIGH_DUTY` - Enables directed advertising high duty.
* :kconfig:option:`CONFIG_BLE_ADV_DIRECTED_ADVERTISING_TIMEOUT` - Sets the directed advertising timeout.
* :kconfig:option:`CONFIG_BLE_ADV_DIRECTED_ADVERTISING_INTERVAL` - Sets the directed advertising interval.
* :kconfig:option:`CONFIG_BLE_ADV_FAST_ADVERTISING` - Enables fast advertising.
* :kconfig:option:`CONFIG_BLE_ADV_FAST_ADVERTISING_TIMEOUT` - Sets the fast advertising timeout in 10 ms units.
* :kconfig:option:`CONFIG_BLE_ADV_FAST_ADVERTISING_INTERVAL` - Sets the fast advertising interval in 0.625 ms units.
* :kconfig:option:`CONFIG_BLE_ADV_SLOW_ADVERTISING` - Enables slow advertising.
* :kconfig:option:`CONFIG_BLE_ADV_SLOW_ADVERTISING_TIMEOUT` - Sets the slow advertising timeout.
* :kconfig:option:`CONFIG_BLE_ADV_SLOW_ADVERTISING_INTERVAL` - Sets the slow advertising interval.
* :kconfig:option:`CONFIG_BLE_ADV_PRIMARY_PHY_AUTO` - Sets the primary PHY to auto.
* :kconfig:option:`CONFIG_BLE_ADV_PRIMARY_PHY_1MBPS` - Sets the primary PHY to 1 Megabit per second.
* :kconfig:option:`CONFIG_BLE_ADV_PRIMARY_PHY_2MBPS` - Sets the primary PHY to 2 Megabit per second.
* :kconfig:option:`CONFIG_BLE_ADV_PRIMARY_PHY_CODED` - Sets the primary PHY to coded PHY.
* :kconfig:option:`CONFIG_BLE_ADV_SECONDARY_PHY_AUTO` - Sets the secondary PHY to auto.
* :kconfig:option:`CONFIG_BLE_ADV_SECONDARY_PHY_1MBPS` - Sets the secondary PHY to 1 Megabit per second.
* :kconfig:option:`CONFIG_BLE_ADV_SECONDARY_PHY_2MBPS` - Sets the secondary PHY to 2 Megabit per second.
* :kconfig:option:`CONFIG_BLE_ADV_SECONDARY_PHY_CODED` - Sets the secondary PHY to coded PHY.

Initialization
==============

The library is initialized by calling the :c:func:`ble_adv_init` function.
See the :c:struct:`ble_adv_config` struct for configuration details.

Usage
*****

Enable the advertising modes that you want to use in Kconfig before compiling the application.
You should also provide an event handler that is called when advertising transitions to a new mode.
Use the event handler to add functionality, for example to indicate such transitions to the user by flashing an LED when advertising starts, or to power down the application when no peer is found.

If enabled, the event handler must handle requests to update the whitelist (:c:enum:`BLE_ADV_EVT_WHITELIST_REQUEST`) and peer address (:c:enum:`BLE_ADV_EVT_PEER_ADDR_REQUEST`).
If the peer address request event is ignored, the directed advertising mode cannot be used.
Likewise, if the whitelist request event is ignored, the fast and slow advertising modes will not use the whitelist.
When replying to the :c:enum:`BLE_ADV_EVT_WHITELIST_REQUEST` event, the application must provide the whitelist in the following way:

* If the application uses :ref:`lib_peer_manager`: Retrieve the whitelist by calling the :c:func:`pm_whitelist_get` function.
  The :c:func:`pm_whitelist_set` function must have been called before.
  Then, call the :c:func:`ble_adv_whitelist_reply` function with the output of the :c:func:`pm_whitelist_get` function.
* If the application does not use :ref:`lib_peer_manager`: call the :c:func:`sd_ble_gap_whitelist_set` function. Then, call the :c:func:`ble_advertising_whitelist_reply` function.
  After initialization, call the :c:func:`ble_advertising_start` function to start advertising in the desired mode.

The application must reply to the :c:enum:`BLE_ADV_EVT_PEER_ADDR_REQUEST` event by calling the :c:func:`ble_adv_peer_addr_reply` function.

.. note::

   When setting connection-specific configurations using the :c:func:`sd_ble_cfg_set` function, you must create a tag for each configuration.
   This tag must be supplied when calling the :c:func:`sd_ble_gap_adv_start` function and the :c:function:`sd_ble_gap_connect` function.
   If your application uses the Advertising Module, you must call the :c:func:`ble_advertising_conn_cfg_tag_set` function before starting advertising.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/bm/bluetooth/ble_adv.h`
| Source files: :file:`lib/bluetooth/ble_adv/`

:ref:`Bluetooth LE Advertising library API reference <api_ble_adv>`

| Header file: :file:`include/bm/bluetooth/ble_adv_data.h`
| Source files: :file:`lib/bluetooth/ble_adv/`

:ref:`Bluetooth LE Advertising library Advertising and Scan Response Data Encoder API reference <api_ble_adv_data_encoder>`
