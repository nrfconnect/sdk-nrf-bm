.. _nrf_bm_release_notes_100:

Release Notes for |BMlong| v1.0.0
#################################

|BMshort| v1.0.0 is the first supported release of this product, demonstrating a set of samples for the `nRF54L15 DK`_ with build targets supporting the following SoCs:

* `nRF54L15`_
* `nRF54L10`_ (emulated on the nRF54L15 DK)
* `nRF54L05`_ (emulated on the nRF54L15 DK)

The supported DK version is `nRF54L15 DK v1.0.0`_.

This release includes two versions of the SoftDevice - a Bluetooth® LE stack to be used on the nRF54L15 DK:

* S115 - Support for peripheral role only
* S145 - Support for both peripheral and central role, as well as more connections

Highlights
**********

The overall software maturity status of |BMlong| v1.0.0 is supported unless stated differently for individual features or components.
This makes it suitable for product development using nRF54L15, nRF54L10, and nRF54L05 Wireless SoCs, including the nRF54L15 DK targets.

Added the following features as supported:

* The S115 SoftDevice, which was part of |BMshort| v0.9.0 as experimental, is now supported.
  For information about the qualification of this SoftDevice variant, refer to the `nRF54L15 Compatibility Matrix`_, or respective pages for nRF54L105 and nRF54L10.
  Updates to the compatibility matrices will be published in the upcoming weeks.

* The S145 SoftDevice, a new variant that enables central and multirole operation, has been added as supported.
  For information about the qualification of this SoftDevice variant, refer to the `nRF54L15 Compatibility Matrix`_, or respective pages for nRF54L105 and nRF54L10.
  Updates to the compatibility matrices will be published in the upcoming weeks.

* MCUBoot (IRoT)/DFU:

  * CRACKEN/KMU is now supported.
  * Possible to be configured as IRoT for your applications.
  * Available in two variants: debug and size-optimized.

* New samples and libraries.
  Refer to the changelog for detailed information.

* |BMshort| v1.0.0 uses nrfx 4.0.0.
  For migration of nrfx drivers, see the `nrfx 4.0 migration guide`_.
  Details from the `nrfx 3.0 migration guide`_ might be required when migrating from nRF5.

Added the following features as experimental:

* NFC Type 4 Tag and Type 2 Tag libraries, including samples.

* Bluetooth LE Central functionality as part of samples and libraries utilizing the S145 SoftDevice.

  .. note::
     While the S145 SoftDevice has supported maturity, its overall integration in the SDK is experimental.

Release tag
***********

The release tag for the |BMshort| manifest repository is **v1.0.0**.
Check the :file:`west.yml` file for the corresponding tags in the project repositories.

To use this release, install it using |nRFVSC| by following the instructions in :ref:`install_nrf_bm`.

Alternatively, check out the tag in the manifest repository, run ``west config manifest.path nrf-bm``, and then ``west update``.

This release of |BMshort| is based on |NCS| **v3.2.0**.

IDE and tool support
********************

`nRF Connect for Visual Studio Code`_ is the recommended IDE for |BMshort| v1.0.0.
See the :ref:`Installation <install_nrf_bm>` section for more information about supported operating systems and toolchain.

Changelog
*********

The following sections provide detailed lists of changes by component.

S115 SoftDevice
===============

* Updated S115 SoftDevice version to v9.0.0.

S145 SoftDevice
===============

* Added the S145 SoftDevice v9.0.0 for central support.

SoftDevice Handler
==================

* Added:

  * The :kconfig:option:`CONFIG_NRF_SDH_CLOCK_HFINT_CALIBRATION_INTERVAL` Kconfig option to control the HFINT calibration interval.
  * The :kconfig:option:`CONFIG_NRF_SDH_CLOCK_HFCLK_LATENCY` Kconfig option to inform the SoftDevice about the ramp-up time of the high-frequency crystal oscillator.
  * The :kconfig:option:`CONFIG_NRF_SDH_SOC_RAND_SEED` Kconfig option to control whether to automatically respond to SoftDevice random seed requests.
  * Priority levels for SoftDevice event observers: HIGHEST, HIGH, USER, USER_LOW, LOWEST.
  * The :c:func:`nrf_sdh_ble_evt_to_str` function to stringify a BLE event.
  * The :c:func:`nrf_sdh_soc_evt_to_str` function to stringify a SoC event.
  * The :c:func:`nrf_sdh_observer_ready` function to prepare an observer for a SoftDevice state change.

* Updated:

  * The return type of the :c:type:`nrf_sdh_state_evt_handler_t` event handler to ``int``.
  * The LF clock source selection with a choice and tied it to the choice of GRTC timer source :kconfig:option:`CONFIG_NRF_GRTC_TIMER_SOURCE`.
    The default GRTC timer source of the ``bm_nrf54l15dk`` board target is LFXO (:kconfig:option:`CONFIG_NRF_GRTC_TIMER_SOURCE_LFXO`).
    Select the :kconfig:option:`CONFIG_NRF_GRTC_TIMER_SOURCE_LFLPRC` Kconfig option if it is desired to run the GRTC from RC.
  * The :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_RC_CTIV` and :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_RC_TEMP_CTIV` Kconfig options with ranges, default values, help text, and conditional prompt.
    These Kconfig options are zero if the LF clock source is XO and are only user-configurable when the LF clock source is RC.
  * The :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_ACCURACY` Kconfig option with a Kconfig choice to limit the selection to valid values.

* Removed:

  * The ``nrf_sdh_ble_app_ram_start_get`` function, use ``DT_REG_ADDR(DT_CHOSEN(zephyr_sram))`` instead.
  * The ``NRF_SDH_STATE_REQ_OBSERVER`` macro and relative data types.
    Register a state event observer and return non-zero to :c:enum:`NRF_SDH_STATE_EVT_ENABLE_PREPARE`
    or :c:enum:`NRF_SDH_STATE_EVT_DISABLE_PREPARE` to abort state changes instead.
  * The ``nrf_sdh_request_continue`` function.
  * The ``nrf_sdh_is_enabled`` function.
    Use the SoftDevice native function :c:func:`sd_softdevice_is_enabled` instead.

Boards
======

* Added

  * The nRF54L15 DK board variants for the S145 SoftDevice.
  * The :file:`bm_nrf54l15dk_nrf54l051015_oscillator.dtsi` file with HF and LF external oscillator configuration choices for the nRF54L05, nRF54L10, and nRF54L15.

* MCUboot partition size has been reduced from 36 KiB to 31 KiB for the following board targets:

  * ``bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice/mcuboot``
  * ``bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice/mcuboot``
  * ``bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice/mcuboot``

* Removed unused peripheral nodes from the devicetree.

Build system
============

* Added automatic flashing of SoftDevice files for projects that do not use MCUboot when ``west flash`` is invoked.

DFU
===

* Added:

  * Support for KMU usage for MCUboot keys, along with west auto-provisioning support.
    Use ``west flash --erase`` or ``west flash --recover`` during the first programming of a board to program the KMU with the keys.
    You can control this feature with sysbuild Kconfig options :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_USING_KMU` to use KMU for key storage and :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_GENERATE_DEFAULT_KMU_KEYFILE` to auto-provision the KMU when using the mentioned ``west flash`` commands.
  * Support for setting up the DFU Device Bluetooth name remotely using MCUmgr.
    Use the :kconfig:option:`SB_CONFIG_BM_APP_CAN_SETUP_FIRMWARE_LOADER_NAME` Kconfig option to enable this feature.

* Updated:

  * By refactoring the code for the UART MCUmgr application into a separate library to facilitate reuse in other applications.
  * The MCUmgr image management to prevent erasing of the firmware loader by itself.
    Such operation would break the DFU functionality.

Interrupts
==========

* Interrupts in |BMlong| now use the IRQ vector table directly instead of the software ISR table.
  This saves eight bytes of memory for each IRQ, which is approximately 2 kB for the nRF54L05, nRF54L10, and nRF54L15 application core.
  With this update, applications must now use macros :c:macro:`IRQ_DIRECT_CONNECT` and :c:macro:`ISR_DIRECT_DECLARE` instead of :c:macro:`IRQ_CONNECT` when defining an ISR.

  For example, an ISR defined with :c:macro:`IRQ_CONNECT` in the following way:

  .. code-block:: c

     static nrfx_gpiote_t gpiote20 = NRFX_GPIOTE_INSTANCE(NRF_GPIOTE20);

     int main(void)
     {
             IRQ_CONNECT(
                     NRFX_IRQ_NUMBER_GET(NRF_GPIOTE20),
                     10,
                     nrfx_gpiote_irq_handler,
                     &gpiote20,
                     0
             );

  Must now be defined like this:

  .. code-block:: c

     static nrfx_gpiote_t gpiote20 = NRFX_GPIOTE_INSTANCE(NRF_GPIOTE20);

     ISR_DIRECT_DECLARE(gpiote_20_direct_isr)
     {
             nrfx_gpiote_irq_handler(&gpiote20);
             return 0;
     }

     int main(void)
     {
             IRQ_DIRECT_CONNECT(
                     NRFX_IRQ_NUMBER_GET(NRF_GPIOTE20),
                     10,
                     gpiote_20_direct_isr,
                     0
             );

Logging
=======

* Removed ``module-dep=LOG`` in Kconfig files.
  This is no longer defined.

Drivers
=======

* :ref:`driver_lpuarte` driver:

  * Added the :kconfig:option:`CONFIG_BM_SW_LPUARTE_HFXO` Kconfig option to enable the HFCLK when the SoftDevice is enabled.

Libraries
=========

* Added the following libraries:

  * :ref:`lib_ble_radio_notification`.
  * :ref:`lib_ble_db_discovery`.
  * :ref:`lib_ble_scan`.

* Updated the following libraries and Bluetooth LE services to return ``nrf_errors`` instead of ``errnos``:

  * :ref:`lib_ble_adv` library.
  * :ref:`lib_ble_conn_params` library.
  * :ref:`lib_ble_gatt_queue` library.
  * :ref:`lib_ble_queued_writes` library.
  * :ref:`lib_ble_service_bas` service.
  * :ref:`lib_ble_service_dis` service.
  * :ref:`lib_ble_service_hrs` service.
  * :ref:`lib_ble_service_lbs` service.
  * :ref:`lib_ble_service_mcumgr` service.
  * :ref:`lib_ble_service_nus` service.
  * Bluetooth LE Record Access Control Point library.

* :ref:`lib_ble_adv` library:

   * Updated the advertising and scan response data encoder provided in the :file:`include/bm/bluetooth/ble_adv_data.h` file so that it can be used by enabling the :kconfig:option:`CONFIG_BLE_ADV_DATA` Kconfig option.
     You can enable this without using the :kconfig:option:`CONFIG_BLE_ADV` Kconfig option.

   * Renamed:

     * The ``ble_adv_whitelist_reply`` function to :c:func:`ble_adv_allow_list_reply`.
     * The ``ble_adv_restart_without_whitelist`` function to :c:func:`ble_adv_restart_without_allow_list`.
     * The ``BLE_ADV_EVT_FAST_WHITELIST`` enumerator in the :c:enum:`ble_adv_evt_type` enumeration to :c:enumerator:`BLE_ADV_EVT_FAST_ALLOW_LIST`.
     * The ``BLE_ADV_EVT_SLOW_WHITELIST`` enumerator in the :c:enum:`ble_adv_evt_type` enumeration to :c:enumerator:`BLE_ADV_EVT_SLOW_ALLOW_LIST`.
     * The ``BLE_ADV_EVT_WHITELIST_REQUEST`` enumerator in the :c:enum:`ble_adv_evt_type` enumeration to :c:enumerator:`BLE_ADV_EVT_ALLOW_LIST_REQUEST`.
     * The ``whitelist_reply_expected`` member in the :c:struct:`ble_adv` structure to :c:member:`ble_adv.allow_list_reply_expected`.
     * The ``whitelist_temporarily_disabled`` member in the :c:struct:`ble_adv` structure to :c:member:`ble_adv.allow_list_temporarily_disabled`.
     * The ``whitelist_in_use`` member in the :c:struct:`ble_adv` structure to :c:member:`ble_adv.allow_list_in_use`.
     * The ``slave_conn_int`` member in the :c:struct:`ble_adv_data` structure to :c:member:`ble_adv_data.periph_conn_int`.
     * The ``CONFIG_BLE_ADV_USE_WHITELIST`` Kconfig option to :kconfig:option:`CONFIG_BLE_ADV_USE_ALLOW_LIST`.

   * Fixed:

     * An issue with the allow list functionality that made it possible for non-allow-listed devices to connect if advertising was started in either directed or directed high duty mode, but the directed modes had been disabled with Kconfig options.
     * Two minor issues with the directed advertising set configuration that caused directed advertising to not work as intended.

* :ref:`lib_ble_conn_params` library:

   * Added:

    * Missing Kconfig dependencies.
    * Error event.

   * Updated the name of the ``id`` member in the :c:struct:`ble_conn_params_evt` structure to :c:member:`ble_conn_params_evt.evt_type`.

* :ref:`lib_bm_zms` library:

   * Added the :c:struct:`bm_zms_fs_config` struct for configuring a Zephyr Memory Storage file system instance at initialization.

   * Updated:

     * The :c:func:`bm_zms_mount` function to return ``-EFAULT`` when the input parameter ``fs`` is ``NULL``.
     * The :c:func:`bm_zms_clear` function to return ``-EFAULT`` when the input parameter ``fs`` is ``NULL``.
     * The :c:func:`bm_zms_write` function to return ``-EFAULT`` when the input parameter ``fs`` is ``NULL``.
     * The :c:func:`bm_zms_delete` function to return ``-EFAULT`` when the input parameter ``fs`` is ``NULL``.
     * The :c:func:`bm_zms_read` function to return ``-EFAULT`` when the input parameter ``fs`` is ``NULL``.
     * The :c:func:`bm_zms_read_hist` function to return ``-EFAULT`` when the input parameter ``fs`` is ``NULL``.
     * The :c:func:`bm_zms_get_data_length` function to return ``-EFAULT`` when the input parameter ``fs`` is ``NULL``.
     * The :c:func:`bm_zms_calc_free_space` function to return ``-EFAULT`` when the input parameter ``fs`` is ``NULL``.
     * The :c:func:`bm_zms_active_sector_free_space` function to return ``-EFAULT`` when the input parameter ``fs`` is ``NULL``.
     * The :c:func:`bm_zms_mount` function to expect an additional input parameter of type pointer to struct :c:struct:`bm_zms_fs_config` for configuring a Zephyr Memory Storage file system instance at initialization.
     * By renaming the type ``bm_zms_evt_id_t`` to enum :c:enum:`bm_zms_evt_type`.
     * By renaming the type ``bm_zms_evt_t`` to struct :c:struct:`bm_zms_evt`.
     * By renaming the event ``BM_ZMS_EVT_INIT`` to :c:enum:`BM_ZMS_EVT_MOUNT`.

   * Removed:

     * The ``CONFIG_BM_ZMS_MAX_USERS`` Kconfig option.
       Now, the library expects at most one callback for each instance of the struct :c:struct:`bm_zms_fs`.
     * The ``bm_zms_init_flags.cb_registred`` member as it was not used anymore.
     * The ``bm_zms_register`` function.
       The event handler is now configured using the :c:struct:`bm_zms_fs_config` struct.
     * The selection of the :kconfig:option:`CONFIG_EXPERIMENTAL` Kconfig option.

* :ref:`lib_peer_manager` library:

   * Updated:

     * The ``CONFIG_PM_SERVICE_CHANGED_ENABLED`` Kconfig option is renamed to :kconfig:option:`CONFIG_PM_SERVICE_CHANGED`.
     * The ``CONFIG_PM_PEER_RANKS_ENABLED`` Kconfig option is renamed to :kconfig:option:`CONFIG_PM_PEER_RANKS`.
     * The ``CONFIG_PM_LESC_ENABLED`` Kconfig option is renamed to :kconfig:option:`CONFIG_PM_LESC`.
     * The ``CONFIG_PM_RA_PROTECTION_ENABLED`` Kconfig option is renamed to :kconfig:option:`CONFIG_PM_RA_PROTECTION`.
     * The :kconfig:option:`CONFIG_PM_SERVICE_CHANGED` Kconfig option to depend on the :kconfig:option:`CONFIG_NRF_SDH_BLE_SERVICE_CHANGED` Kconfig option.
     * All instances of ``pm_peer_id_t`` to ``uint16_t`` and removed the ``pm_peer_id_t`` type.
     * All instances of ``pm_store_token_t`` to ``uint32_t`` and removed the ``pm_store_token_t`` type.
     * All instances of ``pm_sec_error_code_t`` to ``uint16_t`` and removed the ``pm_sec_error_code_t`` type.
     * All instances of ``ble_gatt_db_srv_t`` to struct :c:struct:`ble_gatt_db_srv` and removed the ``ble_gatt_db_srv_t`` type.
     * All instances of ``ble_gatt_db_char_t`` to struct :c:struct:`ble_gatt_db_char` and removed the ``ble_gatt_db_char_t`` type.
     * All instances of ``pm_peer_id_list_skip_t`` to enum :c:enum:`pm_peer_id_list_skip` and removed the ``pm_peer_id_list_skip_t`` type.
     * All instances of ``pm_peer_data_id_t`` to enum :c:enum:`pm_peer_data_id` and removed the ``pm_peer_data_id_t`` type.
     * All instances of ``pm_conn_sec_procedure_t`` to enum :c:enum:`pm_conn_sec_procedure` and removed the ``pm_conn_sec_procedure_t`` type.
     * All instances of ``pm_conn_sec_config_t`` to struct :c:struct:`pm_conn_sec_config` and removed the ``pm_conn_sec_config_t`` type.
     * All instances of ``pm_peer_data_bonding_t`` to struct :c:struct:`pm_peer_data_bonding` and removed the ``pm_peer_data_bonding_t`` type.
     * All instances of ``pm_peer_data_local_gatt_db_t`` to struct :c:struct:`pm_peer_data_local_gatt_db` and removed the ``pm_peer_data_local_gatt_db_t`` type.
     * All instances of ``pm_privacy_params_t`` to :c:type:`ble_gap_privacy_params_t` and removed the ``pm_privacy_params_t`` type.
     * All instances of ``pm_conn_sec_status_t`` to struct :c:struct:`pm_conn_sec_status` and removed the ``pm_conn_sec_status_t`` type.
     * All instances of ``pm_evt_id_t`` to enum :c:enum:`pm_evt_id` and removed the ``pm_evt_id_t`` type.
     * All instances of ``pm_conn_config_req_evt_t`` to struct :c:struct:`pm_conn_config_req_evt` and removed the ``pm_conn_config_req_evt_t`` type.
     * All instances of ``pm_conn_sec_start_evt_t`` to struct :c:struct:`pm_conn_sec_start_evt` and removed the ``pm_conn_sec_start_evt_t`` type.
     * All instances of ``pm_conn_secured_evt_t`` to struct :c:struct:`pm_conn_secured_evt` and removed the ``pm_conn_secured_evt_t`` type.
     * All instances of ``pm_conn_secure_failed_evt_t`` to struct :c:struct:`pm_conn_secure_failed_evt` and removed the ``pm_conn_secure_failed_evt_t`` type.
     * All instances of ``pm_conn_sec_params_req_evt_t`` to struct :c:struct:`pm_conn_sec_params_req_evt` and removed the ``pm_conn_sec_params_req_evt_t`` type.
     * All instances of ``pm_peer_data_op_t`` to enum :c:enum:`pm_peer_data_op` and removed the ``pm_peer_data_op_t`` type.
     * All instances of ``pm_peer_data_update_succeeded_evt_t`` to struct :c:struct:`pm_peer_data_update_succeeded_evt` and removed the ``pm_peer_data_update_succeeded_evt_t`` type.
     * All instances of ``pm_peer_data_update_failed_t`` to struct :c:struct:`pm_peer_data_update_failed_evt` and removed the ``pm_peer_data_update_failed_t`` type.
     * All instances of ``pm_failure_evt_t`` to struct :c:struct:`pm_failure_evt` and removed the ``pm_failure_evt_t`` type.
     * All instances of ``pm_evt_t`` to struct :c:struct:`pm_evt` and removed the ``pm_evt_t`` type.
     * The name of the ``pm_whitelist_get`` function to :c:func:`pm_allow_list_get`.
     * The name of the ``pm_whitelist_set`` function to :c:func:`pm_allow_list_set`.
     * The name of the ``PM_EVT_SLAVE_SECURITY_REQ`` enumerator in the :c:enum:`pm_evt_id` enumeration to :c:enumerator:`PM_EVT_PERIPHERAL_SECURITY_REQ`.
     * The name of the ``slave_security_req`` member in the :c:struct:`pm_evt` structure to :c:member:`pm_evt.peripheral_security_req`.

   * Removed the selection of the :kconfig:option:`CONFIG_EXPERIMENTAL` Kconfig option.

* :ref:`lib_storage` library:

  * Added the :c:struct:`bm_storage_config` struct for configuring a storage instance at initialization.

  * Updated:

    * To use errno instead of nrf_errors.
    * The :c:func:`bm_storage_init` function to expect an additional input parameter of type pointer to struct :c:struct:`bm_storage_config` for configuring the storage instance that is being initialized.

  * Fixed an issue where the :c:func:`bm_storage_erase` function caused a division-by-0 fault using the SD or RRAM backend.

Bluetooth LE Services
---------------------

* Added the following services:

  * :ref:`lib_ble_service_bms`.
  * Heart Rate Service Central.

* Updated the way of configuring the characteristic security for the following Bluetooth LE services:

  * :ref:`lib_ble_service_bas`
  * :ref:`lib_ble_service_cgms`
  * :ref:`lib_ble_service_dis`
  * :ref:`lib_ble_service_hids`
  * :ref:`lib_ble_service_hrs`
  * :ref:`lib_ble_service_lbs`
  * :ref:`lib_ble_service_mcumgr`
  * :ref:`lib_ble_service_nus`

* :ref:`lib_ble_service_bas` service:

  * Added the error event to align event handling with other services.
    The event is currently unused.

* :ref:`lib_ble_service_hrs` service:

  * Added the error event to align event handling with other services.
    The event is currently unused.

* :ref:`lib_ble_service_lbs` service:

  * Added the error event to align event handling with other services.
    The event is currently unused.

* :ref:`lib_ble_service_nus` service:

  * Added the error event to align event handling with other services.
  * Updated the name of the ``type`` member in the :c:struct:`ble_nus_evt` structure to :c:member:`ble_nus_evt.evt_type`.
  * Fixed an issue where the client context was shared between all instances.

* :ref:`lib_ble_service_cgms` service:

  * Updated the default characteristic security to encryption without MITM protection.
    This is done to conform to the CGMS specification.

* :ref:`lib_ble_gatt_queue`:

  * Updated the event handling to align with other libraries.
    The :c:struct:`ble_gq_req` now takes the :c:type:`ble_gq_evt_handler_t` event handler and the :c:member:`ble_gq_req.ctx` context.

* :ref:`lib_bm_scheduler` library:

  * Renamed library from ``event_scheduler`` to ``bm_scheduler``.

* :ref:`lib_bm_buttons` library:

  * Fixed the GPIO trigger configuration when the ``active_state`` is ``BM_BUTTONS_ACTIVE_HIGH``

Libraries for NFC
-----------------

* Added experimental support for Near Field Communication (NFC).

Samples
=======

* Updated all sample Kconfig options to be prefixed with ``APP_``.

Bluetooth LE samples
--------------------

* Added the following Bluetooth LE samples:

  * :ref:`ble_radio_ntf_sample` sample.
  * :ref:`ble_bms_sample` sample.
  * :ref:`ble_hrs_central_sample` sample.

* Updated:

   * The :ref:`ble_cgms_sample` sample to require at least encryption without MITM tection to access the CGM service characteristics.
     This is done to conform to the CGMS specification.
   * The :ref:`ble_cgms_sample` sample to support pairing and bonding.

NFC samples
-----------

* Added the following NFC samples:

  * :ref:`record_text_t2t_sample` sample.
  * :ref:`record_text_t4t_sample` sample.

Peripheral samples
------------------

* Added the :ref:`pwm_sample` sample.

* :ref:`bm_lpuarte_sample` sample:

  * Enabled the SoftDevice to request HFCLK in the :ref:`driver_lpuarte` driver.

DFU samples
-----------

* Moved the MCUmgr samples to the :file:`applications/firmware_loader` folder.

Subsystem samples
-----------------

* Moved the :ref:`bm_storage_sample` and :ref:`bm_zms_sample` samples to the :file:`samples/subsys` folder.
* Removed the RRAM storage backend from the :ref:`bm_storage_sample` and :ref:`bm_zms_sample` samples.

Known issues and limitations
============================

* The |BMshort| delivery does not have a common library for handling GPIOTE.
  This results in issues when enabling multiple libraries that require GPIOTE, such as :ref:`driver_lpuarte` and :ref:`lib_bm_buttons`.

* Some Bluetooth LE libraries allow for the creation of multiple instances, although only one instance is supported.
  Defining multiple instances of the same library may lead to unexpected behavior.

Documentation
=============

* Added:

  * Documentation for the following libraries:

    * :ref:`lib_bm_buttons`
    * :ref:`lib_bm_timer`
    * :ref:`lib_ble_adv`
    * :ref:`lib_ble_gatt_queue`
    * :ref:`lib_ble_queued_writes`
    * :ref:`lib_bm_scheduler`
    * :ref:`lib_sensorsim`
    * :ref:`lib_storage`

  * :ref:`lib_nfc_integration` for NFC.
  * :ref:`S145 SoftDevice release notes and migration documents <s145_docs>`.
  * :ref:`nrf5_bm_migration_errors` section in the :ref:`nrf5_bm_migration`.

* Updated:

  * The :ref:`dfu_memory_partitioning` section.
