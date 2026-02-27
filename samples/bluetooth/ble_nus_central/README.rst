.. _ble_nus_central_sample:

Bluetooth: Nordic UART Service Central
######################################

.. contents::
   :local:
   :depth: 2

This sample demonstrates how to use the Nordic UART Service (NUS) Central.
It uses the NUS central service to send data back and forth between a UART connection and a Bluetooth® LE connection, emulating a serial port over Bluetooth LE.

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

This sample scans for devices that advertise with the :ref:`lib_ble_service_nus` correct UUID and initiates a connection when a device is found.
When a device is connected, the sample starts the service discovery procedure.
If this succeeds, the sample enables peer tx notifications to receive data from the peer.

.. _ble_nus_central_sample_testing:

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_nus_central/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

This sample requires two devices to test, one running this sample and another one running the :ref:`ble_nus_sample` sample.

Complete the following steps to test the sample:

1. Compile and program the application.
#. In the Serial Terminal, observe that the ``BLE NUS central example started.`` message is printed.
#. Program the other development kit with the :ref:`ble_nus_sample` sample and reset it.
#. Observe that the ``Connecting to target`` message is printed.
#. Observe that the ``Discovery complete.`` message is printed.
#. Observe that the ``Connected to device with Nordic UART Service.`` message is printed.
#. Send a message using the serial monitor and observe that it is received correctly by the peer.
