.. _hello_softdevice_sample:

Hello SoftDevice
################

.. contents::
   :local:
   :depth: 2

The Hello SoftDevice sample demonstrates how to enable and disable the SoftDevice on a Nordic DK.

The SoftDevice is a precompiled and linked binary software implementing the Bluetooth Low Energy wireless protocol stack.

Requirements
************

The sample supports the following development kits:

.. list-table::
   :header-rows: 1

   * - Hardware platform
     - PCA
     - SoftDevice
     - Board target
   * - `nRF54L15 DK`_
     - PCA10156
     - S115
     - lite_nrf54l15/nrf54l15/cpuapp

Overview
********

The sample demonstrates the basic management of the SoftDevice in NCS Lite.
It shows how to enable and disable the SoftDevice, handle various types of events related to the SoftDevice, and manage Bluetooth LE functionality.

1. The sample starts by printing "Hello SoftDevice sample started".
#. It then proceeds to enable the SoftDevice using ``nrf_sdh_enable_request()``.
#. The Bluetooth LE functionality of the SoftDevice is enabled with ``nrf_sdh_ble_enable()``, passing the default connection configuration tag.
#. After enabling Bluetooth LE, the function waits for 2 seconds using ``k_busy_wait()``.
#. The SoftDevice is then disabled using ``nrf_sdh_disable_request()``.
#. Finally, it prints that the SoftDevice is disabled and ends the program with a goodbye message.

Programming the S115 SoftDevice
*******************************

.. include:: /includes/softdevice_flash.txt

Building and running
********************

This sample can be found under :file:`samples/bluetooth/hello_softdevice/` in the |NCSL| folder structure.

.. include:: /includes/build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``Hello SoftDevice sample started`` message is printed.
#. Observe that the ``SoftDevice enabled`` message is printed.
#. Observe that the ``Bluetooth enabled`` message is printed.
#. Observe that the ``SoftDevice disabled`` message is printed.
#. Observe that the ``Bye`` message is printed.
