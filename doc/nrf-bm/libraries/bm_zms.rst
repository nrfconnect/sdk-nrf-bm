.. _lib_bm_zms:

Bare Metal Zephyr Memory Storage (BM_ZMS)
#########################################

Bare Metal Zephyr Memory Storage is a key-value storage system that is designed to work with all types of non-volatile storage technologies.

The Bare Metal version is designed to work with asynchronous and synchronous storage APIs.

General behavior
****************

BM_ZMS divides the memory space into sectors (minimum 2), and each sector is filled with key-value pairs until it is full.

The key-value pair is divided into two parts:

* The key part is written in an ATE (Allocation Table Entry) called ``ID-ATE`` which is stored starting from the bottom of the sector.
* The value part is defined as ``data`` and is stored raw starting from the top of the sector.

Additionally, each sector includes header ATEs at the last positions.
These ATEs are essential for describing the sector's status (whether it is 'closed' or 'open') and for indicating the current version of BM_ZMS.

When the current sector is full, the library verifies first that the following sector is empty.
It then garbage collects the sector N+2, where N is the current sector number, by moving the valid ATEs to the N+1 empty sector.
The library then erases the garbage-collected sector and closes the current sector by writing a ``garbage_collect_done`` ATE and the ``close`` ATE (one of the header entries).

Afterwards, it moves forward to the next sector and starts writing entries again.

This behavior is repeated until it reaches the end of the partition.
Then, it starts again from the first sector after garbage collecting it and erasing its content.

Composition of a sector
=======================
A sector is organized in the following format (example with three sectors):

.. list-table::
   :widths: 25 25 25
   :header-rows: 1

   * - Sector 0 (closed)
     - Sector 1 (open)
     - Sector 2 (empty)
   * - Data_a0
     - Data_b0
     - Data_c0
   * - Data_a1
     - Data_b1
     - Data_c1
   * - Data_a2
     - Data_b2
     - Data_c2
   * - GC_done
     -    .
     -    .
   * -    .
     -    .
     -    .
   * -    .
     -    .
     -    .
   * -    .
     - ID ATE_b2
     - ID ATE_c2
   * - ID ATE_a2
     - ID ATE_b1
     - ID ATE_c1
   * - ID ATE_a1
     - ID ATE_b0
     - ID ATE_c0
   * - ID ATE_a0
     - GC_done ATE
     - GC_done ATE
   * - Close ATE (cyc=1)
     - Close ATE (cyc=1)
     - Close ATE (cyc=1)
   * - Empty ATE (cyc=1)
     - Empty ATE (cyc=2)
     - Empty ATE (cyc=2)

Definition of each element in the sector
========================================

``Empty ATE`` is written when erasing a sector (last position of the sector).

``Close ATE`` is written when closing a sector (second to last position of the sector).

``GC_done ATE`` is written to indicate that the next sector has already been garbage-collected.
This ATE can appear at any position of the sector.

``ID ATE`` are entries that contain a 32-bit key and describe where the data is stored, its size, and its CRC32.

``Data`` is the actual value associated with the ID-ATE.

BM_ZMS workflow
***************

The following sections describe in detail the operations performed by this library.

Registering the user callback handler
=====================================

BM_ZMS can be used with asynchronous and synchronous storage APIs.

To use it with asynchronous APIs, it is recommended to register a callback handler that will be called when an operation (write/init) has finished.
Read operations are synchronous and do not require a callback handler.

Mounting the storage system
===========================

Mounting the storage system starts by checking that the file system properties are correct, such as the sector_size and sector_count.
Then, the ``bm_zms_mount`` function must be called to make the storage ready.

To mount the file system, the following elements in the ``bm_zms_fs`` structure must be initialized:

.. code-block:: c

	struct bm_zms_fs {
		/** File system offset in non-volatile memory **/
		off_t offset;

		/** Storage system is split into sectors, each sector size must be multiple of
		 * erase-blocks if the device has erase capabilities
		 */
		uint32_t sector_size;
		/** Number of sectors in the file system */
		uint32_t sector_count;
	};

Initialization
==============

As BM_ZMS has a fast-forward write mechanism, it must find the last sector and the last pointer of the entry where it stopped the last time.

To do this, it looks for a closed sector followed by an open one.
Then, within the open sector, it finds (recovers) the last written ATE.
After that, it checks that the sector after this one is empty, or it will erase it.

If this initialization is successful, the library sets the flag ``bm_zms_fs.init_flags.initialized`` to true.
For asynchronous storage backends, you must wait for the initialization to finish before triggering a write or read operation.

BM_ZMS ID/data write
====================

BM_ZMS has an internal FIFO that is used to store write requests before they are processed.

The FIFO size is configurable through the :kconfig:option:`CONFIG_BM_ZMS_OP_QUEUE_SIZE` Kconfig option.
For asynchronous backends, the ``bm_zms_write`` function will return immediately once the write request is added to the FIFO.
The return value is either 0 (success) or an error code.

Once a write request is processed, the callback handler (if registered) is called with the result of the operation.
If BM_ZMS still has some queued write operations to process, it sets the ``bm_zms_fs.ongoing_writes`` flag to the number of operations that have not finished yet.

If the sector is full (cannot hold the current data + ATE), BM_ZMS moves to the next sector, garbage collects the sector after the newly opened one, and then erases it.
Data whose size is smaller or equal to 8 bytes is written within the ATE.

BM_ZMS ID/data read (with history)
==================================

By default, BM_ZMS looks for the last data with the same ID by browsing through all stored ATEs from the most recent ones to the oldest ones.
If it finds a valid ATE with a matching ID, it retrieves its data and returns the number of bytes that were read.

If a history count is provided and different than 0, older data with the same ID is retrieved.

BM_ZMS free space calculation
=============================

BM_ZMS can also return the free space remaining in the partition.

However, this operation is very time-consuming as it needs to browse through all valid ATEs in all sectors of the partition and for each valid ATE tries to check if an older one exists.
It is not recommended for applications to use this function often, as it is time-consuming and might slow down the calling context.

The cycle counter
=================

Each sector has a lead cycle counter which is a ``uint8_t`` that is used to validate all the other ATEs.

The lead cycle counter is stored in the empty ATE.
To become valid, an ATE must have the same cycle counter as the one stored in the empty ATE.
Each time an ATE is moved from a sector to another it must get the cycle counter of the destination sector.

To erase a sector, the cycle counter of the empty ATE is incremented and a single write of the empty ATE is performed.
All the ATEs in that sector then become invalid.

Closing sectors
===============

To close a sector, a ``close`` ATE is added at the end of the sector which must have the same cycle counter as the empty ATE.
When closing a sector, all the remaining space that has not been used is filled with garbage data to avoid having old ATEs with a valid cycle counter.

ATE (Allocation Table Entry) structure
======================================

An entry has 16 bytes as shown in the :c:struct:`bm_zms_ate` structure.

.. note::
   The CRC of the data is checked only when a full read of the data is performed.
   The CRC of the data is not checked for a partial read, as it is computed for the whole element.

Available space for user data (key-value pairs)
***********************************************

BM_ZMS always needs an empty sector to be able to perform the garbage collection (GC).
Assuming that 4 sectors exist in a partition, BM_ZMS will only use 3 sectors to store key-value pairs and keep one sector empty to be able to perform GC.
The empty sector will rotate between the 4 sectors in the partition.

.. note::
   The maximum single data length that can be written at once in a sector is 64 KB.

Small data values
=================

Values smaller than or equal to 8 bytes are stored within the entry (ATE) itself, without writing data at the top of the sector.

BM_ZMS has an entry size of 16 bytes which means that the maximum available space in a partition to store data is computed in this scenario in the following way:

.. math::

   \small\frac{(NUM\_SECTORS - 1) \times (SECTOR\_SIZE - (5 \times ATE\_SIZE)) \times (DATA\_SIZE)}{ATE\_SIZE}

Where:

``NUM_SECTOR``: Total number of sectors

``SECTOR_SIZE``: Size of the sector

``ATE_SIZE``: 16 bytes

``(5 * ATE_SIZE)``: Reserved ATEs for header and delete items

``DATA_SIZE``: Size of the small data values (range from 1 to 8)

For example for 4 sectors of 1024 bytes, free space for 8-byte length data is :math:`\frac{3 \times 944 \times 8}{16} = 1416 \, \text{ bytes}`.

Large data values
=================

Large data values (> 8 bytes) are stored separately at the top of the sector.

In this case, it is hard to estimate the free available space, as it depends on the size of the data.
However, it can be taken into account that for N bytes of data (N > 8 bytes), an additional 16 bytes of ATE must be added at the bottom of the sector.

For example, for a partition that has 4 sectors of 1024 bytes and for data size of 64 bytes, only 3 sectors are available for writes with a capacity of 944 bytes each.
Each key-value pair needs an extra 16 bytes for the ATE, which makes it possible to store 11 pairs in each sector (:math:`\frac{944}{80}`).
Total data that that can be stored in this partition in this case is :math:`11 \times 3 \times 64 = 2112 \text{ bytes}`.

Wear leveling
*************

This storage system is optimized for devices that do not require an erase.

BM_ZMS uses a cycle count mechanism that avoids emulating erase operations for these devices.
It also guarantees that every memory location is written to only once for each cycle of a sector write.

The garbage collection operation also reduces the memory cell life expectancy as it performs write operations when moving blocks from one sector to another.
To prevent the garbage collector from affecting the life expectancy of the device, it is recommended to set the size of the partition appropriately.
Its size should be the double of the maximum size of data (including headers) that could be written in the storage.

See `Available space for user data <#available-space-for-user-data-key-value-pairs>`_.

Device lifetime calculation
===========================

Storage devices, whether they are traditional flash or newer technologies like RRAM/MRAM, have a limited life expectancy which is determined by the number of times memory cells can be erased and written to.

Flash devices are erased one page at a time as part of their functional behavior (otherwise memory cells cannot be overwritten), and for storage devices that do not require an erase operation, memory cells can be overwritten directly.

The following is a typical scenario that allows to calculate the life expectancy of a device:

Assuming that an 8-byte variable is stored using the same ID but its content changes every minute.
The partition has 4 sectors with 1024 bytes each.
Each write of the variable requires 16 bytes of storage.
As there are 944 bytes available for ATEs for each sector, and because BM_ZMS is a fast-forward storage system, the first location of the first sector is going to be rewritten after :math:`\frac{(944 \times 4)}{16} = 236 \text{minutes}`.

In addition to the normal writes, the garbage collector will move the data that is still valid from old sectors to new ones.
The same ID and a big partition size are being used and thus no data will be moved by the garbage collector in this case.
For storage devices that can be written to 20000 times, the storage will last about 4 720 000 minutes (~9 years).

For a more general formula, it is first necessary to compute the effective the used size in BM_ZMS by a typical set of data.
For ID/data pairs with data <= 8 bytes, ``effective_size`` is 16 bytes.
For ID/data pairs with data > 8 bytes, ``effective_size`` is ``16 + sizeof(data)`` bytes.
Assuming that ``total_effective_size`` is the total size of the data that is written in the storage, and that the partition is sized appropriately (double of the effective size) to avoid having the garbage collector moving blocks all the time, the expected lifetime of the device in minutes can be computed as:

.. math::

   \small\frac{(SECTOR\_EFFECTIVE\_SIZE \times SECTOR\_NUMBER \times MAX\_NUM\_WRITES)}{(TOTAL\_EFFECTIVE\_SIZE \times WR\_MIN)}

Where:

``SECTOR_EFFECTIVE_SIZE``: The sector size minus the header size (80 bytes)

``SECTOR_NUMBER``: The number of sectors

``MAX_NUM_WRITES``: The life expectancy of the storage device in number of writes

``TOTAL_EFFECTIVE_SIZE``: Total effective size of the set of written data

``WR_MIN``: Number of writes of the set of data per minute

Features
********

The current version of this library offers the following features:

**Version 1**

* Support for storage devices that do not require an erase operation (only one write operation to invalidate a sector).
* Support for large partition and sector sizes (64-bit address space).
* Support for 32-bit IDs.
* Small-sized data (<= 8 bytes) are stored in the ATE itself.
* Built-in data CRC32 (included in the ATE).
* Versioning of BM_ZMS (to handle future evolutions).
* Support for large ``write-block-size`` (only for platforms that need it).

Recommendations to increase performance
***************************************

Sector size and count
=====================

* The total size of the storage partition should be set appropriately to achieve the best performance with BM_ZMS.
  All the information regarding the effectively available free space in BM_ZMS can be found in the documentation.
  See `Available space for user data <#available-space-for-user-data-key-value-pairs>`_.
  It is recommended to choose a storage partition size that is double the size of the key-value pairs that will be written in the storage.

* The sector size must be set in such a way that a sector can fit the maximum data size that will be stored.
  Increasing the sector size will slow down the garbage collection operation and make it occur less frequently.
  Decreasing its size, on the opposite, will make the garbage collection operation faster but also occur more frequently.

* Storing small data (<= 8 bytes) in BM_ZMS entries can increase the performance, as this data is written within the entry.

Cache size
==========

* When using the BM_ZMS API directly, the recommendation for the cache size is to make it at least equal to the number of different entries that will be written in the storage.

* Each additional cache entry will add 8 bytes to your RAM usage.
  Cache size should be carefully chosen.

API Reference
*************

| Header file: :file:`include/bm/bm_zms.h`
| Source files: :file:`lib/bm_zms/`

:ref:`Bare Metal Zephyr Memory Storage API reference <api_ble_bm_zms>`
