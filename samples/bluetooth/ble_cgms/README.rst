.. _ble_cgms_sample:

Bluetooth: Continuous Glucose Monitoring Service
################################################

.. contents::
   :local:
   :depth: 2

The Continuous Glucose Monitoring Service sample demonstrates how to implement the Continuous Glucose Monitoring Service profile using |BMlong|.

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

The Continuous Glucose Monitoring Service (CGMS) is a service that exposes glucose and other data from a personal Continuous Glucose Monitoring sensor.

User interface
**************

Button 0:
   When pairing with authentication, press this button to confirm the passkey shown in the COM listener and complete pairing with the other device.
   See `Testing`_.

Button 1:
   Keep the button pressed while resetting the board to delete bonding information for all peers stored on the device.

   When pairing with authentication, press this button to reject the passkey shown in the COM listener to prevent pairing with the other device.

Button 2:
   Decrease the simulated glucose concentration.

Button 3:
   Increase the simulated glucose concentration.

LED 0:
   Lit when the device is initialized.

LED 1:
   Lit when a device is connected.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_cgms/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

1. Compile and program the application.
#. Connect the device to the computer.
#. Connect to the kit with a terminal emulator (for example, the `Serial Terminal app`_).
   Note that the kit has two UARTs, where only one will output the log.
#. Reset the kit.
#. In the Serial Terminal, observe that the ``Continuous Glucose Monitoring sample initialized.`` message is printed.
#. Observe that the device is advertising under the default name ``nRF_BM_CGMS``.
   You can configure this name using the ``CONFIG_BLE_ADV_NAME`` Kconfig option.
   For information on how to do this, see `Configuring Kconfig`_.
#. Connect to your device using the `nRF Toolbox`_ mobile application with the :guilabel:`Continuous Glucose` service.

#. Your mobile phone should now attempt to pair with your device to encrypt the link.
   When prompted, check that the passkeys are the same before confirming by pressing **Button 0** on the kit and confirming on the mobile phone.

#. Observe that the text ``Connected`` and ``CGMS Start Session`` is printed in the COM listener.
#. Observe that you receive the simulated battery status of the kit in the mobile application.
#. Observe that you receive the CGMS records in the mobile application.
   The default time between each record is one minute.
