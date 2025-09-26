.. _ble_hids_keyboard_sample:

Bluetooth: Human Interface Device Service Keyboard
##################################################

.. contents::
   :local:
   :depth: 2

The Bluetooth LE HIDS keyboard sample demonstrates how to use the :ref:`lib_ble_service_hids` to implement a keyboard input device that you can connect to your computer.

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

The sample uses the buttons on the development kit to simulate keys on a keyboard.

One button simulates the letter keys by generating letter keystrokes for a predefined string.
A second button simulates the Shift button and shows how to modify the letter keystrokes.
An LED displays the Caps Lock state, which can be modified by another connected keyboard.

This sample exposes the HID GATT Service.
It uses a report map for a generic keyboard.

User interface
**************

Button 0:
   Sends one character of the predefined input ("hello\\n") to the computer.

   During bonding, press it to confirm that the passkey is correct.
   See `Testing`_.

Button 1:
   Simulates the Shift key.

   Keep the button pressed while resetting the board to delete bonding information for all peers stored on the device.

   During bonding, press it to reject the passkey.

LED 0:
   Lit when the device is advertising.

LED 1:
   Lit when a device is connected.

LED 3:
   Lit when Caps Lock is on.

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

1. Compile and program the application.
#. In the Serial Terminal, using the `Serial Terminal app`_ or |VSC|, observe that the ``BLE HIDS Keyboard sample started.`` message is printed.
#. Observe that the ``Advertising as nRF_BM_HIDS_KB`` message is printed.
#. On your computer or mobile phone, open the Bluetooth settings and scan for advertising devices.
   Your device should be advertising as ``nRF_BM_HIDS_KB``.
   If the device is not advertising, you might need to use the :guilabel:`Reset Board` option in |VSC|.
#. :guilabel:`Connect` to your device.

   After having connected, your computer or mobile phone may attempt to pair or bond with your device in order to encrypt the link.

   You may be prompted to compare or enter a passkey as part of the authentication step.
   If prompted, provide the passkey from the terminal output, or confirm that the passkey is correct by pressing **Button 0** on the kit.

   The terminal output in |VSC| indicates ``Peer connected``.
#. Observe that the device is detected as a keyboard.
#. Repeatedly press **Button 0** on the kit.
   Every button press sends one character of the test message "hello" to your device (the test message includes a carriage return).
#. Press **Button 1** and hold it while pressing **Button 0**.
   Observe that the next letter of the "hello" message appears as a capital letter.
   This is because **Button 1** simulates the Shift key.
