.. _memfault_bm:

Memfault on Bare Metal
######################

.. contents::
   :local:
   :depth: 2

|BMshort| reuses the standard |NCS| Memfault SDK integration (device information, metrics, trace events, coredumps, and chunk export).
There is no separate Memfault SDK fork for |BMshort|.

However, |BMshort| runs without Zephyr multithreading (``CONFIG_MULTITHREADING=n``).
Applications typically use a single main loop plus ISRs (SoftDevice events, :ref:`lib_bm_timer` callbacks, GPIO, and similar).
Memfault APIs can therefore be invoked from more than one execution context even though there are no RTOS threads.

This page describes which Memfault APIs may be called from each execution context, what must be deferred to the main loop, and why.

Execution contexts
******************

On |BMshort|, treat these as distinct callers of Memfault:

Main loop
   The cooperative idle loop of the application.
   Examples: Periodic Memfault heartbeat timer processing, flushing deferred trace events, chunk export.

ISR
   Interrupt handlers that can preempt the main loop.
   Examples: :ref:`lib_bm_buttons` debounce timer callbacks, :ref:`lib_bm_timer` expiry handlers, SoftDevice event handlers if Memfault is called directly from them.

Fault handler
   HardFault and other exception handlers used for coredump collection.
   This context is effectively exclusive.

The Memfault SDK protects shared data with ``memfault_lock()`` and ``memfault_unlock()``.
On threaded |NCS| builds these map to a recursive ``k_mutex``.
On |BMshort| they map to an ``irq_lock()``-based critical section when the :kconfig:option:`CONFIG_BM_MEMFAULT_LOCK` Kconfig option is enabled.

Platform lock
*************

Enable the :kconfig:option:`CONFIG_BM_MEMFAULT_LOCK` Kconfig option (default on |BMshort| Memfault builds) to use :file:`subsys/memfault/memfault_platform_lock_bm.c`.

This implementation:

* Disables interrupts for the duration of a Memfault critical section while the main loop holds the lock, preventing ISR re-entry into Memfault.
* Supports nested ``memfault_lock()`` calls, as required by the SDK.
* Replaces :file:`memfault_platform_lock.c` from the Memfault Zephyr port when the :kconfig:option:`CONFIG_BM_MEMFAULT_LOCK` Kconfig option is enabled (nrf-bm excludes that SDK source from the build so ``k_mutex`` is not required).

Disabling the :kconfig:option:`CONFIG_BM_MEMFAULT_LOCK` Kconfig option falls back to the default Zephyr Memfault lock and requires a working ``k_mutex`` implementation.
This is not supported on typical |BMshort| configurations.

Usage rules
***********

The following table summarizes the execution context each Memfault API may be called from, and the reason for each rule.

.. list-table:: Memfault API usage on |BMshort|
   :header-rows: 1
   :widths: 28 18 54

   * - API / operation
     - Context
     - Why
   * - ``MEMFAULT_METRIC_ADD``, ``MEMFAULT_METRIC_SET_*``
     - ISR or main
     - Updates in-memory metric values only.
       Protected by ``memfault_lock()``.
   * - ``MEMFAULT_METRIC_TIMER_START``, ``MEMFAULT_METRIC_TIMER_STOP``
     - ISR or main
     - Protected by ``memfault_lock()``, so safe in either context.
       Timestamps are recorded at call time, so call start/stop at the moment the timed activity begins or ends.
       Do not defer these from an ISR to the main loop if the interval must reflect an ISR event (for example a button press).
       Calling from the main loop is fine when the timed activity is defined in main-loop context.
   * - ``MEMFAULT_TRACE_EVENT*``
     - ISR or main
     - From ISR, the SDK stores a pending event and returns quickly.
       Call ``memfault_trace_event_try_flush_isr_event()`` from the main loop to serialize it.
   * - ``memfault_metrics_heartbeat_debug_trigger()``, ``memfault_metrics_heartbeat_collect()``
     - Main loop only
     - Serializes metrics into event storage.
       Concurrent serialization while the main loop reads storage (for example during chunk export) can corrupt data.
   * - ``memfault_log_trigger_collection()``
     - Main loop only
     - Same rationale as heartbeat serialization: writes to shared event storage.
   * - ``memfault_packetizer_*``
     - Main loop only
     - Reads and consumes serialized Memfault data for export.
       When exporting over Bluetooth® LE, :ref:`lib_ble_service_mds` calls these functions from :c:func:`ble_mds_process`, so the application does not call them directly.
   * - Heartbeat timer callback (``MemfaultPlatformTimerCallback``)
     - Main loop only
     - The callback serializes metrics into event storage, so it must not run in the timer ISR.
       For how to schedule the callback with the :ref:`lib_bm_timer`, see :ref:`memfault_bm_heartbeat_timer`.
   * - Coredump capture (HardFault path)
     - Fault handler
     - Runs with normal execution stopped; not subject to the ISR/main rules above.

Rules in practice
=================

Call from ISR when the operation is short and either:

* only touches in-memory SDK state protected by ``memfault_lock()``, or
* is explicitly designed for ISR use (trace events), or
* must capture a timestamp at ISR time (for example ``MEMFAULT_METRIC_TIMER_STOP`` on a button press).

Call from the main loop when that is where the event or timed activity occurs (for example starting a timer metric when entering a main-loop state).

Defer to the main loop when the operation:

* serializes data into event storage,
* reads or exports stored chunks, or
* performs non-trivial work that must not overlap with export.

.. _memfault_bm_main_loop_pattern:

Recommended main-loop pattern
=============================

When Memfault is used from ISRs, the main loop must regularly drain the work that cannot be done in ISR context.
The pattern is the same in every case: the ISR records what happened and sets a flag, and the main loop performs the serialization.
For example, the ``memfault_metrics_heartbeat_debug_trigger()`` function serializes metrics into event storage, so it must run in the main loop and not in the ISR that triggered the heartbeat.

The ``memfault_metrics_timer_process()`` function and the ``memfault_heartbeat_pending`` flag are application-defined, not Memfault SDK API.
The :ref:`ble_mds_sample` sample implements both.

.. code-block:: c

   /* Application state, written in an ISR and read in the main loop. */
   static volatile bool memfault_heartbeat_pending;

   /* ISR context: record the event, defer the serialization. */
   MEMFAULT_METRIC_TIMER_STOP(button_elapsed_time_ms);
   memfault_heartbeat_pending = true;

   /* Main loop: run the deferred work. */
   memfault_metrics_timer_process();            /* CONFIG_MEMFAULT_METRICS_TIMER_CUSTOM */
   memfault_trace_event_try_flush_isr_event();
   if (memfault_heartbeat_pending) {
       memfault_heartbeat_pending = false;
       memfault_metrics_heartbeat_debug_trigger();
   }
   ble_mds_process(&mds);                       /* CONFIG_BLE_MDS */

Choosing the context for timer metrics
======================================

``MEMFAULT_METRIC_TIMER_START`` and ``MEMFAULT_METRIC_TIMER_STOP`` may be called from ISR or main loop.
Choose the context based on when the timed activity starts or stops, because the SDK records time at the call.

Call ``MEMFAULT_METRIC_TIMER_STOP()`` in the ISR when stopping measurement on an ISR event, such as a button press, so that the elapsed time matches the user action.
Call start and stop from the main loop when the timed activity is tied to main-loop logic, for example when measuring time spent in a connection handler.

.. caution::

   Do not defer start and stop from the ISR to the main loop only to keep Memfault out of the ISR.
   That records the main-loop processing time instead of the event time.
   Defer the heartbeat serialization instead, as shown in :ref:`memfault_bm_main_loop_pattern`.

.. _memfault_bm_prerequisites:

Prerequisites
*************

Memfault remote diagnostics are integrated with `nRF Cloud`_.
Before Memfault can decode the data that a |BMshort| application uploads, make sure that the following prerequisites are met:

* You have a Memfault account and a Memfault project.
  If you do not already have access, register through the `nRF Cloud Memfault registration`_ page, and then `create a Memfault project`_ for the fleet you want to monitor.
* The project key of that project is set with the :kconfig:option:`CONFIG_MEMFAULT_NCS_PROJECT_KEY` Kconfig option.
  Each project has a unique project key, and the key must match the project where you expect the device to appear.
* The :file:`zephyr.elf` symbol file is uploaded to Memfault for every ``software_version`` that the device reports, as set with the :kconfig:option:`CONFIG_MEMFAULT_NCS_FW_VERSION` Kconfig option.

Without a matching symbol file, chunks still upload, but coredumps and trace events cannot be decoded.
For an example of how to upload a symbol file, see the testing steps of the :ref:`ble_mds_sample` sample.

For an overview of Memfault in the |NCS|, see `nRF Cloud powered by Memfault`_.

Configuration
*************

In addition to the :kconfig:option:`CONFIG_BM_MEMFAULT_LOCK` Kconfig option, typical |BMshort| Memfault applications enable:

* :kconfig:option:`CONFIG_MEMFAULT` - Enable the Memfault SDK.
* :kconfig:option:`CONFIG_MEMFAULT_METRICS_TIMER_CUSTOM` - Use :ref:`lib_bm_timer` to schedule periodic heartbeat wakeups; defer the heartbeat callback to the main loop instead of using a Zephyr ``k_work`` / ``k_timer`` in the default Memfault port.
* :kconfig:option:`CONFIG_MEMFAULT_REBOOT_REASON_GET_BASIC` - Basic reboot-reason collection compatible with all |BMshort| board variants.
* :kconfig:option:`CONFIG_MEMFAULT_COREDUMP_COLLECT_KERNEL_REGION` = ``n`` and :kconfig:option:`CONFIG_MEMFAULT_COREDUMP_COLLECT_TASKS_REGIONS` = ``n`` - No Zephyr kernel or task regions on bare metal.
* Optional: enable the :kconfig:option:`CONFIG_BLE_MDS` Kconfig option to export over Bluetooth LE.
  See :ref:`lib_ble_service_mds` for MDS-specific options.

Integrating Memfault in an application
**************************************

In addition to the Kconfig options above, a |BMshort| application must provide the following before Memfault can collect and export data.

Memfault configuration files
============================

The Memfault SDK includes three application-provided files by name, so the directory holding them must be on the include path.
The :ref:`ble_mds_sample` sample keeps them in :file:`memfault_config` and adds that directory from its :file:`CMakeLists.txt`:

.. code-block:: cmake

   zephyr_include_directories(memfault_config)

All three of the following files must exist, even if some of them are empty.
The build fails if any of them is not found on the include path.

* :file:`memfault_platform_config.h` - Memfault SDK settings that are not exposed through Kconfig.
* :file:`memfault_metrics_heartbeat_config.def` - Application-defined heartbeat metrics.
* :file:`memfault_trace_reason_user_config.def` - Application-defined trace reasons.

Fault handler
=============

The :ref:`lib_nrf_sdh` owns the ``HardFault_Handler`` vector and forwards faults that belong to the application to the weak ``C_HardFault_Handler`` symbol.
Point the Memfault fault handler at that symbol in :file:`memfault_platform_config.h`:

.. code-block:: c

   #define MEMFAULT_EXC_HANDLER_HARD_FAULT C_HardFault_Handler

Without this define, the Memfault fault handler is never reached and no coredump is captured.

.. _memfault_bm_heartbeat_timer:

Heartbeat timer
===============

With the :kconfig:option:`CONFIG_MEMFAULT_METRICS_TIMER_CUSTOM` Kconfig option, the application owns the heartbeat schedule.
Implement ``memfault_platform_metrics_timer_boot()``, which the Memfault SDK calls once during initialization with the heartbeat period and a callback:

.. code-block:: c

   bool memfault_platform_metrics_timer_boot(uint32_t period_sec,
                                             MemfaultPlatformTimerCallback callback);

Store the callback, start a repeating :ref:`lib_bm_timer` for the requested period, and return ``true``.
The callback serializes metrics into event storage, so the timer handler must only set a flag and the main loop must invoke the callback when that flag is set.

Operating system name and version
=================================

The Memfault SDK reports the operating system it was built against as part of the build ID information.
On |NCS| this defaults to ``ncs`` with the |NCS| version.
To report |BMshort| instead, override both macros in :file:`memfault_platform_config.h`:

.. code-block:: c

   #include "ncs_bare_metal_version.h"
   #define MEMFAULT_OS_VERSION_NAME "ncs bm"
   #define MEMFAULT_OS_VERSION_STRING NCS_BARE_METAL_VERSION_STRING

The Memfault SDK fails to build if only one of the two macros is defined.
The :file:`ncs_bare_metal_version.h` header is generated by the build system from the |BMshort| :file:`VERSION` file, so this reports the |BMshort| version and not the application version.
The application version is reported separately as the Memfault ``software_version`` through the :kconfig:option:`CONFIG_MEMFAULT_NCS_FW_VERSION` Kconfig option.

Sample
******

The :ref:`ble_mds_sample` sample applies the rules on this page together with MDS export.

Dependencies
************

This integration depends on the Memfault firmware SDK and the |NCS| Memfault module (``nrf/modules/memfault-firmware-sdk``), pulled in when the :kconfig:option:`CONFIG_MEMFAULT` Kconfig option is enabled.

API documentation
*****************

| Source files: :file:`subsys/memfault/`

The |BMshort| Memfault integration does not expose an API of its own.
It provides the ``memfault_lock()`` and ``memfault_unlock()`` platform overrides that the Memfault SDK requires on |BMshort|, and applications call the Memfault SDK API directly.

* For the Memfault SDK API, see the `Memfault SDK`_ documentation.
* For exporting Memfault data over Bluetooth LE, see :ref:`Memfault Diagnostic Service API reference <api_ble_mds>`.
