.. _lib_bm_timer:

Bare Metal Timer library
########################

.. contents::
   :local:
   :depth: 2

The Bare Metal (BM) Timer library provides a wrapper around Zephyr's Timers functionality, utilizing the GRTC peripheral.

The BM Timer has a resolution of 1 µs and can run in all power modes.
This allows for waking up the system after a specific period or at specific intervals, independent of power modes.
The BM Timer allows the application to create multiple timer instances, each with dedicated, user defined timeout handlers.
The timers can be individually started and stopped.

Configuration
*************

The library is enabled and configured entirely using the Kconfig system.
Set the :kconfig:option:`CONFIG_BM_TIMER` Kconfig option to enable the library.

You can adjust the timer IRQ priority using the :kconfig:option:`CONFIG_BM_TIMER_IRQ_PRIO` Kconfig option.

Initialization
==============

To initialize a timer instance, call the :c:func:`bm_timer_init` function.
Specify the timer mode and provide a user callback function that will be called when the timer expires.

The timer can be initialized in the following modes:

* :c:macro:`BM_TIMER_MODE_SINGLE_SHOT` - The timer expires only once after it is started.
* :c:macro:`BM_TIMER_MODE_REPEATED` - The timer automatically restarts upon expiring until it is stopped.

Usage
*****

After initialization, start the timer by calling the :c:func:`bm_timer_start` function, providing the timeout and user-provided context passed to the callback.
The timeout is provided in ticks.

The library provides macros to convert standard time units to ticks:

* :c:macro:`BM_TIMER_US_TO_TICKS` - Converts microseconds to ticks.
* :c:macro:`BM_TIMER_MS_TO_TICKS` - Converts milliseconds to ticks.

To stop a timer, call the :c:func:`bm_timer_stop` function.

Sample
******

Usage of this library is demonstrated in the :ref:`timer_sample` sample.

Dependencies
************

This library requires one of the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* Clock control - :kconfig:option:`CONFIG_CLOCK_CONTROL`

API documentation
*****************

| Header file: :file:`include/bm/bm_timer.h`
| Source files: :file:`lib/bm_timer/`

:ref:`Timer library API reference <api_bm_timer>`
