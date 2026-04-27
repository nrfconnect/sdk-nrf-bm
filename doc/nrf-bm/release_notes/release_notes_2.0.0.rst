.. _nrf_bm_release_notes_200:

Release Notes for |BMlong| v2.0.0
#################################

|BMshort| v2.0.0 is a major release that expands hardware support, adds maturity to key features, and introduces several new libraries, services, and samples.
This release is based on |NCS| v3.3.0.

Highlights
**********

* Added support for three new SoCs with their dedicated development kits: the `nRF54LM20A`_ (`nRF54LM20 DK`_), `nRF54LS05B`_ (`nRF54LS05 DK`_), and `nRF54LV10A`_ (`nRF54LV10 DK`_).
  For more details regarding the maturity of features on those SoCs, refer to :ref:`bm_software_maturity`.
* Updated the SoftDevice to v10.0.0, a new major version that targets both the existing and newly added devices.
  See the :ref:`SoftDevice release notes <softdevice_docs>` for details.
* Expanded the Bluetooth® LE Central role support through additional libraries and samples, including the :ref:`lib_ble_service_bas_client` and :ref:`lib_ble_service_nus_client` services, and the :ref:`ble_nus_central_sample` sample.
* Core libraries for Bluetooth LE Central role applications, :ref:`lib_ble_scan` library and :ref:`lib_ble_db_discovery`, are now Supported.
* Improved the quality and maintainability through fixes and hardening across Bluetooth LE libraries, drivers, DFU, NFC integration, and documentation, including clearer SoftDevice documentation per SoC variant where applicable.

Supported hardware
******************

|BMshort| v2.0.0 provides a set of samples for the following development kits of the nRF54L Series:

* `nRF54L15 DK`_ - with build targets supporting three SoCs: `nRF54L15`_, `nRF54L10`_, and `nRF54L05`_.
* `nRF54LM20 DK`_, supporting the `nRF54LM20A`_ SoC.
* `nRF54LS05 DK`_, supporting the `nRF54LS05B`_ SoC.
* `nRF54LV10 DK`_, supporting the `nRF54LV10A`_ SoC.

SoftDevice variants
*******************

This release includes two versions of the SoftDevice, the Nordic Bluetooth LE protocol stack used on the nRF54L DKs.
Both SoftDevices have been updated to **v10.0.0** in this release:

* **S115** - Supports the peripheral role only.
  Recommended for memory-constrained peripheral-only applications.

* **S145** - Supports both peripheral and central roles, and allows a higher number of simultaneous connections.
  Recommended for applications that act as a central, or that need to handle more than one connection.

Separate documentation and API references are now provided for each SoC-specific variant of the S115 and S145 SoftDevices (nRF54L15, nRF54LM20, nRF54LS05, and nRF54LV10).
See :ref:`softdevice_docs` and :ref:`nrf_bm_api` for details.

Release tag
***********

The release tag for the |BMshort| manifest repository is **v2.0.0**.
Check the :file:`west.yml` file for the corresponding tags in the project repositories.

To use this release, install it using |nRFVSC| by following the instructions in :ref:`install_nrf_bm`.

Alternatively, check out the tag in the manifest repository, run ``west config manifest.path nrf-bm``, and then ``west update``.

This release of |BMshort| is based on |NCS| **v3.3.0**.

IDE and tool support
********************

`nRF Connect for Visual Studio Code`_ is the recommended IDE for |BMshort| v2.0.0.
See the :ref:`Installation <install_nrf_bm>` section for more information about supported operating systems and toolchain.

Changelog
*********

The following sections provide detailed lists of changes by component.

SDK installation
================

* Updated the steps to install prerequisites in the :ref:`install_nrf_bm` page.
  Installation of the recommended version of SEGGER J-Link is now handled by `nRF Connect for Desktop`_.

S115 SoftDevice
===============

* Updated the SoftDevice to v10.0.0.
  See the :ref:`SoftDevice release notes <softdevice_docs>` for details.

S145 SoftDevice
===============

* Updated the SoftDevice to v10.0.0.
  See the :ref:`SoftDevice release notes <softdevice_docs>` for details.

SoftDevice Handler
==================

* Added:

   * The :kconfig:option:`CONFIG_NRF_SDH_LOG_SD_INFO` Kconfig option to log SoftDevice information like version and firmware ID when enabling the SoftDevice.
   * The :kconfig:option:`CONFIG_NRF_SDH_BLE_LOG_SD_RAM_USAGE` Kconfig option to log SoftDevice RAM usage and the application RAM minimum address when enabling Bluetooth LE in the SoftDevice.
   * The :c:func:`nrf_sdh_ble_sd_ram_usage_get` function for returning the number of bytes used or required for SoftDevice RAM.

* Updated:

   * The :c:macro:`NRF_SDH_STATE_EVT_OBSERVER` and :c:macro:`NRF_SDH_STACK_EVT_OBSERVER` macros to not declare the handler prototype.
   * The log error message when there is insufficient RAM for the SoftDevice when enabling Bluetooth LE in the SoftDevice.

Boards
======

* Added support for the following boards:
  See :ref:`bm_software_maturity` for details.

   * PCA10184 (`nRF54LM20 DK`_)
   * PCA10188 (`nRF54LV10 DK`_)
   * PCA10214 (`nRF54LS05 DK`_)

* Updated:

   * The SRAM sizes for the ``bm_nrf54l15dk`` board target to not overlap with VPR context.
   * The board memory layout for all boards to align with the new SoftDevice.
   * The LPUARTE pins in the :file:`board-config.h` file for ``bm_nrf54l15dk`` to avoid conflicts for LEDs, buttons, and other peripherals where possible.
   * The board RAM memory layout for all boards to use a single node for SoftDevice RAM, combining the previously separate static and dynamic nodes.

* Disabled all Peripheral Resource Sharing (PRS) boxes for the ``bm_nrf54l15dk`` board variants.

Build system
============

* Updated the Kconfig dependencies, including those for drivers, libraries, and Bluetooth LE services.

DFU
===

* Added:

   * Experimental support for the nRF54LV10A, nRF54LM20A, and nRF54LS05B SoCs.
   * The :kconfig:option:`CONFIG_BM_MCUMGR_GRP_IMG_BUFFER_SZ` Kconfig option to set the size of the data buffer.
   * The :kconfig:option:`CONFIG_BM_MCUMGR_GRP_IMG_NVM_WRITE_BLOCKS_MAX` Kconfig option to set the maximum number of non-volatile memory wear units written to NVM at once.

* Updated:

   * The maturity status of DFU for the nRF54L15, nRF54L10, and nRF54L05 SoCs from Experimental to Supported.
   * Image upload NVM writes to be handled in synchronization with SD.
   * The support for setting up the DFU Device Bluetooth name remotely.
     This new implementation consumes less RAM and RRAM than the previous one.
     The new solution is enabled by the :kconfig:option:`CONFIG_BM_FLAT_SETTINGS_BLUETOOTH_NAME` Kconfig option.
     It employs the newly added inter-application RAM clipboard storage.

   * Firmware loader to not clear the retained DFU Device Bluetooth name after loading it from retained memory.
     This allows the advertising name to persist after updating SoftDevice and/or firmware loader, or if the application image validation fails and the device reboots into firmware loader.

* Removed:

   * The ``CONFIG_NCS_BM_SETTINGS_BLUETOOTH_NAME`` Kconfig option and zephyr-rtos settings subsystem handlers for setting up the DFU Device Bluetooth name remotely.

Interrupts
==========

* Added the :c:macro:`BM_IRQ_DIRECT_CONNECT` and :c:macro:`BM_IRQ_SET_PRIORITY` macros that should be used when connecting IRQ and setting IRQ priority.

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

   * Added:

     * The capability to compile more than one backend.
       An instance can be configured to use a specific backend by using the :c:member:`bm_storage_config.api` field.
     * The :c:member:`bm_storage_info.wear_unit` field to represent the NVM wear granularity.
     * The :c:func:`bm_storage_nvm_info_get` function to retrieve NVM information, such as the size of the program unit.
     * The :c:member:`bm_storage_config.is_wear_aligned` configuration flag to enforce wear unit alignment on operations.
     * The :c:member:`bm_storage_config.is_write_padded` configuration flag to automatically pad write operations to the alignment unit.
     * The :c:func:`bm_storage_evt_dispatch` function for event delivery.

   * Updated:

     * The :c:func:`bm_storage_is_busy` function to return ``false`` instead of ``true`` when called with a ``NULL`` pointer or an uninitialized instance.
     * The :c:func:`bm_storage_init` function to return an error when the instance is already initialized.
     * The SoftDevice, RRAM, and native_sim backends to support deinitialization.
     * The SoftDevice, RRAM, and native_sim backends to support the erase operation.
     * The :c:member:`bm_storage_info.erase_value` field from ``uint32_t`` to ``uint8_t``.
     * The SoftDevice backend to support chunking of write operations.
     * The ``no_explicit_erase`` field in :c:struct:`bm_storage_info` by renaming it to ``is_erase_before_write`` to explicitly convey that the memory must be erased before it can be written to.
     * The SoftDevice backend's :c:member:`bm_storage_info.program_unit` from 16 to 4 bytes, reflecting the true minimum programmable unit.
     * The ``start_addr`` and ``end_addr`` fields in :c:struct:`bm_storage_config` and :c:struct:`bm_storage` by replacing them with by ``addr`` and ``size``.
       The API now uses relative addressing (0-based offsets within the partition).
     * The :c:func:`bm_storage_write` and :c:func:`bm_storage_erase` functions to return ``-ENOMEM`` when out of memory, instead of ``-EIO``.
     * The :c:func:`bm_storage_read`, :c:func:`bm_storage_write`, and :c:func:`bm_storage_erase` functions to return ``-EINVAL`` on alignment errors, instead of ``-EFAULT``.
     * The :c:enum:`bm_storage_evt_dispatch_type` enum and the :c:member:`bm_storage_evt.dispatch_type` field by replacing them with a boolean :c:member:`bm_storage_evt.is_async`.
     * All backends to use the new :c:func:`bm_storage_evt_dispatch` function for event delivery.
       For backends without an internal operation queue (such as RRAM), enabling :c:func:`bm_scheduler` defers synchronous events to the main thread context.
     * The SoftDevice backend to process operations iteratively instead of recursively when the SoftDevice is disabled.
       Re-entrant calls from event handlers are safely enqueued and processed by the loop.

* ``bm_rmem`` library:

   * Added the new inter-application RAM clipboard storage.
     It is a simple SRAM-based storage mechanism designed to provide data clipboards between applications.
     It can be enabled by the :kconfig:option:`CONFIG_BM_RMEM` Kconfig option.

Filesystem
----------

* :ref:`lib_bm_zms`

   * Added the :c:member:`bm_zms_fs_config.storage_api` field to select the storage backend API.
   * Fixed a bug where a read immediately following a write could start searching for data using an invalid address if the write triggered a sector close.
     A backup read address is now stored before closing a sector to ensure that read operations start from a valid address.

Libraries
=========

* Added the :ref:`lib_bm_gpiote` library.

* :ref:`lib_peer_manager` library:

   * Added:

     * The :kconfig:option:`CONFIG_BLE_GATT_DB_MAX_CHARS` Kconfig option for configuring the number of characteristics in a :c:struct:`ble_gatt_db_srv` structure.
       The number of characteristics was previously set using the :c:macro:`BLE_GATT_DB_MAX_CHARS` definition in the :file:`ble_gatt_db.h` file.
     * A missing word alignment check of the ``data`` parameter to the :c:func:`pm_peer_data_store` function.

   * Updated the ``params`` union field of the :c:struct:`pm_evt` structure to an anonymous union.

   * Fixed:

     * The spelling of the ``characteristics`` member of the :c:struct:`ble_gatt_db_srv` structure.
     * The :c:func:`pm_peer_data_store`, :c:func:`pm_peer_data_remote_db_store`, and :c:func:`pm_peer_data_app_data_store` functions to allow data lengths that are not word aligned.
     * An issue where the :c:func:`pm_peer_delete` function under some circumstances could fail to delete peer data.

   * Removed the ``CONFIG_MBEDTLS_PSA_STATIC_KEY_SLOT_BUFFER_SIZE`` Kconfig option.
     The PSA Crypto core can deduce the key slot buffer size based on the keys enabled in the build, so there is no need to define the size manually.

* :ref:`lib_bm_buttons` library:

   * Updated to use the :ref:`lib_bm_gpiote` library.

* :ref:`lib_ble_adv` library:

   * Added:

     * The ``const`` keyword to the configuration structure parameter of the :c:func:`ble_adv_init` function to reflect that the function only reads from the configuration and does not modify it.
     * The advertising name to the configuration structure of the :c:func:`ble_adv_init` function.
     * The :c:func:`ble_adv_stop` function to stop advertising.

   * Updated:

     * The :kconfig:option:`CONFIG_BLE_ADV_EXTENDED_ADVERTISING` Kconfig option to be disabled by default and dependent on the new :kconfig:option:`CONFIG_SOFTDEVICE_EXTENDED_ADVERTISING` Kconfig option.
     * The :kconfig:option:`CONFIG_BLE_ADV_DIRECTED_ADVERTISING` Kconfig option to be disabled by default.

   * Fixed:

     * An issue causing fast advertising with allow list to incorrectly send event :c:enumerator:`BLE_ADV_EVT_FAST` when it should have sent event :c:enumerator:`BLE_ADV_EVT_FAST_ALLOW_LIST`.
     * An issue causing slow advertising with allow list to incorrectly send event :c:enumerator:`BLE_ADV_EVT_SLOW` when it should have sent event :c:enumerator:`BLE_ADV_EVT_SLOW_ALLOW_LIST`.
     * An issue in the data module where the short name would not be matched in certain cases.

   * Removed:

     * The ``CONFIG_BLE_ADV_NAME`` Kconfig option.
       Instead, the application must set the device name by calling the :c:func:`sd_ble_gap_device_name_set` function.

* :ref:`lib_ble_db_discovery` library:

   * Updated:

     * The maturity status of the library to Supported.
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

     * The maturity status of the library to Supported.
     * The functions to use the ``uint32_t`` type instead of ``int`` when returning nrf_errors.
     * The ``params`` union field of the :c:struct:`ble_scan_evt` structure to an anonymous union.
     * The ``allow_list_used`` function by renaming it to :c:func:`ble_scan_is_allow_list_used`.
     * The name of the :c:type:`ble_gap_evt_adv_report_t` fields in the :c:struct:`ble_scan_evt` struct to ``adv_report``.

   * Fixed an issue with active scanning where the multifilter match was used.
     A match would not be triggered unless the data for all types of enabled filters were provided in either the advertising or scan response data.
     Now, the data can be provided in a mix of the advertising and scan response data.

* :ref:`Bluetooth LE Connection state library <api_ble_conn_state>`:

   * Deprecated the library.
     Applications and other libraries and services should keep an internal state of required connections instead.

Bluetooth LE Services
---------------------

* Added:

  * The :ref:`lib_ble_service_bas_client` service.
  * The :c:member:`ble_cgms_config.initial_comm_interval` to the :c:struct:`ble_cgms_config` structure to set the initial communication interval.

* Updated:

  * The Bluetooth: Heart Rate Service Central (``ble_hrs_central``) by renaming it to the :ref:`lib_ble_service_hrs_client` sample.
  * All services to return errors from the SoftDevice directly.

* Removed the BMS authorization code Kconfig options (:kconfig:option:`CONFIG_BLE_BMS_AUTHORIZATION_CODE` and :kconfig:option:`CONFIG_BLE_BMS_USE_AUTHORIZATION_CODE`) from the service library, as they are only used by the BMS sample.

* :ref:`lib_ble_service_bas`:

   * Updated:

      * The :c:func:`ble_bas_battery_level_update` function to be able to send multiple notifications back to back with the same battery level value.
        Furthermore, the function will now only update the battery level characteristic value if the value changes.
      * The :c:func:`ble_bas_battery_level_update` function to return :c:macro:`NRF_ERROR_INVALID_PARAM` if the battery level value is outside of the valid range of ``0`` to ``100``.
      * The :c:func:`ble_bas_battery_level_update` function with support for notifying the current battery level instead of providing a new battery level value and then notifying.
        This is done by setting the battery level parameter of the :c:func:`ble_bas_battery_level_update` to the value of :c:macro:`BLE_BAS_BATTERY_LEVEL_LAST`.

   * Removed the ``ble_bas_battery_level_notify`` function in favor of the :c:func:`ble_bas_battery_level_update` function.

* :ref:`lib_ble_service_hrs`:

   * Updated:

      * The :c:func:`ble_hrs_heart_rate_measurement_send` function to not consume RR interval data when notifications are disabled in the CCCD and notification of the HRM data would fail.
      * The :c:func:`ble_hrs_heart_rate_measurement_send` function to log the heart rate value on the debug log level.
        Previously, this value was logged on the info log level, which caused excessive logging when periodically sending heart rate measurements.

* :ref:`lib_ble_service_hrs_client` service:

   * Added:

      * The :c:func:`ble_hrs_client_hrm_notif_disable` function to disable Heart Rate Measurement notifications.
      * Validation of the :c:member:`ble_hrs_client_config.evt_handler` and :c:member:`ble_hrs_client_config.gatt_queue` configuration fields in :c:func:`ble_hrs_client_init`.
      * State validation in the :c:func:`ble_hrs_client_hrm_notif_enable` and :c:func:`ble_hrs_client_hrm_notif_disable` functions, returning :c:macro:`NRF_ERROR_INVALID_STATE` when the connection or CCCD handle has not been assigned.

   * Updated:

      * The :c:struct:`ble_hrs_client_evt` structure by aligning with other client services.
      * The ``hrs_db`` structure by renaming it to :c:struct:`ble_hrs_handles`.
      * The ``ble_hrm`` structure by renaming it to :c:struct:`ble_hrs_measurement`.
      * The ``params`` union field of the :c:struct:`ble_hrs_client_evt` structure to an anonymous union.

   * Fixed a potential buffer over-read when parsing malformed Heart Rate Measurement notifications.

* :ref:`lib_ble_service_nus`:

   * Removed the ``ble_nus_client_context`` structure and the ``link_ctx`` field from the :c:struct:`ble_nus_evt` structure.
     The service now reads the CCCD directly from the SoftDevice instead of caching notification state internally.

* :ref:`lib_ble_service_nus_client` service:

   * Added the Nordic UART Service (NUS) client.

* :ref:`lib_ble_service_hids` service:

   * Updated the ``params`` union field of the :c:struct:`ble_hids_evt` structure to an anonymous union.

* :ref:`lib_ble_service_bms` service:

   * Updated the security configuration of the :c:struct:`ble_bms_config` structure to align with other services.
     Added the :c:macro:`BLE_BMS_CONFIG_SEC_MODE_DEFAULT` macro to set the default security configuration.

Libraries for NFC
-----------------

* Added:

  * The NFC libraries for NFC Connection Handover and Bluetooth LE Out-of-Band (OOB) pairing.
  * The workaround to ensure the ``FRAMEDELAYMAX`` register is set to the default value when a field is lost.
    This avoids violating protocol timing, which can lead readers to reject the NFC tag's response.

* Updated:

  * The maturity status of NFC libraries from Experimental to Supported for the nRF54L05, nRF54L10, and nRF54L15 SoCs.
    The nRF54LM20A SoC is still in Experimental quality.
  * The NFC subsystem code by migrating it to |BMshort|.
    It does not reuse code from |NCS| anymore.
    The NFC-related Kconfig options provided by |BMshort| have the ``BM_NFC_`` prefix.
    The following list shows the mapping from |NCS| Kconfig options to |BMshort| Kconfig options:

    * ``CONFIG_NFCT_IRQ_PRIORITY`` --> :kconfig:option:`CONFIG_BM_NFCT_IRQ_PRIORITY`
    * ``CONFIG_NFC_PLATFORM_LOG_LEVEL*`` --> :kconfig:option:`CONFIG_BM_NFC_PLATFORM_LOG_LEVEL*`
    * ``CONFIG_NFC_NDEF*`` --> :kconfig:option:`CONFIG_BM_NFC_NDEF*`
    * ``CONFIG_NFC_T4T_NDEF_FILE`` --> :kconfig:option:`CONFIG_BM_NFC_T4T_NDEF_FILE`

    Use ``#include <bm/nfc/...>`` to include NFC-related header files provided by |BMshort| instead of ``#include <nfc/...>``.

Utils
-----

* :ref:`api_ble_date_time_char`:

   * Moved the :file:`ble_date_time.h` header file from :file:`include/bm/bluetooth/services/` to :file:`include/bm/bluetooth/`.

* :ref:`api_ble_common`:

   * Renamed the ``common.h`` header file to :file:`ble_common.h`.
   * Moved the :file:`ble_common.h` header file from :file:`include/bm/bluetooth/services/` to :file:`include/bm/bluetooth/`.
   * Removed the ``gap_conn_sec_mode_from_u8`` function from the :file:`ble_common.h` file.
     Use the :c:macro:`BLE_GAP_CONN_SEC_MODE_OPEN` and similar macros instead.
   * Updated the return value of the :c:func:`is_notification_enabled` and :c:func:`is_indication_enabled` functions to ``bool`` to reflect that it returns a boolean value and not an ``uint16_t``.

Samples
=======

* Updated the prefix for sample Kconfig options from ``APP`` to ``SAMPLE``.

* Removed the battery measurement simulation from the following samples:

   * :ref:`ble_cgms_sample`
   * :ref:`ble_hids_mouse_sample`
   * :ref:`ble_hids_keyboard_sample`

* Aligned LED and button behavior across samples.

* All samples that are compatible with the new board targets PCA10188 (`nRF54LV10 DK`_), PCA10184 (`nRF54LM20 DK`_), and PCA10214 (`nRF54LS05 DK`_) have been updated to reflect support for these boards.

Bluetooth LE samples
--------------------

* Added:

   * The :ref:`ble_nus_central_sample` sample.
   * The :ref:`peripheral_nfc_pairing_sample` sample.

* :ref:`ble_bms_sample` sample:

   * Added sample-specific Kconfig options for the BMS authorization code by moving them from the service library scope and renaming them from ``CONFIG_BLE_BMS_AUTHORIZATION_CODE`` and ``CONFIG_BLE_BMS_USE_AUTHORIZATION_CODE`` to :kconfig:option:`CONFIG_SAMPLE_BLE_BMS_AUTHORIZATION_CODE` and :kconfig:option:`CONFIG_SAMPLE_BLE_BMS_USE_AUTHORIZATION_CODE`.

* :ref:`ble_hids_keyboard_sample`:

   * Added support for boot mode.

* :ref:`ble_hids_mouse_sample` sample:

   * Fixed an issue where the sample did not enter or exit boot mode properly based on the HID events.

* :ref:`ble_hrs_central_sample` sample:

   * Added the :ref:`lib_ble_service_bas_client` service to the sample.

* :ref:`ble_nus_sample` sample:

   * Updated to align with changes to the :ref:`driver_lpuarte` driver.

* :ref:`ble_pwr_profiling_sample` sample:

   * Updated to use a dedicated variable to hold the service attribute handle instead of incorrectly using the connection handle variable for this during service initialization.

NFC samples
-----------

* Included nRF54LM20A as an available SoC for the :ref:`peripheral_nfc_pairing_sample`, :ref:`record_text_t4t_sample` and :ref:`record_text_t2t_sample` samples, but the NFC libraries for this SoC are still in Experimental quality.
* Updated to use ``CONFIG_BM_NFC_*`` Kconfig options provided by |BMshort| instead of ``CONFIG_NFC_*`` options provided by |NCS|.
  Use ``#include <bm/nfc/...>`` headers provided by |BMshort| instead of ``#include <nfc/...>`` headers from |NCS|.

Peripheral samples
------------------

* Added the :ref:`bm_ppi_sample` sample.

* :ref:`bm_lpuarte_sample` sample:

  * Updated to align with changes to the :ref:`driver_lpuarte` driver.

* :ref:`uarte_sample` sample:

   * Updated the sample to use the logging module instead of console.

DFU samples
-----------

* :ref:`ble_mcuboot_recovery_entry_sample` sample:

  * Updated:

    * MCUboot and firmware loader by enabling building of a size-optimized configuration of these components.
      To enable the size-optimized configuration, set :makevar:`FILE_SUFFIX` to ``size_opt`` when building the sample.
    * By enabling migration to the new solution for setting up the DFU Device Bluetooth name remotely.
      See the :ref:`ug_dfu` page for details.
    * To clear the retained DFU Device Bluetooth name on boot.
      This was previously done by the firmware loader.

Subsystem samples
-----------------

* Added the :ref:`shell_bm_uarte_sample`.

Documentation
=============

* Added:

  * The :ref:`drivers_alloc_int` page.
  * The :ref:`board_memory_layouts` section, which documents RRAM and SRAM partition layouts for supported boards.

* Updated:

  * Sample documentation with clearer, more descriptive user guides, including updated explanations of configuration options, hardware connections, and testing procedures.
  * The :ref:`softdevice_docs` and :ref:`nrf_bm_api` pages to provide separate documentation and API references for each SoC-specific variant of the S115 and S145 SoftDevices (nRF54L15, nRF54LM20, nRF54LS05, and nRF54LV10).
