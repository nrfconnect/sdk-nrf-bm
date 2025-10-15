.. _lib_ble_service_mcumgr:

MCU manager Service (MCUmgr)
############################

.. contents::
   :local:
   :depth: 2

The MCU manager service allows for remote management of the device over Bluetooth.

It is based on the Simple Management Protocol (SMP) provided by `MCUmgr`_, an open source project that provides a management subsystem that is portable across multiple real-time operating systems.

Configuration
*************

Set the :kconfig:option:`CONFIG_BLE_MCUMGR` Kconfig option to enable the service.

Initialization
==============

The service is initialized by calling the :c:func:`ble_mcumgr_init` function.

Usage
*****

The MCUmgr Bluetooth service UUID type can be retrieved by calling the :c:func:`ble_mcumgr_service_uuid_type` function.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/bm/bluetooth/services/ble_mcumgr.h`
| Source files: :file:`subsys/bluetooth/services/ble_mcumgr/`

:ref:`MCU manager Service (MCUmgr) API reference <api_mcu_manager_service>`
