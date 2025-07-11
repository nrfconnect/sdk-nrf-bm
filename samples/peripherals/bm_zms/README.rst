.. _bm_zms_sample:

BM_ZMS
######

.. contents::
   :local:
   :depth: 2

The BM_ZMS sample demonstrates how to use the Bare Metal ZMS (Zephyr Memory Storage) library to
manage key-value pairs, counters, and long arrays in a storage system.
It shows how to add, read, and delete items in the storage.

Refer to the `Kconfig fragments`_ section for more information on configuration.

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
     - bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice/mcuboot
   * - `nRF54L15 DK`_ (emulating nRF54L10)
     - PCA10156
     - bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice
     - bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice/mcuboot
   * - `nRF54L15 DK`_ (emulating nRF54L05)
     - PCA10156
     - bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice
     - bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice/mcuboot

Kconfig fragments
*****************

By default, the sample uses the SoftDevice storage backend.
To build the sample for the RRAM backend, you can use the :file:`rram.conf` Kconfig fragment.
To configure it for this sample, follow the steps outlined in the
`Configuring and building the sample`_ section.

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

     [00:00:00.005,746] <inf> app: BM_ZMS sample started
     [00:00:00.005,767] <inf> nrf_sdh: State change request: enable
     [00:00:00.012,611] <inf> bm_zms: 3 Sectors of 1024 bytes
     [00:00:00.012,621] <inf> bm_zms: alloc wra: 0, 3c0
     [00:00:00.012,627] <inf> bm_zms: data wra: 0, 0
     [00:00:00.012,639] <inf> app: ITERATION: 0
     [00:00:00.012,662] <inf> app: Adding IP_ADDRESS 172.16.254.1 at id 1
     [00:00:00.014,854] <inf> app: Adding key/value at id beefdead
     [00:00:00.016,115] <inf> app: Adding counter at id 2
     [00:00:00.017,294] <inf> app: Adding Longarray at id 3
     [00:00:00.021,432] <inf> bm_zms: 3 Sectors of 1024 bytes
     [00:00:00.021,442] <inf> bm_zms: alloc wra: 0, 380
     [00:00:00.021,449] <inf> bm_zms: data wra: 0, 90
     [00:00:00.021,454] <inf> app: ITERATION: 1
     [00:00:00.021,487] <inf> app: ID: 1, IP Address: 172.16.254.1
     [00:00:00.021,504] <inf> app: Adding IP_ADDRESS 172.16.254.1 at id 1
     [00:00:00.023,849] <inf> app: Id: beefdead
     [00:00:00.023,865] <inf> app: Key:
                                   de ad be ef de ad be ef                          |........
     [00:00:00.023,869] <inf> app: Adding key/value at id beefdead
     [00:00:00.025,077] <inf> app: Id: 2, loop_cnt: 0
     [00:00:00.025,082] <inf> app: Adding counter at id 2
     [00:00:00.026,304] <inf> app: Id: 3
     [00:00:00.026,325] <inf> app: Longarray:
                                   00 01 02 03 04 05 06 07  08 09 0a 0b 0c 0d 0e 0f |........ ........
                                   10 11 12 13 14 15 16 17  18 19 1a 1b 1c 1d 1e 1f |........ ........
                                   20 21 22 23 24 25 26 27  28 29 2a 2b 2c 2d 2e 2f | !"#$%&' ()*+,-./
                                   30 31 32 33 34 35 36 37  38 39 3a 3b 3c 3d 3e 3f |01234567 89:;<=>?
                                   40 41 42 43 44 45 46 47  48 49 4a 4b 4c 4d 4e 4f |@ABCDEFG HIJKLMNO
                                   50 51 52 53 54 55 56 57  58 59 5a 5b 5c 5d 5e 5f |PQRSTUVW XYZ[\]^_
                                   60 61 62 63 64 65 66 67  68 69 6a 6b 6c 6d 6e 6f |`abcdefg hijklmno
                                   70 71 72 73 74 75 76 77  78 79 7a 7b 7c 7d 7e 7f |pqrstuvw xyz{|}~.
     [00:00:00.026,330] <inf> app: Adding Longarray at id 3
     [00:00:00.377,601] <err> bm_zms: No space in flash, gc_count 2, sector_count 3
     [00:00:00.408,655] <inf> app: Memory is full let's delete all items
     [00:00:01.191,239] <inf> app: Free space in storage is 1904 bytes
     [00:00:01.191,334] <inf> app: BM_ZMS sample finished Successfully

* For the RRAM storage backend:

  .. code-block:: console

     [00:00:00.005,745] <inf> app: BM_ZMS sample started
     [00:00:00.006,190] <inf> bm_zms: 3 Sectors of 1024 bytes
     [00:00:00.006,198] <inf> bm_zms: alloc wra: 0, 3c0
     [00:00:00.006,204] <inf> bm_zms: data wra: 0, 0
     [00:00:00.006,210] <inf> app: ITERATION: 0
     [00:00:00.006,232] <inf> app: Adding IP_ADDRESS 172.16.254.1 at id 1
     [00:00:00.006,450] <inf> app: Adding key/value at id beefdead
     [00:00:00.006,577] <inf> app: Adding counter at id 2
     [00:00:00.006,745] <inf> app: Adding Longarray at id 3
     [00:00:00.007,465] <inf> bm_zms: 3 Sectors of 1024 bytes
     [00:00:00.007,472] <inf> bm_zms: alloc wra: 0, 380
     [00:00:00.007,479] <inf> bm_zms: data wra: 0, 90
     [00:00:00.007,483] <inf> app: ITERATION: 1
     [00:00:00.007,515] <inf> app: ID: 1, IP Address: 172.16.254.1
     [00:00:00.007,532] <inf> app: Adding IP_ADDRESS 172.16.254.1 at id 1
     [00:00:00.007,800] <inf> app: Id: beefdead
     [00:00:00.007,812] <inf> app: Key:
                                   de ad be ef de ad be ef                          |........
     [00:00:00.007,816] <inf> app: Adding key/value at id beefdead
     [00:00:00.007,957] <inf> app: Id: 2, loop_cnt: 0
     [00:00:00.007,962] <inf> app: Adding counter at id 2
     [00:00:00.008,140] <inf> app: Id: 3
     [00:00:00.008,157] <inf> app: Longarray:
                                   00 01 02 03 04 05 06 07  08 09 0a 0b 0c 0d 0e 0f |........ ........
                                   10 11 12 13 14 15 16 17  18 19 1a 1b 1c 1d 1e 1f |........ ........
                                   20 21 22 23 24 25 26 27  28 29 2a 2b 2c 2d 2e 2f | !"#$%&' ()*+,-./
                                   30 31 32 33 34 35 36 37  38 39 3a 3b 3c 3d 3e 3f |01234567 89:;<=>?
                                   40 41 42 43 44 45 46 47  48 49 4a 4b 4c 4d 4e 4f |@ABCDEFG HIJKLMNO
                                   50 51 52 53 54 55 56 57  58 59 5a 5b 5c 5d 5e 5f |PQRSTUVW XYZ[\]^_
                                   60 61 62 63 64 65 66 67  68 69 6a 6b 6c 6d 6e 6f |`abcdefg hijklmno
                                   70 71 72 73 74 75 76 77  78 79 7a 7b 7c 7d 7e 7f |pqrstuvw xyz{|}~.
     [00:00:00.008,161] <inf> app: Adding Longarray at id 3
     [00:00:00.053,102] <err> bm_zms: No space in flash, gc_count 2, sector_count 3
     [00:00:00.083,706] <inf> app: Memory is full let's delete all items
     [00:00:00.190,012] <inf> app: Free space in storage is 1904 bytes
     [00:00:00.190,239] <inf> app: BM_ZMS sample finished Successfully
