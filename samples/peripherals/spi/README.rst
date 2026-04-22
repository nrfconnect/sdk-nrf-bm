.. _bm_spi_sample:

SPI
###

.. contents::
   :local:
   :depth: 2

The SPI sample demonstrates how to configure and use the SPIM and SPIS peripherals with the nrfx drivers.

Requirements
************

The sample supports the following development kits:

.. tabs::

   .. group-tab:: Simple board variants

      The following board variants do **not** have DFU capabilities:

      .. include:: /includes/supported_boards_all_non-mcuboot_variants_s115.txt

      .. include:: /includes/supported_boards_all_non-mcuboot_variants_s145.txt

   .. group-tab:: MCUboot board variants

      The following board variants have DFU capabilities:

      .. include:: /includes/supported_boards_all_mcuboot_variants_s115.txt

      .. include:: /includes/supported_boards_all_mcuboot_variants_s145.txt

Overview
********

The sample initializes the **SPIM** and **SPIS** instances with the pins configured in the board's :file:`board-config.h`.
When **Button 2** is pressed, the master sends a configurable string (:kconfig:option:`CONFIG_SAMPLE_SPI_MSG`) to the slave.
The slave receives the data, logs it, and toggles **LED 2**.

User interface
**************

LED 0
  Lit when the sample is initialized and ready.

LED 2
  Toggles each time the SPI slave completes a reception.

Button 2
  Sends a message from the local SPIM to the connected SPIS.

.. _bm_spi_wiring:

Building and running
********************

This sample can be found under :file:`samples/peripherals/spi/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

After building and flashing the sample, test it as follows:

1. Wire the four SPIM pins to the SPIS pins on the same board, matching each signal by name (SCK→SCK, MOSI→MOSI, MISO→MISO, CSN→CSN).
   The pins are defined in the board's :file:`board-config.h` header (``BOARD_APP_SPIM_*`` and ``BOARD_APP_SPIS_*``).

   .. include:: /includes/spi_board_connections.txt

   .. note:: Board-specific behavior

      * **nRF54L15 DK** — SPI Master pins overlap with **LED 3** (P1.14, flickers during transfers) and **Button 0** (P1.13, do not press during transfers).
      * **nRF54LV10 DK** — The SPI Slave pins (P0.00-P0.03) are shared with one of the debugger's virtual serial ports.
        Before running the sample, open the `Board Configurator`_ app in `nRF Connect for Desktop`_ and disable the **Connect port VCOM** entry that is mapped to pins **P0.00-P0.03**, to release these pins from the debugger.
        Leave the other **Connect port VCOM** entry (mapped to pins **P1.04-P1.07**) enabled, since it is the virtual serial port used to read the sample's log output.

#. Open the console log and verify that ``SPI sample initialized`` appears in the log.
#. Press **Button 2**.
#. Observe that the log shows a master "sent" entry and a slave "received" entry, both containing the string set by the :kconfig:option:`CONFIG_SAMPLE_SPI_MSG` Kconfig option.
#. Observe that **LED 2** toggles on each completed reception.
