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

      The following board variants do **not** have DFU capabilities:

      **S115**

      .. list-table::
         :header-rows: 1

         * - Hardware platform
           - PCA
           - Board target
         * - `nRF54L15 DK`_
           - PCA10156
           - bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice
         * - `nRF54L15 DK`_ (emulating nRF54L10)
           - PCA10156
           - bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice
         * - `nRF54L15 DK`_ (emulating nRF54L05)
           - PCA10156
           - bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice
         * - `nRF54LM20 DK`_
           - PCA10184
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s115_softdevice

      **S145**

      .. list-table::
         :header-rows: 1

         * - Hardware platform
           - PCA
           - Board target
         * - `nRF54L15 DK`_
           - PCA10156
           - bm_nrf54l15dk/nrf54l15/cpuapp/s145_softdevice
         * - `nRF54L15 DK`_ (emulating nRF54L10)
           - PCA10156
           - bm_nrf54l15dk/nrf54l10/cpuapp/s145_softdevice
         * - `nRF54L15 DK`_ (emulating nRF54L05)
           - PCA10156
           - bm_nrf54l15dk/nrf54l05/cpuapp/s145_softdevice
         * - `nRF54LM20 DK`_
           - PCA10184
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s145_softdevice

   .. group-tab:: MCUboot board variants

      The following board variants have DFU capabilities:

      **S115**

      .. list-table::
         :header-rows: 1

         * - Hardware platform
           - PCA
           - Board target
         * - `nRF54L15 DK`_
           - PCA10156
           - bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice/mcuboot
         * - `nRF54L15 DK`_ (emulating nRF54L10)
           - PCA10156
           - bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice/mcuboot
         * - `nRF54L15 DK`_ (emulating nRF54L05)
           - PCA10156
           - bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice/mcuboot
         * - `nRF54LM20 DK`_
           - PCA10184
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s115_softdevice/mcuboot
         * - nRF54LS05 DK
           - PCA10214
           - bm_nrf54ls05dk/nrf54ls05b/cpuapp/s115_softdevice/mcuboot

      **S145**

      .. list-table::
         :header-rows: 1

         * - Hardware platform
           - PCA
           - Board target
         * - `nRF54L15 DK`_
           - PCA10156
           - bm_nrf54l15dk/nrf54l15/cpuapp/s145_softdevice/mcuboot
         * - `nRF54L15 DK`_ (emulating nRF54L10)
           - PCA10156
           - bm_nrf54l15dk/nrf54l10/cpuapp/s145_softdevice/mcuboot
         * - `nRF54L15 DK`_ (emulating nRF54L05)
           - PCA10156
           - bm_nrf54l15dk/nrf54l05/cpuapp/s145_softdevice/mcuboot
         * - `nRF54LM20 DK`_
           - PCA10184
           - bm_nrf54lm20dk/nrf54lm20a/cpuapp/s145_softdevice/mcuboot

Overview
********

The sample initializes a PWM instance that blinks **LED 1** and **LED 3** on the device.

.. note::

  This sample uses GPIO pins connected to LEDs to demonstrate PWM functionality.
  The PWM signal can only be exposed on GPIO pins that belong to the same domain as the PWM instance.
  This limits which LEDs on a given DK can be used.
  For example, on the nRF54L Series, PWM20/21/22 and GPIO Port P1 share the same domain.
  Therefore, on the nRF54L15 DK, only LEDs connected to P1 work with PWM: **LED 1** and **LED 3**.

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
