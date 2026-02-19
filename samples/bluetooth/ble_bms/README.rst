.. _ble_bms_sample:

Bluetooth: Bond Management Service (BMS)
########################################

.. contents::
   :local:
   :depth: 2

This sample demonstrates how to use the Bond Management Service (BMS).

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
     - bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice
   * - `nRF54L15 DK`_ (emulating nRF54L10)
     - PCA10156
     - S115
     - bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice
   * - `nRF54L15 DK`_ (emulating nRF54L05)
     - PCA10156
     - S115
     - bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice

Overview
********

When connected, the sample waits for the client's requests to perform any bond-deleting operation.

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
   During bonding, press the button to confirm that the passkey is correct.
   See `Testing`_.

Button 1:
   Keep the button pressed while resetting the board to delete bonding information for all peers stored on the device.
   During bonding, press it to reject the passkey.

LED 0:
   Lit when the device is advertising.

LED 1:
   Lit when a device is connected.

.. _ble_bms_sample_testing:

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_bms/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

1. Compile and program the application.
#. Connect to the kit that runs this sample with a terminal emulator (for example, the `Serial Terminal app`_).
#. Reset the kit.
#. In the Serial Terminal, observe that the ``BLE BMS sample started`` message is printed.
#. Observe that the ``Advertising as nRF_BM_BMS`` message is printed.
#. Open `nRF Connect for Desktop`_.
#. Open the `Bluetooth Low Energy app`_ and select the connected device that is used for communication.
#. Connect to the device from the app.
   The device is advertising as ``nRF_BM_BMS``.
#. Bind with the device:

   a. Click the :guilabel:`Settings` button for the device in the app.
   #. Select :guilabel:`Pair`.
   #. Select :guilabel:`Keyboard and display` in the IO capabilities setting.
   #. Select :guilabel:`Perform Bonding`.
   #. Click :guilabel:`Pair`.

#. Check the logs to verify that the connection security is updated.
#. Disconnect the device in the app.
#. Reconnect again and verify that the connection security is updated automatically.
#. Verify that the Feature Characteristic of the Bond Management Service displays ``10 08 02``.
   This means that the following features are supported:

   * Deletion of the bonds for the current connection of the requesting device.
   * Deletion of all bonds on the Server with the Authorization Code.
   * Deletion of all bonds on the Server except the ones of the requesting device with the Authorization Code.

#. Write ``03`` to the Bond Management Service Control Point Characteristic.
   ``03`` is the command to delete the current bond.
#. Disconnect the device to trigger the bond deletion procedures.
#. Delete the bond information of the central device in the app:

   a. Click the :guilabel:`Settings` button for the device in the app.
   #. Click :guilabel:`Delete bond information`.

#. Reconnect the devices and verify that the connection security is not updated.
#. Bond both devices again.
#. Write ``06 41 42 43 44`` to the Bond Management Service Control Point Characteristic.
   ``06`` is the command to delete all bonds on the server, followed by the authorization code ``ABCD``.
#. Disconnect the device to trigger the bond deletion procedures.
#. Delete the bond information of the central device again.
#. Reconnect the devices again and verify that the connection security is not updated.
