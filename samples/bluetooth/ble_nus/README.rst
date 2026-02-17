.. _ble_nus_sample:

Bluetooth: Nordic UART Service (NUS)
####################################

.. contents::
   :local:
   :depth: 2

This sample demonstrates how to use the Nordic UART Service (NUS).
It uses the NUS service to send data back and forth between a UART connection and a Bluetooth® LE connection, emulating a serial port over Bluetooth LE.

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

When connected, the sample forwards any data received on the RX pin of the UART 0 peripheral to the Bluetooth LE unit.

Any data sent from the Bluetooth LE unit is sent out of the UART 0 peripheral’s TX pin.

Building and running with LPUARTE
=================================

The :file:`lpuarte.conf` file configures the sample to use the :ref:`LPUARTE <driver_lpuarte>` driver for the NUS Service.
This is useful for reducing the power consumption.
The file must be added to the build configuration is VS Code or as an extra argument to west: ``-DEXTRA_CONF_FILE="lpuarte.conf"``.

To test the NUS sample with the :ref:`LPUARTE <driver_lpuarte>` driver in loopback mode, connect pin ``P1.08`` (REQ) with ``P1.09`` (RDY) and ``P1.10`` (RX) with ``P1.11`` (TX) on the nRF54L15DK, as given in the :file:`board-config.h`.
It is also possible to test between two devices running NUS with LPUART by connecting the above mentioned pins and ``GND`` between the devices.
Make sure the ``REQ`` pin on one board is connected to the ``RDY`` on the other board, and vice versa.

.. note::

   With the LPUARTE configuration, the console is used only for application logging and not for NUS data.
   Output on the NUS TX line will be handled as input on the NUS RX line on the same device (loopback) or as NUS input to the other device (when using two devices).

User interface
**************

LED 0:
  Lit when the device is initialized.

LED 1:
  Lit when a device is connected.

.. note::

  LEDs are only used in the normal UARTE configuration. In LPUARTE mode, LEDs are disabled to avoid interfering with the RX pin and to allow proper low-power operation.

  In LPUARTE mode, LED 1 may appear on even when no phone is connected because it shares pin with the RX signal; RX activity can toggle the LED, which is expected behavior.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_nus/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

The following steps are valid for the default sample configuration.
It does not use the Low Power UARTE (LPUARTE) configuration fragment.

1. Compile and program the application.
#. Connect the device to the computer to access UART 0 and UART 1.
   If you use a development kit, UART 0 and 1 are forwarded as COM ports (Windows) or ttyACM devices (Linux) after you connect the development kit over USB.
   One instance is used for logging (if enabled), the other for the NUS service.
#. Connect to the kit with a terminal emulator (for example, the `Serial Terminal app`_) to both UARTs.
#. Reset the kit.
#. Observe that the device is advertising under the default name ``nRF_BM_NUS``.
   You can configure this name using the ``CONFIG_BLE_ADV_NAME`` Kconfig option.
   For information on how to do this, see `Configuring Kconfig`_.
#. Observe that the text ``BLE NUS sample initialized`` is printed on the COM listener running on the computer.
#. Connect to your device using the `nRF Toolbox`_ mobile application with the :guilabel:`Universal Asynchronous Receiver/Transmitter (UART)` service.
#. Write a text in the second COM listener running on the computer and press Enter.
   Observe that the text is displayed in the terminal on the mobile phone.
#. Write a text in the terminal on the mobile phone and press :guilabel:`Send`.
   Observe that the text is displayed in the second COM listener running on the computer.
