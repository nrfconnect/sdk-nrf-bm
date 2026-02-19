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
   Increase the simulated glucose concentration.

   Keep the button pressed while resetting the board to delete bonding information for all peers stored on the device.

   During bonding, press it to reject the passkey.

Button 3:
   Decrease the simulated glucose concentration.

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
#. In the Serial Terminal, observe that the ``Continuous Glucose Monitoring sample started.`` message is printed.
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
