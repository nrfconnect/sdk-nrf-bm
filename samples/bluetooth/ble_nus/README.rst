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

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_nus/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

The sample can be tested in 2 ways:

* Default UART configuration
* LPUARTE configuration

The following steps are valid for the default sample configuration.
It does not use the Low Power UARTE (LPUARTE) configuration fragment.

1. Compile and program the application.
#. Connect the device to the computer to access UART 0 and UART 1.
   If you use a development kit, UART 0 and 1 are forwarded as COM ports (Windows) or ttyACM devices (Linux) after you connect the development kit over USB.
   One instance is used for logging (if enabled with :kconfig:option:`CONFIG_LOG`), the other for the NUS service.
#. Connect to the kit with a terminal emulator (for example, the `Serial Terminal app`_) to both UARTs, with baud rate 115200.
#. Reset the kit.
#. Observe that the device is advertising under the default name ``nRF_BM_NUS``.
   You can configure this name using the :kconfig:option:`CONFIG_BLE_ADV_NAME` Kconfig option.
   For information on how to do this, see `Configuring Kconfig`_.
#. Observe that the text ``BLE NUS sample started`` is printed on the COM listener running on the computer.
#. Connect to your device using the `nRF Toolbox`_ mobile application with the :guilabel:`Universal Asynchronous Receiver/Transmitter (UART)` service.
#. Write a text in the second COM listener running on the computer and press Enter.
   Observe that the text is displayed in the terminal on the mobile phone.
#. Write a text in the terminal on the mobile phone and press :guilabel:`Send`.
   Observe that the text is displayed in the second COM listener running on the computer.

You can test the sample with the Low Power UARTE (LPUARTE) drive using two Nordic devices and two phones, or a single device in loopback mode with one phone.

1. Build the sample with the LPUARTE configuration by adding the extra Kconfig fragment :file:`lpuarte.conf`
#. Flash the sample to your device(s).
#. When using two devices, connect the pins as follows:

   - Device 1 **TX (P1.11)** → Device 2 **RX (P1.10)**
   - Device 1 **RX (P1.10)** → Device 2 **TX (P1.11)**
   - Device 1 **REQ (P1.08)** → Device 2 **RDY (P1.09)**
   - Device 1 **RDY (P1.09)** → Device 2 **REQ (P1.08)**
   - Connect **GND** between both devices

#. When using a single device, connect the pins as follows:

   - **TX (P1.11)** → **RX (P1.10)**
   - **REQ (P1.08)** → **RDY (P1.09)**

#. Power on the device(s).
#. Connect phone 1 to device 1 and phone 2 to device 2 using the `nRF Toolbox`_ mobile application with the :guilabel:`Universal Asynchronous Receiver/Transmitter (UART)` service.
#. Send a message from phone 1.
   Observe that it is received on phone 2.
#. Send a message back from phone 2.
   Observe that it appears on phone 1.

You have now created a simple two-device Bluetooth LE messaging setup.
