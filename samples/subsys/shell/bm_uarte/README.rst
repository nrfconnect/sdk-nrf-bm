.. _shell_bm_uarte_sample:

UARTE shell sample
##################

.. contents::
   :local:
   :depth: 2

The Shell sample demonstrates how to initialize and use the shell
subsystem with an NRFX UARTE based backend with |BMlong|.

Requirements
************

The sample supports the following development kits:

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

Overview
********

The sample initializes and starts an interactive shell, which can be
terminated by using the "terminate" shell command.

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``bm-uarte:~$`` prompt is printed.
#. Write ``help`` and press enter.
#. Observe supported commands are printed.
#. Write ``terminate`` and press enter.
#. Observe that ``goodbye`` message is printed.
#. Observe that ``bm-uarte:~$`` prompt is printed.
#. Observe that shell is now unresponsive.
