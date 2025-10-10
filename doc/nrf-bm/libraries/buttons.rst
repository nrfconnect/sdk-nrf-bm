.. _lib_buttons:

Button handling library
#######################

.. contents::
   :local:
   :depth: 2


Overview
********

The button handling library uses the GPIOTE Handler to detect when a button has been pushed.
To handle debouncing, it will start a timer in the GPIOTE event handler.
The button will only be reported as pushed to the application if the corresponding pin is still active when the timer expires.
If there is a new GPIOTE event while the timer is running, the timer is restarted.

The button handling library uses the ``bm_timer`` module.
The user must ensure that the queue in bm_timer is large enough to hold the :c:func:`bm_timer_stop` and :c:func:`bm_timer_start` function operations which will be executed on each event from the GPIOTE module (2 operations), as well as other ``bm_timer`` operations queued simultaneously in the application.

Configuration
*************

The library is enabled using the Kconfig system.
Set the :kconfig:option:`CONFIG_BM_BUTTONS` Kconfig option to enable the library.

The number of pins available for use by the button handling library is set by the :kconfig:option:`CONFIG_BM_BUTTONS_NUM_PINS` Kconfig option.

Initialization
==============

The button handling library is initialized by calling the :c:func:`bm_buttons_init` function.
The init function takes in an array of button configurations, see the :c:struct:`bm_buttons_config` struct for details.
It also takes in the detection delay time used for the debouncing.
Each button configuration takes a separate :c:type:`bm_buttons_handler_t` function.
The button pin number and state is provided to the event handler on a button event.
Thus, all configurations can use the same event handler.

Usage
*****

After initialization the buttons are enabled by calling the :c:func:`bm_buttons_enable` function.
Likewise, the buttons can be disabled by calling the :c:func:`bm_buttons_disable` function.

Once a button is pressed and the debouncing process succeeds (the button state has not changed during the detection_delay period), the buttons event handler function is called.

While enabled, it is at any time possible to test if a button is pressed by calling the :c:func:`bm_buttons_is_pressed` function.

The button library can be deinitialized by calling the :c:func:`bm_buttons_deinit` function.

Dependencies
************

This library uses the following |BMshort| libraries:

* Timer - :kconfig:option:`CONFIG_BM_TIMER`

API documentation
*****************

| Header file: :file:`include/bm_buttons.h`
| Source files: :file:`lib/bm_buttons/`

:ref:`Button handling library API reference <api_bm_buttons>`
