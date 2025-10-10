.. _lib_ble_queued_writes:

Bluetooth: Queued Writes
########################

.. contents::
   :local:
   :depth: 2

The Bluetooth Low Energy® queued writes library provides an implementation of Queued Writes.

Overview
********

Writing to a GATT characteristic can be done in a single write command or in a series of commands.
Splitting the write operation into several commands makes it possible to write attribute values that are longer than a single packet.
Such a series of write commands, called a Queued Write operation, consists of multiple prepare write commands followed by one execute write command.

The Queued Writes module provides an implementation of Queued Writes, using the Generic Attribute Profile (GATT) Server interface of the SoftDevice.
Add this module to your GATT server implementation to enable support for Queued Writes for some or all of the characteristics.
A GATT client can then write to these characteristics using a series of commands.

Configuration
*************

The library is enabled and configured using the Kconfig system.
Set the :kconfig:option:`CONFIG_BLE_QWR` Kconfig option to enable the library.

The :kconfig:option:`CONFIG_BLE_QWR_MAX_ATTR` Kconfig option sets the maximum number of attributes that can be handled by the queued writes library.

Initialization
==============

To initialize an instance of the module, call the :c:func:`ble_qwr_init` function.
You must provide an configuration structure that minimum contains an event handler.
For supporting Queued Writes operations, a buffer for storing the received data must be provided as part of the configuration.
If the buffer is not provided, the library will reject all Queued Writes requests.
Next, call the :c:func:`ble_qwr_attr_register` function repeatedly to register each characteristic attribute for which you want to enable Queued Writes.
Attributes are identified by their handles.

Usage
*****

The callback function is used to authorize the write requests.
When the module receives an execute write request, it calls the callback function with an :c:enum:`BLE_QWR_EVT_AUTH_REQUEST` event to determine whether the request should be accepted:

* To authorize the request, the callback function must return :c:macro:`BLE_GATT_STATUS_SUCCESS`.
  The module then writes all received data and notifies the GATT server that the data is ready to use by calling the callback function again, this time with an :c:enum:`BLE_QWR_EVT_EXECUTE_WRITE` event.
* To reject the request (for example, because the received data is not valid or the application is busy), the callback function must return a BLE GATT status code other than :c:macro:`BLE_GATT_STATUS_SUCCESS`.
  The module then deletes the received data.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/ble_qwr.h`
| Source files: :file:`lib/ble_qwr/`

:ref:`Bluetooth LE Queued Writes library API reference <api_queued_writes>`
