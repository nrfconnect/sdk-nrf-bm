.. _ble_mds_sample:

Bluetooth: Memfault Diagnostic Service (MDS)
############################################

.. contents::
   :local:
   :depth: 2

The Memfault Diagnostic Service sample demonstrates how to expose diagnostic data collected by the `Memfault SDK`_ over Bluetooth LE using |BMlong|.
The sample advertises the Memfault Diagnostic Service (MDS) and the Battery Service.
A Bluetooth gateway can connect to the device, read the Memfault upload information, and stream diagnostic chunks to the Memfault cloud.

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

The sample uses the Memfault SDK as a module in the |BMshort| to collect coredumps, reboot reasons, metrics, and trace events from the device.
The Memfault Diagnostic Service (MDS) GATT service exports this data over Bluetooth LE to an MDS gateway, which forwards it to the Memfault cloud.

.. _ble_mds_sample_getting_started_memfault:

Getting started with Memfault
*****************************

To view decoded metrics, trace events, and coredumps from this sample in the Memfault web UI, complete the cloud setup below before testing with `nRF Connect Device Manager`_.

Memfault account and project
============================

Memfault remote diagnostics are integrated with `nRF Cloud`_.
If you do not already have access:

1. Register through the `nRF Cloud Memfault registration`_ page.
2. `Create a Memfault project`_ for the fleet you want to monitor.
   Each project has a unique **project key**.
3. Set the project key in the :kconfig:option:`CONFIG_MEMFAULT_NCS_PROJECT_KEY` Kconfig option in :file:`prj.conf` before building for real use (replace the placeholder key used for CI builds).

The key must match the Memfault project where you expect the device to appear.
For a broader overview of Memfault in |NCS|, see `nRF Cloud powered by Memfault`_.

Symbol file upload
==================

Memfault must associate uploaded diagnostic data with the firmware that produced it.
For **each firmware version** you build and run:

1. Build and program the sample.
2. Upload the ELF symbol file from the build output: :file:`<build>/ble_mds/zephyr/zephyr.elf`.
3. In the `Memfault Dashboard`_, link that symbol file to the **same** ``software_version`` string the device reports.

The sample reports ``software_version`` from the :file:`VERSION` file (see :ref:`ble_mds_sample_firmware_version`).
The version in Memfault must match, or decoding will fail or be incomplete.

Symbol files are required to decode:

* `Memfault: Coredumps`_
* `Memfault: Error Tracking with Trace Events`_ with logs
* Other symbolicated debug information shown in the Memfault UI

Without a matching symbol file, chunks may upload successfully but appear with limited or unusable detail.

Typical upload flow in the Memfault web UI:

1. Log in to the `Memfault Dashboard`_ and open your project.
2. Navigate to :guilabel:`Fleet` > :guilabel:`Devices`.
3. Connect the sample with `nRF Connect Device Manager`_ and generate some diagnostic data.
4. Select the **software version** reported by your device.
5. Click :guilabel:`Upload` and select :file:`zephyr.elf` from your build directory.

Upload a new symbol file whenever you change :file:`VERSION` or release a new firmware build.

Data upload over Bluetooth LE
=============================

This sample does not send data to Memfault over IP.
Instead, it uses the Memfault Diagnostic Service (MDS) over Bluetooth LE to stream diagnostic chunks to a gateway like `nRF Connect Device Manager`_, which forwards them to Memfault.
Firmware must call :c:func:`ble_mds_process` from the main loop.
See :ref:`lib_ble_service_mds` for the MDS library API and :ref:`memfault_bm` for bare-metal Memfault usage rules.

Metrics
=======

The sample defines the following application metrics in :file:`samples/bluetooth/ble_mds/memfault_config/memfault_metrics_heartbeat_config.def`:

* ``button_press_count`` - The number of button presses.
* ``battery_soc_pct`` - The simulated battery level.
* ``button_elapsed_time_ms`` - The time measured between two button presses.

Trace events
============

The sample defines the ``button_state_changed`` trace reason in :file:`samples/bluetooth/ble_mds/memfault_config/memfault_trace_reason_user_config.def`.
The event is collected when **Button 1** changes state.

Core dumps
==========

Press **Button 3** to trigger a hardfault exception by division by zero.
After reboot, reconnect with an MDS gateway to transfer the collected coredump data to Memfault.

User interface
**************

LED 0:
   Lit when the device is initialized.

LED 1:
   Lit when a device is connected.

Button 0:
   Starts or stops the ``button_elapsed_time_ms`` metric timer.
   The second press stops the timer and triggers a Memfault heartbeat using a deferred approach.
   Heartbeat deferral follows :ref:`memfault_bm`.

Button 1:
   Records the ``button_state_changed`` trace event on press and release.

Button 2:
   Increments the ``button_press_count`` metric.

Button 3:
   Simulates a crash by triggering a hardfault exception.

Configuration
*************

Before testing with a real gateway, complete :ref:`ble_mds_sample_getting_started_memfault`.

Sample settings
================
* :kconfig:option:`CONFIG_SAMPLE_BLE_DEVICE_NAME` - The advertised device name. Defaults to ``"nRF BM Memfault"``.
* :kconfig:option:`CONFIG_SAMPLE_BLE_MDS_BATTERY_LEVEL_MEAS_INTERVAL` - Battery level measurement interval, in milliseconds. Defaults to ``1000``.

Bluetooth LE settings
======================
* :kconfig:option:`CONFIG_BLE_MDS` - Enables the Memfault Diagnostic Service. See :ref:`lib_ble_service_mds` for the full set of MDS-specific options.
* :kconfig:option:`CONFIG_BLE_DIS_SERIAL_NUMBER` - The device serial number reported over the Device Information Service. Set to a placeholder (``"ABCD"``) in this sample.

:file:`Kconfig` also propagates the application :file:`VERSION` file to Memfault and DIS; see :ref:`ble_mds_sample_firmware_version` for the full mapping.

Memfault settings
==================

* :kconfig:option:`CONFIG_MEMFAULT_NCS_PROJECT_KEY` - Your Memfault project key. The sample ships with a placeholder key used for CI builds; set a real key before deploying (see :ref:`ble_mds_sample_getting_started_memfault`).
* :kconfig:option:`CONFIG_MEMFAULT_NCS_FW_VERSION_STATIC` - Reports the static version from :file:`VERSION` instead of the SDK's default dynamic ``<prefix>+<build-id>`` string.
* :kconfig:option:`CONFIG_MEMFAULT_NCS_DEVICE_ID_HW_ID` and :kconfig:option:`CONFIG_HW_ID_LIBRARY_SOURCE_DEVICE_ID` - Together, make each device report a hardware-derived Memfault device ID.
* :kconfig:option:`CONFIG_MEMFAULT_LOGGING_ENABLE` - Set to ``n`` in this sample. :kconfig:option:`CONFIG_BLE_MDS_LOG_COLLECTION` has no effect while this is disabled.
* :kconfig:option:`CONFIG_MEMFAULT_EVENT_STORAGE_SIZE` - Set to ``2048`` bytes in this sample.

For more details on Memfault configuration, see :ref:`memfault_bm` and :file:`samples/bluetooth/ble_mds/memfault_config/memfault_platform_config.h`.

This sample follows the ISR vs main-loop rules in :ref:`memfault_bm` (metric timers in the button ISR; heartbeat serialization in the main loop).

For Memfault SDK options that are not configurable through Kconfig, use :file:`samples/bluetooth/ble_mds/memfault_config/memfault_platform_config.h`.

.. _ble_mds_sample_firmware_version:

Firmware version
================

Firmware version strings are managed from a single :file:`VERSION` file in the application root directory.
Update ``VERSION_MAJOR``, ``VERSION_MINOR``, and ``PATCHLEVEL`` there when you release a new build.
The sample currently ships with version ``0.1.0``.

:file:`Kconfig` propagates ``$(APPVERSION)`` from that file to both Memfault and the Bluetooth Device Information Service (DIS):

* :kconfig:option:`CONFIG_MEMFAULT_NCS_FW_VERSION` - Memfault ``software_version``, defaults to ``$(APPVERSION)``
* :kconfig:option:`CONFIG_BLE_DIS_FW_REVISION` - DIS Firmware Revision characteristic, defaults to :kconfig:option:`CONFIG_MEMFAULT_NCS_FW_VERSION`
* :kconfig:option:`CONFIG_MEMFAULT_NCS_FW_TYPE` - Memfault ``software_type``, defaults to ``"app"``
* :kconfig:option:`CONFIG_BLE_DIS_SW_REVISION` - DIS Software Revision characteristic, defaults to :kconfig:option:`CONFIG_MEMFAULT_NCS_FW_TYPE`
* :kconfig:option:`CONFIG_MEMFAULT_NCS_HW_VERSION` - Memfault ``hardware_version``, defaults to ``"hw 54.15.0"``
* :kconfig:option:`CONFIG_BLE_DIS_HW_REVISION` - DIS Hardware Revision characteristic, defaults to :kconfig:option:`CONFIG_MEMFAULT_NCS_HW_VERSION`

:file:`prj.conf` enables :kconfig:option:`CONFIG_MEMFAULT_NCS_FW_VERSION_STATIC`, so Memfault reports the static version from :file:`VERSION` instead of the default dynamic ``<prefix>+<build-id>`` string.

To change the hardware revision string, update the default for :kconfig:option:`CONFIG_MEMFAULT_NCS_HW_VERSION` in :file:`Kconfig`.
The device serial number is configured separately, through :kconfig:option:`CONFIG_BLE_DIS_SERIAL_NUMBER` in :file:`prj.conf`.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_mds/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

Test this sample with `nRF Connect Device Manager`_ after completing :ref:`ble_mds_sample_getting_started_memfault`.

1. Compile and program the application.
#. Connect to the kit with a terminal emulator, for example the `Serial Terminal app`_.
#. Reset the kit.
#. In the terminal, observe that the ``BLE MDS sample initialized`` message is printed.
#. Observe that the ``Advertising as nRF_BM_MDS`` message is printed.
   You can configure this name using the :kconfig:option:`CONFIG_SAMPLE_BLE_DEVICE_NAME` Kconfig option.
#. Open `nRF Connect Device Manager`_ and scan for devices.
#. Connect to the device and open the diagnostics view.
#. Use the buttons to generate metrics, trace events, and a coredump.
#. Explore the Memfault web UI to inspect the uploaded data.

Dependencies
************

This sample uses the following |BMshort| libraries:

* :c:func:`ble_adv_init`
* :c:func:`ble_bas_init`
* :c:func:`ble_dis_init`
* :c:func:`ble_mds_init`
* :c:func:`bm_buttons_init`
* :c:func:`bm_timer_init`

In addition, it uses the `Memfault firmware SDK`_.

.. _`Memfault firmware SDK`: https://github.com/memfault/memfault-firmware-sdk
