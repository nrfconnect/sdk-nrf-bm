.. _timer_sample:

Timer
#####

.. contents::
   :local:
   :depth: 2

The Timer sample demonstrates the use of both single-shot and periodic timers in a Zephyr-based application.

Requirements
************

The sample supports the following development kits:

.. list-table::
   :header-rows: 1

   * - Hardware platforms
     - PCA
     - SoftDevice
     - Board target
   * - `nRF54L15 DK`_
     - PCA10156
     - S115
     - nrf54l15dk/nrf54l15/cpuapp

Overview
********

The single-shot timer is used to sequence a series of actions (printing "Hello", "world!", and "bye!\n"), while the periodic timer provides a visual indication of ongoing activity.

Building and running
********************

This sample can be found under :file:`samples/timer/` in the |NCSL| folder structure.

Programming the S115 SoftDevice
*******************************

The SoftDevice binary is located in :file:`subsys/softdevice/hex/s115` in the |NCSL| folder structure.

You must program the SoftDevice using the command line:

1.
#.

.. _timer_sample_testing:

Testing
=======

After programming the sample to your development kit, complete the following steps to test it:

1.
#.

Dependencies
************

This sample uses the following |NCS| libraries:

* file:`include/lite_timer.h`
