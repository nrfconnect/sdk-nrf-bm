.. _ble_hrs_sample:

Bluetooth: Heart Rate Service
#############################

.. contents::
   :local:
   :depth: 2

This Heart Rate Service sample demonstrates how you can implement the Heart Rate profile using |BMlong|.

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

Programming the S115 SoftDevice
*******************************

.. include:: /includes/softdevice_flash.txt

.. _ble_hrs_sample_testing:

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_hrs/` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

You can test this sample using `nRF Connect for Desktop`_ with the `Bluetooth Low Energy app`_ and the `Serial Terminal app`.
Make sure that these are installed before starting the testing procedure.

1. Compile and program the application.
#. In the Serial Terminal, observe that the ``BLE HRS sample started`` message is printed.
#. Observe that the ``Advertising as nRF_BM_HRS`` message is printed.
#. In nRF Connect for Desktop, scan for advertising devices.
   Your device should be advertising as ``nRF_BM_HRS``.
   If the device is not advertising, you might need to use the :guilabel:`Reset Board` option in |VSC|.
#. :guilabel:`Connect` to your device.
   The terminal output in |VSC| indicates ``Peer connected``.
#. Observe that the services are shown in the connected device and that you can start receiving values for the Heart Rate and the Battery Service by clicking the 'Play' button.
   Heart Rate notifications are received every second, and Battery Level notifications are received every two seconds.
