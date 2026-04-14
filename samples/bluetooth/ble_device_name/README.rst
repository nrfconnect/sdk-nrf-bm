.. _ble_device_name_sample:

Bluetooth: Persistent Device Name
#################################

.. contents::
   :local:
   :depth: 2

The Device Name sample demonstrates how to read and write the BLE GAP Device Name characteristic using |BMlong|.
A connected peer can change the device name over BLE. The new name takes effect immediately, persists across disconnects, and survives power cycles.

Requirements
************

The sample supports the following development kits:

.. tabs::

   .. group-tab:: Simple board variants

      The following board variants do **not** have DFU capabilities:

      .. list-table::
         :header-rows: 1

         * - Hardware platform
           - PCA
           - SoftDevice
           - Board target
         * - `nRF54L15 DK`_
           - PCA10156
           - S115
           - bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice
         * - `nRF54L15 DK`_ (emulating nRF54L10)
           - PCA10156
           - S115
           - bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice
         * - `nRF54L15 DK`_ (emulating nRF54L05)
           - PCA10156
           - S115
           - bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice
         * - `nRF54LM20 DK`_
           - PCA10184
           - S115
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s115_softdevice
         * - nRF54LS05 DK
           - PCA10214
           - S115
           - bm_nrf54ls05dk/nrf54ls05b/cpuapp/s115_softdevice
         * - `nRF54LV10 DK`_
           - PCA10188
           - S115
           - bm_nrf54lv10dk/nrf54lv10a/cpuapp/s115_softdevice
         * - `nRF54L15 DK`_
           - PCA10156
           - S145
           - bm_nrf54l15dk/nrf54l15/cpuapp/s145_softdevice
         * - `nRF54L15 DK`_ (emulating nRF54L10)
           - PCA10156
           - S145
           - bm_nrf54l15dk/nrf54l10/cpuapp/s145_softdevice
         * - `nRF54L15 DK`_ (emulating nRF54L05)
           - PCA10156
           - S145
           - bm_nrf54l15dk/nrf54l05/cpuapp/s145_softdevice
         * - `nRF54LM20 DK`_
           - PCA10184
           - S145
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s145_softdevice
         * - nRF54LS05 DK
           - PCA10214
           - S145
           - bm_nrf54ls05dk/nrf54ls05b/cpuapp/s145_softdevice
         * - `nRF54LV10 DK`_
           - PCA10188
           - S145
           - bm_nrf54lv10dk/nrf54lv10a/cpuapp/s145_softdevice

   .. group-tab:: MCUboot board variants

      The following board variants have DFU capabilities:

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
         * - `nRF54LM20 DK`_
           - PCA10184
           - S115
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s115_softdevice/mcuboot
         * - nRF54LS05 DK
           - PCA10214
           - S115
           - bm_nrf54ls05dk/nrf54ls05b/cpuapp/s115_softdevice/mcuboot
         * - `nRF54LV10 DK`_
           - PCA10188
           - S115
           - bm_nrf54lv10dk/nrf54lv10a/cpuapp/s115_softdevice/mcuboot
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
         * - `nRF54LM20 DK`_
           - PCA10184
           - S145
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s145_softdevice/mcuboot
         * - nRF54LS05 DK
           - PCA10214
           - S145
           - bm_nrf54ls05dk/nrf54ls05b/cpuapp/s145_softdevice/mcuboot
         * - `nRF54LV10 DK`_
           - PCA10188
           - S145
           - bm_nrf54lv10dk/nrf54lv10a/cpuapp/s145_softdevice/mcuboot

Overview
********

The sample advertises with a configurable device name and allows any connected peer to write a new name to the GAP Device Name characteristic (UUID 0x2A00).
When a new name is written:

* The SoftDevice GAP name is updated immediately.
* The advertising data is refreshed so scanners see the new name after disconnect.
* The name is saved to flash (BM ZMS) so it persists across reboots.

On boot, the sample loads the previously saved name from storage.
If none exists, it falls back to the default configured via :kconfig:option:`CONFIG_SAMPLE_BLE_DEVICE_NAME`.

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
#. Observe that ``Advertising as nRF_BM_GAP_DN`` is printed in the terminal.
   You can configure this default name using the :kconfig:option:`CONFIG_SAMPLE_BLE_DEVICE_NAME` Kconfig option.
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
