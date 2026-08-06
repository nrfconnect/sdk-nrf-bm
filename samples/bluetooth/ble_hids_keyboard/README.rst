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

      The following board variants do **not** have DFU capabilities:

      .. include:: /includes/supported_boards_all_non-mcuboot_variants_s115.txt

      .. include:: /includes/supported_boards_all_non-mcuboot_variants_s145.txt

   .. group-tab:: MCUboot board variants

      The following board variants have DFU capabilities:

      .. include:: /includes/supported_boards_all_mcuboot_variants_s115.txt

      .. include:: /includes/supported_boards_all_mcuboot_variants_s145.txt

Overview
********

The sample uses the buttons on the development kit to simulate keys on a keyboard.

One button simulates the letter keys by generating letter keystrokes for a predefined string.
A second button simulates the Shift button and shows how to modify the letter keystrokes.
An LED displays the Caps Lock state, which can be modified by another connected keyboard.

This sample exposes the HID GATT Service.
It uses a report map for a generic keyboard.

.. include:: /includes/allow_list_sample.txt

User interface
**************

Button 0:
   When pairing with authentication, press this button to confirm the passkey shown in the COM listener and complete pairing with the other device.
   See `Testing`_.

Button 1:
   Keep the button pressed while resetting the board to delete bonding information for all peers stored on the device.

   When pairing with authentication, press this button to reject the passkey shown in the COM listener to prevent pairing with the other device.

Button 2:
   Simulate the Shift key.

Button 3:
   Send one character of the predefined input ("hello\\n") to the computer.

LED 0:
   Lit when the device is initialized.

LED 1:
   Lit when a device is connected.

LED 2:
   Lit when Caps Lock is active on the computer.

Security
********

The sample integrates :ref:`lib_peer_manager` (backed by :ref:`lib_bm_zms` for persistent storage) to support pairing and bonding.

The following pairing and bonding parameters are configured:

.. list-table:: Peer Manager security parameters
   :header-rows: 1

   * - Parameter
     - Value
     - Effect
   * - Bonding
     - Enabled
     - Bonding information is stored for reconnection.
   * - LE Secure Connections
     - Enabled
     - Pairing uses LESC instead of legacy pairing.
   * - MITM protection
     - Disabled
     - Pairing does not protect against man-in-the-middle attacks.
   * - Keypress notifications
     - Enabled
     - Keypress events are sent to the peer during passkey entry.
   * - I/O capabilities
     - Display and Yes/No entry
     - Pairing uses numeric comparison.

This sample configures the following services and its characteristics with these specific operation modes and security levels:

+----------------------------+-----------------------------+-------------------+-------------------------------+---------------------------------------------------------------------------------------------------------------------------------------+
| Service                    | Characteristic              | Operation         | Security level                | Effect                                                                                                                                |
+============================+=============================+===================+===============================+=======================================================================================================================================+
| HID Service                | HID Information             | Read              | Encryption, no authentication | Connection must be encrypted (paired) to read the HID version and device capability flags.                                            |
+                            +-----------------------------+-------------------+-------------------------------+---------------------------------------------------------------------------------------------------------------------------------------+
|                            | Report Map                  | Read              | Encryption, no authentication | Connection must be encrypted (paired) to read the report format the host needs to parse keyboard reports.                             |
+                            +-----------------------------+-------------------+-------------------------------+---------------------------------------------------------------------------------------------------------------------------------------+
|                            | Protocol Mode               | Read/Write        | Encryption, no authentication | Connection must be encrypted (paired) to switch the keyboard between Boot Protocol Mode and Report Protocol Mode.                     |
+                            +-----------------------------+-------------------+-------------------------------+---------------------------------------------------------------------------------------------------------------------------------------+
|                            | Boot Keyboard Input Report  | Read/Write/Notify | Encryption, no authentication | Connection must be encrypted (paired) to send key presses to hosts that only support the HID boot protocol, such as a BIOS.           |
+                            +-----------------------------+-------------------+-------------------------------+---------------------------------------------------------------------------------------------------------------------------------------+
|                            | Boot Keyboard Output Report | Read/Write        | Encryption, no authentication | Connection must be encrypted (paired) to receive LED state updates (Caps Lock, Num Lock, Scroll Lock) from boot protocol hosts.       |
+                            +-----------------------------+-------------------+-------------------------------+---------------------------------------------------------------------------------------------------------------------------------------+
|                            | Input Report                | Read/Write/Notify | Encryption, no authentication | Connection must be encrypted (paired) to send key press and release events to the host.                                               |
+                            +-----------------------------+-------------------+-------------------------------+---------------------------------------------------------------------------------------------------------------------------------------+
|                            | Output Report               | Read/Write        | Encryption, no authentication | Connection must be encrypted (paired) to receive LED state updates (Caps Lock, Num Lock, Scroll Lock) from the host.                  |
+                            +-----------------------------+-------------------+-------------------------------+---------------------------------------------------------------------------------------------------------------------------------------+
|                            | Feature Report              | Read/Write        | Encryption, no authentication | Connection must be encrypted (paired) to exchange vendor-specific configuration data outside the normal input and output reports.     |
+                            +-----------------------------+-------------------+-------------------------------+---------------------------------------------------------------------------------------------------------------------------------------+
|                            | Control Point               | Write             | Encryption, no authentication | Connection must be encrypted (paired) to suspend or resume HID report notifications, for example when the host enters low power mode. |
+----------------------------+-----------------------------+-------------------+-------------------------------+---------------------------------------------------------------------------------------------------------------------------------------+
| Device Information Service | All characteristics         | Read              | Open, no security             | Any connected device can read device information such as the manufacturer name and firmware version.                                  |
+----------------------------+-----------------------------+-------------------+-------------------------------+---------------------------------------------------------------------------------------------------------------------------------------+
| Battery Service            | All characteristics         | Read/Notify       | Open, no security             | Any connected device can read the battery level and subscribe to battery level notifications.                                         |
+----------------------------+-----------------------------+-------------------+-------------------------------+---------------------------------------------------------------------------------------------------------------------------------------+

See `Testing`_ for the pairing and bonding steps.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_hids_keyboard/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

1. Compile and program the application.
#. In the Serial Terminal, using the `Serial Terminal app`_ or |VSC|, observe that the ``BLE HIDS Keyboard sample initialized`` message is printed.
#. Observe that the ``Advertising as nRF_BM_HIDS_KB`` message is printed.
   You can configure the advertising name using the :kconfig:option:`CONFIG_SAMPLE_BLE_DEVICE_NAME` Kconfig option.
   For information on how to do this, see `Configuring Kconfig`_.
#. On your computer or mobile phone, open the Bluetooth settings and scan for advertising devices.
   If the device is not advertising, reset the board with the :guilabel:`Reset Board` option in |VSC| or by pressing the reset button on the development kit.
#. :guilabel:`Connect` to your device.

   The terminal output in |VSC| indicates ``Peer connected``.

   After having connected, your computer or mobile phone may attempt to pair or bond with your device in order to encrypt the link.

   You may be prompted to compare or enter a passkey as part of the authentication step.
   If prompted, provide the passkey from the terminal output, or confirm that the passkey is correct by pressing **Button 0** on the kit.
#. Observe that the device is detected as a keyboard.
#. Repeatedly press **Button 3** on the kit.
   Every button press sends one character of the test message "hello" to your device (the test message includes a carriage return).
#. Press **Button 2** and hold it while pressing **Button 3**.
   Observe that the next letter of the "hello" message appears as a capital letter.
   This is because **Button 2** simulates the Shift key.
