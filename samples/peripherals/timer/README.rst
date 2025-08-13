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

   * - Hardware platform
     - PCA
     - Board target
   * - `nRF54L15 DK`_
     - PCA10156
     - bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice
   * - `nRF54L15 DK`_ (emulating nRF54L10)
     - PCA10156
     - bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice
   * - `nRF54L15 DK`_ (emulating nRF54L05)
     - PCA10156
     - bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice

Overview
********

The single-shot timer is used to sequence a series of actions (logging ``Hello``, ``world!``, and ``bye!``), while the periodic timer provides a visual indication of ongoing activity.

Building and running
********************

This sample can be found under :file:`samples/peripherals/timer/` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/configure_and_build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``Timer sample started`` message is logged.
#. Observe that the ``Timers initialized`` message is logged.
#. Observe that the three words ``Hello``, ``world!``, and ``Bye!`` are logged one after another interlaced with period signs that are logged every half second.

   With the default Kconfig options of the sample, the words ``Hello``, ``world!``, and ``Bye!`` are logged after one, three, and seven seconds, respectively.
