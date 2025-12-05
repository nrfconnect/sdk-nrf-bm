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

SoftDevice backend options:

* :kconfig:option:`CONFIG_BM_STORAGE_BACKEND_SD_QUEUE_SIZE` – Queue size for pending operations.
* :kconfig:option:`CONFIG_BM_STORAGE_BACKEND_SD_MAX_RETRIES` – Maximum retries if the SoftDevice is busy.

Initialization
==============

Each storage instance is represented by a :c:struct:`bm_storage` structure.

To initialize a storage instance, use the :c:func:`bm_storage_init` function, providing a configuration struct :c:struct:`bm_storage_config` with the following information:

* :c:member:`bm_storage_config.evt_handler` – Event callback.
* :c:member:`bm_storage_config.start_addr` and :c:member:`bm_storage_config.end_addr` – Accessible address range.

You can uninitialize a storage instance with the :c:func:`bm_storage_uninit` function.

Usage
*****

The following is a list of operations you can perform with this library.

Read
====

Use the :c:func:`bm_storage_read` function to copy data from NVM into RAM.
The data length must be a multiple of the backend’s program unit and within the configured region.

.. note::

   The program unit is the smallest block of data that the backend can write in a single operation.
   Both the destination address and the length must be aligned to this size.
   The program unit is reported by :c:member:`bm_storage_info.program_unit`.

Write
=====

Use the :c:func:`bm_storage_write` function to write data to NVM.
Writes are validated against alignment and range, and completion is reported through :c:member:`bm_storage.evt_handler`.

.. note::

   Writes must be aligned to the backend’s program unit, reported by :c:member:`bm_storage_info.program_unit`.
   This ensures that both the write address and the write length are correct for the underlying memory technology.

Erase
=====

Use the :c:func:`bm_storage_erase` function to erase a region in NVM.
``len`` must be a multiple of the erase unit.
If not supported by the backend, the call may return ``NRF_ERROR_NOT_SUPPORTED``.
This means that the backend does not require the region to be erased before another write operation.

.. note::

   The erase unit is the minimum erasable block in NVM.
   Erase operations must start at an address aligned by the erase unit and use a length that is a multiple of this value.
   The erase unit is reported by :c:member:`bm_storage_info.erase_unit`.

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

| Header file: :file:`include/bm_storage.h`
| Source files: :file:`lib/bm_storage/`

:ref:`Storage library API reference <api_storage>`
