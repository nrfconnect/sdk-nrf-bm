.. _ble_hrs_sample:

Bluetooth: Heart Rate Service
#############################

.. contents::
   :local:
   :depth: 2

This Heart Rate Service sample demonstrates how you can implement the Heart Rate profile using |NCSL|.

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

TO BE REVIEWED, roughly based on https://docs.nordicsemi.com/bundle/sdk_nrf5_v17.1.0/page/ble_sdk_app_hrs.html

When the application starts, three timers are started.
These timers control the generation of various parts of the Heart Rate Measurement characteristic value:

* Heart Rate
* RR Interval
* Sensor Contact Detected

A timer for generating battery measurements is also started.

The sensor measurements are simulated the following way: <<TBD add links>>

* Heart Rate: See Sensor Data Simulator.
* RR Interval: See Sensor Data Simulator.
* Sensor Contact: The state is toggled each time the timer expires.
* Battery Level: See Sensor Data Simulator.

When notification of Heart Rate Measurement characteristic is enabled, the Heart Rate Measurement, containing the current value for all the components of the Heart Rate Measurement characteristic, is notified each time the Heart Rate measurement timer expires.
When notification of Battery Level characteristic is enabled, the Battery Level is notified each time the Battery Level measurement timer expires.

Building and running
********************

This sample can be found under :file:`samples/ble_hrs/` in the |NCSL| folder structure.

Programming the S115 SoftDevice
*******************************

The SoftDevice binary is located in :file:`subsys/softdevice/hex/s115` in the |NCSL| folder structure.

You must program the SoftDevice using the command line:

1.
#.

.. _ble_hrs_sample_testing:

Testing
=======

After programming the sample to your development kit, complete the following steps to test it:

1.
#.

Dependencies
************

This sample uses the following |NCS| libraries:

* file:`include/ble_hrs.h`
