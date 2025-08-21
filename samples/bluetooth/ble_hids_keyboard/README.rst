.. _ble_hids_keyboard_sample:

Bluetooth: Human Interface Device Service Keyboard
##################################################

.. contents::
   :local:
   :depth: 2

The Peripheral HIDS keyboard sample demonstrates how to use the Human Interface Device (HID) Service to implement a keyboard input device that you can connect to your computer.

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
     - bm_nrf54l15dk/nrf54l15/cpuapp/softdevice_s115
   * - `nRF54L15 DK`_ (emulating nRF54L10)
     - PCA10156
     - S115
     - bm_nrf54l15dk/nrf54l10/cpuapp/softdevice_s115
   * - `nRF54L15 DK`_ (emulating nRF54L05)
     - PCA10156
     - S115
     - bm_nrf54l15dk/nrf54l05/cpuapp/softdevice_s115

Overview
********

The sample uses the buttons on a development kit to simulate keys on a keyboard.
One button simulates the letter keys by generating letter keystrokes for a predefined string.
A second button simulates the Shift button and shows how to modify the letter keystrokes.
An LED displays the Caps Lock state, which can be modified by another connected keyboard.

This sample exposes the HID GATT Service.
It uses a report map for a generic keyboard.

User interface
**************

Button 0:
   Sends one character of the predefined input ("hello\\n") to the computer.

Button 1:
   Simulates the Shift key.

LED 0:
   Blinks with a period of two seconds with the duty cycle set to 50% when the main loop is running and the device is advertising.

LED 1:
   Lit when at least one device is connected.

LED 2:
   Indicates if Caps Lock is enabled.

Programming the S115 SoftDevice
*******************************

.. include:: /includes/softdevice_flash.txt

.. _ble_hids_keyboard_sample_testing:


Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_hids_keyboard/` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/configure_and_build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

TODO
