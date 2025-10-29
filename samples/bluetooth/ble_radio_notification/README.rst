.. _ble_radio_ntf_sample:

Bluetooth: Radio Notifications
##############################

.. contents::
   :local:
   :depth: 2

The Radio Notification sample demonstrates how you can use the :ref:`lib_ble_radio_notification` library using |BMlong|.

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

The sample initializes the radio notification library and registers a handler that triggers both when the radio is enabled and when it is disabled.
The handler then toggles **LED 0** based on the current state of the radio.

Programming the S115 SoftDevice
*******************************

.. include:: /includes/softdevice_flash.txt

.. _ble_radio_notification_sample_testing:

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_radio_notification/` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/configure_and_build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

You can test this sample using `nRF Connect for Desktop`_ with the `Serial Terminal app`_.
Make sure that these are installed before starting the testing procedure.

1. Compile and program the application.
#. In the Serial Terminal, observe that the ``BLE Radio Notification sample started`` message is printed.
#. Observe that the **LED 0** blinks while the device is advertising and the radio is active.
#. Observe that the **LED 0** stops blinking when advertising stops.
