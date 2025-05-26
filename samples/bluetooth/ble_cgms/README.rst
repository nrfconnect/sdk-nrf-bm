.. _ble_cgms_sample:

Bluetooth: Continuous Glucose Monitoring Service
################################################

.. contents::
   :local:
   :depth: 2

This Continuous Glucose Monitoring Service sample demonstrates how you can implement the Continuous Glucose Monitoring Service profile using |BMlong|.

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

The Continuous Glocose Monitoring Service (CGMS) is a service that exposes glucose and other data from a personal Continuous Glucose Monitoring sensor.

Programming the S115 SoftDevice
*******************************

.. include:: /includes/softdevice_flash.txt

.. _ble_cgms_sample_testing:

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_cgms/` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

1. Connect the device to the computer to access UART 0.
   If you use a development kit, UART 0 is forwarded as COM ports (Windows) or ttyACM devices (Linux) after you connect the development kit over USB.
#. Connect to the kit with a terminal emulator to the UART. Note that the kit has two UARTs, where only one will output the log.
#. Reset the kit.
#. Observe that the device is advertising under the default name ``nRF_BM_CGMS``.
   You can configure this name using the ``CONFIG_BLE_ADV_NAME`` Kconfig option.
   For information on how to do this, see `Configuring Kconfig`_.
#. Observe that the text "Continuous Glucose Monitoring sample started." is printed on the COM listener running on the computer.
#. Connect to your device using the `nRF Toolbox`_ mobile application with the ``Continuous Glucose`` service.
   You must choose ``All`` devices in the scanner to see the device.
#. Observe that the text "Connected" and "CGMS Start Session" is printed in the COM listener.
#. Observe that you receive the simulated battery status of the kit in the mobile application.
#. Observe that you receive the CGMS records in the mobile application. Default time between each record is one minute.
