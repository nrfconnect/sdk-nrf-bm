.. _nrf_bm_release_notes_1099:

Changelog for |BMlong| v1.0.99
##############################

The most relevant changes that are present on the main branch of the nRF Connect SDK Bare Metal, as compared to the latest official release, are tracked in this file.

.. note::

   This file is a work in progress and might not cover all relevant changes.

Changelog
*********

The following sections provide detailed lists of changes by component.

SDK installation
================

No changes since the latest nRF Connect SDK Bare Metal release.

S115 SoftDevice
===============

No changes since the latest nRF Connect SDK Bare Metal release.

S145 SoftDevice
===============

No changes since the latest nRF Connect SDK Bare Metal release.

SoftDevice Handler
==================

* Updated the :c:macro:`NRF_SDH_STATE_EVT_OBSERVER` and :c:macro:`NRF_SDH_STACK_EVT_OBSERVER` macros to not declare the handler prototype.

Boards
======

No changes since the latest nRF Connect SDK Bare Metal release.

Build system
============

* Updated the Kconfig dependencies, including those for drivers, libraries, and Bluetooth LE services.

DFU
===

No changes since the latest nRF Connect SDK Bare Metal release.

Interrupts
==========

No changes since the latest nRF Connect SDK Bare Metal release.

Logging
=======

No changes since the latest nRF Connect SDK Bare Metal release.

Drivers
=======

* :ref:`driver_lpuarte`:

   * Updated to use the :ref:`lib_bm_gpiote` library.
   * Removed the ``gpiote_inst`` and ``gpiote_inst_num`` members from the :c:struct:`bm_lpuarte_config` structure.

Subsystems
==========

Storage
=======

* :ref:`lib_storage`:

   Added:
     * The capability to compile more than one backend.
     * The :c:func:`bm_storage_nvm_info_get` function to retrieve NVM information, such as the size of the program unit and other.

   Updated:
     * The :c:func:`bm_storage_is_busy` function to return ``false`` instead of ``true`` when called with a ``NULL`` pointer or an uninitialized instance.
     * The :c:func:`bm_storage_init` function to return an error when the instance is already initialized.
     * The SoftDevice, RRAM, and native_sim backends to support deinitialization.
     * The SoftDevice, RRAM, and native_sim backends to support the erase operation.
     * The SoftDevice backend to support chunking of write operations.
     * The :c:func:`bm_storage_write` and :c:func:`bm_storage_erase` functions to return ``-ENOMEM`` when out of memory, instead of ``-EIO``.
     * The :c:func:`bm_storage_read`, :c:func:`bm_storage_write`, and :c:func:`bm_storage_erase` functions to return ``-EINVAL`` on alignment errors, instead of ``-EFAULT``.
     * The :c:enum:`bm_storage_evt_dispatch_type` enum and the :c:member:`bm_storage_evt.dispatch_type` field have been replaced by a boolean :c:member:`bm_storage_evt.is_async`.

Libraries
=========

* :ref:`lib_peer_manager` library:

   * Removed the ``CONFIG_MBEDTLS_PSA_STATIC_KEY_SLOT_BUFFER_SIZE`` Kconfig option.
     The PSA Crypto core can deduce the key slot buffer size based on the keys enabled in the build, so there is no need to define the size manually.

* Added the :ref:`lib_bm_gpiote` library.

* :ref:`lib_bm_buttons` library:

   * Updated to use the :ref:`lib_bm_gpiote` library.

* :ref:`lib_ble_adv`:

   * Added the ``const`` keyword to the configuration structure parameter of the :c:func:`ble_adv_init` function to reflect that the function only reads from the configuration and does not modify it.

   * Fixed:

      * An issue causing fast advertising with allow list to incorrectly send event :c:enumerator:`BLE_ADV_EVT_FAST` when it should have sent event :c:enumerator:`BLE_ADV_EVT_FAST_ALLOW_LIST`.
      * An issue causing slow advertising with allow list to incorrectly send event :c:enumerator:`BLE_ADV_EVT_SLOW` when it should have sent event :c:enumerator:`BLE_ADV_EVT_SLOW_ALLOW_LIST`.

   * Updated:

      * Changed the :kconfig:option:`CONFIG_BLE_ADV_EXTENDED_ADVERTISING` Kconfig option to be disabled by default and dependent on the new :kconfig:option:`CONFIG_SOFTDEVICE_EXTENDED_ADVERTISING` Kconfig option.
      * Changed the :kconfig:option:`CONFIG_BLE_ADV_DIRECTED_ADVERTISING` Kconfig option to be disabled by default.

* :ref:`lib_ble_scan`:

   * Updated functions to use the ``uint32_t`` type instead of ``int`` when returning nrf_errors.

Bluetooth LE Services
---------------------

* Renamed the ``ble_hrs_central`` service to the :ref:`lib_ble_service_hrs_client` sample.
* Updated all services to return errors from the SoftDevice directly.

Libraries for NFC
-----------------

* The NFC subsystem code has been migrated to |BMshort| and does not reuse code form |NCS| anymore.
  The NFC related Kconfig options provided by |BMshort| have the ``BM_NFC_`` prefix.
  The following list shows mapping from |NCS| Kconfig options to |BMshort| Kconfig options:

  * ``CONFIG_NFCT_IRQ_PRIORITY`` ``-->`` :kconfig:option:`CONFIG_BM_NFCT_IRQ_PRIORITY`
  * ``CONFIG_NFC_PLATFORM_LOG_LEVEL*`` ``-->`` :kconfig:option:`CONFIG_BM_NFC_PLATFORM_LOG_LEVEL*`
  * ``CONFIG_NFC_NDEF*`` ``-->`` :kconfig:option:`CONFIG_BM_NFC_NDEF*`
  * ``CONFIG_NFC_T4T_NDEF_FILE`` ``-->`` :kconfig:option:`CONFIG_BM_NFC_T4T_NDEF_FILE`

  Use ``#include <bm/nfc/..>`` to include NFC related header files provided by |BMshort| instead of ``#include <nfc/...>``.

* Added NFC libraries for NFC Connection Handover and Bluetooth LE Out-of-Band (OOB) pairing.

Samples
=======

* Updated the prefix for sample Kconfig options from ``APP`` to ``SAMPLE``.

* Removed the battery measurement simulation from the following samples:

   * :ref:`ble_cgms_sample`
   * :ref:`ble_hids_mouse_sample`
   * :ref:`ble_hids_keyboard_sample`

Bluetooth LE samples
--------------------

* :ref:`ble_hids_keyboard_sample`:

   * Added support for boot mode.

* :ref:`ble_hids_mouse_sample`:

   * Fixed an issue where the sample did not enter or exit boot mode properly based on the HID events.

* :ref:`ble_nus_sample` sample:

   * Updated to align with changes to the :ref:`driver_lpuarte` driver.

* :ref:`ble_pwr_profiling_sample`:

   * Updated to use a dedicated variable to hold the service attribute handle instead of incorrectly using the connection handle variable for this during service initialization.

* :ref:`peripheral_nfc_pairing_sample`:

   * Added the new sample.

NFC samples
-----------

* Use ``CONFIG_BM_NFC_*`` Kconfig options provided by |BMshort| instead of ``CONFIG_NFC_*`` options provided by |NCS|
  Use ``#include <bm/nfc/...>`` headers provided by |BMshort| instead of ``#include <nfc/...>`` headers from |NCS|.

Peripheral samples
------------------

* :ref:`bm_lpuarte_sample` sample:

   * Updated to align with changes to the :ref:`driver_lpuarte` driver.

DFU samples
-----------

No changes since the latest nRF Connect SDK Bare Metal release.

Subsystem samples
-----------------

No changes since the latest nRF Connect SDK Bare Metal release.

Known issues and limitations
============================

No changes since the latest nRF Connect SDK Bare Metal release.

Documentation
=============

No changes since the latest nRF Connect SDK Bare Metal release.
