.. _hello_softdevice_sample:

Bluetooth: Hello SoftDevice
###########################

.. contents::
   :local:
   :depth: 2

The Hello SoftDevice sample demonstrates how to enable and disable the SoftDevice on a Nordic DK.

The SoftDevice is a precompiled and linked binary software implementing the Bluetooth Low Energy wireless protocol stack.

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
     - bm_nrf54l15dk/nrf54l15/cpuapp/softdevice_s115
   * - `nRF54L15 DK`_ (emulating nRF54L10)
     - PCA10156
     - S115
     - bm_nrf54l15dk/nrf54l10/cpuapp/softdevice_s115
   * - `nRF54L15 DK`_ (emulating nRF54L05)
     - PCA10156
     - S115
     - bm_nrf54l15dk/nrf54l05/cpuapp/softdevice_s115

Overview
********

The sample demonstrates the basic management of the SoftDevice in |BMlong|.
It shows how to enable and disable the SoftDevice, handle various types of events related to the SoftDevice, and manage Bluetooth LE functionality.

Programming the S115 SoftDevice
*******************************

.. include:: /includes/softdevice_flash.txt

Building and running
********************

This sample can be found under :file:`samples/bluetooth/hello_softdevice/` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``Hello SoftDevice sample started`` message is printed.
#. Observe that the ``SoftDevice enabled`` message is printed.
#. Observe that the ``Bluetooth enabled`` message is printed.
#. Observe that the ``SoftDevice disabled`` message is printed.
#. Observe that the ``Bye`` message is printed.
