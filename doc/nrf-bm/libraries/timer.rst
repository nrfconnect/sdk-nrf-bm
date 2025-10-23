.. _lib_bm_timer:

Timer library
#############

.. contents::
   :local:
   :depth: 2

The timer library allows the application to create multiple timer instances.

Functions such as starting and stopping timers, checking for timeouts, and invoking user-defined timeout handlers are all managed within the GRTC interrupt handler.

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

The timer library is using the GRTC peripheral, available in all power modes including ``System OFF``.
The GRTC peripheral has a resolution of 1 microsecond.

.. note::

   The timer accuracy is reduced in low power mode.
   The low frequency clock source, running 32.768 kHz, provides a clock tick approximately every 30.5 microseconds.

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
