.. _hello_softdevice_sample:

Bluetooth: Hello SoftDevice
###########################

.. contents::
   :local:
   :depth: 2

The Hello SoftDevice sample demonstrates how to enable and disable the SoftDevice on a Nordic Semiconductor DK.

The SoftDevice is a precompiled firmware image implementing the Bluetooth® Low Energy wireless protocol stack.

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
           - bm_nrf54lv10dk/nrf54lv10a/cpuapp/s115_softdevice
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
           - bm_nrf54lv10dk/nrf54lv10a/cpuapp/s145_softdevice

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
         * - nRF54LS05 DK
           - PCA10214
           - S115
           - bm_nrf54ls05dk/nrf54ls05b/cpuapp/s115_softdevice/mcuboot
         * - `nRF54LV10 DK`_
           - PCA10188
           - S115
           - bm_nrf54lv10dk/nrf54lv10a/cpuapp/s115_softdevice/mcuboot
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
         * - nRF54LS05 DK
           - PCA10214
           - S145
           - bm_nrf54ls05dk/nrf54ls05b/cpuapp/s145_softdevice/mcuboot
         * - `nRF54LV10 DK`_
           - PCA10188
           - S145
           - bm_nrf54lv10dk/nrf54lv10a/cpuapp/s145_softdevice/mcuboot

Overview
********

The sample demonstrates the basic management of the SoftDevice in |BMlong|.
It shows how to enable and disable the SoftDevice, handle various types of events related to the SoftDevice, and manage the Bluetooth LE functionality.

User interface
**************

LED 2:
  Lit when the SoftDevice is enabled.

Building and running
********************

This sample can be found under :file:`samples/bluetooth/hello_softdevice/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``Hello SoftDevice sample started`` message is printed.
#. Observe that the ``SoftDevice enabled`` message is printed and **LED 2** is ON.
#. Observe that the ``Bluetooth enabled`` message is printed.
#. Observe that the ``SoftDevice disabled`` message is printed and **LED 2** is OFF.
#. Observe that the ``Bye`` message is printed.
