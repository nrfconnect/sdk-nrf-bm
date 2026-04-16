.. _ble_hids_mouse_sample:

Bluetooth: Human Interface Device Service Mouse
###############################################

.. contents::
   :local:
   :depth: 2

The Peripheral HIDS mouse sample demonstrates how to use the :ref:`lib_ble_service_hids` to implement a mouse input device that you can connect to your computer.

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

The sample uses the buttons on the development kit to simulate the movement of a mouse.
The four buttons simulate movement to the left, up, right, and down, respectively.
Mouse clicks are not simulated.

This sample exposes the HID GATT Service.
It uses a report map for a generic mouse.

.. include:: /includes/allow_list_sample.txt

User interface
**************

Button 0:
   Simulate mouse movement to the left.

   When pairing with authentication, press this button to confirm the passkey shown in the COM listener and complete pairing with the other device.
   See `Testing`_.

Button 1:
   Simulate mouse movement upwards.

   Keep the button pressed while resetting the board to delete bonding information for all peers stored on the device.

   When pairing with authentication, press this button to reject the passkey shown in the COM listener to prevent pairing with the other device.

Button 2:
   Simulate mouse movement to the right.

Button 3:
   Simulate mouse movement downwards.

LED 0:
   Lit when the device is initialized.

LED 1:
   Lit when a device is connected.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_hids_mouse/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample using a computer or a smartphone.

1. Compile and program the application.
#. In the Serial Terminal, using the `Serial Terminal app`_ or |VSC|, observe that the ``BLE HIDS Mouse sample initialized`` message is printed.
#. Observe that the ``Advertising as nRF_BM_HIDS_MOUSE`` message is printed.
   You can configure the advertising name using the :kconfig:option:`CONFIG_SAMPLE_BLE_DEVICE_NAME` Kconfig option.
   For information on how to do this, see `Configuring Kconfig`_.
#. On your computer or mobile phone, open the Bluetooth settings and scan for advertising devices.
    If the device is not advertising, reset the board with the :guilabel:`Reset Board` option in |VSC| or by pressing the reset button on the development kit.
#. :guilabel:`Connect` to your device.

   After having connected, your computer or mobile phone may attempt to pair or bond with your device in order to encrypt the link.

   You may be prompted to compare or enter a passkey as part of the authentication step.
   If prompted, provide the passkey from the terminal output, or confirm that the passkey is correct by pressing **Button 0** on the kit.

   The terminal output in |VSC| indicates ``Peer connected``.
#. Observe that the device is detected as a mouse, and you can move the mouse by pressing the buttons on the DK.
