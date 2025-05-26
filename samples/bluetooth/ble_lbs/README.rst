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

The LED Button Service (LBS) is a custom service that receives information about the state of an LED and sends notifications when a button changes its state.

You can use the sample to transmit the button state from your development kit to another device.

Programming the S115 SoftDevice
*******************************

.. include:: /includes/softdevice_flash.txt

.. _ble_lbs_sample_testing:


Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_lbs/` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

You can test this sample using `nRF Connect for Desktop`_ with the `Bluetooth Low Energy app`_ and the `Serial Terminal app`.
Make sure that these are installed before starting the testing procedure.

1. Compile and program the application.
#. In the Serial Terminal, observe that the ``BLE LBS sample started`` message is printed.
#. Observe that the ``Advertising as nRF_BM_LBS`` message is printed.
#. In nRF Connect for Desktop, scan for advertising devices.
   Your device should be advertising as ``nRF_BM_LBS``.
   If the device is not advertising, you might need to use the :guilabel:`Reset Board` option in |VSC|.
#. :guilabel:`Connect` to your device.
   The terminal output in |VSC| indicates ``Peer connected``.
#. In nRF Connect for Desktop, go to **Nordic LED and Button Service** and change **Blinky LED state** to ``01``.
   The terminal output in |VSC| indicates ``Received LED ON!``.
#. Change the **Blinky LED state** value back to ``00``.
   The terminal output in |VSC| indicates ``Received LED OFF!``.
