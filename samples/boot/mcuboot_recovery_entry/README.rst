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
Before the reset, the mobile device can also set up the advertising name of the firmware loader using the MCUmgr settings management commands.
The command sets the retention register for entering the firmware loader mode and reboots the device.
It then starts advertising the MCUmgr service allowing for a firmware update to be loaded.

For more details on this workflow, see `System reset`_ in Zephyr documentation.

Programming the S115 SoftDevice
*******************************

The SoftDevice is automatically flashed when programming the board.
No additional steps are needed for programming the SoftDevice when building with MCUboot firmware loader mode enabled.

Building and running
********************

This sample can be found under :file:`samples/boot/mcuboot_recovery_entry/` in the |BMshort| folder structure.
The sample supports the default configuration for evaluation purposes and the size-optimized configuration of the DFU component for reference.
To enable the size-optimized configuration, set :makevar:`FILE_SUFFIX` to ``size_opt`` when building the sample.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

.. _ble_mcuboot_recovery_entry_sample_testing:

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
#. In the reset control panel, select :guilabel:`Switch to Firmware Loader mode`.
   Optionally, specify the advertising name in the text field below and tap :guilabel:`Send Reset Command`.
#. Observe in the terminal that the device reboots into the Firmware Loader mode and advertises with the advertising name specified or the default Firmware Loader name ``nRF_BM_MCUmgr``.
#. Using the `nRF Connect Device Manager`_ mobile application, go back to the device list and refresh it by swiping down.
   Connect to your device that now advertises with the advertising name specified or the default firmware loader name ``nRF_BM_MCUmgr``.
#. Under the :guilabel:`image` tab, tap :guilabel:`Advanced`.
   Then, in the image control pane, tap :guilabel:`Read`.
   Observe that the details of the currently loaded application are displayed.
#. Build an application with a modified image version by changing the :kconfig:option:`CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION` Kconfig option.
   One way of changing the option is to add ``CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION="1.0.0+0"`` to the sample's :file:`prj.conf` file to override the default version string.
#. Transfer the :file:`<build_dir>/installer_softdevice_firmware_loader.bin` and :file:`<build_dir>/<SAMPLE_name>/zephyr/zephyr.signed.bin` files to your mobile device.
#. In nRF Connect Device Manager application, tap the :guilabel:`Image` tab at the bottom to move to the image management tab.
   If you only see the basic image tab view, tap :guilabel:`Advanced` to switch to the advanced image tab view.
#. Under :guilabel:`Firmware Upload` select the update file to load:

   * Choose :file:`installer_softdevice_firmware_loader.bin` to update the SoftDevice and Firmware Loader images.
     This is only needed if the SoftDevice or Firmware Loader images have been updated.
     This image must be loaded first if it is needed.
   * Choose :file:`zephyr.signed.bin` to load the application update.

#. Tap the :guilabel:`Upload` button, then select the :guilabel:`Image 0` option to begin the firmware update process.
#. The mobile device will connect and load the firmware update to the device.
#. Once completed, reboot the device.
   If the Installer image was loaded, then it will apply the updates and reboot into Firmware Loader mode automatically and allow for loading the application firmware update using the same process.
   If an application update was loaded, then the new application will begin executing.

Dependencies
************

The sample uses the following Zephyr libraries:

* https://docs.zephyrproject.org/latest/hardware/peripherals/retained_mem.html
* https://docs.zephyrproject.org/latest/services/retention/index.html
* https://docs.zephyrproject.org/latest/services/device_mgmt/mcumgr.html
