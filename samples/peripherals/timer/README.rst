.. _timer_sample:

Timer
#####

.. contents::
   :local:
   :depth: 2

The Timer sample demonstrates how to configure and use the Bare Metal Timer (:ref:`lib_bm_timer`) library to generate both single-shot and periodic callback events to your application code.

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
           - S115
           - bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice/mcuboot
         * - `nRF54L15 DK`_ (emulating nRF54L10)
           - PCA10156
           - S115
           - bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice/mcuboot
         * - `nRF54L15 DK`_ (emulating nRF54L05)
           - PCA10156
           - S115
           - bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice/mcuboot

Overview
********

The single-shot timer is used to sequence a series of actions (logging ``Hello``, ``world!``, and ``bye!``), while the periodic timer provides a visual indication of ongoing activity.

User interface
**************

LED 0:
  Lit when the device is initialized.

Building and running
********************

This sample can be found under :file:`samples/peripherals/timer/` in the |BMshort| folder structure.

For details on how to create, configure and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``Timer sample started`` message is logged.
#. Observe that the ``Timer sample initialized`` message is logged.
#. Observe that the three words ``Hello``, ``world!``, and ``Bye!`` are logged one after another interlaced with period signs that are logged every half second.

   With the default Kconfig options of the sample, the words ``Hello``, ``world!``, and ``Bye!`` are logged after one, three, and seven seconds, respectively.
