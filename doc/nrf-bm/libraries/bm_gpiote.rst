.. _lib_bm_gpiote:

Bare Metal GPIOTE
#################

.. contents::
   :local:
   :depth: 2

The Bare Metal GPIOTE is a library for providing the GPIOTE instances to multiple users.

Overview
********

This library handles initialization, ISR declaration and IRQ connection of the available GPIOTE instances.
The modules allows for multiple users of the GPIOTE instances and lets the user retrieve the correct GPIOTE instance based on the GPIO port.

Configuration
*************

The library is enabled using the Kconfig system.
Set the :kconfig:option:`CONFIG_BM_GPIOTE` Kconfig option to enable the library.

Use the :kconfig:option:`CONFIG_BM_GPIOTE_IRQ_PRIO` Kconfig option to set the GPIOTE priority.

Initialization
==============

The library is initialized automatically on application startup.

.. note::
   The :c:func:`nrfx_gpiote_init` function is called by this module to initialize the GPIOTE instances on application startup.
   There is no deinit function provided.
   Each GPIOTE instance can freely be reinitialized using the nrfx interface directly by the application.
   In such case, care must be taken not to affect other users of the GPIOTE.

Usage
*****

Call the :c:func:`bm_gpiote_instance_get` function to retrieve the GPIOTE instance for the given GPIO port.
Use the instance retrieved with the :ref:`driver_nrfx` GPIOTE interface.

Dependencies
************

* :kconfig:option:`CONFIG_NRFX_GPIOTE`

API documentation
*****************

| Header file: :file:`include/bm/bm_gpiote.h`
| Source files: :file:`lib/bm_gpiote/`

:ref:`Bare Metal GPIOTE library API reference <api_bm_gpiote>`
