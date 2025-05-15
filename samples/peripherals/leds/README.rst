.. _led_sample:

LED
###

.. contents::
   :local:
   :depth: 2

The LED sample demonstrates how to configure and use LEDs with |BMlong|.

Requirements
************

The sample supports the following development kits:

.. list-table::
   :header-rows: 1

   * - Hardware platform
     - PCA
     - Board target
   * - `nRF54L15 DK`_
     - PCA10156
     - bm_nrf54l15/nrf54l15/cpuapp

Overview
********

The sample initializes and blinks LED0 on the device.

Building and running
********************

This sample can be found under :file:`samples/peripherals/buttons/` in the |BMshort| folder structure.

.. include:: /includes/build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``LEDs sample started`` message is printed.
#. Observe that LED0 is blinking.
