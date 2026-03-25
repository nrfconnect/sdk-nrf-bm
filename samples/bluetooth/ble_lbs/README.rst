.. _ble_lbs_sample:

Bluetooth: LED Button Service
#############################

.. contents::
   :local:
   :depth: 2

This sample demonstrates how to use the LED Button Service (LBS).

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
         * - `nRF54LM20 DK`_
           - PCA10184
           - S115
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s115_softdevice
         * - nRF54LS05 DK
           - PCA10214
           - S115
           - bm_nrf54ls05dk/nrf54ls05b/cpuapp/s115_softdevice
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
         * - `nRF54LM20 DK`_
           - PCA10184
           - S145
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s145_softdevice
         * - nRF54LS05 DK
           - PCA10214
           - S145
           - bm_nrf54ls05dk/nrf54ls05b/cpuapp/s145_softdevice
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
         * - `nRF54LM20 DK`_
           - PCA10184
           - S115
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s115_softdevice/mcuboot
         * - `nRF54LV10 DK`_
           - PCA10188
           - S115
           - bm_nrf54lv10dk/nrf54lv10/cpuapp/s115_softdevice/mcuboot
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
         * - `nRF54LM20 DK`_
           - PCA10184
           - S145
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s145_softdevice/mcuboot
         * - `nRF54LV10 DK`_
           - PCA10188
           - S145
           - bm_nrf54lv10dk/nrf54lv10/cpuapp/s145_softdevice/mcuboot

Overview
********

The LED Button Service (LBS) is a custom service that controls the state of an LED and sends notifications when a button changes its state.

You can use the sample to transmit the button state from your development kit to another device.

User interface
**************

Button 2:
  Button for the LBS Button State characteristic.

LED 0:
  Lit when the device is initialized.

LED 1:
  Lit when a device is connected.

LED 2:
  LED controlled through the LBS LED State characteristic.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/ble_lbs/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample using `nRF Connect for Desktop`_ with the `Bluetooth Low Energy app`_ and the `Serial Terminal app`_.
Make sure that these are installed before starting the testing procedure.

1. Compile and program the application.
#. In the Serial Terminal, observe that the ``BLE LBS sample initialized`` message is printed.
#. Observe that the ``Advertising as nRF_BM_LBS`` message is printed.
   You can configure the advertising name using the :kconfig:option:`CONFIG_SAMPLE_BLE_DEVICE_NAME` Kconfig option.
   For information on how to do this, see `Configuring Kconfig`_.
#. In nRF Connect for Desktop, scan for advertising devices.
    If the device is not advertising, reset the board with the :guilabel:`Reset Board` option in |VSC| or by pressing the reset button on the development kit.
#. :guilabel:`Connect` to your device.
   The terminal output in |VSC| indicates ``Peer connected``.
#. In nRF Connect for Desktop, go to :guilabel:`Nordic LED and Button Service` and change :guilabel:`Blinky LED state` to :guilabel:`01`.
   The terminal output in |VSC| indicates ``Received LED ON!``.
#. Change the :guilabel:`Blinky LED state` value back to :guilabel:`00`.
   The terminal output in |VSC| indicates ``Received LED OFF!``.
