.. _ble_bms_sample:

Bluetooth: Bond Management Service (BMS)
########################################

.. contents::
   :local:
   :depth: 2

This sample demonstrates how to use the Bond Management Service (LBS).

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

User interface
**************

TODO

Programming the S115 SoftDevice
*******************************

.. include:: /includes/softdevice_flash.txt

.. _ble_bms_sample_testing:


Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_bms/` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/configure_and_build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

1. Compile and program the application.
#. In the Serial Terminal, observe that the ``BLE BMS sample started`` message is printed.
#. Observe that the ``Advertising as nRF_BM_BMS`` message is printed.
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
#. Reconnect the devices and verify that the connection security is not updated.
#. Bond both devices again.
#. Write ``06 41 42 43 44`` to the Bond Management Service Control Point Characteristic.
   ``06`` is the command to delete all bonds on the server, followed by the authorization code ``ABCD``.
#. Disconnect the device to trigger the bond deletion procedures.
#. Reconnect the devices again and verify that the connection security is not updated.
