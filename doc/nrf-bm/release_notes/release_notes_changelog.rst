.. _nrf_bm_release_notes_2099:

Changelog for |BMlong| v2.0.99
##############################

This changelog reflects the most relevant changes from the latest official release.

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

* Fixed an issue where using the :kconfig:option:`CONFIG_NRF_SDH_LOG_SD_INFO` Kconfig option for MCUboot board targets would log invalid SoftDevice version data.
  The logging now takes the SoftDevice partition offset into account for those board targets.

Boards
======

* Added support for the nRF54LS05A SoC, emulated on the nRF54LS05 DK through the ``bm_nrf54ls05dk/nrf54ls05a/cpuapp/*`` board targets.

* Added ``BOARD_EXTERNAL_MEMORY_*`` macros to the :file:`board-config.h` file of the ``bm_nrf54l15dk`` and ``bm_nrf54lm20dk`` board targets  (SPIM instance, SCK/MOSI/MISO/CS and WP#/RST# strap pins) for on-board SPI external flash.
  Other BM development kits do not have external flash memory on the board, so their :file:`board-config.h` files omit these macros.

* Updated:

   * All boards to use ``zephyr,mapped-partition`` instead of ``zephyr,fixed-partitions`` and ``zephyr,fixed-subpartitions``.
     When referencing partition nodes, the code now uses ``PARTITION_*`` macros instead of ``DT_*`` and ``FIXED_PARTITION_*`` macros.
     The use of the :kconfig:option:`CONFIG_FLASH_LOAD_OFFSET` Kconfig option is also replaced by ``PARTITION_*`` macros.
   * All boards to use specific J-Link devices from the nRF54L series.
     This improves debugging in VS Code.
   * The number of required board qualifiers (reduced by one), as of changes in upstream Zephyr.

* Removed:

   * The override for ``cpuapp_rram`` size for all boards as RRAM is no longer reserved for the RISC-V core by default.

Build system
============

* Updated:

  * The :file:`west.yml` file to add ``memfault-firmware-sdk`` to the |NCS| manifest allowlist.
  * The :file:`zephyr/module.yml` file to declare a build dependency on ``memfault-firmware-sdk``.

Interrupts
==========

No changes since the latest nRF Connect SDK Bare Metal release.

Logging
=======

No changes since the latest nRF Connect SDK Bare Metal release.

Drivers
=======

No changes since the latest nRF Connect SDK Bare Metal release.

Subsystems
==========

Storage
-------

No changes since the latest nRF Connect SDK Bare Metal release.

Filesystem
----------

No changes since the latest nRF Connect SDK Bare Metal release.

Memfault
--------

* Added Memfault platform support in :file:`subsys/memfault/`:

  * :kconfig:option:`CONFIG_BM_MEMFAULT_LOCK` - ``irq_lock()``-based ``memfault_lock()`` / ``memfault_unlock()`` for builds without Zephyr multithreading (default when ``NCS_BM && MEMFAULT && !MULTITHREADING``).
    Enabling this option also excludes the ``k_mutex``-based ``memfault_platform_lock.c`` file of the Memfault SDK from the build, so that Memfault links without :kconfig:option:`CONFIG_MULTITHREADING`.

Libraries
=========

* Added the :ref:`lib_bm_spi_mngr` library for queued SPI controller transactions on a single SPIM instance.
  Enable it using the :kconfig:option:`CONFIG_BM_SPI_MNGR` Kconfig option.
  See :ref:`lib_bm_spi_mngr` for an overview and :ref:`SPI transaction manager API reference <api_bm_spi_mngr>` for the full API.

* :ref:`lib_ble_conn_params`:

   * Added:

      * Support for selecting more than one PHY mode (1M, 2M, and Coded) when setting the PHY mode preference with Kconfig.
      * Support for the :c:macro:`BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST` SoftDevice event.

   * Updated:

      * The :c:func:`ble_conn_params_phy_radio_mode_set` function to return :c:macro:`NRF_ERROR_INVALID_PARAM` if the ``phy_pref`` parameter contains PHY modes not supported by the SoftDevice.
      * The :c:func:`ble_conn_params_override` function to allow runtime overrides of the acceptable connection parameter window used when validating peripheral requests to change the connection parameters.
        Previously, this window was set statically by Kconfig options and was not possible to override at runtime.

   * Fixed:

      * An issue where the :c:func:`ble_conn_params_phy_radio_mode_get` function would incorrectly return the PHY mode mask of a pending update rather than the currently active PHY mode if a PHY update initiated by the :c:func:`ble_conn_params_phy_radio_mode_set` function was still in progress.
      * An issue where the SoftDevice define :c:macro:`BLE_GAP_PHYS_SUPPORTED` was used instead of the PHY preferences set with Kconfig when initiating or responding to a PHY update procedure.

* :ref:`lib_ble_adv` library:

   * Added the :c:func:`ble_adv_data_manufacturer_data_find` function to locate manufacturer-specific data in an advertising payload and prefix-match it against a target value.

* :ref:`lib_ble_scan` library:

   * Added:

      * Support for filtering by manufacturer-specific data using the :c:macro:`BLE_SCAN_MANUFACTURER_DATA_FILTER` filter type.
      * The :kconfig:option:`CONFIG_BLE_SCAN_MANUFACTURER_DATA_COUNT` and :kconfig:option:`CONFIG_BLE_SCAN_MANUFACTURER_DATA_MAX_LEN` Kconfig options to configure the manufacturer data filter capacity and maximum payload length.

* :ref:`lib_peer_manager` library:

   * Updated:

      * The :c:func:`pm_init` function to clear the list of event handlers registered with the :c:func:`pm_register` function.
      * The :c:func:`pm_init` function to clear the default security parameters set with the :c:func:`pm_sec_params_set` function.
      * The :c:func:`pm_register` function to return ``NRF_ERROR_NULL`` when the event handler parameter is ``NULL``.
        The check was documented but was missing.
      * The LESC key agreement handling to clear the static RAM copy of the ECDH shared secret after the secret have been handed over to the SoftDevice using the :c:func:`sd_ble_gap_lesc_dhkey_reply` function.
      * The :kconfig:option:`CONFIG_PM_LESC_GENERATE_NEW_KEYS` Kconfig option to be enabled by default.
        This option forces the use of new ECDH key pair for each pairing procedure.

   * Fixed:

      * An issue where calling the :c:func:`pm_init` function two or more times would cause some of the internal asynchronous operation flags to have incorrect states.
      * The :c:func:`pm_address_resolve` function to return ``false`` instead of ``NRF_ERROR_INVALID_STATE`` when Peer Manager is not initialized.

Bluetooth LE Services
---------------------

* Added the :ref:`lib_ble_service_mds` service for exporting Memfault diagnostic chunks over Bluetooth LE.

* :ref:`lib_ble_scan`:

   * Changed :c:member:`ble_scan_filter_data.addr_filter.addr` and :c:member:`ble_scan_filter_data.name_filter.name` to ``const`` in the :c:struct:`ble_scan_filter_data` structure.

* :ref:`lib_ble_service_dis`:

   * Added support for configuring the Device Information Service characteristics at run time through the new :c:struct:`ble_dis_values` structure, passed using the ``values`` field of :c:struct:`ble_dis_config`.
     When ``values`` is ``NULL``, the service is built from the Kconfig defaults as before.

* :ref:`lib_ble_service_mcumgr`:

   * Fixed an issue where a DFU over Bluetooth LE could stall when using small ATT MTU or data length values.
     The SMP response is split into many notifications, which could fill the SoftDevice notification (HVN) TX queue and cause :c:func:`sd_ble_gatts_hvx` to return :c:macro:`NRF_ERROR_RESOURCES`, dropping the remaining data.
     Notifications that fail with :c:macro:`NRF_ERROR_RESOURCES` are now retransmitted on the :c:macro:`BLE_GATTS_EVT_HVN_TX_COMPLETE` event once the SoftDevice frees queue space.

* :ref:`lib_ble_service_hrs`:

   * Fixed an issue where the :c:func:`on_connect` and :c:func:`on_disconnect` functions would wrongly override the connection handle upon connecting to a different device.

* :ref:`lib_ble_service_hrs_client`:

   * Added the :c:enumerator:`BLE_HRS_CLIENT_EVT_BSL_UPDATE` event to the :c:enum:`ble_hrs_client_evt_type` enum.

Libraries for NFC
-----------------

No changes since the latest nRF Connect SDK Bare Metal release.

Utils
-----

No changes since the latest nRF Connect SDK Bare Metal release.

Samples
=======

* The :kconfig:option:`CONFIG_PSA_CRYPTO` Kconfig option is now used to enable cryptography instead of the :kconfig:option:`CONFIG_NRF_SECURITY` Kconfig option.

Peripheral samples
------------------

   * Added:

      * The :ref:`radio_test` sample.
      * The :ref:`spi_mngr_sample` sample, demonstrating read, page program, and sector erase on the on-board external NOR flash using the :ref:`lib_bm_spi_mngr` library.
      * The :ref:`bm_spi_sample` sample.

Bluetooth LE samples
--------------------

* Added the :ref:`ble_mds_sample` sample.

* Updated:

   * All samples that use the :ref:`lib_peer_manager` library to use a minimum encryption key size of 16 bytes in their default security parameters.
   * The following samples and applications, which do not support pairing, to call the :c:func:`sd_ble_gatts_sys_attr_set` function only in response to the :c:macro:`BLE_GATTS_EVT_SYS_ATTR_MISSING` event, and not in response to the :c:macro:`BLE_GAP_EVT_CONNECTED` event:

      * :ref:`ug_dfu_firmware_loader` (Bluetooth LE)
      * :ref:`ble_lbs_sample`
      * :ref:`ble_nus_sample`
      * :ref:`ble_mcuboot_recovery_entry_sample`

* Removed the authentication status logging from the following samples and applications that do not support pairing (do not use the :ref:`lib_peer_manager` library):

   * :ref:`ug_dfu_firmware_loader` (Bluetooth LE)
   * :ref:`ble_lbs_sample`
   * :ref:`ble_nus_sample`
   * :ref:`ble_pwr_profiling_sample`
   * :ref:`ble_mcuboot_recovery_entry_sample`

* :ref:`ble_cgms_sample` sample:

   * Removed the call to the :c:func:`sd_ble_gatts_sys_attr_set` function from the main source file.
     The :ref:`lib_peer_manager` library takes care of calling this function when :ref:`lib_peer_manager` is used.

* :ref:`ble_hrs_sample` sample:

   * Removed redundant logging of authentication status from the main source file.
     Authentication status is logged by :ref:`lib_peer_manager` library.

* :ref:`ble_hrs_central_sample` sample:

   * Fixed:

      * The disconnect button handler to only disconnect on button press, and not on button release.
      * The allow list disabling button to only trigger on button press, and not on button release.
      * An issue with the endianness of the target peripheral address when displaying the address on a :c:enumerator:`BLE_SCAN_EVT_CONNECTED` event and when supplying the scan filter address with the :kconfig:option:`CONFIG_SAMPLE_TARGET_PERIPHERAL_ADDR` Kconfig option.

* :ref:`ble_nus_sample` sample:

   * Fixed a bug with the UARTE RX buffer address provided with index 1.

* :ref:`ble_nus_central_sample` sample:

   * Updated to use Button 1 to disconnect from the target peripheral to align with other central samples.

   * Fixed:

      * The disconnect button handler to only disconnect on button press, and not on button release.
      * A bug with the UARTE RX buffer address provided with index 1.

NFC samples
-----------

No changes since the latest nRF Connect SDK Bare Metal release.

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

* Updated the memory layout diagram generation script to parse RRAM and SRAM sizes directly from devicetree instead of hardcoded per-SoC values.

* Added:

  * The :ref:`memfault_bm` page, which documents the Memfault integration on bare metal, including platform locking, the rules for calling Memfault APIs from ISR and main loop context, and the Memfault cloud prerequisites.
  * The :ref:`lib_ble_service_mds` library page.
  * The :ref:`ble_mds_sample` page, which includes the steps for getting started with Memfault.
  * The :ref:`api_ble_mds` API reference.
