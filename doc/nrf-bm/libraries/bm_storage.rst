.. _lib_storage:

Storage Library
###############

.. contents::
   :local:
   :depth: 2

The Storage library provides abstractions for reading, writing, and erasing non-volatile memory (NVM).

Overview
********

The library supports multiple storage instances, each bound to a specific memory region, and reports operation results through user-defined event handlers.
Depending on the backend and runtime state, operations may be synchronous or asynchronous.

Configuration
*************

The library is enabled and configured entirely using the Kconfig system.
Set the :kconfig:option:`CONFIG_BM_STORAGE` Kconfig option to enable the library.

Select a storage backend by enabling one of the following Kconfig options:

* :kconfig:option:`CONFIG_BM_STORAGE_BACKEND_RRAM` – RRAM backend. The events reported are synchronous.
* :kconfig:option:`CONFIG_BM_STORAGE_BACKEND_SD` – SoftDevice backend. The events reported are asynchronous.

The selected backend's API instance is made available through the :file:`include/bm/storage/bm_storage_backends.h` header, which is included transitively by :file:`include/bm/storage/bm_storage.h`.

SoftDevice backend options:

* :kconfig:option:`CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE` – Queue size for pending operations.
* :kconfig:option:`CONFIG_BM_STORAGE_BACKEND_SD_MAX_RETRIES` – Maximum retries if the SoftDevice is busy.
* :kconfig:option:`CONFIG_BM_STORAGE_BACKEND_SD_MAX_WRITE_SIZE` - Maximum number of bytes to write in a single call to ``sd_flash_write()``.
* :kconfig:option:`CONFIG_BM_STORAGE_BACKEND_SD_ERASE_BLOCKS` – Batch erase operations up to ``_MAX_WRITE_SIZE`` bytes at once.

Initialization
==============

Each storage instance is represented by a :c:struct:`bm_storage` structure.

To initialize a storage instance, use the :c:func:`bm_storage_init` function, providing a configuration struct :c:struct:`bm_storage_config` with the following information:

* :c:member:`bm_storage_config.evt_handler` – Event callback.
* :c:member:`bm_storage_config.api` – Backend API implementation (for example, ``&bm_storage_sd_api`` or ``&bm_storage_rram_api``).
* :c:member:`bm_storage_config.start_addr` and :c:member:`bm_storage_config.end_addr` – Accessible address range.

The following example shows how to initialize a storage instance with a backend API:

.. code-block:: c

   #include <bm/storage/bm_storage.h>

   struct bm_storage storage;
   struct bm_storage_config config = {
       .evt_handler = my_handler,
       .api = &bm_storage_sd_api,
       .start_addr = START,
       .end_addr = END,
   };

   bm_storage_init(&storage, &config);

You can uninitialize a storage instance with the :c:func:`bm_storage_uninit` function.

Usage
*****

The following is a list of operations you can perform with this library.

Read
====

Use the :c:func:`bm_storage_read` function to copy data from NVM into RAM.

Write
=====

Use the :c:func:`bm_storage_write` function to write data to NVM.
The completion of the operation is reported by the :c:enum:`BM_STORAGE_EVT_WRITE_RESULT` event.

.. note::

   The program unit is the minimum programmable block in NVM.
   Write operations must start at an address aligned by the program unit and use a length that is a multiple of this value.

   If :c:member:`bm_storage_config.flags.is_wear_aligned` is set during initialization, the wear unit (:c:member:`bm_storage_info.wear_unit`) is used for alignment instead.

Erase
=====

Use the :c:func:`bm_storage_erase` function to erase a region in NVM.
The completion of the operation is reported by the :c:enum:`BM_STORAGE_EVT_ERASE_RESULT` event.

.. note::

   The erase unit is the minimum erasable block in NVM.
   Erase operations must start at an address aligned by the erase unit and use a length that is a multiple of this value.
   The erase unit is reported by :c:member:`bm_storage_info.erase_unit`.

When the erase operation is not supported by the hardware, the backend will emulate it by writing the memory’s erased value to the NVM area.
If the erase operation is emulated, the alignment requirements are the same as those for the write operation.

Busy state
==========

Use the :c:func:`bm_storage_is_busy` function to check whether a backend is executing an operation.

Events
======

The following events may be reported to the user callback:

* :c:enum:`BM_STORAGE_EVT_WRITE_RESULT` – Write operation completed.
* :c:enum:`BM_STORAGE_EVT_ERASE_RESULT` – Erase operation completed.

Each event includes the result code, information about the address range of the associated operation, and whether the operation is synchronous or asynchronous.

Sample
******

The usage of this library is demonstrated in the :ref:`bm_storage_sample` sample.

Dependencies
************

* :kconfig:option:`CONFIG_BM_STORAGE_BACKEND_RRAM`:

  This backend requires the following Kconfig options to be disabled:

  * :kconfig:option:`CONFIG_SOFTDEVICE`

* :kconfig:option:`CONFIG_BM_STORAGE_BACKEND_SD`:

  This backend requires the following Kconfig options to be enabled:

  * :kconfig:option:`CONFIG_SOFTDEVICE`
  * :kconfig:option:`CONFIG_NRF_SDH`
  * :kconfig:option:`CONFIG_RING_BUFFER`

API documentation
*****************

| Header file: :file:`include/bm/storage/bm_storage.h`
| Header file: :file:`include/bm/storage/bm_storage_backends.h`
| Source files: :file:`lib/bm_storage/`

:ref:`Storage library API reference <api_storage>`
