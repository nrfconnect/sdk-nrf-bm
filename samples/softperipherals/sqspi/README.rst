.. _sQSPI_sample:

Software QSPI
#############

.. contents::
   :local:
   :depth: 2

Software QSPI (sQSPI) is an emulation of the Quad Serial Peripheral Interface using the Fast Lightweight Peripheral Processor (FLPR).
The sQSPI sample demonstrates how to use softperipheral quad SPI to perform read, page program, and sector erase operations on the external flash memory of the development kit.

Requirements
************

The sample supports the following development kits with on-board SPI external flash:

.. tabs::

   .. group-tab:: Simple board variants

      The following board variants do not have DFU capabilities:

      **S115**:

      .. list-table::
         :header-rows: 1

         * - Hardware platform
           - PCA
           - Board target
         * - `nRF54L15 DK`_
           - PCA10156
           - ``bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice``
         * - `nRF54L15 DK`_ (emulating nRF54L10)
           - PCA10156
           - ``bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice``
         * - `nRF54L15 DK`_ (emulating nRF54L05)
           - PCA10156
           - ``bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice``
         * - `nRF54LM20 DK`_
           - PCA10184
           - ``bm_nrf54lm20dk/nrf54lm20a/cpuapp/s115_softdevice``


.. note::
   This sample requires the nRF54LM20 DK version 1.0.0 or later.

.. note::
   Other nRF54L Series DKs do not have on-board external flash memory and are out of scope for this sample.
   However, the sQSPI driver itself is supported on some nRF54L Series SoCs (see `VPR pin mapping`_ for the supported SoCs and pins).
   You can still use sQSPI on these boards by connecting an external flash to the supported pins of the SoC and adapting the sample configuration accordingly (pin assignments in the :file:`board-config.h` file, command set, and timing parameters to match your device).

.. important::
   Before flashing the sample, you must enable :guilabel:`External memory` using the `Board Configurator`_ app in `nRF Connect for Desktop`_, and then write the configuration to the board.
   Otherwise, the on-board flash is not routed to the SoC, and the sample will not work.

Overview
********

The sample uses the sQSPI soft peripheral driver to perform read, program, and erase operations on the external flash.
The peripheral logic runs on the `Fast Lightweight Peripheral Processor (FLPR)`_, a dedicated `VPR`_ CPU based on the RISC-V architecture in the fast domain; the application core controls it through the sQSPI driver API.
Reads and page programs use quad SPI, erase and setup commands use single SPI.
All operations target the same flash address (set by the :kconfig:option:`CONFIG_SAMPLE_SQSPI_FLASH_ADDR` Kconfig option).
The flash is the on-board MX25-class NOR device on the development kit.
Its SPI pins and strap pins that configure fixed hardware settings, such as write-protect and reset state at power on, are defined by the ``BOARD_EXTERNAL_MEMORY_*`` macros in the :file:`board-config.h` file.

Configuration
*************

You can modify the following options (available in the Kconfig file at :file:`samples/softperipherals/sqspi/Kconfig`):

* :kconfig:option:`CONFIG_SAMPLE_SQSPI_MSG` - Sets the message written to the external flash on program.
* :kconfig:option:`CONFIG_SAMPLE_SQSPI_FLASH_ADDR` - Sets the external flash address targeted by read, program, and erase.
  Keep it a multiple of 0x1000 (4 KiB sector size) to avoid a wrapped, corrupted write.

User interface
**************

LED 0:
   Lit after flash and button initialization succeeds.

Button 1:
   Erase external memory.

Button 2:
   Read external memory.

Button 3:
   Program external memory.

Building and running
********************

This sample can be found under :file:`samples/softperipherals/sqspi/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample by performing the following steps:

1. Enable :guilabel:`External memory` using the `Board Configurator`_ app in `nRF Connect for Desktop`_, and then write the configuration to the board.
#. Compile and program the application.
#. Observe that the ``sQSPI sample initialized`` message is printed, followed by the list of available buttons and their operations.
#. Press **Button 2** to read the on-board flash.
   The hex dump shows either all ``0xFF`` (erased) or the data last written to the address.
#. Press **Button 3** to program the configured message (set with :kconfig:option:`CONFIG_SAMPLE_SQSPI_MSG`, default ``Hello World!``).
   The log prints ``Flash operation complete``.
#. Press **Button 2** again.
   The hex dump now starts with the message bytes, confirming the write succeeded.
   For the default configuration, this is ``48 65 6c 6c 6f 20 57 6f 72 6c 64 21 00`` (ASCII for ``Hello World!`` followed by the string terminator).
#. Power-cycle the board and press **Button 2**.
   The same message reappears, confirming the data persists across power cycles.
#. Press **Button 1** to erase the sector and wait for the flash's self-timed erase operation to complete.
#. Power-cycle the board again and press **Button 2**.
   The hex dump shows all ``0xFF``, confirming the erase succeeded.
