.. _bm_ppi_sample:

PPI - Programmable Peripheral Interconnect
##########################################

.. contents::
   :local:
   :depth: 2

The Programmable Peripheral Interconnect (PPI) sample demonstrates how to configure and use PPI with |BMlong|.

Requirements
************

The sample supports the following development kits:

.. tabs::

   .. group-tab:: Simple board variants

      The following board variants do **not** have DFU capabilities:

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

      The following board variants have DFU capabilities:

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

The sample sets up an LED to be toggled when a timer compare event is triggered.

User interface
**************

LED 0:
   Lit when the device is initialized.

LED 1:
   Blinking when the timer is running.

Building and running
********************

This sample can be found under :file:`samples/peripherals/ppi/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Connect to the kit that runs this sample with a terminal emulator (for example, the `Serial Terminal app`_).
#. Reset the kit.
#. In the Serial Terminal, observe that the ``PPI sample started``` message is printed.
#. Observe that **LED 1** is blinking.
