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

The sample initializes the application UARTE instance, specified in the :file:`board-config.h` file in the board.
It then outputs a message on the UART before echoing the message entered by the user.

User interface
**************

LED 0:
  Lit when the device is initialized.

Building and running
********************

This sample can be found under :file:`samples/peripherals/uarte/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Connect to the kit with a terminal emulator (for example, the `Serial Terminal app`_).
   Note that the kit has two UARTs, where one will output the log and the other is used for the sample UARTE instance.
#. Observe that the ``UARTE sample initialized`` message is printed in one terminal.
#. Observe that the ``Hello world! I will echo the lines you enter:`` message is printed in another terminal.
#. Enter a message in the terminal.
   Observe that you receive the same line in response when pressing Enter (sending ``\r`` or ``\n`` to the device).
