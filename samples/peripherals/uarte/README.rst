.. _uarte_sample:

UARTE
#####

.. contents::
   :local:
   :depth: 2

The UARTE sample demonstrates how to configure and use the UARTE peripheral with nrfx drivers.

Requirements
************

The sample supports the following development kits:

.. list-table::
   :header-rows: 1

   * - Hardware platform
     - PCA
     - Board target
   * - `nRF54L15 DK`_
     - bm_nrf54l15dk/nrf54l15/cpuapp

Overview
********

The sample initializes the application UARTE instance, specified in the board-config.h file in the board.
It then outputs a message on the uart before echoing what is entered by the user.

Building and running
********************

This sample can be found under :file:`samples/bperipherals/uart/` in the |NCSL| folder structure.

.. include:: /includes/build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``UART sample started`` message is printed in one terminal.
#. Observe that the ``Hello world! I will echo the lines you enter:`` message is printed in another terminal.
#. Enter a message in the terminal.
   Observe that you get the line in response when pressing enter (sending ``\r`` or ``\n`` to the device).
