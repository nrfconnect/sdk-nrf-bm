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

Building and running
********************

This sample can be found under :file:`samples/peripherals/lpuarte/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample by performing the following steps:

#. Compile and program the application.
#. Connect the logic analyzer to the shorted pins, to confirm UARTE activity.
   The request/response pins are HIGH in the idle state.
   When a transmission starts, the pins are pulled LOW to signal the start of data transfer.
   At the same time, activity should be visible on the UART RX/TX pins.

   Configure the logic analyzer to use an ``asynchronous serial (UART) protocol`` on the RX/TX
   connection.
   With the default sample configuration, use a baud rate of ``115200``, and verify that a
   ``5-byte message`` is transmitted with the decimal values ``1``, ``2``, ``3``, ``4``, and ``5``.
#. Measure the current to confirm that the power consumption indicates that high-frequency clock is disabled during the idle stage.

During the idle stage, the UARTE receiver is ready to start reception, as the request pin wakes it up.
