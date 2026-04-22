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

The sample initializes the **SPIM** and **SPIS** instances with the pins configured in the :file:`board-config.h` header.
When **Button 2** is pressed, a configurable string (:kconfig:option:`CONFIG_SAMPLE_SPI_MSG`) is sent from the local SPI controller to a connected SPI target.
The local SPI target receives data from a connected SPI controller, logs the received data and toggles **LED 2**.

Wiring
******

The sample requires the following pin wiring:

Single-device loopback setup:
   Wire the four SPIM pins to the SPIS pins on the same board, matching each signal by name (SCK→SCK, MOSI→MOSI, MISO→MISO, CSN→CSN).

Two-device setup:
   Wire the four SPIM pins on device 1 to the SPIS pins on device 2, matching each signal by name (SCK→SCK, MOSI→MOSI, MISO→MISO, CSN→CSN).
   Optionally, wire the four SPIS pins on device 1 to the SPIM pins on device 2 to test SPIM and SPIS on both devices.

.. note::

   The sample configures the SPIM peripheral with an extra delay between chip select (CSN) and the first SCK edge (see the :kconfig:option:`CONFIG_SAMPLE_SPI_CSN_TO_CLK_DELAY` Kconfig option).
   The delay is required when talking to an SPI target which needs more time to wake up, like the SPIS peripheral on nRF54L devices, where the high-frequency clock is powered down during sleep and must be running before data can be transferred correctly.
   For more information, see the SPIS peripheral and electrical specification chapters in the product specification of your SoC.

.. include:: /includes/spi_board_connections.txt

.. note:: Board-specific behavior

   * **nRF54L15 DK** — The SPI controller pins overlap with **LED 3** (P1.14, flickers during transfers) and **Button 0** (P1.13, do not press during transfers).
   * **nRF54LV10 DK** — The SPI target pins (P0.00-P0.03) are shared with one of the debugger's virtual serial ports.
     Before running the sample, open the `Board Configurator`_ app in `nRF Connect for Desktop`_ and disable the **Connect port VCOM** entry that is mapped to pins **P0.00-P0.03**, to release these pins from the debugger.
     Leave the other **Connect port VCOM** entry (mapped to pins **P1.04-P1.07**) enabled, since it is the virtual serial port used to read the sample's log output.

User interface
**************

LED 0:
   Lit when the device is initialized.

LED 2:
   Toggles when the SPIS peripheral completes a reception.

Button 2:
   Send string from the local SPIM to a connected SPI target.

Configuration
*************

You can modify the following options (available in the Kconfig file at :file:`samples/peripherals/spi`):

* :kconfig:option:`CONFIG_SAMPLE_SPI_MSG` - Set the message that the SPI controller sends to the SPI target when **Button 2** is pressed.

* :kconfig:option:`CONFIG_SAMPLE_SPI_RX_BUF_SIZE` - Set the size, in bytes, of the buffer used by the SPI target to receive data.

* :kconfig:option:`CONFIG_SAMPLE_SPI_CSN_TO_CLK_DELAY` - Set the delay, in SPIM core clock periods, between chip select (CSN) going active and the first SCK edge.
  The required delay depends on how long the connected SPI target takes to wake up and become ready to receive data, which varies between devices.
  Refer to the electrical specification chapter of the SPI target device's datasheet for its wake-up time.

Building and running
********************

This sample can be found under :file:`samples/peripherals/spi/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test the sample in two ways, depending on the selected wiring: the single-device loopback setup or the two-device setup.

.. tabs::

   .. group-tab:: Single-device loopback

      Test the sample on one development kit, with its SPI controller wired to its own SPI target.

      1. Wire the SPIM pins to the SPIS pins on the same kit, as described in `Wiring`_.
      #. Compile and program the application.
      #. Connect to the kit with a terminal emulator (for example, the `Serial Terminal app`_).
      #. Observe that the ``SPI sample initialized`` message is printed.
      #. Press **Button 2**.
      #. Observe that the terminal prints ``Message sent`` followed by the string set by the :kconfig:option:`CONFIG_SAMPLE_SPI_MSG` Kconfig option (``Hello World!`` by default).
      #. Observe that the terminal prints ``Message received``, the number of received bytes, and the same string.
      #. Observe that **LED 2** toggles.

   .. group-tab:: Two-device setup

      Test the sample on two development kits, with the SPI controller on device 1 wired to the SPI target on device 2.

      1. Wire device 1 and device 2 as described in `Wiring`_.
      #. Compile and program the application on both kits.
      #. Connect to both kits with a terminal emulator (for example, the `Serial Terminal app`_).
         In the following steps, these are referred to as the **device 1 terminal** and the **device 2 terminal**.
      #. Observe that both terminals print the ``SPI sample initialized`` message.
      #. Press **Button 2** on device 1.
      #. Observe that the device 1 terminal prints ``Message sent`` followed by the string set by the :kconfig:option:`CONFIG_SAMPLE_SPI_MSG` Kconfig option (``Hello World!`` by default).
      #. Observe that the device 2 terminal prints ``Message received``, the number of received bytes, and the same string.
      #. Observe that **LED 2** toggles on device 2.
      #. If you also wired the second data path, press **Button 2** on device 2 and repeat the previous three checks with the roles of the two devices reversed.
