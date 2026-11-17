.. _ble_smp_updater_sample:

Bluetooth: SMP updater
######################

.. contents::
   :local:
   :depth: 2

The SMP updater sample serves the MCUmgr Simple Management Protocol (SMP) over Bluetooth LE and writes a plain MCUboot image in place into an update slot.
It is the bare-metal form of the IronSide DFU Bluetooth LE alternate image.
It calls the SoftDevice API directly, without the |BMshort| Bluetooth LE libraries or MCUmgr.

Requirements
************

The sample supports the ``bm_nrf54lm20dk/nrf54lm20a/cpuapp/s115_softdevice`` board variant.

It runs on S115 12.0.0-1.prototype, which the sample carries in its :file:`softdevice` folder.
A board overlay moves the SoftDevice partition to the base address of this version, 0x1E5800.

A second development kit that runs the Zephyr ``hci_uart`` sample serves as the Bluetooth LE controller for ``nrfutil mcu-manager ble``.

Overview
********

The sample runs from address 0 as a secure image.
The update slot starts at 0x20000 and ends at the storage partition.

The sample advertises as ``IronSide DFU`` with the SMP service UUID, accepts one connection, and refuses pairing.
It serves the following SMP commands:

* MCUmgr parameters (group 0, command 6).
* Bootloader information (group 0, command 8), which reports ``IronSide`` and, for the ``mode`` query, the firmware loader mode.
* Reset (group 0, command 5).
* Image state read (group 1, command 0), which lists the update slot as image 1, slot 0, once it holds a complete image.
* Image upload (group 1, command 1) for image 1.

Other commands return ``MGMT_ERR_ENOTSUP``.

An upload is refused when the image does not fit the update slot or has an encryption flag set.
The sample writes the image as it arrives and does not check its hash or signature.
A disconnect ends the upload, and the host restarts it from offset 0.

Building and running
********************

Build the sample with sysbuild, which also programs the SoftDevice:

.. code-block:: console

   west build -b bm_nrf54lm20dk/nrf54lm20a/cpuapp/s115_softdevice samples/bluetooth/ble_smp_updater
   west flash

Testing
=======

1. Program the ``hci_uart`` sample to the second development kit, and note its serial port.
#. Sign a test image with imgtool, for example:

   .. code-block:: console

      imgtool sign --header-size 0x800 --pad-header --align 4 --version 1.2.3 --slot-size 0x1c1800 app.bin test.bin

#. Upload the image:

   .. code-block:: console

      nrfutil mcu-manager ble image-upload --hci-serial-port <port> --device-name "IronSide DFU" --image-number 1 --firmware test.bin

#. List the image and compare its hash with the hash of the file:

   .. code-block:: console

      nrfutil mcu-manager ble image-list --hci-serial-port <port> --device-name "IronSide DFU"
      nrfutil mcu-manager image-hash --firmware test.bin
