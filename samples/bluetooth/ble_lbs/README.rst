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

.. tabs::

   .. group-tab:: Simple board variants

      The following board variants do **not** have DFU capabilities.

      .. list-table::
         :header-rows: 1

         * - Hardware platform
           - PCA
           - SoftDevice
           - Board target
         * - `nRF54L15 DK`_
           - PCA10156
           - S115
           - bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice
         * - `nRF54L15 DK`_ (emulating nRF54L10)
           - PCA10156
           - S115
           - bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice
         * - `nRF54L15 DK`_ (emulating nRF54L05)
           - PCA10156
           - S115
           - bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice

   .. group-tab:: MCUboot board variants

      The following board variants have DFU capabilities.

      .. list-table::
         :header-rows: 1

         * - Hardware platform
           - PCA
           - SoftDevice
           - Board target
         * - `nRF54L15 DK`_
           - PCA10156
           - S115
           - bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice/mcuboot
         * - `nRF54L15 DK`_ (emulating nRF54L10)
           - PCA10156
           - S115
           - bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice/mcuboot
         * - `nRF54L15 DK`_ (emulating nRF54L05)
           - PCA10156
           - S115
           - bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice/mcuboot

Overview
********

The LED Button Service (LBS) is a custom service that controls the state of an LED and sends notifications when a button changes its state.

You can use the sample to transmit the button state from your development kit to another device.

User interface
**************

Button 2:
  Press to send a notification on the LBS Button State characteristic.

LED 0:
  Lit when the device is initialized.

LED 1:
  Lit when a device is connected.

LED 2:
  LED controlled through the LBS LED State characteristic.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_lbs/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample using `nRF Connect for Desktop`_ with the `Bluetooth Low Energy app`_ and the `Serial Terminal app`_.
Make sure that these are installed before starting the testing procedure.

1. Compile and program the application.
#. In the Serial Terminal, observe that the ``BLE LBS sample initialized`` message is printed.
#. Observe that the ``Advertising as nRF_BM_LBS`` message is printed.
#. In nRF Connect for Desktop, scan for advertising devices.
   Your device should be advertising as ``nRF_BM_LBS``.
   If the device is not advertising, you might need to use the :guilabel:`Reset Board` option in |VSC|.
#. :guilabel:`Connect` to your device.
   The terminal output in |VSC| indicates ``Peer connected``.
#. In nRF Connect for Desktop, go to :guilabel:`Nordic LED and Button Service` and change :guilabel:`Blinky LED state` to :guilabel:`01`.
   The terminal output in |VSC| indicates ``Received LED ON!``.
#. Change the :guilabel:`Blinky LED state` value back to :guilabel:`00`.
   The terminal output in |VSC| indicates ``Received LED OFF!``.
