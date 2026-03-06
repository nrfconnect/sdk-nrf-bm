.. _nrf_bm_release_notes_1099:

Changelog for |BMlong| v1.0.99
##############################

This changelog reflects the most relevant changes from the latest official release.

Changelog
*********

The following sections provide detailed lists of changes by component.

SDK installation
================

* Updated the steps to install prerequisites in the :ref:`install_nrf_bm` page.
  Installation of the recommended version of SEGGER J-Link is now handled by `nRF Connect for Desktop`_.

S115 SoftDevice
===============

* Updated the SoftDevice to v10.0.0-1.prototype.
  See the SoftDevice release notes for details.

S145 SoftDevice
===============

* Updated the SoftDevice to v10.0.0-1.prototype.
  See the SoftDevice release notes for details.

SoftDevice Handler
==================

* Updated the :c:macro:`NRF_SDH_STATE_EVT_OBSERVER` and :c:macro:`NRF_SDH_STACK_EVT_OBSERVER` macros to not declare the handler prototype.

Boards
======

* Added experimental support for the following boards:

   * PCA10188 (`nRF54LV10 DK`_)
   * PCA10184 (`nRF54LM20 DK`_)
   * PCA10214 (nRF54LS05 DK)

* Updated:

   * The SRAM sizes for the ``bm_nrf54l15dk`` board target to not overlap with VPR context.
   * The board memory layout for all boards to align with the new SoftDevice.
   * The LPUARTE pins in ``board-config.h`` for ``bm_nrf54l15dk`` to avoid conflicts for LEDs, buttons and other peripherals where possible.

* Disabled all Peripheral Resource Sharing (PRS) boxes for the ``bm_nrf54l15dk`` board variants.

Build system
============

* Updated the Kconfig dependencies, including those for drivers, libraries, and Bluetooth LE services.

DFU
===

No changes since the latest nRF Connect SDK Bare Metal release.

Interrupts
==========

Added the :c:macro:`BM_IRQ_DIRECT_CONNECT` and :c:macro:`BM_IRQ_SET_PRIORITY` macros that should be used when connecting IRQ and setting IRQ priority.

Logging
=======

No changes since the latest nRF Connect SDK Bare Metal release.

Drivers
=======

* :ref:`driver_lpuarte` driver:

   * Updated to use the :ref:`lib_bm_gpiote` library.
   * Removed the ``gpiote_inst`` and ``gpiote_inst_num`` members from the :c:struct:`bm_lpuarte_config` structure.

Subsystems
==========

Storage
-------

* :ref:`lib_storage`:

   Added:
     * The capability to compile more than one backend. An instance can be configured to use a specific backend by using the :c:member:`bm_storage_config.api` field.
     * The :c:member:`bm_storage_info.wear_unit` field to represent the NVM wear granularity.
     * The :c:func:`bm_storage_nvm_info_get` function to retrieve NVM information, such as the size of the program unit and other.
     * The :c:member:`bm_storage_config.is_wear_aligned` configuration flag to enforce wear unit alignment on operations.
     * The :c:member:`bm_storage_config.is_write_padded` configuration flag to automatically pad write operations to the alignment unit.

   Updated:
     * The :c:func:`bm_storage_is_busy` function to return ``false`` instead of ``true`` when called with a ``NULL`` pointer or an uninitialized instance.
     * The :c:func:`bm_storage_init` function to return an error when the instance is already initialized.
     * The SoftDevice, RRAM, and native_sim backends to support deinitialization.
     * The SoftDevice, RRAM, and native_sim backends to support the erase operation.
     * The :c:member:`bm_storage_info.erase_value` field from a ``uint32_t`` to a ``uint8_t``.
     * The SoftDevice backend to support chunking of write operations.
     * The ``no_explicit_erase`` field in :c:struct:`bm_storage_info` has been renamed to ``is_erase_before_write`` to explicitly convey that the memory must be erased before it can be written to.
     * The SoftDevice backend's :c:member:`bm_storage_info.program_unit` from 16 to 4 bytes, reflecting the true minimum programmable unit.
     * The ``start_addr`` and ``end_addr`` fields in :c:struct:`bm_storage_config` and :c:struct:`bm_storage` have been replaced by ``addr`` and ``size``. The API now uses relative addressing (0-based offsets within the partition).
     * The :c:func:`bm_storage_write` and :c:func:`bm_storage_erase` functions to return ``-ENOMEM`` when out of memory, instead of ``-EIO``.
     * The :c:func:`bm_storage_read`, :c:func:`bm_storage_write`, and :c:func:`bm_storage_erase` functions to return ``-EINVAL`` on alignment errors, instead of ``-EFAULT``.
     * The :c:enum:`bm_storage_evt_dispatch_type` enum and the :c:member:`bm_storage_evt.dispatch_type` field have been replaced by a boolean :c:member:`bm_storage_evt.is_async`.

Filesystem
----------

* :ref:`lib_bm_zms`

   * Added the :c:member:`bm_zms_fs_config.storage_api` field to select the storage backend API.

Libraries
=========

* Added the :ref:`lib_bm_gpiote` library.

* :ref:`lib_peer_manager` library:

   * Added the :kconfig:option:`CONFIG_BLE_GATT_DB_MAX_CHARS` Kconfig option for configuring the number of characteristics in a :c:struct:`ble_gatt_db_srv` structure.
     The number of characteristics was previously set using the :c:macro:`BLE_GATT_DB_MAX_CHARS` definition in the :file:`ble_gatt_db.h` file.
   * Fixed spelling of the ``characteristics`` member of the :c:struct:`ble_gatt_db_srv` structure.
   * Updated the ``params`` union field of the :c:struct:`pm_evt` structure to an anonymous union.
   * Removed the ``CONFIG_MBEDTLS_PSA_STATIC_KEY_SLOT_BUFFER_SIZE`` Kconfig option.
     The PSA Crypto core can deduce the key slot buffer size based on the keys enabled in the build, so there is no need to define the size manually.

* :ref:`lib_bm_buttons` library:

   * Updated to use the :ref:`lib_bm_gpiote` library.

* :ref:`lib_ble_adv` library:

   * Added:

      * The ``const`` keyword to the configuration structure parameter of the :c:func:`ble_adv_init` function to reflect that the function only reads from the configuration and does not modify it.
      * The advertising name to the configuration structure of the :c:func:`ble_adv_init` function.

   * Updated:

      * The :kconfig:option:`CONFIG_BLE_ADV_EXTENDED_ADVERTISING` Kconfig option to be disabled by default and dependent on the new :kconfig:option:`CONFIG_SOFTDEVICE_EXTENDED_ADVERTISING` Kconfig option.
      * The :kconfig:option:`CONFIG_BLE_ADV_DIRECTED_ADVERTISING` Kconfig option to be disabled by default.

   * Removed:

      * The ``CONFIG_BLE_ADV_NAME`` Kconfig option.
        Instead, the application must set the device name by calling the :c:func:`sd_ble_gap_device_name_set` function.

   * Fixed:

      * An issue causing fast advertising with allow list to incorrectly send event :c:enumerator:`BLE_ADV_EVT_FAST` when it should have sent event :c:enumerator:`BLE_ADV_EVT_FAST_ALLOW_LIST`.
      * An issue causing slow advertising with allow list to incorrectly send event :c:enumerator:`BLE_ADV_EVT_SLOW` when it should have sent event :c:enumerator:`BLE_ADV_EVT_SLOW_ALLOW_LIST`.
      * An issue in the data module where the short name would not be matched in certain cases.

* :ref:`lib_ble_db_discovery` library:

   The library is now supported.

   * Updated:

      * The ``params`` union field of the :c:struct:`ble_db_discovery_evt` structure to an anonymous union.
      * The :c:enumerator:`BLE_DB_DISCOVERY_COMPLETE` event to pass the service discovery result by-reference instead of by-value.
        This removes one copy operation for each :c:enumerator:`BLE_DB_DISCOVERY_COMPLETE` event and significantly reduces the size of both :c:struct:`ble_db_discovery_evt` and :c:struct:`ble_db_discovery` structures.

   * Fixed:

      * An issue where starting a second round of discovery, after either a disconnect or a completed discovery, could result in incorrect internal state.
        This could eventually lead to insufficient memory for discovering new services.

   * Removed:

      * The ``ble_db_discovery_user_evt`` structure after a rework.

* :ref:`lib_ble_scan` library:

   * Added the :c:struct:`ble_scan_filter_data` structure as input to the :c:func:`ble_scan_filter_add` function.
   * Updated:

      * The functions to use the ``uint32_t`` type instead of ``int`` when returning nrf_errors.
      * The ``params`` union field of the :c:struct:`ble_scan_evt` structure to an anonymous union.
      * The ``allow_list_used`` function was renamed to :c:func:`ble_scan_is_allow_list_used`.
      * The name of the :c:type:`ble_gap_evt_adv_report_t` fields in the :c:struct:`ble_scan_evt` struct to ``adv_report``.

   * Fixed an issue with active scanning where the multifilter match was used.
     A match would not be triggered unless the data for all types of enabled filters were provided in either the advertising or scan response data.
     Now the data can be provided in a mix of the advertising and scan response data.

* Bluetooth LE Connection state library:

   * Deprecated the library.
     Applications and other libraries and services should keep an internal state of required connections instead.

Bluetooth LE Services
---------------------

* Added the :c:member:`ble_cgms_config.initial_comm_interval` to the :c:struct:`ble_cgms_config` structure to set the initial communication interval.
* Renamed the Bluetooth: Heart Rate Service Central (``ble_hrs_central``) to the :ref:`lib_ble_service_hrs_client` sample.
* Updated all services to return errors from the SoftDevice directly.
* Removed the BMS authorization code Kconfig options (:kconfig:option:`CONFIG_BLE_BMS_AUTHORIZATION_CODE` and :kconfig:option:`CONFIG_BLE_BMS_USE_AUTHORIZATION_CODE`) from the service library, as they are only used by the BMS sample.

* :ref:`lib_ble_service_hrs_client` service:

   * Added:

      * The :c:func:`ble_hrs_client_hrm_notif_disable` function to disable Heart Rate Measurement notifications.
      * Validation of the :c:member:`ble_hrs_client_config.evt_handler` and :c:member:`ble_hrs_client_config.gatt_queue` configuration fields in :c:func:`ble_hrs_client_init`.
      * State validation in :c:func:`ble_hrs_client_hrm_notif_enable` and :c:func:`ble_hrs_client_hrm_notif_disable`, returning :c:macro:`NRF_ERROR_INVALID_STATE` when the connection or CCCD handle has not been assigned.

   * Fixed a potential buffer over-read when parsing malformed Heart Rate Measurement notifications.

* :ref:`lib_ble_service_hids` service:

   * Updated the ``params`` union field of the :c:struct:`ble_hids_evt` structure to an anonymous union.

* :ref:`lib_ble_service_hrs_client` service:

   * Updated the ``params`` union field of the :c:struct:`ble_hrs_client_evt` structure to an anonymous union.

Libraries for NFC
-----------------

* The NFC subsystem code has been migrated to |BMshort| and does not reuse code form |NCS| anymore.
  The NFC related Kconfig options provided by |BMshort| have the ``BM_NFC_`` prefix.
  The following list shows mapping from |NCS| Kconfig options to |BMshort| Kconfig options:

  * ``CONFIG_NFCT_IRQ_PRIORITY`` --> :kconfig:option:`CONFIG_BM_NFCT_IRQ_PRIORITY`
  * ``CONFIG_NFC_PLATFORM_LOG_LEVEL*`` --> :kconfig:option:`CONFIG_BM_NFC_PLATFORM_LOG_LEVEL*`
  * ``CONFIG_NFC_NDEF*`` --> :kconfig:option:`CONFIG_BM_NFC_NDEF*`
  * ``CONFIG_NFC_T4T_NDEF_FILE`` --> :kconfig:option:`CONFIG_BM_NFC_T4T_NDEF_FILE`

  Use ``#include <bm/nfc/..>`` to include NFC related header files provided by |BMshort| instead of ``#include <nfc/...>``.

* Added NFC libraries for NFC Connection Handover and Bluetooth LE Out-of-Band (OOB) pairing.

Samples
=======

* Updated the prefix for sample Kconfig options from ``APP`` to ``SAMPLE``.

* Removed the battery measurement simulation from the following samples:

   * :ref:`ble_cgms_sample`
   * :ref:`ble_hids_mouse_sample`
   * :ref:`ble_hids_keyboard_sample`

* Aligned LED and button behavior across samples.

Bluetooth LE samples
--------------------

* Added the :ref:`peripheral_nfc_pairing_sample`.

* :ref:`ble_bms_sample` sample:

   * Added sample-specific Kconfig options for the BMS authorization code by moving them from the service library scope and renaming them from ``CONFIG_BLE_BMS_AUTHORIZATION_CODE`` and ``CONFIG_BLE_BMS_USE_AUTHORIZATION_CODE`` to :kconfig:option:`CONFIG_APP_BLE_BMS_AUTHORIZATION_CODE` and :kconfig:option:`CONFIG_APP_BLE_BMS_USE_AUTHORIZATION_CODE`.

* :ref:`ble_hrs_sample` sample:

   * Added support for the new board targets:

     * PCA10188 (`nRF54LV10 DK`_)
     * PCA10184 (`nRF54LM20 DK`_)
     * PCA10214 (nRF54LS05 DK)

* :ref:`ble_hids_keyboard_sample` sample:

   * Added support for boot mode.

* :ref:`ble_hids_mouse_sample` sample:

   * Fixed an issue where the sample did not enter or exit boot mode properly based on the HID events.

* :ref:`ble_nus_sample` sample:

   * Added support for the new board targets:

     * PCA10188 (`nRF54LV10 DK`_)
     * PCA10184 (`nRF54LM20 DK`_)
     * PCA10214 (nRF54LS05 DK)

   * Updated to align with changes to the :ref:`driver_lpuarte` driver.

* :ref:`ble_pwr_profiling_sample` sample:

   * Updated to use a dedicated variable to hold the service attribute handle instead of incorrectly using the connection handle variable for this during service initialization.

NFC samples
-----------

* Updated to use ``CONFIG_BM_NFC_*`` Kconfig options provided by |BMshort| instead of ``CONFIG_NFC_*`` options provided by |NCS|.
  Use ``#include <bm/nfc/...>`` headers provided by |BMshort| instead of ``#include <nfc/...>`` headers from |NCS|.

Peripheral samples
------------------

* Added the :ref:`bm_ppi_sample` sample.

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

* Added the :ref:`drivers_alloc_int` page.
* Improved sample documentation with clearer, more descriptive user guides, including updated explanations of configuration options, hardware connections, and testing procedures.
* Added the :ref:`board_memory_layouts` section, which documents RRAM and SRAM partition layouts for supported boards.
