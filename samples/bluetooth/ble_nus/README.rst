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

      The following board variants do **not** have DFU capabilities:

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
         * - `nRF54LV10 DK`_
           - PCA10188
           - S115
           - bm_nrf54lv10dk/nrf54lv10/cpuapp/s115_softdevice
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
         * - `nRF54LV10 DK`_
           - PCA10188
           - S145
           - bm_nrf54lv10dk/nrf54lv10/cpuapp/s145_softdevice

   .. group-tab:: MCUboot board variants

      The following board variants have DFU capabilities:

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

Overview
********

When connected, the sample forwards any data received on the RX pin of the UART 0 peripheral to the Bluetooth LE unit.

Any data sent from the Bluetooth LE unit is sent out of the UART 0 peripheral’s TX pin.

User interface
**************

LED 0:
  Lit when the device is initialized.

LED 1:
  Lit when a device is connected.

.. note::

  LEDs are only used in the normal UARTE configuration. In LPUARTE mode, LEDs are disabled to avoid interfering with the RX pin and to allow proper low-power operation.

  In LPUARTE mode, LED 1 may appear on even when no device is connected for some development kits because it shares pin with the RX signal; RX activity can toggle the LED, which is expected behavior.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_nus/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Building and running with LPUARTE
=================================

The :file:`lpuarte.conf` file configures the sample to use the :ref:`LPUARTE <driver_lpuarte>` driver for the NUS Service.
This is useful for reducing the power consumption.
The file must be added to the build configuration is VS Code or as an extra argument to west: ``-DEXTRA_CONF_FILE="lpuarte.conf"``.

Testing
=======

The sample can be tested in two ways, depending on the selected UART configuration: the default UARTE configuration or the Low Power UARTE (LPUARTE) configuration.

.. tabs::

   .. group-tab:: UARTE configuration

      Test the sample using the standard UARTE interface with a single device and one phone. This is the default configuration.

      1. Compile and program the application.
      #. Connect the device to the computer to access UART 0 and UART 1.
         If you use a development kit, UART 0 and 1 are forwarded as COM ports (Windows) or ttyACM devices (Linux) after you connect the development kit over USB.
         One instance is used for logging (if enabled with :kconfig:option:`CONFIG_LOG`), while the other is used for the NUS service.
      #. Connect to the kit with a terminal emulator (for example, the `Serial Terminal app`_) for both UARTs.
      #. Reset the kit.
      #. Observe that the device is advertising under the default name ``nRF_BM_NUS``.
         You can configure this name using the :kconfig:option:`CONFIG_BLE_ADV_NAME` Kconfig option.
         For information on how to do this, see `Configuring Kconfig`_.
      #. Observe that the text ``BLE NUS sample initialized`` is printed on the COM listener running on the computer.
      #. Connect to your device using the `nRF Toolbox`_ mobile application with the :guilabel:`Universal Asynchronous Receiver/Transmitter (UART)` service.
      #. Write a text in the second COM listener running on the computer and press Enter.
      #. Write a text in the terminal on the mobile phone and press :guilabel:`Send`.
         Observe that the text is displayed in the second COM listener running on the computer.

   .. group-tab:: LPUARTE configuration

      Test the sample with the Low Power UARTE (:ref:`LPUARTE <driver_lpuarte>`) interface, either in loopback mode on a single device or between two devices.

      1. Build the sample with the LPUARTE configuration by adding the extra Kconfig fragment :file:`lpuarte.conf`.
      #. Flash the sample to your device(s).
      #. Connect the pins as described below, as defined in :file:`board-config.h`.

         #. For Two-device setup:

            - Device 1 **TX (P1.11)** → Device 2 **RX (P1.10)**
            - Device 1 **RX (P1.10)** → Device 2 **TX (P1.11)**
            - Device 1 **REQ (P1.08)** → Device 2 **RDY (P1.09)**
            - Device 1 **RDY (P1.09)** → Device 2 **REQ (P1.08)**
            - Connect **GND** between both devices.

         #. For Single-device loopback setup:

            - **TX (P1.11)** → **RX (P1.10)**
            - **REQ (P1.08)** → **RDY (P1.09)**

      #. Power on the device(s).
      #. Connect the phone(s) to the device(s) using the `nRF Toolbox`_ mobile application with the :guilabel:`Universal Asynchronous Receiver/Transmitter (UART)` service.
      #. Send a message from one phone.
         Observe that it is received on the other phone or looped back.
      #. Send a message in the opposite direction.
         Observe that it is received correctly.

      .. note::

         With the LPUARTE configuration, the console is used only for application logging and not for NUS data.
         Output on the NUS TX line is handled as input on the NUS RX line on the same device (loopback) or as NUS input on the other device when using two devices.
