.. _lib_ble_service_dis:

Bluetooth: Device Information Service (DIS)
###########################################

.. contents::
   :local:
   :depth: 2

This module implements the Device Information Service.

Overview
********

During initialization, the module adds the Device Information Service to the Bluetooth LE stack database.
It then encodes the supplied information, and adds the corresponding characteristics.

.. caution::

   To maintain compliance with Nordic Semiconductor Bluetooth profile qualification listings, this section of source code must not be modified.

Configuration
*************

Set the :kconfig:option:`CONFIG_BLE_DIS` Kconfig option to enable the service.

The DIS service can be configured by using the following Kconfig options:

* The :kconfig:option:`BLE_DIS_MANUFACTURER_NAME` sets the manufacturer name.
* The :kconfig:option:`BLE_DIS_MODEL_NUMBER` sets the model number.
* The :kconfig:option:`BLE_DIS_SERIAL_NUMBER` sets the serial number.
* The :kconfig:option:`BLE_DIS_HW_REVISION` sets the hardware revision.
* The :kconfig:option:`BLE_DIS_FW_REVISION` sets the firmware revision.
* The :kconfig:option:`BLE_DIS_SW_REVISION` sets the software revision.
* The :kconfig:option:`BLE_DIS_SYSTEM_ID` includes the system ID characteristic in the Device Information Service.
* The :kconfig:option:`BLE_DIS_SYSTEM_ID_OUI` sets the organization unique ID.
* The :kconfig:option:`BLE_DIS_SYSTEM_ID_MID` sets the manufacturer unique ID.
* The :kconfig:option:`BLE_DIS_PNP_ID` includes plug and play ID characteristic.
* The :kconfig:option:`BLE_DIS_PNP_VID_SRC` sets the vendor ID source.
* The :kconfig:option:`BLE_DIS_PNP_VID` sets the vendor ID.
* The :kconfig:option:`BLE_DIS_PNP_PID` sets the product ID.
* The :kconfig:option:`BLE_DIS_PNP_VER` sets the product version.
* The :kconfig:option:`BLE_DIS_REGULATORY_CERT` includes IEEE regulatory certifications.
* The :kconfig:option:`BLE_DIS_REGULATORY_CERT_LIST` sets the regulatory certification list.
* The :kconfig:option:`BLE_DIS_CHAR_SEC_MODE_JUST_WORKS` sets the service security mode to open link.
* The :kconfig:option:`BLE_DIS_CHAR_SEC_MODE_ENCRYPTED` sets the service security mode to encrypted.
* The :kconfig:option:`BLE_DIS_CHAR_SEC_MODE_ENCRYPTED_MITM` sets the service security mode to encrypted with man in the middle protection.
* The :kconfig:option:`BLE_DIS_CHAR_SEC_MODE_LESC_ENCRYPTED_MITM` sets the service security mode to LESC encryption with man-in-the-middle protection.
* The :kconfig:option:`BLE_DIS_CHAR_SEC_MODE_SIGNED` sets the service security mode to signing or encryption required.
* The :kconfig:option:`BLE_DIS_CHAR_SEC_MODE_SIGNED_MITM` sets the service security mode to signing or encryption required, with man in the middle protection.

Initialization
==============

The service is initialized by calling the :c:func:`ble_dis_init` function.
Configuration is otherwise done through the Kconfig options.

Usage
*****

When enabled, the module will add the Device Information Service with information as specified by the Kconfig options.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/bluetooth/services/ble_dis.h`
| Source files: :file:`subsys/bluetooth/services/ble_dis/`

:ref:`Device Information Service API reference <api_dis>`
