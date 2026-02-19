.. _ble_mcuboot_recovery_retention_sample:

MCUboot: Recovery retention
###########################

.. contents::
   :local:
   :depth: 2

This sample demonstrates how to reboot into `MCUboot`_'s recovery (firmware loader) mode using the retention boot mode subsystem.
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
   * - `nRF54L15 DK`_
     - PCA10156
     - S145
     - bm_nrf54l15dk/nrf54l15/cpuapp/s145_softdevice/mcuboot
   * - `nRF54L15 DK`_ (emulating nRF54L10)
     - PCA10156
     - S145
     - bm_nrf54l15dk/nrf54l10/cpuapp/s145_softdevice/mcuboot
   * - `nRF54L15 DK`_ (emulating nRF54L05)
     - PCA10156
     - S145
     - bm_nrf54l15dk/nrf54l05/cpuapp/s145_softdevice/mcuboot

Overview
********

After the application boots, it waits 3 seconds before setting the retention register for entering the firmware loader mode and then reboots the device.
The device then starts advertising the MCUmgr service allowing for a firmware update to be loaded.

Programming the S115 SoftDevice
*******************************

The SoftDevice is automatically flashed when programming the board.
No additional steps are needed for programming the SoftDevice when building with MCUboot firmware loader mode enabled.

Building and running
********************

This sample can be found under :file:`samples/boot/mcuboot_recovery_retention/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

.. _ble_mcuboot_recovery_retention_sample_testing:

Testing
=======

1. Compile and program the application.
#. Connect the device to the computer to access UART 0.
   If you use a development kit, UART 0 is forwarded as a COM port (Windows) or ttyACM device (Linux) after you connect the development kit over USB.
#. Connect to the DK with a terminal emulator (for example, the `Serial Terminal app`_) to UART 0.
#. Reset the DK.
#. Observe that the device boots and reboots into firmware loader mode and then advertises with the default name ``nRF_BM_MCUmgr``.

   You can configure this name using the ``CONFIG_BLE_ADV_NAME`` Kconfig option of the ``firmware_loader`` image.
   For information on how to do this, see `Configuring Kconfig`_.
#. Connect to your device using the `nRF Connect Device Manager`_ mobile application.
#. Under the :guilabel:`image` tab, tap :guilabel:`Advanced`.
   Then, in the image control pane, tap :guilabel:`Read`.
   Observe that the details of the currently loaded application are displayed.
