.. _leds_sample:

LEDs
####

.. contents::
   :local:
   :depth: 2

The LEDs sample demonstrates how to configure and use LEDs with |BMlong|.

Requirements
************

The sample supports the following development kits:

.. tabs::

   .. group-tab:: Simple board variants

      The following board variants do **not** have DFU capabilities:

      .. include:: /includes/supported_boards_all_non-mcuboot_variants_s115.txt

      .. include:: /includes/supported_boards_all_non-mcuboot_variants_s145.txt

   .. group-tab:: MCUboot board variants

      The following board variants have DFU capabilities:

      .. include:: /includes/supported_boards_all_mcuboot_variants_s115.txt

      .. include:: /includes/supported_boards_all_mcuboot_variants_s145.txt

Overview
********

The sample initializes and blinks LED0 on the device.

User interface
**************

LED 0:
  Lit when the device is initialized.

LED 2:
  Blinky LED.

Building and running
********************

This sample can be found under :file:`samples/peripherals/leds/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``LEDs sample initialized`` message is printed.
#. Observe that LED0 is blinking.
