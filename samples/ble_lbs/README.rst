.. _ble_lbs_sample:

Bluetooth: LED Button Service
#############################

.. contents::
   :local:
   :depth: 2

This sample demonstrates how to use the LED Button Service (LBS).

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

The LED Button Service (LBS) is a custom service that receives information about the state of an LED and sends notifications when a button changes its state.

You can use the sample to transmit the button state from your development kit to another device.

Building and running
********************

This sample can be found under :file:`samples/ble_lbs/` in the |NCSL| folder structure.

.. include:: /includes/build_sample.txt

.. include:: /includes/program_sample.txt

Programming the S115 SoftDevice
*******************************

.. include:: /includes/softdevice_flash.txt

.. _ble_lbs_sample_testing:

Testing
=======

You can test this sample using `nRF Connect for Desktop`_ with the `Bluetooth Low Energy app`_:

1. Compile and program the application.
#. Observe that the ``BLE LBS sample started`` message is printed.
#. Observe that the ``Advertising as NCS-Lite LBS`` message is printed.
#. In nRF Connect for Desktop, scan for advertising devices.
   Your device should be advertising as ``NCS-Lite LBS``.
   If the device is not advertising, you might need to use the :guilabel:`Reset Board` option in |VSC|.
#. :guilabel:`Connect` to your device.
   The terminal output in |VSC| indicates ``Peer connected``.
#. In nRF Connect for Desktop, go to **Nordic LED and Button Service** and change **Blinky LED state** to ``01``.
   The terminal output in |VSC| indicates ``Received LED ON!``.
#. Change the **Blinky LED state** value back to ``00``.
   The terminal output in |VSC| indicates ``Received LED OFF!``.
