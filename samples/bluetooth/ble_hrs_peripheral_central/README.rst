.. _ble_hrs_peripheral_central_sample:

Bluetooth: Heart Rate Service Peripheral Central
################################################

.. contents::
   :local:
   :depth: 2

The Heart Rate Service Peripheral Central sample demonstrates how you can implement the Heart Rate profile as a server and client using |BMlong|.

Requirements
************

The sample supports the following development kits:

.. tabs::

   .. group-tab:: Simple board variants

      The following board variants do **not** have DFU capabilities:

      .. include:: /includes/supported_boards_all_non-mcuboot_variants_s145.txt

   .. group-tab:: MCUboot board variants

      The following board variants have DFU capabilities:

      .. include:: /includes/supported_boards_all_mcuboot_variants_s145.txt

Overview
********

This sample acts simultaneously as both a peripheral and a central device.

* As a peripheral it advertises with the :ref:`lib_ble_service_hrs` UUID (0x180D).
  A central can connect to this device and subscribe to the Heart Rate Measurement characteristic to receive heart rate notifications.
* As a central the sample scans for other devices that advertise with the :ref:`lib_ble_service_hrs` UUID (0x180D).
  When a device is found, it connects and starts service discovery.
  If the heart rate service is found, it subscribes to receive heart rate notifications that will be forwarded to a connected central device.

User interface
**************

Button 0:
   Press to disable allow list.

   When pairing with authentication, press this button to confirm the passkey shown in the COM listener and complete pairing with the other device.

Button 1:
   Press to disconnect from the connected peripheral device.

   Keep the button pressed while resetting the board to delete bonding information for all peers stored on the device.

   When pairing with authentication, press this button to reject the passkey shown in the COM listener to prevent pairing with the other device.

Button 2:
   Press to disconnect from the connected central device.

LED 0:
   Lit when the device is initialized.

LED 1:
   Lit when connected to a peripheral device.

LED 2:
   Lit when connected to a central device.

.. _ble_hrs_peripheral_central_sample_testing:

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_hrs_peripheral_central/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Scan filtering options
======================

The sample always scans for devices advertising the Heart Rate Service UUID (``0x180D``).
Two optional filters can narrow this down further:

.. list-table::
   :header-rows: 1

   * - Kconfig option
     - Matches on
   * - :kconfig:option:`CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_NAME`
     - Advertised device name (e.g. ``"MyDeviceName"``)
   * - :kconfig:option:`CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_ADDR`
     - Exact 48-bit Bluetooth address

.. note::

   Use of ``peripheral`` in the option name refers to the *remote* device being scanned for, not this device's role.

Testing
=======

This sample can be tested with three devices:

* A device running this sample.
* A device running the :ref:`ble_hrs_sample` sample.
* A central device, for example, a phone or a tablet with `nRF Connect for Mobile`_ or `nRF Toolbox`_.

Complete the following steps to test the sample:

1. Compile and program the application.
#. Observe that the ``BLE HRS Peripheral Central sample initialized`` message is printed.
#. In the Serial Terminal, observe that the ``Advertising as nRF_BM_HRS_bridge`` message is printed.
   You can configure the advertising name using the :kconfig:option:`CONFIG_SAMPLE_BLE_DEVICE_NAME` Kconfig option.
   For information on how to do this, see `Configuring Kconfig`_.
#. Program the second development kit with the :ref:`ble_hrs_sample` sample.
#. Observe that the ``Scan filter match`` message is printed, followed by ``Connecting to target`` and ``Connected``, when connecting to the peripheral device.
#. Observe that the ``Heart rate service discovered`` message is printed.
#. Connect to the device from nRF Connect (the device is advertising as "nRF_BM_HRS_bridge").
#. Observe that the ``Connecting to target`` and ``Connected`` messages are printed when connecting to the central device.
#. Observe that the device starts receiving heart rate measurement notifications and forwarding them to the central.
#. Note the address printed in the log when connecting, e.g:

   .. code-block:: console

      Connecting to target AA:BB:CC:DD:EE:FF

#. Enable the address filter in Kconfig with:

   .. code-block:: cfg

      CONFIG_SAMPLE_USE_TARGET_PERIPHERAL_ADDR=y
      CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR=0xAABBCCDDEEFF

   Rebuild and flash. Confirm the sample still connects to the same peripheral.
#. Change the address to a wrong value, rebuild and flash. Confirm the sample no
   longer connects to any peripheral.
