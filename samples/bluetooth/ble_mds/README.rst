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

The sample uses the bare-metal SoftDevice Bluetooth libraries to initialize advertising, the Battery Service, the Device Information Service, and MDS.
The Memfault SDK collects reboot information, logs, metrics, trace events, and coredumps.
When an MDS gateway subscribes to the Data Export characteristic and enables streaming, the application calls :c:func:`ble_mds_process` from the main loop to send pending Memfault chunks.

Application metrics
===================

The sample defines the following application metrics in :file:`samples/bluetooth/ble_mds/memfault_config/memfault_metrics_heartbeat_config.def`:

* ``button_press_count`` - The number of **Button 2** presses.
* ``battery_soc_pct`` - The simulated battery level.
* ``button_elapsed_time_ms`` - The time measured between two **Button 0** presses.

Pressing **Button 0** the second time stops the timer and triggers a heartbeat collection.

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
   Blinks while the sample is running.

LED 1:
   Lit when a Bluetooth LE central is connected.

Button 0:
   Starts or stops the ``button_elapsed_time_ms`` metric timer.
   The second press stops the timer and triggers a Memfault heartbeat.

Button 1:
   Triggers the ``button_state_changed`` trace event on press and release.

Button 2:
   Increments the ``button_press_count`` metric and triggers a Memfault heartbeat.

Button 3:
   Simulates a crash by triggering a hardfault exception.

Configuration
*************

The sample configuration is split between the :file:`prj.conf` file and sample-specific Kconfig options in :file:`Kconfig`.

Set :kconfig:option:`CONFIG_MEMFAULT_NCS_PROJECT_KEY` to the project key for your Memfault project before using a real gateway.
The sample uses :kconfig:option:`CONFIG_MEMFAULT_NCS_DEVICE_ID_HW_ID` with :kconfig:option:`CONFIG_HW_ID_LIBRARY_SOURCE_DEVICE_ID` so each device reports a hardware-derived Memfault device ID.
It selects :kconfig:option:`CONFIG_MEMFAULT_REBOOT_REASON_GET_BASIC` for compatibility across the bare-metal board variants supported by this sample.
You can configure the advertising name with the :kconfig:option:`CONFIG_SAMPLE_BLE_DEVICE_NAME` Kconfig option.

The MDS library settings use the ``CONFIG_BLE_MDS`` prefix.
For Memfault SDK options that are not configurable through Kconfig, use :file:`samples/bluetooth/ble_mds/memfault_config/memfault_platform_config.h`.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_mds/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing the sample
==================

You can test this sample using `nRF Connect Device Manager`_ or another MDS-compatible Bluetooth gateway.

1. Configure :kconfig:option:`CONFIG_MEMFAULT_NCS_PROJECT_KEY` for your Memfault project.
#. Compile and program the application.
#. Connect to the kit with a terminal emulator, for example the `Serial Terminal app`_.
#. Reset the kit.
#. In the terminal, observe that the ``BLE MDS sample initialized`` message is printed.
#. Observe that the ``Advertising as nRF_BM_MDS`` message is printed.
   You can configure this name using the :kconfig:option:`CONFIG_SAMPLE_BLE_DEVICE_NAME` Kconfig option.
#. Open `nRF Connect Device Manager`_ and scan for devices.
#. Connect to the device and open the diagnostics view.
#. Use the buttons to generate metrics, trace events, and a coredump.
#. Upload the generated symbol file from the build directory to your Memfault project so that uploaded data can be decoded.

Dependencies
************

This sample uses the following |BMshort| libraries:

* :c:func:`ble_adv_init`
* :c:func:`ble_bas_init`
* :c:func:`ble_dis_init`
* :c:func:`ble_mds_init`
* :c:func:`bm_buttons_init`
* :c:func:`bm_timer_init`

In addition, it uses the `Memfault SDK`_.

.. _Memfault SDK: https://github.com/memfault/memfault-firmware-sdk
