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
     - nrf54l15dk/nrf54l15/cpuapp

Overview
********

The sample demonstrates the basic lifecycle management of the SoftDevice in a Zephyr-based environment.
It shows how to enable and disable the SoftDevice, handle various types of events related to the SoftDevice, and manage BLE functionality.

1. The sample starts by printing "Hello World!" along with the target board configuration.
#. It then proceeds to enable the SoftDevice using ``nrf_sdh_enable_request()``.
#. After successfully enabling the SoftDevice, it retrieves the start address of the application RAM required by the Bluetooth LE functionality using ``nrf_sdh_ble_app_ram_start_get()``.
#. It sets up a default Bluetooth LE configuration with ``nrf_sdh_ble_default_cfg_set()`` using the previously defined CONN_TAG.
#. The Bluetooth LE functionality of the SoftDevice is enabled with ``nrf_sdh_ble_enable()``, passing the start address of the application RAM.
#. After enabling BLE, the function waits for 2 seconds using ``k_busy_wait()``.
#. The SoftDevice is then disabled using ``nrf_sdh_disable_request()``.
#. Finally, it prints that the SoftDevice is disabled and ends the program with a goodbye message.

Building and running
********************

This sample can be found under :file:`samples/hello_softdevice/` in the |NCSL| folder structure.

Programming the S115 SoftDevice
*******************************

The SoftDevice binary is located in :file:`subsys/softdevice/hex/s115` in the |NCSL| folder structure.

You must program the SoftDevice using the command line:

1.
#.

.. _hello_softdevice_testing:

Testing
=======

After programming the sample to your development kit, complete the following steps to test it:

1.
#.

Dependencies
************

This sample uses the following |NCS| libraries:

* file:`include/nrf_sdh.h`
* file:`include/nrf_sdh_ble.h`
