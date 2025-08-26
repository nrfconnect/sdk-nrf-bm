.. _ble_mcuboot_recovery_entry_sample:

MCUboot: Recovery entry
#######################

.. contents::
   :local:
   :depth: 2

This sample demonstrates how to reboot into `MCUboot`_'s recovery (firmware loader) mode using the retention boot mode subsystem via Bluetooth Low Energy and `MCUmgr`_.
See :ref:`ug_dfu` for details on DFU support in |BMshort|.

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
     - bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice/mcuboot
   * - `nRF54L15 DK`_ (emulating nRF54L10)
     - PCA10156
     - S115
     - bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice/mcuboot
   * - `nRF54L15 DK`_ (emulating nRF54L05)
     - PCA10156
     - S115
     - bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice/mcuboot

Overview
********

After the application boots, it begins advertising as ``nRF_BM_Entry``, advertising the MCUmgr service.

A mobile device can then connect and issue the OS mgmt reset command with the optional ``boot_mode`` parameter provided and set to 1.
The command sets the retention register for entering the firmware loader mode and reboots the device.
It then starts advertising the MCUmgr service allowing for a firmware update to be loaded.

For more details on this workflow, see `System reset`_ in Zephyr documentation.

Programming the S115 SoftDevice
*******************************

The SoftDevice is automatically flashed when programming the board.
No additional steps are needed for programming the SoftDevice when building with MCUboot firmware loader mode enabled.

.. _ble_mcuboot_recovery_entry_sample_testing:

Building and running
********************

This sample can be found under :file:`samples/boot/mcuboot_recovery_entry` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/configure_and_build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

1. Compile and program the application.
#. Connect the device to the computer to access the UART.
   If you use a development kit, the UART is forwarded as a COM port (Windows) or ttyACM device (Linux) after you connect the development kit over USB.
#. Connect to the DK with a terminal emulator (for example, the `Serial Terminal app`_) to VCOM1.
#. Reset the DK.
#. Observe in the terminal that the device boots into application mode and then advertises with the default name ``nRF_BM_Entry``.
#. Connect to your device using the `nRF Connect Device Manager`_ mobile application.
#. Under the :guilabel:`image` tab, tap :guilabel:`Advanced`.
   Then, in the reset control pane, select :guilabel:`Switch to bootloader mode` and tap :guilabel:`Send Reset Command`.
   Observe in the terminal that the device reboots into Firmware Loader mode and advertises with the default Firmware Loader name ``nRF_BM_MCUmgr``.
#. Using the `nRF Connect Device Manager`_ mobile application, go back to the device list and refresh it by swiping down.
   Connect to your device that now advertises with the default firmware loader name ``nRF_BM_MCUmgr``.
#. Under the :guilabel:`image` tab, tap :guilabel:`Advanced`.
   Then, in the image control pane, tap :guilabel:`Read`.
   Observe that the details of the currently loaded application are displayed.

Dependencies
************

The sample uses the following Zephyr libraries:

* https://docs.zephyrproject.org/latest/hardware/peripherals/retained_mem.html
* https://docs.zephyrproject.org/latest/services/retention/index.html
* https://docs.zephyrproject.org/latest/services/device_mgmt/mcumgr.html
