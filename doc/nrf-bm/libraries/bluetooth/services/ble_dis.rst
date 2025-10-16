.. _lib_ble_service_dis:

Device Information Service (DIS)
################################

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

* :kconfig:option:`BLE_DIS_MANUFACTURER_NAME` - Sets the manufacturer name.
* :kconfig:option:`BLE_DIS_MODEL_NUMBER` - Sets the model number.
* :kconfig:option:`BLE_DIS_SERIAL_NUMBER` - Sets the serial number.
* :kconfig:option:`BLE_DIS_HW_REVISION` - Sets the hardware revision.
* :kconfig:option:`BLE_DIS_FW_REVISION` - Sets the firmware revision.
* :kconfig:option:`BLE_DIS_SW_REVISION` - Sets the software revision.
* :kconfig:option:`BLE_DIS_SYSTEM_ID` - Includes the system ID characteristic in the Device Information Service.
* :kconfig:option:`BLE_DIS_SYSTEM_ID_OUI` - Sets the organization unique ID.
* :kconfig:option:`BLE_DIS_SYSTEM_ID_MID` - Sets the manufacturer unique ID.
* :kconfig:option:`BLE_DIS_PNP_ID` - Includes plug and play ID characteristic.
* :kconfig:option:`BLE_DIS_PNP_VID_SRC` - Sets the vendor ID source.
* :kconfig:option:`BLE_DIS_PNP_VID` - Sets the vendor ID.
* :kconfig:option:`BLE_DIS_PNP_PID` - Sets the product ID.
* :kconfig:option:`BLE_DIS_PNP_VER` - Sets the product version.
* :kconfig:option:`BLE_DIS_REGULATORY_CERT` - Includes IEEE regulatory certifications.
* :kconfig:option:`BLE_DIS_REGULATORY_CERT_LIST` - Sets the regulatory certification list.
* :kconfig:option:`BLE_DIS_CHAR_SEC_MODE_JUST_WORKS` - Sets the service security mode to open link.
* :kconfig:option:`BLE_DIS_CHAR_SEC_MODE_ENCRYPTED` - Sets the service security mode to encrypted.
* :kconfig:option:`BLE_DIS_CHAR_SEC_MODE_ENCRYPTED_MITM` - Sets the service security mode to encrypted with man in the middle protection.
* :kconfig:option:`BLE_DIS_CHAR_SEC_MODE_LESC_ENCRYPTED_MITM` - Sets the service security mode to LESC encryption with man-in-the-middle protection.
* :kconfig:option:`BLE_DIS_CHAR_SEC_MODE_SIGNED` - Sets the service security mode to signing or encryption required.
* :kconfig:option:`BLE_DIS_CHAR_SEC_MODE_SIGNED_MITM` - Sets the service security mode to signing or encryption required, with man in the middle protection.

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

| Header file: :file:`include/bm/bluetooth/services/ble_dis.h`
| Source files: :file:`subsys/bluetooth/services/ble_dis/`

:ref:`Device Information Service API reference <api_dis>`
