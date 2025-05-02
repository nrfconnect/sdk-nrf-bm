.. _ble_nus_sample:

Bluetooth: Nordic UART Service (NUS)
####################################

.. contents::
   :local:
   :depth: 2

This sample demonstrates how to use the Nordic UART Service (NUS).
It uses the NUS service to send data back and forth between a UART connection and a Bluetooth LE connection, emulating a serial port over Bluetooth LE.

Requirements
************

The sample supports the following development kits:

.. list-table::
   :header-rows: 1

   * - Hardware platform
     - PCA
     - SoftDevice
     - Board target
   * - `nRF54L15 DK`_
     - PCA10156
     - S115
     - lite_nrf54l15/nrf54l15/cpuapp

Overview
********

When connected, the sample forwards any data received on the RX pin of the UART 0 peripheral to the Bluetooth LE unit.

Any data sent from the Bluetooth LE unit is sent out of the UART 0 peripheral’s TX pin.

Programming the S115 SoftDevice
*******************************

.. include:: /includes/softdevice_flash.txt

.. _ble_nus_sample_testing:

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_nus/` in the |NCSL| folder structure.

.. include:: /includes/build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

1. Connect the device to the computer to access UART 0 and UART 1.
   If you use a development kit, UART 0 and 1 are forwarded as COM ports (Windows) or ttyACM devices (Linux) after you connect the development kit over USB.
#. Connect to the kit with a terminal emulator to both UARTs.
#. Reset the kit.
#. Observe that LED 0 is blinking and the device is advertising under the default name ``NCS-Lite NUS``.
   You can configure this name using the ``CONFIG_BLE_ADV_NAME`` Kconfig option.
   For information on how to do this, see `Configuring Kconfig`_.
#. Observe that the text "BLE NUS sample started" is printed on the COM listener running on the computer.
#. Connect to your device using the `nRF Toolbox`_ mobile application with the ``Universal Asynchronous Receiver/Transmitter (UART)`` service.
   You must choose ``All`` devices in the scanner to see the device.
#. Write a text in the second COM listener running on the computer and press enter.
   Observe that the text is displayed in the terminal on the mobile phone.
#. Write a text in the terminal on the mobile phone and press send.
   Observe that the text is displayed in the second COM listener running on the computer.
