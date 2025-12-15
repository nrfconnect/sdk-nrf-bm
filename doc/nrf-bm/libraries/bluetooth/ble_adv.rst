.. _lib_ble_adv:

Bluetooth: Advertising library
##############################

.. contents::
   :local:
   :depth: 2

The advertising library handles the Bluetooth LE advertising of your application.
It supports a variety of common advertising scenarios.
For non-connectable advertising, the library serves as a reference together with the :ref:`api_ble_adv_data_encoder` API.

Overview
********

The library covers the most common advertising behaviors and can be adjusted by enabling or disabling specific :ref:`advertising modes <lib_ble_adv_modes>`.

The library event handler allows for the extension of advertising functionalities, such as adding indicators for active advertising modes.

.. _lib_ble_adv_modes:

Advertising modes
=================

The library supports the following connectable advertising modes:

* **Directed High Duty** - After disconnecting, the application immediately attempts to reconnect to the most recently connected peer.
  This advertising mode is useful for staying connected to one peer and seamlessly recovering from accidental disconnects.
  This mode is only allowed for limited duration since it has a high chance of blocking other wireless traffic.
  Directed High Duty will only work if extended advertising is disabled.
* **Directed** - Direct advertising to the last connected peer.
  This mode has a lower duty cycle compared to Directed High Duty.
* **Fast** - Rapidly advertise to surrounding devices.
* **Slow** - Advertise to surrounding devices with a longer advertising interval than in Fast advertising mode.
  This advertising mode conserves power and generates less traffic for other wireless devices that might be present.
  Finding a device and connecting to it might take more time in Slow advertising mode.
* **Idle** (Advertising off) - The application stops advertising.

The advertising modes are enabled by using Kconfig options.
See the :ref:`lib_ble_adv_configure` section below.

When advertising, the library transitions through the enabled advertising modes until a connection is made or advertising times out.
The flow of advertising is: Direct High Duty -> Direct -> Fast -> Slow -> Idle.

If you start advertising in Direct mode, the library first attempts Direct advertising.
If no information about a previous connection is available or the previous peer is not available, the library starts Fast advertising.
If no peer connects before the Fast advertising times out, the application moves on to Slow advertising with a longer advertising interval.
If no peer connects before the configured timeout, advertising stops.

Disabled advertising modes are skipped (Directed High Duty is always skipped if extended advertising is enabled).

You can configure the timeout thresholds and advertising intervals for Direct, Fast, and Slow advertising.
Direct High Duty is a one-shot burst and its timeout threshold and advertising interval cannot be configured.

Allow list
==========

Allow list advertising affects the filtering parameters for Fast and Slow advertising modes.
The allow list stores information about all devices that have been connected before.
If you enable the use of the allow list, the application specifically advertises to the devices that are on the allow list.

You can enable or disable allow list advertising by using Kconfig options.
After initialization, you can temporarily disable allow list advertising for one connection using the :c:func:`ble_adv_restart_without_allow_list` function.
After the device disconnects, the allow list will take effect again.

To permanently disable allow list advertising, you must disable it in Kconfig.

.. _lib_ble_adv_configure:

Configuration
*************

The library is enabled and configured using the Kconfig system:

* General settings:

  * :kconfig:option:`CONFIG_BLE_ADV` - Enables the advertising library.
  * :kconfig:option:`CONFIG_BLE_ADV_DATA` - Enables the advertising and scan response data encoder.
    You can enable this without :kconfig:option:`CONFIG_BLE_ADV` if the full library is not required.
  * :kconfig:option:`CONFIG_BLE_ADV_NAME` - Sets the advertising name of the device.
  * :kconfig:option:`CONFIG_BLE_ADV_RESTART_ON_DISCONNECT`- Starts advertising upon disconnect.

* Directed advertising settings:

  * :kconfig:option:`CONFIG_BLE_ADV_DIRECTED_ADVERTISING` - Enables Directed advertising.
  * :kconfig:option:`CONFIG_BLE_ADV_DIRECTED_ADVERTISING_HIGH_DUTY` - Enables Directed High Duty advertising.
  * :kconfig:option:`CONFIG_BLE_ADV_DIRECTED_ADVERTISING_TIMEOUT` - Sets the Directed advertising timeout in units of 10 ms.
  * :kconfig:option:`CONFIG_BLE_ADV_DIRECTED_ADVERTISING_INTERVAL` - Sets the Directed advertising interval in units of 0.625 ms.

* Fast advertising settings:

  * :kconfig:option:`CONFIG_BLE_ADV_FAST_ADVERTISING` - Enables Fast advertising.
  * :kconfig:option:`CONFIG_BLE_ADV_FAST_ADVERTISING_TIMEOUT` - Sets the Fast advertising timeout in units of 10 ms.
  * :kconfig:option:`CONFIG_BLE_ADV_FAST_ADVERTISING_INTERVAL` - Sets the Fast advertising interval in units of 0.625 ms.

* Slow advertising settings:

  * :kconfig:option:`CONFIG_BLE_ADV_SLOW_ADVERTISING` - Enables Slow advertising.
  * :kconfig:option:`CONFIG_BLE_ADV_SLOW_ADVERTISING_TIMEOUT` - Sets the Slow advertising timeout in units of 10 ms.
  * :kconfig:option:`CONFIG_BLE_ADV_SLOW_ADVERTISING_INTERVAL` - Sets the Slow advertising interval in units of 0.625 ms.

* Allow list and extended advertising:

  * :kconfig:option:`CONFIG_BLE_ADV_USE_ALLOW_LIST` - Enables the use of allow list.
  * :kconfig:option:`CONFIG_BLE_ADV_EXTENDED_ADVERTISING` - Enables extended advertising.

* PHY-related settings:

  * :kconfig:option:`CONFIG_BLE_ADV_PRIMARY_PHY_AUTO` - Sets the primary PHY to auto.
  * :kconfig:option:`CONFIG_BLE_ADV_PRIMARY_PHY_1MBPS` - Sets the primary PHY to 1 Mbit/s.
  * :kconfig:option:`CONFIG_BLE_ADV_PRIMARY_PHY_2MBPS` - Sets the primary PHY to 2 Mbit/s.
  * :kconfig:option:`CONFIG_BLE_ADV_PRIMARY_PHY_CODED` - Sets the primary PHY to coded PHY.
  * :kconfig:option:`CONFIG_BLE_ADV_SECONDARY_PHY_AUTO` - Sets the secondary PHY to auto.
  * :kconfig:option:`CONFIG_BLE_ADV_SECONDARY_PHY_1MBPS` - Sets the secondary PHY to 1 Mbit/s.
  * :kconfig:option:`CONFIG_BLE_ADV_SECONDARY_PHY_2MBPS` - Sets the secondary PHY to 2 Mbit/s.
  * :kconfig:option:`CONFIG_BLE_ADV_SECONDARY_PHY_CODED` - Sets the secondary PHY to coded PHY.

Initialization
==============

The library is initialized by calling the :c:func:`ble_adv_init` function.
See the :c:struct:`ble_adv_config` struct for configuration details.

Usage
*****

Before compiling your application, enable the intended :ref:`advertising modes <lib_ble_adv_modes>` through the Kconfig system.

Make sure to provide an event handler that is called when advertising transitions to a new mode.
You can then use this event handler to add functionality, for example to indicate mode transitions to the user by flashing an LED when advertising starts, or to power down the application when no peer is found.

If enabled, the event handler must handle requests to update the allow list (:c:enum:`BLE_ADV_EVT_ALLOW_LIST_REQUEST`) and peer address (:c:enum:`BLE_ADV_EVT_PEER_ADDR_REQUEST`).
If the peer address request event is ignored, the Directed advertising mode cannot be used.
Likewise, if the allow list request event is ignored, the Fast and Slow advertising modes will not use the allow list.

When replying to the :c:enum:`BLE_ADV_EVT_ALLOW_LIST_REQUEST` event, the application must provide the allow list in the following way:

* If the application uses :ref:`lib_peer_manager`: Retrieve the allow list by calling the :c:func:`pm_allow_list_get` function.
  Make sure that the :c:func:`pm_allow_list_set` function was previously called.
  Then, call the :c:func:`ble_adv_allow_list_reply` function with the output of the :c:func:`pm_allow_list_get` function.
* If the application does not use :ref:`lib_peer_manager`: call the :c:func:`sd_ble_gap_whitelist_set` function. Then, call the :c:func:`ble_adv_allow_list_reply` function.
  After initialization, call the :c:func:`ble_adv_start` function to start advertising in the intended mode.

The application must reply to the :c:enum:`BLE_ADV_EVT_PEER_ADDR_REQUEST` event by calling the :c:func:`ble_adv_peer_addr_reply` function.

.. note::

   When setting connection-specific configurations using the :c:func:`sd_ble_cfg_set` function, you must create a tag for each configuration.
   This tag must be provided when calling the :c:func:`sd_ble_gap_adv_start` function and the :c:func:`sd_ble_gap_connect` function.

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

:ref:`Advertising and Scan Response Data Encoder API reference <api_ble_adv_data_encoder>`
