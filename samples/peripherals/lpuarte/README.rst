.. _bm_lpuarte_sample:

LPUARTE
#######

.. contents::
   :local:
   :depth: 2

The LPUARTE sample demonstrates how to configure and use the LPUARTE driver as a low power alternative to regular UART.

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

The sample also requires the following pins to be shorted:

   .. list-table:: Pin connections.
      :widths: auto
      :header-rows: 1

      * - Development Kit
        - nRF54L15 DK pins
      * - Request-Response Pins
        - P1.08-P1.09
      * - UART RX-TX Pins
        - P1.10-P1.11

Additionally, it requires a logic analyzer.

Overview
********

The sample initializes the application LPUARTE instance, specified in the :file:`board-config.h` file in the board.
It then implements a simple loopback using a single LPUARTE instance.
By default, the console and logging are disabled to demonstrate low power consumption when UART is active.

User interface
**************

LED 0:
  If configured, lit when the device is initialized. (Configurable using the :kconfig:option:`CONFIG_SAMPLE_LPUARTE_INIT_LED` kconfig option)

Building and running
********************

This sample can be found under :file:`samples/peripherals/lpuarte/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
2. Connect the logic analyzer to the shorted pins, to confirm UARTE activity.
3. Measure the current to confirm that the power consumption indicates that high-frequency clock is disabled during the idle stage.

During the idle stage, the UARTE receiver is ready to start reception, as the request pin wakes it up.
