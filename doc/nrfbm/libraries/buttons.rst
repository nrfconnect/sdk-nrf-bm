.. _lib_bm_buttons:

Button handling library
#######################

.. contents::
   :local:
   :depth: 2

The button handling library uses the GPIOTE to detect button presses.

Overview
********

The library initiates a timer within the GPIOTE event handler to manage debouncing.
The button is only reported to the application as pressed if the corresponding pin is still active when the timer expires.

Configuration
*************

The library is enabled using the Kconfig system.
Set the :kconfig:option:`CONFIG_BM_BUTTONS` Kconfig option to enable the library.

Use the :kconfig:option:`CONFIG_BM_BUTTONS_NUM_PINS` Kconfig option to configure the number of pins available to the library.

Initialization
==============

Initialize the library by calling the :c:func:`bm_buttons_init` function.
This function requires an array of button configurations (:c:struct:`bm_buttons_config`).
You must also provide the detection delay time used for the debouncing.

Each button configuration is associated with a separate :c:type:`bm_buttons_handler_t` function.
The event handler receives the button pin number and state during a button event.
In this way, all configurations can use the same event handler.

Usage
*****

After initialization, enable the buttons by calling the :c:func:`bm_buttons_enable` function.
They can be disabled by calling the :c:func:`bm_buttons_disable` function.

Once a button is pressed and the debouncing process succeeds (the button state has not changed during the detection delay period), its configured event handler function is triggered.

When a button is enabled, you can call the :c:func:`bm_buttons_is_pressed` function to check whether it is pressed or not.

To deinitialize the library, call the :c:func:`bm_buttons_deinit` function.

Dependencies
************

This library uses the following |BMshort| libraries:

* Timer - :kconfig:option:`CONFIG_BM_TIMER`

API documentation
*****************

| Header file: :file:`include/bm/bm_buttons.h`
| Source files: :file:`lib/bm_buttons/`

:ref:`Button handling library API reference <api_bm_buttons>`
