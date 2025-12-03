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

This sample scans for devices that advertise with the :ref:`lib_ble_service_hrs` UUID (0x180D) and initiates a connection when a device is found.
When a device is connected, the sample starts the service discovery procedure.
If this succeeds, the sample subscribes to the Heart Rate Measurement characteristic to receive heart rate notifications.

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

This sample requires two devices to test, one running this sample and another one running the :ref:`ble_hrs_sample` sample.

Complete the following steps to test the sample:

1. Compile and program the application.
#. In the Serial Terminal, observe that the ``BLE HRS Central sample started`` message is printed.
#. Program the other development kit with the :ref:`ble_hrs_sample` sample and reset it.
#. Observe that the ``Scan filter match`` message is printed, followed by ``Connecting to target`` and ``Connected``.
#. Observe that the ``Heart rate service discovered.`` message is printed.
#. Observe that the device starts receiving heart rate measurement notifications.
