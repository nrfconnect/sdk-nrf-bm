.. _lib_timer:

Timer Library
#############

.. contents::
   :local:
   :depth: 2

Overview
********

The timer library (also referred to as bm_timer) enables the application to create multiple timer instances.
Start and stop requests, checking for time-outs and invoking the user time-out handlers is performed in the GRTC interrupt handler.

Configuration
*************

The library is enabled and configured entirely using the Kconfig system.
Set the :kconfig:option:`CONFIG_BM_TIMER` Kconfig option to enable the library.

You can set the timer IRQ priority by setting the :kconfig:option:`CONFIG_BM_TIMER_IRQ_PRIO` Kconfig option.

Initialization
==============

A timer instance is initialized by calling the :c:func:`bm_timer_init` function, selecting the timer mode and providing the user callback for when the timer expires.
The timer can be initialized in the following modes:

* :c:macro:`BM_TIMER_MODE_SINGLE_SHOT` - Expire only once when started.
* :c:macro:`BM_TIMER_MODE_REPEATED` - Restart when expired until stopped.

Usage
*****

After initialization, the timer is started by calling the :c:func:`bm_timer_start` function, providing the timeout and user provided context passed to the callback.
The timeout is given in ticks.
The library provides macros for providing the time in standard time units:

* :c:macro:`BM_TIMER_US_TO_TICKS` - Converts microseconds to ticks.
* :c:macro:`BM_TIMER_MS_TO_TICKS` - Converts milliseconds to ticks.

A timer is stopped by calling the :c:func:`bm_timer_stop` function.

Dependencies
************

This library requires one of the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* Clock control - :kconfig:option:`CONFIG_CLOCK_CONTROL`

API documentation
*****************

| Header file: :file:`include/bm_timer.h`
| Source files: :file:`lib/bm_timer/`

:ref:`Timer library API reference <api_timer>`
