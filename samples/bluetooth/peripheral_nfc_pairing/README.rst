.. _peripheral_nfc_pairing_sample:

Bluetooth: NFC pairing
######################

.. contents::
   :local:
   :depth: 2

The NFC pairing sample demonstrates Bluetooth® LE out-of-band pairing using an NFC tag.
You can use it to test the touch-to-pair feature between Nordic Semiconductor's devices and an NFC polling device with Bluetooth LE support, for example, a mobile phone.

The sample provides minimal Bluetooth functionality in Peripheral role and on GATT level it implements only the Device Information Service.

The sample supports pairing in one of the following modes:

* LE Secure Connections Just Works pairing
* LE Secure Connections OOB pairing
* Legacy OOB pairing
* Legacy Just Works pairing

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

The sample additionally requires an NFC polling device (for example, a smartphone or a tablet with NFC support).

Overview
********

When the application starts, it initializes and starts the NFCT peripheral that is used for pairing.
The application does not start advertising immediately, but only when the NFC tag is read by an NFC polling device, for example a smartphone or a tablet with NFC support.
The message that the tag sends to the NFC device contains the data required to initiate pairing.
To start the NFC data transfer, the NFC device must touch the NFC antenna that is connected to the development kit.

After reading the tag the initiator can connect and pair with the device that is advertising.
The connection state of the device is signaled by the LEDs.

NFC data exchange
*****************

From the NFC perspective, the sample implements a Type 4 Tag that contains a Handover Select Message with carrier information NDEF records.

User interface
**************

Button 1:
   Keep the button pressed while resetting the board to delete bonding information for all peers stored on the device.

LED 0:
   Lit when the device is initialized.

LED 1:
   Lit when a device is connected.

LED 2:
   Lit when an NFC field is present.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/peripheral_nfc_pairing/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

1. Compile and program the application.
#. Connect the device to the computer.
#. Connect to the kit with a terminal emulator (for example, the `Serial Terminal app`_).
   Note that the kit has two UARTs, where only one will output the log.
#. Reset the kit.
#. In the Serial Terminal, observe that the ``BLE Peripheral NFC Pairing sample started.`` message is printed.
#. Touch the NFC antenna with the smartphone or tablet and observe that **LED 2** is lit.
#. Confirm pairing with :guilabel:`nRF_BM_Nordic_NFC_pairing` in a pop-up window on the smartphone or tablet and observe that **LED 1** lights up.
#. Move the smartphone or tablet away from the NFC antenna and observe that **LED 2** turns off.
