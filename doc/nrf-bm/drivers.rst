.. _drivers:

Drivers
#######

|BMlong| provides drivers for typical use cases with Nordic Semiconductor devices.
This will mostly be more higher-level drivers and should be seen as a supplement to nrfx.

.. note::

   Only the drivers located under :file:`nrf-bm/drivers` and the ones provided in nrfx are in the scope of the |BMshort|.
   All other drivers that are included in the distribution must be ignored when working with the Bare Metal option.

Pre-allocated resources
***********************

Some peripherals will be pre-allocated by the system and must not be accessed directly by the application.

By default, |BMshort| initializes a system timer that is used by both the :ref:`lib_bm_timer` and logging timestamp.
This will allocate ``GRTC`` channel ``0`` and ``GRTC_2_IRQn``.
The initialization starts ``GRTC`` and enables ``SYSCOUNTER``, as required by the SoftDevice.

When SoftDevice is enabled, it takes the ownership of the peripherals that it needs.
They will not be available for application to use.
The SoftDevice is reusing both the `SoftDevice Controller`_ and the `Multiprotocol Service Layer`_ from the nRF Connect SDK.
See the Integration Notes documentation for these libraries to get an overview of the resource it will be using.

For details on SoftDevice behavior, see the `S115 SoftDevice Specification`_.

nrfx
****

The |BMshort| is set up to use nrfx that comes with the nRF Connect SDK.

In |BMshort|, it is expected that the application uses either the nrfx driver or, if needed, interacts directly with the peripheral register interface (nrf hal).

To learn more, see the `nrfx documentation`_ that is distributed as separate bundle.
The nrfx source code is located in :file:`../modules/hal/nordic/nrfx`.

.. note::

   The CMake in Bare Metal option is already set up to include the source code from nrfx.

To include and start using the nrfx driver:

 * Include the interface file, like :file:`#include <nrfx_spim.h>` for the SPI Master driver.
 * Start adding code in your application to set up, initialize and start using the driver.

To start interacting directly with a peripheral through the register interface:

* Include the interface file, like :file:`#include <hal/nrf_gpio.h>` for GPIO.
* Start adding code interacting with the register interface (:file:`nrf_gpio_pin_write(….)` ).

.. toctree::
   :maxdepth: 1
   :glob:
   :caption: Subpages:

   drivers/*
