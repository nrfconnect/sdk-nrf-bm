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
     - Board target
   * - `nRF54L15 DK`_
     - PCA10156
     - bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice
   * - `nRF54L15 DK`_ (emulating nRF54L10)
     - PCA10156
     - bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice
   * - `nRF54L15 DK`_ (emulating nRF54L05)
     - PCA10156
     - bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice

Building and running
********************

This sample can be found under :file:`samples/peripherals/buttons/` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/configure_and_build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``Buttons sample started`` message is printed.
#. Observe that the ``Buttons initialized, press Button 3 to terminate`` message is printed.
#. Press the buttons on the development kit.
   Observe the application printing a message when the buttons are pressed.
#. Press Button 3 on the development kit to terminate the application.
