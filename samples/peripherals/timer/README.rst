.. _timer_sample:

Timer
#####

.. contents::
   :local:
   :depth: 2

The Timer sample demonstrates the use of both single-shot and periodic timers.

Requirements
************

The sample supports the following development kits:

.. list-table::
   :header-rows: 1

   * - Hardware platforms
     - PCA
     - Board target
   * - `nRF54L15 DK`_
     - PCA10156
     - bm_nrf54l15dk/nrf54l15/cpuapp/softdevice_s115
   * - `nRF54L15 DK`_
     - PCA10156
     - bm_nrf54l15dk/nrf54l15/cpuapp/no_softdevice

Overview
********

The single-shot timer is used to sequence a series of actions (printing "Hello", "world!", and "bye!"), while the periodic timer provides a visual indication of ongoing activity.

Building and running
********************

This sample can be found under :file:`samples/peripherals/timer/` in the |BMshort| folder structure.

.. include:: /includes/build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``Timer sample started`` message is printed.
#. Observe that the ``Timers initialized`` message is printed.
#. Observe that the three words ``Hello``, ``world!``, and ``Bye!`` are printed one after another interlaced with period signs that are printed every half second.

   With the default Kconfig options of the sample, the words ``Hello``, ``world!``, and ``Bye!`` are printed after one, three, and seven seconds, respectively.
