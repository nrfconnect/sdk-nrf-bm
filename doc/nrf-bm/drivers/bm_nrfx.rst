.. _driver_nrfx:

nrfx
####

The |BMshort| is set up to use nrfx that comes with the |NCS|.

In |BMshort|, it is expected that the application uses either the nrfx driver or, if needed, interacts directly with the peripheral register interface (nrf hal).

To learn more, see the `nrfx documentation`_ that is distributed as a separate bundle.
The nrfx source code is located in :file:`../modules/hal/nordic/nrfx`.

.. note::

   The CMake in |BMshort| is already set up to include the source code from nrfx.

To include and start using the nrfx driver:

 * Include the interface file, like :file:`#include <nrfx_spim.h>` for the SPI Master driver.
 * Start adding code in your application to set up, initialize, and start using the driver.

To start interacting directly with a peripheral through the register interface:

* Include the interface file, like :file:`#include <hal/nrf_gpio.h>` for GPIO.
* Start adding code interacting with the register interface (``nrf_gpio_pin_write``).
