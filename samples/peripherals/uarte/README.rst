.. _uarte_sample:

UARTE
#####

.. contents::
   :local:
   :depth: 2

The UARTE sample demonstrates how to configure and use the UARTE peripheral with nrfx drivers.

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

The sample initializes the application UARTE instance, specified in the :file:`board-config.h` file in the board.
It then outputs a message on the UART before echoing the message entered by the user.

Building and running
********************

This sample can be found under :file:`samples/peripherals/uarte/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Open a serial terminal configured for standard 115200 baud rate.
#. Observe that the ``UARTE sample started`` message is printed in one terminal.
#. Observe that the ``Hello world! I will echo the lines you enter:`` message is printed in another terminal.
#. Enter a message in the terminal.
   Observe that you receive the same line in response when pressing Enter (sending ``\r`` or ``\n`` to the device).

.. note::

  If no UART output is visible, you may be connected to the logger (RTT or logging) port instead of the application UART port.
  Try selecting a different serial port for the same device, usually one port lower or higher than the logger port.
  Using `nRF Connect for Desktop`_ with the Serial Terminal application is recommended to easily switch between available ports and send messages.
