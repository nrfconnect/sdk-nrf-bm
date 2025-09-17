.. _lib_ble_service_hids:

Bluetooth: Human Interface Device Service (HIDS)
################################################

.. contents::
   :local:
   :depth: 2

Overview
********

This module implements the ``Human Interface Device Service`` with the corresponding set of characteristics.
During initialization it adds the Human Interface Device Service and a set of characteristics as per the Human Interface Device Service specification and the user requirements to the BLE stack database.

If enabled, notification of Input Report characteristics is performed when the application calls the corresponding ``ble_hids_xx_input_report_send()`` function.

Configuration
*************

Set the :kconfig:option:`CONFIG_BLE_HIDS` Kconfig option to enable the service.

The HID service can be configured by using the following Kconfig options:

* The :kconfig:option:`CONFIG_BLE_HIDS_BOOT_MOUSE` Kconfig option adds the boot mouse input characteristic.
* The :kconfig:option:`CONFIG_BLE_HIDS_BOOT_KEYBOARD` Kconfig option adds the boot keyboard input and output characteristics.
* The :kconfig:option:`CONFIG_BLE_HIDS_PROTOCOL_MODE_REPORT` Kconfig option sets the default HID protocol mode to report mode.
* The :kconfig:option:`CONFIG_BLE_HIDS_PROTOCOL_MODE_BOOT` Kconfig option sets the default HID protocol mode to boot mode.
* The :kconfig:option:`CONFIG_BLE_HIDS_INPUT_REPORT_MAX_NUM` Kconfig option sets the number of input reports.
* The :kconfig:option:`CONFIG_BLE_HIDS_INPUT_REPORT_MAX_LEN` Kconfig option sets the maximum length of the input reports.
* The :kconfig:option:`CONFIG_BLE_HIDS_OUTPUT_REPORT_MAX_NUM` Kconfig option sets the number of output reports.
* The :kconfig:option:`CONFIG_BLE_HIDS_OUTPUT_REPORT_MAX_LEN` Kconfig option sets the maximum length of the output reports.
* The :kconfig:option:`CONFIG_BLE_HIDS_FEATURE_REPORT_MAX_NUM` Kconfig option sets the number of feature reports.
* The :kconfig:option:`CONFIG_BLE_HIDS_FEATURE_REPORT_MAX_LEN` Kconfig option sets the maximum length of the feature reports.
* The :kconfig:option:`CONFIG_BLE_HIDS_MAX_CLIENTS` Kconfig option sets the maximum number of HID clients.

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
The application can get the current output reports by calling the :c:func:`ble_hids_outp_rep_send` function.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/bluetooth/services/ble_hids.h`
| Source files: :file:`subsys/bluetooth/services/ble_hids/`

   .. doxygengroup:: ble_hids
      :members:
