.. _bm_zms_sample:

BM_ZMS
######

.. contents::
   :local:
   :depth: 2

The BM_ZMS sample demonstrates how to use the Bare Metal ZMS (Zephyr Memory Storage) library to manage key-value pairs, counters, and long arrays in a storage system.
It shows how to add, read, and delete items in the storage.

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
To build the sample for the RRAM backend, you can use the :file:`rram.conf` Kconfig fragment.
To configure it for this sample, follow the steps outlined in the `Configuring and building the sample`_ section.

Building and running
********************

This sample can be found under :file:`samples/bm_zms` in the |BMshort| folder structure.

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

     [00:00:00.012,012] <inf> app: BM_ZMS sample started
     [00:00:00.012,033] <inf> nrf_sdh: State change request: enable
     [00:00:00.018,839] <inf> bm_zms: 4 Sectors of 1024 bytes
     [00:00:00.018,849] <inf> bm_zms: alloc wra: 0, 3c0
     [00:00:00.018,856] <inf> bm_zms: data wra: 0, 0
     [00:00:00.018,869] <inf> app: ITERATION: 0
     [00:00:00.018,892] <inf> app: Adding IP_ADDRESS 172.16.254.1 at id 1
     [00:00:00.021,326] <inf> app: Adding key/value at id beefdead
     [00:00:00.022,575] <inf> app: Adding counter at id 2
     [00:00:00.023,737] <inf> app: Adding Longarray at id 3
     [00:00:00.027,990] <inf> bm_zms: 4 Sectors of 1024 bytes
     [00:00:00.027,999] <inf> bm_zms: alloc wra: 0, 380
     [00:00:00.028,006] <inf> bm_zms: data wra: 0, 90
     [00:00:00.028,011] <inf> app: ITERATION: 1
     [00:00:00.028,044] <inf> app: ID: 1, IP Address: 172.16.254.1
     [00:00:00.028,062] <inf> app: Adding IP_ADDRESS 172.16.254.1 at id 1
     [00:00:00.030,471] <inf> app: Id: beefdead
     [00:00:00.030,487] <inf> app: Key:
                                   de ad be ef de ad be ef                          |........
     [00:00:00.030,491] <inf> app: Adding key/value at id beefdead
     [00:00:00.031,723] <inf> app: Id: 2, loop_cnt: 0
     [00:00:00.031,729] <inf> app: Adding counter at id 2
     [00:00:00.032,926] <inf> app: Id: 3
     [00:00:00.032,946] <inf> app: Longarray:
                                   00 01 02 03 04 05 06 07  08 09 0a 0b 0c 0d 0e 0f |........ ........
                                   10 11 12 13 14 15 16 17  18 19 1a 1b 1c 1d 1e 1f |........ ........
                                   20 21 22 23 24 25 26 27  28 29 2a 2b 2c 2d 2e 2f | !"#$%&' ()*+,-./
                                   30 31 32 33 34 35 36 37  38 39 3a 3b 3c 3d 3e 3f |01234567 89:;<=>?
                                   40 41 42 43 44 45 46 47  48 49 4a 4b 4c 4d 4e 4f |@ABCDEFG HIJKLMNO
                                   50 51 52 53 54 55 56 57  58 59 5a 5b 5c 5d 5e 5f |PQRSTUVW XYZ[\]^_
                                   60 61 62 63 64 65 66 67  68 69 6a 6b 6c 6d 6e 6f |`abcdefg hijklmno
                                   70 71 72 73 74 75 76 77  78 79 7a 7b 7c 7d 7e 7f |pqrstuvw xyz{|}~.
     [00:00:00.032,950] <inf> app: Adding Longarray at id 3
     [00:00:00.563,183] <err> bm_zms: No space in flash, gc_count 3, sector_count 4
     [00:00:00.630,422] <inf> app: Memory is full let's delete all items
     [00:00:01.767,151] <inf> app: Free space in storage is 2848 bytes
     [00:00:01.767,241] <inf> app: BM_ZMS sample finished Successfully

* For the RRAM storage backend:

  .. code-block:: console

     [00:00:00.012,009] <inf> app: BM_ZMS sample started
     [00:00:00.012,371] <inf> bm_zms: 4 Sectors of 1024 bytes
     [00:00:00.012,380] <inf> bm_zms: alloc wra: 0, 3c0
     [00:00:00.012,387] <inf> bm_zms: data wra: 0, 0
     [00:00:00.012,393] <inf> app: ITERATION: 0
     [00:00:00.012,415] <inf> app: Adding IP_ADDRESS 172.16.254.1 at id 1
     [00:00:00.012,541] <inf> app: Adding key/value at id beefdead
     [00:00:00.012,657] <inf> app: Adding counter at id 2
     [00:00:00.012,772] <inf> app: Adding Longarray at id 3
     [00:00:00.013,419] <inf> bm_zms: 4 Sectors of 1024 bytes
     [00:00:00.013,428] <inf> bm_zms: alloc wra: 0, 380
     [00:00:00.013,434] <inf> bm_zms: data wra: 0, 90
     [00:00:00.013,439] <inf> app: ITERATION: 1
     [00:00:00.013,470] <inf> app: ID: 1, IP Address: 172.16.254.1
     [00:00:00.013,487] <inf> app: Adding IP_ADDRESS 172.16.254.1 at id 1
     [00:00:00.013,675] <inf> app: Id: beefdead
     [00:00:00.013,687] <inf> app: Key:
                                   de ad be ef de ad be ef                          |........
     [00:00:00.013,691] <inf> app: Adding key/value at id beefdead
     [00:00:00.013,841] <inf> app: Id: 2, loop_cnt: 0
     [00:00:00.013,846] <inf> app: Adding counter at id 2
     [00:00:00.013,970] <inf> app: Id: 3
     [00:00:00.013,988] <inf> app: Longarray:
                                   00 01 02 03 04 05 06 07  08 09 0a 0b 0c 0d 0e 0f |........ ........
                                   10 11 12 13 14 15 16 17  18 19 1a 1b 1c 1d 1e 1f |........ ........
                                   20 21 22 23 24 25 26 27  28 29 2a 2b 2c 2d 2e 2f | !"#$%&' ()*+,-./
                                   30 31 32 33 34 35 36 37  38 39 3a 3b 3c 3d 3e 3f |01234567 89:;<=>?
                                   40 41 42 43 44 45 46 47  48 49 4a 4b 4c 4d 4e 4f |@ABCDEFG HIJKLMNO
                                   50 51 52 53 54 55 56 57  58 59 5a 5b 5c 5d 5e 5f |PQRSTUVW XYZ[\]^_
                                   60 61 62 63 64 65 66 67  68 69 6a 6b 6c 6d 6e 6f |`abcdefg hijklmno
                                   70 71 72 73 74 75 76 77  78 79 7a 7b 7c 7d 7e 7f |pqrstuvw xyz{|}~.
     [00:00:00.013,992] <inf> app: Adding Longarray at id 3
     [00:00:00.090,144] <err> bm_zms: No space in flash, gc_count 3, sector_count 4
     [00:00:00.156,662] <inf> app: Memory is full let's delete all items
     [00:00:00.357,856] <inf> app: Free space in storage is 2848 bytes
     [00:00:00.358,163] <inf> app: BM_ZMS sample finished Successfully
