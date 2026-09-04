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
Each device reports a Memfault device ID derived from its hardware ID, so that devices appear individually in the Memfault fleet view.

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

For more information about defining and collecting metrics, see `Memfault: Collecting Device Metrics`_.

Trace events
============

The sample defines the ``button_state_changed`` trace reason in :file:`samples/bluetooth/ble_mds/memfault_config/memfault_trace_reason_user_config.def`.
The event is collected when **Button 1** changes state.

For more information about trace events, see `Memfault: Error Tracking with Trace Events`_.

Coredumps
=========

Press **Button 3** to trigger a HardFault exception by division by zero.
After reboot, reconnect with an MDS gateway to transfer the collected coredump data to Memfault.

For more information about coredumps, see `Memfault: Coredumps`_.

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
   Simulates a crash by triggering a HardFault exception.

Configuration
*************

You can modify the following options:

* :kconfig:option:`CONFIG_MEMFAULT_NCS_PROJECT_KEY` - Your Memfault project key.
  The sample ships with a placeholder key used for CI builds, so set a real key before deploying (see :ref:`Memfault prerequisites <memfault_bm_prerequisites>`).
* :kconfig:option:`CONFIG_BLE_DIS_SERIAL_NUMBER` - The device serial number reported over the Device Information Service.
  Set to the placeholder ``"ABCD"`` in this sample.
* :kconfig:option:`CONFIG_SAMPLE_BLE_DEVICE_NAME` - The advertised device name.
  Defaults to ``"nRF BM Memfault"``.
* :kconfig:option:`CONFIG_SAMPLE_BLE_MDS_BATTERY_LEVEL_MEAS_INTERVAL` - The battery level measurement interval, in milliseconds.
  Defaults to ``1000``.
* :kconfig:option:`CONFIG_MEMFAULT_LOGGING_ENABLE` - Captures log messages into a Memfault RAM buffer so that they are uploaded together with the other diagnostic data.
  Not enabled in this sample, which also leaves the :kconfig:option:`CONFIG_BLE_MDS_LOG_COLLECTION` Kconfig option without effect.

The remaining Kconfig options in :file:`prj.conf` set up the sample itself and are not intended to be changed.
For the options of the Memfault Diagnostic Service, see :ref:`lib_ble_service_mds`.
For the Memfault options, and for the Memfault SDK settings that are not exposed through Kconfig, see :ref:`memfault_bm`.
The sample also follows the ISR and main loop rules of :ref:`memfault_bm`, with the metric timers in the button ISR and the heartbeat serialization in the main loop.

.. _ble_mds_sample_firmware_version:

Firmware version
================

Memfault tags every chunk a device uploads with the ``software_version`` that the device reports, and decodes that data using the symbol file uploaded for the same version.
Version handling is therefore what determines whether coredumps and trace events are readable in the Memfault web UI.

This sample derives all of its version strings from the :file:`VERSION` file in the sample directory.
To release a new build:

1. Increment ``VERSION_MAJOR``, ``VERSION_MINOR``, or ``PATCHLEVEL`` in :file:`VERSION`.
#. Rebuild and program the sample.
#. Upload the new :file:`zephyr.elf` symbol file to Memfault, as described in :ref:`ble_mds_sample_testing`.

.. note::
   Only ``VERSION_MAJOR``, ``VERSION_MINOR``, ``PATCHLEVEL``, and ``EXTRAVERSION`` contribute to the reported version.
   Changing ``VERSION_TWEAK`` alone produces a new binary that reports an unchanged ``software_version``, which makes Memfault decode it with the previously uploaded symbol file.

The sample :file:`Kconfig` file reports the version from the :file:`VERSION` file as the Memfault ``software_version`` and as the Bluetooth Device Information Service (DIS) firmware revision, so that Memfault and DIS always agree.
The software type (``"app"``) and the hardware version (``"hw 54.15.0"``) are mapped the same way, to the DIS software revision and hardware revision.
To report different values, override the corresponding Kconfig options in :file:`prj.conf`.
These options are set as defaults in the sample :file:`Kconfig` file.

The :file:`prj.conf` file also enables the :kconfig:option:`CONFIG_MEMFAULT_NCS_FW_VERSION_STATIC` Kconfig option, which is what makes Memfault report the version from :file:`VERSION` verbatim instead of generating one at build time.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_mds/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

.. _ble_mds_sample_testing:

Testing
=======

Test this sample with `nRF Connect Device Manager`_ after completing the :ref:`Memfault prerequisites <memfault_bm_prerequisites>`.

1. Compile and program the application.
#. Connect to the kit with a terminal emulator, for example the `Serial Terminal app`_.
#. Reset the kit.
#. In the terminal, observe that the ``BLE MDS sample initialized`` message is printed.
#. Observe that the ``Advertising as nRF BM Memfault`` message is printed.
   You can configure this name using the :kconfig:option:`CONFIG_SAMPLE_BLE_DEVICE_NAME` Kconfig option.
#. Open `nRF Connect Device Manager`_ and scan for devices.
#. Connect to the device and open the diagnostics view.
#. Use the buttons to generate metrics, trace events, and a coredump.
#. Upload the symbol file generated from your build to your Memfault project, so that Memfault can decode the data that the device uploads.
   The symbol file is located in the build folder: :file:`<build>/ble_mds/zephyr/zephyr.elf`.

   a. In a web browser, open the `Memfault Dashboard`_ and select your project.
   #. Navigate to :guilabel:`Fleet` > :guilabel:`Devices` in the left side menu.
      You can see your newly connected device and the software version in the list.
   #. Select the software version number for your device and click :guilabel:`Upload` to upload the symbol file.

#. Explore the Memfault web UI to inspect the uploaded data.

Memfault decodes the uploaded data using the symbol file that is linked to the ``software_version`` string the device reports, which this sample derives from the :file:`VERSION` file (see :ref:`ble_mds_sample_firmware_version`).
Upload a new symbol file whenever you change :file:`VERSION` or release a new firmware build.
Without a matching symbol file, chunks still upload, but coredumps, trace events, and other symbolicated information appear with limited or unusable detail.

Dependencies
************

This sample uses the following |BMshort| libraries:

* :ref:`lib_ble_adv`
* :ref:`lib_ble_conn_params`
* :ref:`lib_ble_service_bas`
* :ref:`lib_ble_service_dis`
* :ref:`lib_ble_service_mds`
* :ref:`lib_bm_buttons`
* :ref:`lib_bm_timer`
* :ref:`lib_nrf_sdh`

In addition, it uses the `Memfault SDK`_.
