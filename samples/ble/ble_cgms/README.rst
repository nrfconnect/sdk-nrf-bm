.. _ble_cgms_sample:

Bluetooth: Continuous Glucose Monitoring Service
################################################

.. contents::
   :local:
   :depth: 2

The Continuous Glucose Monitoring Service sample demonstrates how to implement the Continuous Glucose Monitoring Service profile using |BMlong|.

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
     - bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice
   * - `nRF54L15 DK`_ (emulating nRF54L10)
     - PCA10156
     - S115
     - bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice
   * - `nRF54L15 DK`_ (emulating nRF54L05)
     - PCA10156
     - S115
     - bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice


Overview
********

The Continuous Glucose Monitoring Service (CGMS) is a service that exposes glucose and other data from a personal Continuous Glucose Monitoring sensor.

Programming the S115 SoftDevice
*******************************

.. include:: /includes/softdevice_flash.txt

.. _ble_cgms_sample_testing:

Building and running
********************

This sample can be found under :file:`samples/ble/ble_cgms/` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/configure_and_build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

1. Compile and program the application.
#. Connect the device to the computer.
#. Connect to the kit with a terminal emulator (for example, the `Serial Terminal app`_).
   Note that the kit has two UARTs, where only one will output the log.
#. Reset the kit.
#. In the Serial Terminal, observe that the ``Continuous Glucose Monitoring sample started.`` message is printed.
#. Observe that the device is advertising under the default name ``nRF_BM_CGMS``.
   You can configure this name using the ``CONFIG_BLE_ADV_NAME`` Kconfig option.
   For information on how to do this, see `Configuring Kconfig`_.
#. Connect to your device using the `nRF Toolbox`_ mobile application with the :guilabel:`Continuous Glucose` service.
#. Observe that the text ``Connected`` and ``CGMS Start Session`` is printed in the COM listener.
#. Observe that you receive the simulated battery status of the kit in the mobile application.
#. Observe that you receive the CGMS records in the mobile application.
   The default time between each record is one minute.
