.. _lib_bm_spi_mngr:

SPI transaction manager
#######################

.. contents::
   :local:
   :depth: 2

This library runs SPI master (SPIM) work on a single hardware instance, one job at a time.

Overview
********

Applications describe work as *transactions*, where each transaction is one or more *transfers* (TX/RX steps) that run in order on the bus.
The library keeps pending transactions in a FIFO queue and runs them one after another, so the application can keep scheduling work without waiting for the bus to be free.

The library sits on top of the nrfx SPIM driver and offers both a non-blocking mode that queues work and returns immediately, and a blocking mode that waits until the work is done.
Both share the same queue and scheduling engine.

Each transaction can use the default SPIM configuration or supply its own.
This makes it possible to share one SPIM instance between several devices on the same bus, for example by giving each device its own chip select pin.

Configuration
*************

Set the :kconfig:option:`CONFIG_BM_SPI_MNGR` Kconfig option to enable the library.

The option depends on :kconfig:option:`CONFIG_NRFX_SPIM` and selects :kconfig:option:`CONFIG_RING_BUFFER` for the internal queue.

Initialization
==============

The manager instance is declared using the :c:macro:`BM_SPI_MNGR_DEF` macro, specifying the instance name, queue size, and SPIM instance.
The queue size is the number of transactions that can wait in the queue, not counting the one currently running.

Before initializing, connect and enable the SPIM interrupt for the chosen instance, for example with :c:macro:`BM_IRQ_DIRECT_CONNECT`.
The interrupt handler must forward the event to the nrfx SPIM driver.

To initialize the manager, call the :c:func:`bm_spi_mngr_init` function with an :c:type:`nrfx_spim_config_t` configuration, created with :c:macro:`NRFX_SPIM_DEFAULT_CONFIG` and customized as needed.

The following example shows how to declare, connect, and initialize a manager instance:

.. code-block:: c

   #include <bm/bm_spi_mngr.h>

   BM_SPI_MNGR_DEF(spi_mgr, 4, SPIM_INST);

   static nrfx_spim_config_t spim_cfg = NRFX_SPIM_DEFAULT_CONFIG(PIN_SCK, PIN_MOSI, PIN_MISO, PIN_CSN);

   ISR_DIRECT_DECLARE(spim_isr)
   {
           nrfx_spim_irq_handler(spi_mgr.spim);
           return 0;
   }

   BM_IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(SPIM_INST), IRQ_PRIO_LOWEST, spim_isr, 0);
   irq_enable(NRFX_IRQ_NUMBER_GET(SPIM_INST));

   bm_spi_mngr_init(&spi_mgr, &spim_cfg);

You can uninitialize a manager instance with the :c:func:`bm_spi_mngr_uninit` function.
Do not call it while a transaction is running or while a caller is blocked in :c:func:`bm_spi_mngr_perform`, as it does not cancel pending work or release blocked callers.

Usage
*****

Work is described in a :c:struct:`bm_spi_mngr_transaction` structure, holding an array of transfers and the number of transfers.
Use the :c:macro:`BM_SPI_MNGR_TRANSFER` macro to set up each transfer.

A transaction can optionally provide a begin callback, an end callback, and a per-transaction SPIM configuration.
Both callbacks may run from the SPIM interrupt handler, so keep them short, for example setting a flag.

.. note::

   The transaction descriptor and any configuration it points to must stay valid until the transaction finishes, because the library stores only a pointer to it.

The following is a list of operations you can perform with this library.

Schedule (non-blocking)
=======================

Use the :c:func:`bm_spi_mngr_schedule` function to add a transaction to the queue and return immediately.
The transaction starts at once if the bus is idle, otherwise it runs after the transactions ahead of it.
The completion of the transaction is reported by the optional end callback.

Perform (blocking)
==================

Use the :c:func:`bm_spi_mngr_perform` function to schedule a single transaction and wait until it completes.

While waiting, the function can call an optional idle function repeatedly.
Because the function blocks until the transaction finishes, do not call it from an interrupt handler.

Busy state
==========

Use the :c:func:`bm_spi_mngr_is_idle` function to check whether all scheduled work has finished.

Dependencies
************

This library has the following |BMshort| dependencies:

* nrfx SPIM - :kconfig:option:`CONFIG_NRFX_SPIM`
* Zephyr ring buffer - :kconfig:option:`CONFIG_RING_BUFFER`

API documentation
*****************

| Header file: :file:`include/bm/bm_spi_mngr.h`
| Source files: :file:`lib/bm_spi_mngr/`

:ref:`SPI transaction manager API reference <api_bm_spi_mngr>`
