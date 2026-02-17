.. _pwm_sample:

PWM
###

.. contents::
   :local:
   :depth: 2

The PWM sample demonstrates how to configure and use Pulse Width Modulation (PWM) with |BMlong|.

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

The sample initializes a PWM instance that blinks **LED 1** and **LED 3** on the device.

.. note::

  The PWM signal can be exposed only on GPIO pins that belong to the same domain as the target pin.
  For the nRF54L Series, there is only one domain which contains both PWM and GPIO: PWM20/21/22 and GPIO Port P1.
  Therefore, for these devices, only LEDs connected to P1 can work with PWM - for the nRF54L15 DK these are **LED 1** and **LED 3**.

User interface
**************

LED 0:
  Lit when the device is initialized.

LED 1:
  Breathing pattern.

LED 3:
  Breathing pattern.

Building and running
********************

This sample can be found under :file:`samples/peripherals/pwm/` in the |BMshort| folder structure.

For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``PWM sample initialized`` message is printed.
#. Observe that **LED 1** and **LED 3** have a breathing pattern.
