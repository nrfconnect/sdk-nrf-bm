.. _shell_bm_uarte_sample:

UARTE shell sample
##################

.. contents::
   :local:
   :depth: 2

The Shell sample demonstrates how to initialize and use the shell subsystem with an nrfx UARTE based backend with |BMlong|.

Requirements
************

The sample supports the following development kits:

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

Overview
********

The sample initializes and starts an interactive shell, which can be terminated by using the ``terminate`` shell command.

Building and running
********************

This sample can be found under :file:`samples/shell/bm_uarte/` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/configure_and_build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``bm-uarte:~$`` prompt is printed.
#. Type ``terminate`` and press Enter.
#. Observe that the ``goodbye`` message is printed.
#. Observe that the ``bm-uarte:~$`` prompt is printed.
#. Observe that shell is now unresponsive.
