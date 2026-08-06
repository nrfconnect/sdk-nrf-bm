.. _ble_device_name_sample:

Bluetooth: Device name
#################################

.. contents::
   :local:
   :depth: 2

The device name sample demonstrates how the connected peer can change the Bluetooth LE GAP Device name characteristic using |BMlong| and how to store the name in non-volatile memory.

Requirements
************

The sample supports the following development kits:

.. tabs::

   .. group-tab:: Simple board variants

      The following board variants do **not** have DFU capabilities:

      .. include:: /includes/supported_boards_all_non-mcuboot_variants_s115.txt

      .. include:: /includes/supported_boards_all_non-mcuboot_variants_s145.txt

   .. group-tab:: MCUboot board variants

      The following board variants have DFU capabilities:

      .. include:: /includes/supported_boards_all_mcuboot_variants_s115.txt

      .. include:: /includes/supported_boards_all_mcuboot_variants_s145.txt

Overview
********

The sample advertises with a configurable device name and allows any connected peer to write a new name to the GAP Device Name characteristic (UUID 0x2A00).
When a new name is written:

* The Bluetooth LE GAP name is updated in both the SoftDevice and in the advertising data.
* The device name is saved to non-volatile memory to persist across reboots and power loss.

During initialization, the sample reads the device name to use from non-volatile memory.
If none exists, it uses the devise name set by the :kconfig:option:`CONFIG_SAMPLE_BLE_DEVICE_NAME` Kconfig option.

User interface
**************

LED 0:
   Lit when the device is initialized.

LED 1:
   Lit when a device is connected.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_device_name/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

1. Compile and program the application.
#. Connect the device to the computer.
#. Connect to the kit with a terminal emulator (for example, the `Serial Terminal app`_).
#. Reset the kit.
#. Observe that ``Advertising as nRF_BM_Device`` is printed in the terminal.
   You can configure the default name using :kconfig:option:`CONFIG_SAMPLE_BLE_DEVICE_NAME` Kconfig option.
#. Open the nRF Connect for Mobile or Desktop app and connect to the device.
#. In the GAP service, find the **Device Name** characteristic (UUID 0x2A00).
#. Read the current name to verify it matches the advertised name.
#. Write a new name (for example, ``MyDevice``).
#. Observe that the terminal prints::

      Device name updated (8 bytes)
      New name
      4d 79 44 65 76 69 63 65  |MyDevice

#. Observe that ``Device name persisted to storage`` is printed shortly after.
#. Disconnect from the device.
#. Refresh and scan again using the nRF Connect for Mobile or Desktop.
#. Observe that the device now advertises with the new name ``MyDevice``.
#. Reset the kit and observe that the saved name is loaded from storage and used for advertising.
