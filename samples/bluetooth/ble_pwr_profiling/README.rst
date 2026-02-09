.. _ble_pwr_profiling_sample:

Bluetooth: Power Profiling
##########################

.. contents::
   :local:
   :depth: 2

This sample can be used to measure power consumption when Bluetooth® Low Energy is used for communication.
You can measure power consumption during advertising and data transmission.
For this purpose, the sample demonstrates advertising in a connectable and non-connectable mode.

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

Optionally, you can use the `Power Profiler Kit II (PPK2)`_ for power profiling and optimizing your configuration.
You can also use your proprietary solution for measuring the power consumption.

Overview
********

This sample implements the low power mode and the system off mode.

**Low power mode**

The application enters CPU sleep mode when in idle state, which means it has nothing to schedule.
Wake-up events are interrupts triggered by one of the SoC peripheral modules, such as RTC or SystemTick.

**System off mode**

The system is put in system off mode 5 seconds after power up, unless advertising is started, and when the device disconnects from the central device.
This is the deepest power saving mode the system can enter.
In this mode, all core functionalities are powered down, and most peripherals are non-functional or non-responsive.
The only mechanisms that are functional in this mode are reset and wake-up.

To wake up your development kit from the system off state, you have the following options:

* Press the **RESET** button on your development kit.
* Press **Button 2** to start connectable advertising.
* Press **Button 3** to start non-connectable advertising.

The development kit starts advertising automatically when pressing **Button 2** or **Button 3** and goes to **System off mode** when pressing **RESET**.

When you establish a connection with a central device, you can test different connection parameters to optimize the power consumption.
When the central device enables the notification characteristic, your development kit starts sending notifications until the timeout set by the :kconfig:option:`CONFIG_SAMPLE_BLE_PWR_PROFILING_NOTIF_CONNECTION_TIMEOUT` Kconfig option expires.
The device then disconnects from the central and enters the system off mode.
You can press a button to wake up the device and continue testing.

Power profiling
===============

You can use the `Power Profiler Kit II (PPK2)`_ for power profiling with this sample.
See the device documentation for details about preparing your development kit for measurements.

The following parameters have an impact on power consumption:

   * Connection interval
   * Advertising duration
   * Latency
   * Notifications data size and interval

You can configure these parameters with the Kconfig options.
If your central device is the `Bluetooth Low Energy app`_ from `nRF Connect for Desktop`_, you can use it to change the current connection parameters.

Service UUID
============

This sample implements a custom service.

The 128-bit service UUID is ``00001630-1212-EFDE-1523-785FEABCD123``.

Characteristics
===============

The 128-bit characteristic UUID is ``00001631-1212-EFDE-1523-785FEABCD123``.
This characteristic value can be read or sent by the notification mechanism.
The value is an array filled with zeroes, where upon enabling notifications the value of the last byte will increase per notification.
You can configure the length of data using the :kconfig:option:`CONFIG_SAMPLE_BLE_PWR_PROFILING_CHAR_VALUE_LEN` Kconfig option.

This characteristic has a client characteristic configuration descriptor (CCCD) associated with it.

User interface
**************

Button 2:
    Starts connectable advertising and wakes up the SoC from the system off state.

Button 3:
    Starts non-connectable advertising and wakes up the SoC from the system off state.

.. note::
   When you use buttons to wake up the SoC from the system off state, the button state is read in the main thread.
   This causes a delay between the SoC wake up and button press processing.
   If you want to start advertising on system start, you must keep pressing the button until you see a log message confirming the advertising start on the terminal.

LED 0:
  Lit when the device is initialized, if configured.

LED 1:
  Lit when a device is connected, if configured.

.. note::
   The LEDs are disabled by default.
   You can enable LEDs by setting the :kconfig:option:`CONFIG_SAMPLE_BLE_PWR_PROFILING_LED` Kconfig option.
   Enabling the LEDs increases power consumption by ~1.8 µA in active and low-power modes when not connected, and by ~3.7 µA when connected to a device, due to the second LED.

Configuration
*************

The Peripheral Power Profiling sample allows configuring some of its settings using Kconfig.
You can set different options to monitor the power consumption of your development kit.
You can modify the following options (available in the Kconfig file at :file:`samples/bluetooth/ble_pwr_profiling`):

.. _CONFIG_SAMPLE_BLE_PWR_PROFILING_CHAR_VALUE_LEN:

CONFIG_SAMPLE_BLE_PWR_PROFILING_CHAR_VALUE_LEN - Notification data length
   Sets the length of the data sent through notification mechanism.

.. _CONFIG_SAMPLE_BLE_PWR_PROFILING_NOTIF_CONNECTION_TIMEOUT:

CONFIG_SAMPLE_BLE_PWR_PROFILING_NOTIF_CONNECTION_TIMEOUT - Notification timeout
   Sets the notification timeout in milliseconds.
   When this timeout fires the device will stop sending notifications.
   After that, the sample disconnects and enters the system off mode.

.. _CONFIG_SAMPLE_BLE_PWR_PROFILING_CONN_ADVERTISING_TIMEOUT:

CONFIG_SAMPLE_BLE_PWR_PROFILING_CONN_ADVERTISING_TIMEOUT - Connectable advertising timeout
   Sets the connectable advertising duration in N*10 milliseconds unit.
   If the connection is not established during advertising, the device enters the system off state.

.. _CONFIG_SAMPLE_BLE_PWR_PROFILING_CONN_ADVERTISING_INTERVAL:

CONFIG_SAMPLE_BLE_PWR_PROFILING_CONN_ADVERTISING_INTERVAL - Connectable advertising interval
   Sets the connectable advertising interval in 0.625 milliseconds unit.

.. _CONFIG_SAMPLE_BLE_PWR_PROFILING_NONCONN_ADVERTISING_TIMEOUT:

CONFIG_SAMPLE_BLE_PWR_PROFILING_NONCONN_ADVERTISING_TIMEOUT - Non-connectable advertising timeout
   Sets the non-connectable advertising duration in N*10 milliseconds unit.
   When the advertising ends, the device enters the system off state if there is no outgoing connection.

.. _CONFIG_SAMPLE_BLE_PWR_PROFILING_NONCONN_ADVERTISING_INTERVAL:

CONFIG_SAMPLE_BLE_PWR_PROFILING_NONCONN_ADVERTISING_INTERVAL - Non-connectable advertising interval
   Sets the non-connectable advertising interval in 0.625 milliseconds unit.

.. _CONFIG_SAMPLE_BLE_PWR_PROFILING_LED:

CONFIG_SAMPLE_BLE_PWR_PROFILING_LED - Enable LEDs
   Enabled by default. The LEDs can be disabled to reduce power consumption.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_pwr_profiling/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

.. note::
   The sample has logging disabled as default.
   To enable terminal messages (at the cost of a small increase in power consumption), enable the following Kconfig options:
   * :kconfig:option:`CONFIG_LOG`
   * :kconfig:option:`CONFIG_CONSOLE`
   * :kconfig:option:`CONFIG_LOG_BACKEND_BM_UARTE`

Testing
=======

This testing procedure assumes that you are using `nRF Connect for Mobile`_ or `nRF Connect for Desktop`_.
After programming the sample to your development kit, you need another device for measuring the power consumption.
`Power Profiler Kit II (PPK2)`_ is the recommended device for the measurement.

Testing with Bluetooth Low Energy app and Power Profiler Kit II (PPK2)
----------------------------------------------------------------------

1. Set up `Power Profiler Kit II (PPK2)`_ and prepare your development kit for current measurement.
#. Run the `Power Profiler app`_ from nRF Connect for Desktop.
#. Connect to the kit with a terminal emulator (for example, the `Serial Terminal app`_).
#. Reset your development kit.
#. Observe that the sample starts.
   The device enters the system off state after five seconds if advertising is not started.
   Observe a significant power consumption drop when the device enters the system off state.
#. Use the device buttons to wake up your device and start advertising.
   You can measure power consumption during advertising with different intervals.
   Use the Kconfig options to change the interval or other parameters.
#. Connect to the device through the `Bluetooth Low Energy app`_.
#. Set different connection parameters:

   a. Click :guilabel:`Settings` for the connected device in the Bluetooth Low Energy app.
   #. Select :guilabel:`Update connection`.
   #. Set new connection parameters.
   #. Click :guilabel:`Update` to negotiate new parameters with your device.

#. Observe the power consumption with the new connection parameters.
#. In service with UUID :guilabel:`000016301212EFDE1523785FEABCD123`, click :guilabel:`Notify` for the characteristic.
#. Observe that notifications are received.
#. After the timeout set by the :kconfig:option:`CONFIG_SAMPLE_BLE_PWR_PROFILING_NOTIF_CONNECTION_TIMEOUT` option, your development kit disconnects and enters the system off mode.
#. Repeat this test using different wake-up methods and different parameters, and monitor the power consumption for your new setup.
