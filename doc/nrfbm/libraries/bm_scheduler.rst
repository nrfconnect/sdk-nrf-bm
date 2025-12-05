.. _lib_bm_scheduler:

Bare Metal scheduler
####################

.. contents::
   :local:
   :depth: 2

The Bare Metal scheduler is a library for scheduling functions to run on the main thread.

Overview
********

This library enables queuing and execution of functions in the main-thread context.
It can be used, for example, to defer the execution of operations from an ISR to the main thread.
This shortens the time spent in the interrupt service routine (ISR).

Configuration
*************

The library is enabled using the Kconfig system.
Set the :kconfig:option:`CONFIG_BM_SCHEDULER` Kconfig option to enable the library.

Use the :kconfig:option:`CONFIG_BM_SCHEDULER_BUF_SIZE` Kconfig option to set the size of the event scheduler buffer that the events are copied into.

Initialization
==============

The library is initialized automatically on application startup.

Usage
*****

The SoftDevice event handler can call the :c:func:`bm_scheduler_defer` function to schedule an event for later execution in the main thread.

To process these deferred events, call the :c:func:`bm_scheduler_process` function regularly in the main application loop.

Dependencies
************

This library does not have any dependencies.

API documentation
*****************

| Header file: :file:`include/bm/bm_scheduler.h`
| Source files: :file:`lib/bm_scheduler/`

:ref:`Bare Metal scheduler library API reference <api_bm_scheduler>`
