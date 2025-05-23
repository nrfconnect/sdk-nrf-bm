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

.. list-table::
   :header-rows: 1

   * - Hardware platform
     - PCA
     - Board target
   * - `nRF54L15 DK`_
     - PCA10156
     - bm_nrf54l15dk/nrf54l15/cpuapp/no_softdevice
   * - `nRF54L15 DK`_ (emulating nRF54L10)
     - PCA10156
     - bm_nrf54l15dk/nrf54l10/cpuapp/no_softdevice
   * - `nRF54L15 DK`_ (emulating nRF54L05)
     - PCA10156
     - bm_nrf54l15dk/nrf54l05/cpuapp/no_softdevice

Overview
********

The sample initializes and blinks LED0 on the device.

Building and running
********************

This sample can be found under :file:`samples/peripherals/buttons/` in the |BMshort| folder structure.

.. include:: /includes/create_build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``LEDs sample started`` message is printed.
#. Observe that LED0 is blinking.
