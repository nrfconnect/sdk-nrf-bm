.. _ble_hrs_central_sample:

Bluetooth: Heart Rate Service Central
#####################################

.. contents::
   :local:
   :depth: 2

The Heart Rate Service Central sample demonstrates how you can implement the Heart Rate profile as a central using |BMlong|.

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
           - S145
           - bm_nrf54l15dk/nrf54l15/cpuapp/s145_softdevice
         * - `nRF54L15 DK`_ (emulating nRF54L10)
           - PCA10156
           - S145
           - bm_nrf54l15dk/nrf54l10/cpuapp/s145_softdevice
         * - `nRF54L15 DK`_ (emulating nRF54L05)
           - PCA10156
           - S145
           - bm_nrf54l15dk/nrf54l05/cpuapp/s145_softdevice

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
           - S145
           - bm_nrf54l15dk/nrf54l15/cpuapp/s145_softdevice/mcuboot
         * - `nRF54L15 DK`_ (emulating nRF54L10)
           - PCA10156
           - S145
           - bm_nrf54l15dk/nrf54l10/cpuapp/s145_softdevice/mcuboot
         * - `nRF54L15 DK`_ (emulating nRF54L05)
           - PCA10156
           - S145
           - bm_nrf54l15dk/nrf54l05/cpuapp/s145_softdevice/mcuboot

Overview
********

When the application starts, three timers are started.
These timers control the generation of various parts of the Heart Rate Measurement characteristic value:

* Heart Rate
* RR Interval
* Sensor Contact Detected

A timer for generating battery measurements is also started.

The following sensor measurements are simulated:

* Heart Rate
* RR Interval
* Sensor Contact
* Battery Level

When the notification of Heart Rate Measurement characteristic is enabled, the Heart Rate Measurement, containing the current value for all the components of the Heart Rate Measurement characteristic, is notified each time the Heart Rate measurement timer expires.
When the notification of Battery Level characteristic is enabled, the Battery Level is notified each time the Battery Level measurement timer expires.

Programming the S115 SoftDevice
*******************************

.. include:: /includes/softdevice_flash.txt

.. _ble_hrs_sample_testing:

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_hrs/` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/configure_and_build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

You can test this sample using `nRF Connect for Desktop`_ with the `Bluetooth Low Energy app`_ and the `Serial Terminal app`_.
Make sure that these are installed before starting the testing procedure.

1. Compile and program the application.
#. In the Serial Terminal, observe that the ``BLE HRS sample started`` message is printed.
#. Observe that the ``Advertising as nRF_BM_HRS`` message is printed.
#. In nRF Connect for Desktop, scan for advertising devices.
   Your device should be advertising as ``nRF_BM_HRS``.
   If the device is not advertising, you might need to use the :guilabel:`Reset Board` option in |VSC|.
#. Click :guilabel:`Connect` to connect your device.
   The terminal output in |VSC| indicates ``Peer connected``.
#. Observe that the services are shown in the connected device and you can start receiving values for the Heart Rate and the Battery Service when you click the :guilabel:`Play` button.
   Heart Rate notifications are received every second, and Battery Level notifications every two seconds.
