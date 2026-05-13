.. _spi_mngr_sample:

SPI manager external memory
###########################

.. contents::
   :local:
   :depth: 2

The SPI manager sample demonstrates how to use the :ref:`lib_bm_spi_mngr` library with |BMlong| to perform non-blocking read, page program, and sector erase operations on the external flash memory of the development kit.

Requirements
************

This sample is designed to run on a development kit with **on-board SPI external flash**, which is configured through the ``BOARD_EXTERNAL_MEMORY_*`` macros in :file:`board-config.h`.
Currently, only the `nRF54L15 DK`_ and `nRF54LM20 DK`_ include on-board external flash and are therefore the supported targets for this sample.

The following board targets match :file:`sample.yaml`:

.. tabs::

   .. group-tab:: Simple board variants

      The following board variants do **not** have DFU capabilities:

      .. include:: /includes/supported_boards_with_external_flash_memory_non-mcuboot_variants_s115.txt

      .. include:: /includes/supported_boards_with_external_flash_memory_non-mcuboot_variants_s145.txt

   .. group-tab:: MCUboot board variants

      The following board variants have DFU capabilities:

      .. include:: /includes/supported_boards_with_external_flash_memory_mcuboot_variants_s115.txt

      .. include:: /includes/supported_boards_with_external_flash_memory_mcuboot_variants_s145.txt

.. note::
   The `nRF54LV10 DK`_ and `nRF54LS05 DK`_ do **not** have on-board external flash memory and are therefore out of scope for this sample.
   However, the :ref:`lib_bm_spi_mngr` library itself is fully supported on these devkits.
   You can still use the SPI manager feature on these boards by connecting your own external SPI slave device (for example, an external NOR flash) to the SPI pins of the SoC and adapting the sample configuration accordingly (pin assignments in :file:`board-config.h`, command set, and timing parameters to match your device).

.. important::
   Before flashing the sample, you must enable **External memory** using the `Board Configurator`_ app in `nRF Connect for Desktop`_, and then write the configuration to the board.
   Without this step, the on-board flash is not powered or routed to the SoC, and the sample will not work.

Overview
********

The sample uses the SPI manager library to schedule read, program, and erase operations on the external flash without blocking the CPU.
All operations target the same flash address (``FLASH_ADDR`` in :file:`main.c`) and share a single SPI manager instance.

The flash is the on-board MX25-class NOR device on the development kit.
Its SPI and strap pins are defined by the ``BOARD_EXTERNAL_MEMORY_*`` macros in :file:`board-config.h`.

User interface
**************

LED 0:
  Lit when the device is initialized.

Button 1:
  Erase external memory.

Button 2:
  Read external memory.

Button 3:
  Program external memory.

Building and running
********************

This sample can be found under :file:`samples/peripherals/spi_mngr/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``SPI manager sample initialized`` message is printed, followed by the list of available buttons and their operations.
#. Press **Button 2** to read the on-board flash.
   The hex dump shows either all ``0xFF`` (erased) or the data last written to the address.
#. Press **Button 3** to program the configured message (set with :kconfig:option:`CONFIG_SAMPLE_SPI_MNGR_MSG`, default ``Hello World!``).
   The log prints ``Programming "Hello World!" @ 0x00100000``.
#. Press **Button 2** again.
   The hex dump now starts with the message bytes, confirming the write succeeded.
   For the default configuration, this is ``48 65 6c 6c 6f 20 57 6f 72 6c 64 21`` (ASCII for ``Hello World!``), followed by ``0xFF`` padding.
#. Power-cycle the board and press **Button 2**.
   The same message reappears, confirming the data persists across power cycles.
#. Press **Button 1** to erase the sector and wait for the self-timed erase to finish.
#. Power-cycle the board again and press **Button 2**.
   The hex dump shows all ``0xFF``, confirming the erase succeeded.
