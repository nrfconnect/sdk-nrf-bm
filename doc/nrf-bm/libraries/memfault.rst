.. _memfault_bm:

Memfault on Bare Metal
######################

.. contents::
   :local:
   :depth: 2

|BMshort| reuses the standard |NCS| Memfault SDK integration (device information, metrics, trace events, coredumps, and chunk export).
There is no separate Memfault SDK fork for |BMshort|.

However, |BMshort| runs **without Zephyr multithreading** (``CONFIG_MULTITHREADING=n``).
Applications typically use a **single main loop** plus **ISRs** (SoftDevice events, :ref:`lib_bm_timer` callbacks, GPIO, and similar).
Memfault APIs can therefore be invoked from more than one execution context even though there are no RTOS threads.

This page describes **when Memfault APIs may be called**, **what must be deferred to the main loop**, and **why**.

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

The Memfault SDK protects shared data with :c:func:`memfault_lock` and :c:func:`memfault_unlock`.
On threaded |NCS| builds these map to a recursive ``k_mutex``.
On |BMshort| they map to an ``irq_lock()``-based critical section when the :kconfig:option:`CONFIG_BM_MEMFAULT_LOCK` Kconfig option is enabled.

Platform lock
***************

Enable the :kconfig:option:`CONFIG_BM_MEMFAULT_LOCK` Kconfig option (default on |BMshort| Memfault builds) to use :file:`subsys/memfault/memfault_platform_lock_bm.c`.

This implementation:

* Disables interrupts for the duration of a Memfault critical section while the main loop holds the lock, preventing ISR re-entry into Memfault.
* Supports nested :c:func:`memfault_lock` calls, as required by the SDK.
* Replaces :file:`memfault_platform_lock.c` from the Memfault Zephyr port when the :kconfig:option:`CONFIG_BM_MEMFAULT_LOCK` Kconfig option is enabled (nrf-bm excludes that SDK source from the build so ``k_mutex`` is not required).

Do **not** stub ``z_impl_k_mutex_lock()`` / ``z_impl_k_mutex_unlock()`` as no-ops in application code.
That removes all protection while leaving the SDK assuming locking is in place.

Disabling the :kconfig:option:`CONFIG_BM_MEMFAULT_LOCK` Kconfig option falls back to the default Zephyr Memfault lock and requires a working ``k_mutex`` implementation.
This is not supported on typical |BMshort| configurations.

Usage rules
***********

The table below summarizes **where Memfault APIs should be called** and the reason for each rule.

.. list-table:: Memfault API usage on |BMshort|
   :header-rows: 1
   :widths: 28 18 54

   * - API / operation
     - Context
     - Why
   * - ``MEMFAULT_METRIC_ADD``, ``MEMFAULT_METRIC_SET_*``
     - ISR or main
     - Updates in-memory metric values only.
       Protected by :c:func:`memfault_lock` when the :kconfig:option:`CONFIG_BM_MEMFAULT_LOCK` Kconfig option is enabled.
   * - ``MEMFAULT_METRIC_TIMER_START``, ``MEMFAULT_METRIC_TIMER_STOP``
     - ISR or main
     - Safe in either context when protected by :c:func:`memfault_lock`.
       Timestamps are recorded at call time, so call start/stop at the moment the timed activity begins or ends.
       Do **not** defer these from an ISR to the main loop if the interval must reflect an ISR event (for example a button press).
       Calling from the main loop is fine when the timed activity is defined in main-loop context.
   * - ``MEMFAULT_TRACE_EVENT*``
     - ISR or main
     - From ISR, the SDK stores a pending event and returns quickly.
       Call :c:func:`memfault_trace_event_try_flush_isr_event` from the main loop to serialize it.
   * - ``memfault_metrics_heartbeat_debug_trigger()``, ``memfault_metrics_heartbeat_collect()``
     - **Main loop only**
     - Serializes metrics into event storage.
       Concurrent serialization while the main loop reads storage (for example during chunk export) can corrupt data.
   * - ``memfault_log_trigger_collection()``
     - **Main loop only**
     - Same rationale as heartbeat serialization: writes to shared event storage.
   * - ``memfault_packetizer_*``. note: “For BLE export via MDS, see :ref:`lib_ble_service_mds`.”
     - **Main loop only**
     - Reads and consumes serialized Memfault data for export.
   * - ``memfault_platform_metrics_timer_boot()`` custom timer callback
     - **Main loop only**
     - |BMshort| uses the :kconfig:option:`CONFIG_MEMFAULT_METRICS_TIMER_CUSTOM` Kconfig option with the :ref:`lib_bm_timer` to wake periodically; the timer ISR sets a flag and the main loop invokes the callback (see the :ref:`ble_mds_sample` for working example).
   * - Coredump capture (HardFault path)
     - Fault handler
     - Runs with normal execution stopped; not subject to the ISR/main rules above.

Rules in practice
=================

Call from ISR when the operation is short and either:

* only touches in-memory SDK state protected by :c:func:`memfault_lock`, or
* is explicitly designed for ISR use (trace events), or
* must capture a timestamp at ISR time (for example ``MEMFAULT_METRIC_TIMER_STOP`` on a button press).

Call from the main loop when that is where the event or timed activity occurs (for example starting a timer metric when entering a main-loop state).
Do **not** defer timer start/stop from ISR to main solely to avoid calling Memfault in the ISR; that shifts the timestamp and produces incorrect intervals.

Defer to the main loop when the operation:

* serializes data into event storage,
* reads or exports stored chunks, or
* performs non-trivial work that must not overlap with export.

Recommended main-loop pattern
=============================

When Memfault is used from ISRs, the main loop should regularly run at least:

.. code-block:: c

   memfault_metrics_timer_process();                 /* bm_timer-deferred heartbeat, if CONFIG_MEMFAULT_METRICS_TIMER_CUSTOM */
   memfault_trace_event_try_flush_isr_event();       /* flush ISR-pended trace events */
   if (memfault_heartbeat_pending) {                /* application-specific deferral flag that could be enabled in the ISR to trigger heartbeat serialization */
       memfault_heartbeat_pending = false;
       memfault_metrics_heartbeat_debug_trigger();
   }

If exporting over MDS, also call :c:func:`ble_mds_process` from the main loop; see :ref:`lib_ble_service_mds`.

See :ref:`ble_mds_sample` for a full integration example.

Timer metrics and heartbeat serialization
=========================================

``MEMFAULT_METRIC_TIMER_START`` and ``MEMFAULT_METRIC_TIMER_STOP`` may be called from ISR or main loop.
Choose the context based on **when the timed activity starts or stops**, because the SDK records time at the call.

**Do** call ``MEMFAULT_METRIC_TIMER_STOP()`` in the ISR when stopping measurement on an ISR event (such as a button press), so the elapsed time matches the user action.

**Do** call start/stop from the main loop when the timed activity is tied to main-loop logic (for example measuring time spent in a connection handler).

**Do not** defer start/stop from ISR to main only to keep Memfault out of the ISR.
That records the main-loop processing time instead of the event time.

**Do not** call ``memfault_metrics_heartbeat_debug_trigger()`` from the ISR.
Set a flag and trigger the heartbeat from the main loop instead:

.. code-block:: c

   /* ISR: stop timer and schedule serialization */
   MEMFAULT_METRIC_TIMER_STOP(button_elapsed_time_ms);
   memfault_heartbeat_pending = true;

   /* Main loop: serialize when safe */
   if (memfault_heartbeat_pending) {
       memfault_heartbeat_pending = false;
       memfault_metrics_heartbeat_debug_trigger();
   }

Prerequisites
*************

* Memfault account, project key (CONFIG_MEMFAULT_NCS_PROJECT_KEY), and matching zephyr.elf per software_version are required before decoded data appears in Memfault.
* :ref:`ble_mds_sample_getting_started_memfault` for step-by-step setup.
* :ref:`nrf_cloud_memfault` for NCS-wide context.

On |BMshort|, chunks are often exported over Bluetooth LE using MDS (:ref:`lib_ble_service_mds`); the :ref:`ble_mds_sample` demonstrates gateway upload with `nRF Connect Device Manager`_.

Configuration
*************

In addition to the :kconfig:option:`CONFIG_BM_MEMFAULT_LOCK` Kconfig option, typical |BMshort| Memfault applications enable:

* :kconfig:option:`CONFIG_MEMFAULT` – Enable the Memfault SDK.
* :kconfig:option:`CONFIG_MEMFAULT_METRICS_TIMER_CUSTOM` – Use :ref:`lib_bm_timer` to schedule periodic heartbeat wakeups; defer the heartbeat callback to the main loop instead of using a Zephyr ``k_work`` / ``k_timer`` in the default Memfault port.
* :kconfig:option:`CONFIG_MEMFAULT_REBOOT_REASON_GET_BASIC` – Basic reboot-reason collection compatible with all |BMshort| board variants.
* :kconfig:option:`CONFIG_MEMFAULT_COREDUMP_COLLECT_KERNEL_REGION` = ``n`` and :kconfig:option:`CONFIG_MEMFAULT_COREDUMP_COLLECT_TASKS_REGIONS` = ``n`` – No Zephyr kernel or task regions on bare metal.
* Optional: enable :kconfig:option:`CONFIG_BLE_MDS` to export over BLE — see :ref:`lib_ble_service_mds` for MDS-specific options.

Application-specific Memfault options that are not exposed through Kconfig can be set in :file:`memfault_platform_config.h` in the application tree.

Sample
******

The :ref:`ble_mds_sample` applies the rules on this page together with MDS export.

Implementation files
********************

* :file:`subsys/memfault/memfault_platform_lock_bm.c` – ``irq_lock()``-based :c:func:`memfault_lock` / :c:func:`memfault_unlock` implementation.
* :file:`subsys/memfault/Kconfig` – :kconfig:option:`CONFIG_BM_MEMFAULT_LOCK`.

Dependencies
************

This integration depends on the Memfault firmware SDK and the |NCS| Memfault module (``nrf/modules/memfault-firmware-sdk``), pulled in when the :kconfig:option:`CONFIG_MEMFAULT` Kconfig option is enabled.

API documentation
*****************

TBD
