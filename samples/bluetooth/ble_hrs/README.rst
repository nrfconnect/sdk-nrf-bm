.. _ble_hrs_sample:

Bluetooth: Heart Rate Service
#############################

.. contents::
   :local:
   :depth: 2

The Heart Rate Service sample demonstrates how you can implement the Heart Rate profile using |BMlong|.

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

When the application starts, three timers are started.
These timers control the generation of various parts of the Heart Rate Measurement characteristic value:

* Heart Rate
* RR Interval
* Sensor Contact Detected

A timer for generating battery measurements is also started.

The sensor measurements are simulated the following way:

* Heart Rate
* RR Interval
* Sensor Contact
* Battery Level

When notification of Heart Rate Measurement characteristic is enabled, the Heart Rate Measurement, containing the current value for all the components of the Heart Rate Measurement characteristic, is notified each time the Heart Rate measurement timer expires.
When notification of Battery Level characteristic is enabled, the Battery Level is notified each time the Battery Level measurement timer expires.

The sample supports bonding with the connected device.
The services security configurations are kept open to allow reading the characteristics without bonding with the device.

User interface
**************

Button 1:
   Keep the button pressed while resetting the board to delete bonding information for all peers stored on the device.

LED 0:
   Lit when the device is initialized.

LED 1:
   Lit when a device is connected.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_hrs/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample using `nRF Connect for Desktop`_ with the `Bluetooth Low Energy app`_ and the `Serial Terminal app`_.
Make sure that these are installed before starting the testing procedure.

1. Compile and program the application.
#. In the Serial Terminal, observe that the ``BLE HRS sample initialized`` message is printed.
#. Observe that the ``Advertising as nRF_BM_HRS`` message is printed.
#. In nRF Connect for Desktop, scan for advertising devices.
   Your device should be advertising as ``nRF_BM_HRS``.
   If the device is not advertising, you might need to use the :guilabel:`Reset Board` option in |VSC|.
#. :guilabel:`Connect` to your device.

   After having connected, your computer or mobile phone may attempt to pair or bond with your device in order to encrypt the link.

   You may be prompted to enter a passkey as part of the authentication step.
   If prompted, provide the passkey from the terminal output.
   The terminal output in |VSC| indicates ``Peer connected``.
#. Observe that the services are shown in the connected device and that you can start receiving values for the Heart Rate and the Battery Service by clicking the Play button.
   Heart Rate notifications are received every second, and Battery Level notifications are received every two seconds.
