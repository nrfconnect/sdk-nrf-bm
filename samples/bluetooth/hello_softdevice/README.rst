.. _hello_softdevice_sample:

Bluetooth: Hello SoftDevice
###########################

.. contents::
   :local:
   :depth: 2

The Hello SoftDevice sample demonstrates how to enable and disable the SoftDevice on a Nordic Semiconductor DK.

The SoftDevice is a precompiled firmware image implementing the Bluetooth® Low Energy wireless protocol stack.

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

The sample demonstrates the basic management of the SoftDevice in |BMlong|.
It shows how to enable and disable the SoftDevice, handle various types of events related to the SoftDevice, and manage the Bluetooth LE functionality.

User interface
**************

LED 2:
  Lit when the SoftDevice is enabled.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/hello_softdevice/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``Hello SoftDevice sample started`` message is printed.
#. Observe that the ``SoftDevice enabled`` message is printed and **LED 2** is ON.
#. Observe that the ``Bluetooth enabled`` message is printed.
#. Observe that the ``SoftDevice disabled`` message is printed and **LED 2** is OFF.
#. Observe that the ``Bye`` message is printed.
