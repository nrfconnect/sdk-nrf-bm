.. _lib_event_scheduler:

Event Scheduler
###############

.. contents::
   :local:
   :depth: 2

The event scheduler is used for transferring execution from the interrupt context to the main application context.

Overview
********

In some applications, it is beneficial to defer the execution of certain interrupts, such as those from the SoftDevice, to the main application function.
This shortens the time spent in the interrupt service routine (ISR).
It also allows for lower priority events to be raised before the previous event is fully processed.

Configuration
*************

The library is enabled using the Kconfig system.
Set the :kconfig:option:`CONFIG_EVENT_SCHEDULER` Kconfig option to enable the library.

Use the :kconfig:option:`CONFIG_EVENT_SCHEDULER_BUF_SIZE` Kconfig option to set the size of the event scheduler buffer that the events are copied into.

Initialization
==============

The library is initialized automatically on application startup.

Usage
*****

The SoftDevice event handler can call the :c:func:`event_scheduler_defer` function to schedule an event for later execution in the main thread.

To process these deferred events, call the :c:func:`event_scheduler_process` function regularly in the main application function.

Dependencies
************

This library does not have any dependencies.

API documentation
*****************

| Header file: :file:`include/bm/event_scheduler.h`
| Source files: :file:`lib/event_scheduler/`

:ref:`Event Scheduler library API reference <api_event_scheduler>`
