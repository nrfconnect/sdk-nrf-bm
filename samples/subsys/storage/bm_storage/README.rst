.. _bm_storage_sample:

Bare Metal Storage
##################

.. contents::
   :local:
   :depth: 2

The Bare Metal Storage sample demonstrates how to configure and use storage instances with event callbacks.
Depending on the configuration selected, the sample uses either the SoftDevice storage backend or the RRAM storage backend.

Refer to the `Kconfig fragments`_ section for more information on configuration.

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

Kconfig fragments
*****************

By default, the sample uses the SoftDevice storage backend.
To build the sample for the RRAM backend, you can use the :file:`backend-rram.conf` Kconfig fragment.
To configure it for this sample, follow the steps outlined in the `Configuring and building the sample`_ section.

Building and running
********************

This sample can be found under :file:`samples/peripherals/storage/` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/configure_and_build_sample.txt

.. include:: /includes/program_sample.txt

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the following output is printed:

* For the SoftDevice storage backend:

  .. code-block:: console

     [00:00:00.005,789] <inf> app: Storage sample started
     [00:00:00.005,810] <inf> nrf_sdh: State change request: enable
     [00:00:00.011,214] <inf> app: Reading persisted data
     [00:00:00.011,232] <inf> app: output A:
                                   48 65 6c 6c 6f 00 00 00  00 00 00 00 00 00 00 00 |Hello... ........
     [00:00:00.011,244] <inf> app: output B:
                                   57 6f 72 6c 64 21 00 00  00 00 00 00 00 00 00 00 |World!.. ........
     [00:00:00.011,250] <inf> app: Erasing in Partition A, addr: 0x0015B000, size: 16
     [00:00:00.011,297] <inf> app: Erasing in Partition B, addr: 0x0015C000, size: 16
     [00:00:00.011,304] <inf> app: Waiting for writes to complete...
     [00:00:00.012,370] <inf> app: Handler A: bm_storage_evt: WRITE_RESULT 0, DISPATCH_TYPE 1
     [00:00:00.013,444] <inf> app: Handler B: bm_storage_evt: WRITE_RESULT 0, DISPATCH_TYPE 1
     [00:00:00.013,468] <inf> app: output A:
                                   00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........
     [00:00:00.013,481] <inf> app: output B:
                                   00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........
     [00:00:00.013,496] <inf> nrf_sdh: State change request: disable
     [00:00:00.013,531] <inf> app: Writing in Partition A, addr: 0x0015B000, size: 16
     [00:00:00.013,659] <inf> app: Handler A: bm_storage_evt: WRITE_RESULT 0, DISPATCH_TYPE 0
     [00:00:00.013,664] <inf> app: Writing in Partition B, addr: 0x0015C000, size: 16
     [00:00:00.013,769] <inf> app: Handler B: bm_storage_evt: WRITE_RESULT 0, DISPATCH_TYPE 0
     [00:00:00.013,775] <inf> app: Waiting for writes to complete...
     [00:00:00.013,789] <inf> app: output A:
                                   48 65 6c 6c 6f 00 00 00  00 00 00 00 00 00 00 00 |Hello... ........
     [00:00:00.013,801] <inf> app: output B:
                                   57 6f 72 6c 64 21 00 00  00 00 00 00 00 00 00 00 |World!.. ........
     [00:00:00.013,807] <inf> app: Storage sample finished.

* For the RRAM storage backend:

  .. code-block:: console

     [00:00:00.005,783] <inf> app: Storage sample started
     [00:00:00.005,791] <inf> app: Reading persisted data
     [00:00:00.005,808] <inf> app: output A:
                                   48 65 6c 6c 6f 00 00 00  00 00 00 00 00 00 00 00 |Hello... ........
     [00:00:00.005,820] <inf> app: output B:
                                   57 6f 72 6c 64 21 00 00  00 00 00 00 00 00 00 00 |World!.. ........
     [00:00:00.005,825] <inf> app: Erasing in Partition A, addr: 0x0015B000, size: 16
     [00:00:00.005,835] <inf> app: Handler A: bm_storage_evt: WRITE_RESULT 0, DISPATCH_TYPE 0
     [00:00:00.005,886] <inf> app: Erasing in Partition B, addr: 0x0015C000, size: 16
     [00:00:00.005,894] <inf> app: Handler B: bm_storage_evt: WRITE_RESULT 0, DISPATCH_TYPE 0
     [00:00:00.005,949] <inf> app: Waiting for writes to complete...
     [00:00:00.005,960] <inf> app: output A:
                                   00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........
     [00:00:00.005,972] <inf> app: output B:
                                   00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |........ ........
     [00:00:00.005,977] <inf> app: Writing in Partition A, addr: 0x0015B000, size: 16
     [00:00:00.005,985] <inf> app: Handler A: bm_storage_evt: WRITE_RESULT 0, DISPATCH_TYPE 0
     [00:00:00.006,008] <inf> app: Writing in Partition B, addr: 0x0015C000, size: 16
     [00:00:00.006,016] <inf> app: Handler B: bm_storage_evt: WRITE_RESULT 0, DISPATCH_TYPE 0
     [00:00:00.006,042] <inf> app: Waiting for writes to complete...
     [00:00:00.006,053] <inf> app: output A:
                                   48 65 6c 6c 6f 00 00 00  00 00 00 00 00 00 00 00 |Hello... ........
     [00:00:00.006,065] <inf> app: output B:
                                   57 6f 72 6c 64 21 00 00  00 00 00 00 00 00 00 00 |World!.. ........
     [00:00:00.006,071] <inf> app: Storage sample finished.

.. note::

   The addresses may vary depending on the board target selected.
