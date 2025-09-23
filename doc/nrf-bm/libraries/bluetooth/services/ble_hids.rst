.. _lib_ble_service_hids:

Human Interface Device Service (HIDS)
#####################################

.. contents::
   :local:
   :depth: 2

Overview
********

This module implements the Human Interface Device Service with the corresponding set of characteristics.
During initialization, it adds the Human Interface Device Service and a set of characteristics as per the Human Interface Device Service specification and the user requirements to the Bluetooth LE stack database.

If enabled, notification of Input Report characteristics is performed when the application calls one of the corresponding functions:

* :c:func:`ble_hids_inp_rep_send`
* :c:func:`ble_hids_boot_kb_inp_rep_send`
* :c:func:`ble_hids_boot_mouse_inp_rep_send`

Configuration
*************

Set the :kconfig:option:`CONFIG_BLE_HIDS` Kconfig option to enable the service.

The HID service can be configured by using the following Kconfig options:

* :kconfig:option:`CONFIG_BLE_HIDS_BOOT_MOUSE` - Adds the boot mouse input characteristic.
* :kconfig:option:`CONFIG_BLE_HIDS_BOOT_KEYBOARD` - Adds the boot keyboard input and output characteristics.
* :kconfig:option:`CONFIG_BLE_HIDS_PROTOCOL_MODE_REPORT` - Sets the default HID protocol mode to report mode.
* :kconfig:option:`CONFIG_BLE_HIDS_PROTOCOL_MODE_BOOT` - Sets the default HID protocol mode to boot mode.
* :kconfig:option:`CONFIG_BLE_HIDS_INPUT_REPORT_MAX_NUM` - Sets the number of input reports.
* :kconfig:option:`CONFIG_BLE_HIDS_INPUT_REPORT_MAX_LEN` - Sets the maximum length of the input reports.
* :kconfig:option:`CONFIG_BLE_HIDS_OUTPUT_REPORT_MAX_NUM` - Sets the number of output reports.
* :kconfig:option:`CONFIG_BLE_HIDS_OUTPUT_REPORT_MAX_LEN` - Sets the maximum length of the output reports.
* :kconfig:option:`CONFIG_BLE_HIDS_FEATURE_REPORT_MAX_NUM` - Sets the number of feature reports.
* :kconfig:option:`CONFIG_BLE_HIDS_FEATURE_REPORT_MAX_LEN` - Sets the maximum length of the feature reports.
* :kconfig:option:`CONFIG_BLE_HIDS_MAX_CLIENTS` - Sets the maximum number of HID clients.

Initialization
==============

The service instance is declared using the :c:macro:`BLE_HIDS_DEF` macro, specifying the name of the instance.
The service is initialized by calling the :c:func:`ble_hids_init` function.
See the :c:struct:`ble_hids_config` struct for configuration details, in addition to the HID specification.

Usage
*****

Events from the service are forwarded through the event handler specified during initialization.
For a full list of events see the :c:enum:`ble_hids_evt_type` enum.

The application can send input reports by calling the :c:func:`ble_hids_inp_rep_send` function.
Separate functions exist for sending boot keyboard and boot mouse input reports.
See the :c:func:`ble_hids_boot_kb_inp_rep_send` and :c:func:`ble_hids_boot_mouse_inp_rep_send` functions, respectively.
The application can get the current output reports by calling the :c:func:`ble_hids_outp_rep_get` function.

Usage of this service is demonstrated in the following samples:

* :ref:`ble_hids_keyboard_sample`
* :ref:`ble_hids_mouse_sample`

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/bluetooth/services/ble_hids.h`
| Source files: :file:`subsys/bluetooth/services/ble_hids/`

:ref:`Human Interface Device Service (HIDS) API reference <api_human_interface_device_service>`
