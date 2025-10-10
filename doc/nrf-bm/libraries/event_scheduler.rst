.. _lib_event_scheduler:

Event Scheduler
###############

.. contents::
   :local:
   :depth: 2

The event scheduler is used for transferring execution from the interrupt context to the main context.

Overview
********

In some applications it is beneficial to defer the execution of certain interrupts, for example some SoftDevice interrupts, to the main application function.
This shortens the time spent in the interrupt service routine (ISR) and allows for other (low priority) events to be raised before the previous event is fully processed.
Note that the application must take care to only defer events that can be interleaved by the processing of other interrupts.

Configuration
*************

The library is enabled using the Kconfig system.
Set the :kconfig:option:`CONFIG_EVENT_SCHEDULER` Kconfig option to enable the library.

The size of the event scheduler buffer, that the events are copied into, is set by the :kconfig:option:`CONFIG_EVENT_SCHEDULER_BUF_SIZE` Kconfig option.

Initialization
==============

The library is initialized automatically on application startup.

Usage
*****

The SoftDevice event handler can call the :c:func:`event_scheduler_defer` function to schedule an event for execution in the main thread.
The :c:func:`event_scheduler_process` function must be called regulary from the main function to process the deferred events.

Dependencies
************

This library does not have any dependencies.

API documentation
*****************

| Header file: :file:`include/event_scheduler`
| Source files: :file:`lib/event_scheduler/`

:ref:`Event Scheduler library API reference <api_event_scheduler>`
