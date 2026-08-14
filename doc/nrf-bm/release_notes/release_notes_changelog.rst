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

* Updated:

   * All boards to use ``zephyr,mapped-partition`` instead of ``zephyr,fixed-partitions`` and ``zephyr,fixed-subpartitions``.
     When referencing partition nodes, the code now uses ``PARTITION_*`` macros instead of ``DT_*`` and ``FIXED_PARTITION_*`` macros.
     The use of the :kconfig:option:`CONFIG_FLASH_LOAD_OFFSET` Kconfig option is also replaced by ``PARTITION_*`` macros.
   * All boards to use specific J-Link devices from the nRF54L series.
     This improves debugging in VS Code.
   * The number of required board qualifiers (reduced by one), as of changes in upstream Zephyr.

Build system
============

No changes since the latest nRF Connect SDK Bare Metal release.

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

Libraries
=========

* :ref:`lib_ble_conn_params`:

   * Added:

      * Support for selecting more than one PHY mode (1M, 2M, and Coded) when setting the PHY mode preference with Kconfig.
      * Support for the :c:macro:`BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST` SoftDevice event.

   * Updated the :c:func:`ble_conn_params_phy_radio_mode_set` function to return :c:macro:`NRF_ERROR_INVALID_PARAM` if the ``phy_pref`` parameter contains PHY modes not supported by the SoftDevice.
   * Updated the :c:func:`ble_conn_params_override` function to allow runtime overrides of the acceptable connection parameter window used when validating peripheral requests to change the connection parameters.
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

   * Fixed:

      * An issue where calling the :c:func:`pm_init` function two or more times would cause some of the internal asynchronous operation flags to have incorrect states.
      * The :c:func:`pm_address_resolve` function to return ``false`` instead of ``NRF_ERROR_INVALID_STATE`` when Peer Manager is not initialized.

Bluetooth LE Services
---------------------

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

* Added the :ref:`radio_test` sample.


Bluetooth LE samples
--------------------

* Updated the following samples and applications that do not support pairing to call the :c:func:`sd_ble_gatts_sys_attr_set` function only in response to the :c:macro:`BLE_GATTS_EVT_SYS_ATTR_MISSING` event and not as a response to a :c:macro:`BLE_GAP_EVT_CONNECTED` event:

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

Peripheral samples
------------------

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

No changes since the latest nRF Connect SDK Bare Metal release.
