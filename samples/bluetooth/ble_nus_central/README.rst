.. _ble_nus_central_sample:

Bluetooth: Nordic UART Service Central
######################################

.. contents::
   :local:
   :depth: 2

This sample demonstrates how to use the Nordic UART Service (NUS) client with a device acting as a Bluetooth® LE central.
It uses the NUS client service to send data back and forth between a UART connection and a Bluetooth LE connection, emulating a serial port over Bluetooth LE.

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
         * - `nRF54LM20 DK`_
           - PCA10184
           - S145
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s145_softdevice
         * - nRF54LS05 DK
           - PCA10214
           - S145
           - bm_nrf54ls05dk/nrf54ls05b/cpuapp/s145_softdevice

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

This sample scans for devices that advertise with the :ref:`lib_ble_service_nus` UUID and initiates a connection when a device is found.
When a device is connected, the sample starts the service discovery procedure.

When connected, the sample forwards any data received on the RX pin of the UART 0 peripheral to the Bluetooth LE unit.

In addition, the sample enables peer TX notifications to receive data from the peer.

.. _ble_nus_central_sample_testing:

User interface
**************

LED 0:
  Lit when the device is initialized.

LED 1:
  Lit when a device is connected.

Button 3:
  Disconnects from the peer when pressed.

.. note::

  LEDs are only used in the normal UARTE configuration.
  In LPUARTE mode, LEDs are disabled to avoid interfering with the RX pin and to allow proper low-power operation.

  In LPUARTE mode, LED 1 may appear on even when no device is connected for some development kits because it shares pin with the RX signal; RX activity can toggle the LED, which is expected behavior.


Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_nus_central/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Building and running with LPUARTE
=================================

The :file:`lpuarte.conf` file configures the sample to use the :ref:`LPUARTE <driver_lpuarte>` driver for the NUS Client Service.
This is useful for reducing the power consumption.
Add the file to the build configuration in VS Code or as an extra argument to west: ``-DEXTRA_CONF_FILE="lpuarte.conf"``.

Testing
=======

You can test the sample in two ways, depending on the selected UART configuration: the default UARTE configuration or the Low Power UARTE (LPUARTE) configuration.

.. tabs::

   .. group-tab:: UARTE configuration

      Test the sample using the standard UARTE interface with two devices, one running this sample (:ref:`ble_nus_central_sample`) and another running the :ref:`ble_nus_sample` sample.
      This is the default configuration.

      1. Compile and program the application.
      #. Connect the devices to the computer to access UART 0 and UART 1 on both devices.
         If you use development kits, UART 0 and 1 are forwarded as COM ports (Windows) or ttyACM devices (Linux) after you connect the development kits over USB.
         One instance is used for logging (if enabled with :kconfig:option:`CONFIG_LOG`), while the other is used for the NUS Client service.
      #. Connect to the kit with a terminal emulator (for example, the `Serial Terminal app`_) for both UARTs on both development kits.
      #. Reset the kits.
      #. Observe that the device running the :ref:`ble_nus_central_sample` sample is configured to connect to ``nRF_BM_NUS``.
         You can configure this name using the :kconfig:option:`SAMPLE_TARGET_PERIPHERAL_NAME` Kconfig option.
         For information on how to do this, see `Configuring Kconfig`_.
      #. Observe that the device running the :ref:`ble_nus_sample` sample is advertising under the default name ``nRF_BM_NUS``.
         You can configure this name using the :kconfig:option:`CONFIG_BLE_ADV_NAME` Kconfig option.
         For information on how to do this, see `Configuring Kconfig`_.
      #. Observe that the text ``BLE NUS central example started.`` is printed on the COM listener connected to the device running the :ref:`ble_nus_central_sample` sample.
      #. Observe that the text ``BLE NUS sample initialized`` is printed on the COM listener connected to the device running the :ref:`ble_nus_sample` sample.
      #. Write a text in the COM listener running on the computer and press Enter.
      #. Observe that the text is displayed in the second COM listener running on the computer.

   .. group-tab:: LPUARTE configuration

      Test the sample with the Low Power UARTE (:ref:`LPUARTE <driver_lpuarte>`) interface in loopback mode between two devices, one running this sample (:ref:`ble_nus_central_sample`) and another running the :ref:`ble_nus_sample` sample.

      1. Build the sample with the LPUARTE configuration by adding the extra Kconfig fragment :file:`lpuarte.conf`.
      #. Flash the sample to your device.
      #. Connect the pins on the device in loopback mode as defined in :file:`board-config.h`:

         .. include:: /includes/lpuarte_board_connections.txt

          - **LPUARTE TX** → **LPUARTE RX**
          - **LPUARTE REQ** → **LPUARTE RDY**

      #. Power on the devices.
      #. Send a message from the device not connected in loopback mode using a terminal emulator.
         Observe that it is looped back.
