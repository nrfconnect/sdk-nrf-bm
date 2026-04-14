.. _radio_test:

Radio test
########################

.. contents::
   :local:
   :depth: 2

The Radio test sample demonstrates how to configure the 2.4 GHz short-range radio (Bluetooth® LE, IEEE 802.15.4 and proprietary) in a specific mode and then test its performance.
The sample provides a set of predefined commands that allow you to configure the radio in three modes:

* Constant RX or TX carrier
* Modulated TX carrier
* RX or TX sweep

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


The sample also requires one of the following testing devices:

  * Another development kit with the same sample.
  * Another development kit connected to a PC with the `RSSI Viewer app`_ (available in the `nRF Connect for Desktop`_).

.. note::
   You can perform the radio test also using a spectrum analyzer.
   This method of testing is not covered by this documentation.

Overview
********

To run the tests, connect to the development kit through the serial port and send shell commands.
The shell subsystem is used to handle the commands.
At any time during the tests, you can dynamically set the radio parameters, such as output power, bit rate, and channel.
In sweep mode, you can set the time for which the radio scans each channel from one millisecond to 99 milliseconds, in steps of one millisecond.
The sample also allows you to send a data pattern to another development kit.

The sample first enables the high frequency crystal oscillator and configures the shell.
You can then start running commands to set up and control the radio.
See :ref:`radio_test_ui` for a list of available commands.

.. note::
   For the IEEE 802.15.4 mode, the start channel and the end channel must be within the channel range of 11 to 26.
   Use the ``start_channel`` and ``end_channel`` commands to control this setting.

.. _radio_test_ui:

User interface
**************

.. list-table:: Main shell commands (in alphabetical order)
   :header-rows: 1

   * - Command
     - Argument
     - Description
   * - cancel
     -
     - Cancel the sweep or the carrier.
   * - data_rate
     - <sub_cmd>
     - Set the data rate.
   * - end_channel
     - <channel>
     - End channel for the sweep (in MHz, as difference from 2400 MHz).
   * - output_power
     - <sub_cmd>
     - Output power set.
   * - parameters_print
     -
     - Print current delay, channel, and other parameters.
   * - print_rx
     -
     - Print the received RX payload.
   * - start_channel
     - <channel>
     - Start channel for the sweep or the channel for the constant carrier (in MHz, as difference from 2400 MHz).
   * - start_duty_cycle_modulated_tx
     - <duty_cycle>
     - Duty cycle as a percentage (two decimal digits, ranging from 01 to 90).
   * - start_rx
     - <packet_num>
     - Start RX (continuous RX mode is used if no argument is provided).
   * - start_rx_sweep
     -
     - Start the RX sweep.
   * - start_tx_carrier
     -
     - Start the TX carrier.
   * - start_tx_modulated_carrier
     - <packet_num>
     - Start the modulated TX carrier (continuous TX mode is used if no argument is provided).
   * - start_tx_sweep
     -
     - Start the TX sweep.
   * - time_on_channel
     - <time>
     - Time on each channel in ms (between 1 and 99).
   * - toggle_dcdc_state
     - <state>
     - Toggle DC/DC converter state.
   * - transmit_pattern
     - <sub_cmd>
     - Set transmission pattern.


Building and running
********************

This sample can be found under :file:`samples/peripherals/radio_test/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

After programming the sample to your development kit, complete the following steps to test it:

.. tabs::

   .. group-tab:: Testing with another development kit

      1. Connect both development kits to the computer using a USB cable.
         The kits are assigned COM ports (Windows) or ttyACM devices (Linux), which are visible in the Device Manager.
      #. Connect to both kits with a terminal emulator that supports VT100/ANSI escape characters (for example, the `Serial Terminal app`_).
      #. Run the following commands on one of the kits:

         a. Set the data rate with the ``data_rate`` command to ``ble_2Mbit``.
         #. Set the transmission pattern with the ``transmit_pattern`` command to ``pattern_11110000``.
         #. Set the radio channel with the ``start_channel`` command to 40.

      #. Repeat all steps for the second kit.
      #. On both kits, run the ``parameters_print`` command to confirm that the radio configuration is the same on both kits.
      #. Set one kit in the Modulated TX Carrier mode using the ``start_tx_modulated_carrier`` command.
      #. Set the other kit in the RX Carrier mode using the ``start_rx`` command.
      #. Print the received data with the ``print_rx`` command and confirm that they match the transmission pattern (0xF0).

   .. group-tab:: Testing with the RSSI Viewer app

      1. Connect the kit to the computer using a USB cable.
         The kit is assigned a COM port (Windows) or ttyACM device (Linux), which is visible in the Device Manager.
      #. Open a serial port connection to the kit using a terminal emulator that supports VT100/ANSI escape characters (for example, the `Serial Terminal app`_).
      #. Set the start channel with the ``start_channel`` command to 20.
      #. Set the end channel with the ``end_channel`` command to 60.
      #. Set the time on channel with the ``time_on_channel`` command to 50 ms.
      #. Set the kit in the TX sweep mode using the ``start_tx_sweep`` command.
      #. Start the `RSSI Viewer app`_ and select the kit to communicate with.
      #. On the application chart, observe the TX sweep in the form of a wave that starts at 2420 MHz frequency and ends with 2480 MHz.
