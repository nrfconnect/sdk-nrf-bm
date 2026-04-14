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
         * - `nRF54LM20 DK`_
           - PCA10184
           - S115
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s115_softdevice
         * - nRF54LS05 DK
           - PCA10214
           - S115
           - bm_nrf54ls05dk/nrf54ls05b/cpuapp/s115_softdevice
         * - `nRF54LV10 DK`_
           - PCA10188
           - S115
           - bm_nrf54lv10dk/nrf54lv10/cpuapp/s115_softdevice
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
         * - `nRF54LM20 DK`_
           - PCA10184
           - S145
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s145_softdevice
         * - nRF54LS05 DK
           - PCA10214
           - S145
           - bm_nrf54ls05dk/nrf54ls05b/cpuapp/s145_softdevice
         * - `nRF54LV10 DK`_
           - PCA10188
           - S145
           - bm_nrf54lv10dk/nrf54lv10/cpuapp/s145_softdevice

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
         * - `nRF54LM20 DK`_
           - PCA10184
           - S115
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s115_softdevice/mcuboot
         * - `nRF54LV10 DK`_
           - PCA10188
           - S115
           - bm_nrf54lv10dk/nrf54lv10/cpuapp/s115_softdevice/mcuboot
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
         * - `nRF54LM20 DK`_
           - PCA10184
           - S145
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s145_softdevice/mcuboot
         * - `nRF54LV10 DK`_
           - PCA10188
           - S145
           - bm_nrf54lv10dk/nrf54lv10/cpuapp/s145_softdevice/mcuboot


The sample also requires one of the following testing devices:

  * Another development kit with the same sample.
    See :ref:`radio_test_testing_board`.
  * Another development kit connected to a PC with the `RSSI Viewer app`_ (available in the `nRF Connect for Desktop`_).
    See :ref:`radio_test_testing_rssi`.

.. note::
   You can perform the radio test also using a spectrum analyzer.
   This method of testing is not covered by this documentation.

Overview
********

To run the tests, connect to the development kit through the serial port and send shell commands.
Zephyr's :ref:`zephyr:shell_api` module is used to handle the commands.
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

After programming the sample to your development kit, complete the following steps to test it in one of the following two ways:

.. _radio_test_testing_board:

Testing with another development kit
------------------------------------

Complete the following steps:

1. Connect both development kits to the computer using a USB cable.
   The kits are assigned serial ports.
   |serial_port_number_list|
#. |connect_terminal_both_ANSI|
#. Run the following commands on one of the kits:

   a. Set the data rate with the ``data_rate`` command to ``ble_2Mbit``.
   #. Set the transmission pattern with the ``transmit_pattern`` command to ``pattern_11110000``.
   #. Set the radio channel with the ``start_channel`` command to 40.

#. Repeat all steps for the second kit.
#. On both kits, run the ``parameters_print`` command to confirm that the radio configuration is the same on both kits.
#. Set one kit in the Modulated TX Carrier mode using the ``start_tx_modulated_carrier`` command.
#. Set the other kit in the RX Carrier mode using the ``start_rx`` command.
#. Print the received data with the ``print_rx`` command and confirm that they match the transmission pattern (0xF0).

.. _radio_test_testing_rssi:

Testing with the RSSI Viewer app
--------------------------------

Complete the following steps:

1. Connect the kit to the computer using a USB cable.
   The kit is assigned a COM port (Windows) or ttyACM device (Linux), which is visible in the Device Manager.
#. |connect_terminal_ANSI|
#. Set the start channel with the ``start_channel`` command to 20.
#. Set the end channel with the ``end_channel`` command to 60.
#. Set the time on channel with the ``time_on_channel`` command to 50 ms.
#. Set the kit in the TX sweep mode using the ``start_tx_sweep`` command.
#. Start the `RSSI Viewer app`_ and select the kit to communicate with.
#. On the application chart, observe the TX sweep in the form of a wave that starts at 2420 MHz frequency and ends with 2480 MHz.

Dependencies
************

This sample uses the following |NCS| libraries:

  * :ref:`shell_ipc_readme`

This sample has the following nrfx dependencies:

  * :file:`nrfx/drivers/include/nrfx_timer.h`
  * :file:`nrfx/hal/nrf_power.h`
  * :file:`nrfx/hal/nrf_radio.h`

In addition, it uses the following Zephyr libraries:

* :ref:`zephyr:device_model_api`:

   * :file:`drivers/clock_control.h`

* :ref:`zephyr:kernel_api`:

  * :file:`include/init.h`

* :ref:`zephyr:shell_api`:

  * :file:`include/shell/shell.h`
