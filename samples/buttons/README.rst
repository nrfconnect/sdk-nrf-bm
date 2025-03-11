.. _buttons_sample:

Buttons
#######

.. contents::
   :local:
   :depth: 2

The Buttons sample demonstrates how to configure and use buttons with event handling on the nRF54L15 DK.

Requirements
************

The sample supports the following development kits:

.. list-table::
   :header-rows: 1

   * - Hardware platform
     - PCA
     - SoftDevice
     - Board target
   * - `nRF54L15 DK`_
     - PCA10156
     - S115
     - nrf54l15dk/nrf54l15/cpuapp

Overview
********

1. The sample function starts by logging the start of the application and instructs the user to press Button 4 (connected to ``PIN_BTN_3``) to terminate the program.
#. It initializes the running variable to ``true``.
#. An array of ``struct lite_buttons_config`` is defined for configuring each button.
   Each configuration specifies:

     * The pin number.
     * The active state (active low in this case).
     * The pull configuration (pull-up resistor enabled).
     * The handler function to call on a button event.

#. The buttons are initialized with these configurations using ``lite_buttons_init()``.
#. The buttons are then enabled with ``lite_buttons_enable()``.
#. The ``__WFE()`` (Wait For Event) instruction is used to put the processor in a low-power state until an event occurs (like a button press).
#. Once running is set to false (when Button 3 is pressed), the buttons are deinitialized with ``lite_buttons_deinit()``.

Building and running
********************

This sample can be found under :file:`samples/buttons/` in the |NCSL| folder structure.

Programming the S115 SoftDevice
*******************************

The SoftDevice binary is located in :file:`subsys/softdevice/hex/s115` in the |NCSL| folder structure.

You must program the SoftDevice using the command line:

1.
#.

.. _buttons_sample_testing:

Testing
=======

After programming the sample to your development kit, complete the following steps to test it:

1.
#.

Dependencies
************

This sample uses the following |NCS| libraries:

* file:`include/lite_buttons.h`
