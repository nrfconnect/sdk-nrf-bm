.. _lib_bm_gpiote:

Bare Metal GPIOTE
#################

.. contents::
   :local:
   :depth: 2

The Bare Metal GPIOTE is a lightweight library to allow multiple users to share GPIOTE instances.

Overview
********

This library holds the nrfx GPIOTE instance data and handles initialization, ISR declaration, and IRQ connection of the available nrfx GPIOTE instances.
The library allows for multiple users of the GPIOTE instances and lets them retrieve GPIOTE instances based on GPIO port numbers.

.. note::
   You are expected to use the :ref:`driver_nrfx` GPIOTE interface directly for all operations except declaring and initializing the GPIOTE instances themselves.

Configuration
*************

The library is enabled using the Kconfig system.
Set the :kconfig:option:`CONFIG_BM_GPIOTE` Kconfig option to enable the library.

Use the :kconfig:option:`CONFIG_BM_GPIOTE_IRQ_PRIO` Kconfig option to set the GPIOTE interrupt priority.

Set the :kconfig:option:`CONFIG_NRFX_GPIOTE_NUM_OF_EVT_HANDLERS` Kconfig option to match the number of simultaneous GPIOTE event handlers.
The number of handlers applies for each GPIOTE instance and must account for all users, including |BMshort| drivers and libraries.
To reduce the number of handlers required, it is recommended to use the same ``nrfx_gpiote_handler_config_t`` event handler and context for multiple pins on the same port if applicable.

Initialization
==============

The library is initialized automatically on application startup.

.. note::
   The :c:func:`nrfx_gpiote_init` function is called by this library to initialize the GPIOTE instances on application startup.
   The library does not provide any functionality for deinitializing the GPIOTE instances.
   Each nrfx GPIOTE instance can be reinitialized using the nrfx interface.
   In such case, make sure not to affect other users of the GPIOTE.

Usage
*****

Call the :c:func:`bm_gpiote_instance_get` function to retrieve a pointer to the GPIOTE instance for the given GPIO port.
Use the instance retrieved with the :ref:`driver_nrfx` GPIOTE API.

The following sample shows how to set up an interrupt on the rising edge of a pin with GPIOTE:

.. code-block:: c

   #include <bm/bm_gpiote.h>
   #include <nrfx_gpiote.h>

   static void gpiote_evt_handler(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t action, void *ctx)
   {
      printk("Event on pin %d\n", pin);
   }

   static const nrf_gpio_pin_pull_t pull_config = NRF_GPIO_PIN_PULLUP;
   static const nrfx_gpiote_handler_config_t handler_config = {
      .handler = gpiote_evt_handler,
   };
   static const nrfx_gpiote_trigger_config_t trigger_config = {
      .trigger = NRFX_GPIOTE_TRIGGER_LOTOHI,
   };
   static const nrfx_gpiote_input_pin_config_t input_config = {
      .p_pull_config = &pull_config,
      .p_trigger_config = &trigger_config,
      .p_handler_config = &handler_config,
   };
   static nrfx_gpiote_pin_t pin;

   static nrfx_gpiote_t *gpiote_inst = bm_gpiote_instance_get(NRF_PIN_NUMBER_TO_PORT(pin));

   err = nrfx_gpiote_input_configure(gpiote_inst, pin, input_config);
   if (err) {
      LOG_ERR("nrfx_gpiote_input_configure failed, err %d", err);
      return err;
   }

For further details on the usage of the GPIOTE peripheral, see the :ref:`driver_nrfx` GPIOTE API.

Dependencies
************

* :kconfig:option:`CONFIG_NRFX_GPIOTE`

API documentation
*****************

| Header file: :file:`include/bm/bm_gpiote.h`
| Source files: :file:`lib/bm_gpiote/`

:ref:`Bare Metal GPIOTE library API reference <api_bm_gpiote>`
