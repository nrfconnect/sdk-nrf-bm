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

The sample uses the buttons on the development kit to simulate the movement of a mouse.
The four buttons simulate movement to the left, up, right, and down, respectively.
Mouse clicks are not simulated.

This sample exposes the HID GATT Service.
It uses a report map for a generic mouse.

Allow list
==========

By default, the sample does not use allow list advertising, which means that any nearby device can connect and bond with the device.
Bonded devices are stored in internal non-volatile memory (NVM) and are remembered across power cycles.

You can enable allow list advertising by setting
:kconfig:option:`CONFIG_BLE_ADV_USE_ALLOW_LIST` to ``y`` in the base Kconfig fragment (:file:`prj.conf`).
When enabled, only previously bonded devices are allowed to reconnect, which allows faster reconnection and prevents unknown devices from connecting.

When allow list advertising is enabled and you want to add a new bonded device, existing bonds must first be deleted.
This can be done through user interaction, as described in the `User interface`_ section below.

User interface
**************

Button 0:
   Simulate mouse movement to the left.

   During bonding, press it to confirm that the passkey is correct.
   See `Testing`_.

Button 1:
   Simulate mouse movement upwards.

   Keep the button pressed while resetting the board to delete bonding information for all peers stored on the device.

   During bonding, press it to reject the passkey.

Button 2:
   Simulate mouse movement to the right.

Button 3:
   Simulate mouse movement downwards.

LED 0:
   Lit when the device is advertising.

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
#. In the Serial Terminal, using the `Serial Terminal app`_ or |VSC|, observe that the ``BLE HIDS Mouse sample started.`` message is printed.
#. Observe that the ``Advertising as nRF_BM_HIDS_MOUSE`` message is printed.
#. On your computer or mobile phone, open the Bluetooth settings and scan for advertising devices.
   Your device should be advertising as ``nRF_BM_HIDS_MOUSE``.
   If the device is not advertising, you might need to use the :guilabel:`Reset Board` option in |VSC|.
#. :guilabel:`Connect` to your device.

   After having connected, your computer or mobile phone may attempt to pair or bond with your device in order to encrypt the link.

   You may be prompted to compare or enter a passkey as part of the authentication step.
   If prompted, provide the passkey from the terminal output, or confirm that the passkey is correct by pressing **Button 0** on the kit.

   The terminal output in |VSC| indicates ``Peer connected``.
#. Observe that the device is detected as a mouse, and you can move the mouse by pressing the buttons on the DK.
