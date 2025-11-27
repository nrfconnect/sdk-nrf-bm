.. _nrf_bm_release_notes_0999:

Release Notes for |BMlong| v0.9.99
##################################

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

* Added the S145 SoftDevice for central support.

SoftDevice Handler
==================

* Added:

  * The :kconfig:option:`NRF_SDH_CLOCK_HFINT_CALIBRATION_INTERVAL` Kconfig option to control the HFINT calibration interval.
  * The :kconfig:option:`NRF_SDH_CLOCK_HFCLK_LATENCY` Kconfig option to inform the SoftDevice about the ramp-up time of the high-frequency crystal oscillator.
  * The :kconfig:option:`CONFIG_NRF_SDH_SOC_RAND_SEED` Kconfig option to control whether to automatically respond to SoftDevice random seed requests.
  * Priority levels for SoftDevice event observers: HIGHEST, HIGH, USER, USER_LOW, LOWEST.
  * The :c:func:`nrf_sdh_ble_evt_to_str` function to stringify a BLE event.
  * The :c:func:`nrf_sdh_soc_evt_to_str` function to stringify a SoC event.
  * The :c:func:`nrf_sdh_observer_ready` function to ready an observer for a SoftDevice state change.

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
  * The ``nrf_sdh_is_enabled`` function, use the SoftDevice native function :c:func:`sd_softdevice_is_enabled` instead.

Boards
======

* Added

  * The nRF54L15 DK board variants for the S145 SoftDevice.
  * The :file:`bm_nrf54l15dk_nrf54l051015_oscillator.dtsi` file with HF and LF external oscillator configuration choices for the nRF54L05, nRF54L10, and nRF54L15.

* MCUboot partition size has been reduced from 36 KiB to 31 KiB for the following board targets:

  * ``bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice/mcuboot``
  * ``bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice/mcuboot``
  * ``bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice/mcuboot``

* Removed unused peripheral nodes from Devicetree.

Build system
============

* Added automatic flashing of SoftDevice files for projects that do not use MCUboot when ``west flash`` is invoked.

DFU
===

* Support for KMU usage for MCUboot keys has been added, along with west auto-provisioning support (``west flash --erase`` or ``west flash --recover`` must be used during first programming of a board to program the KMU with the keys).
  This feature can be controlled with sysbuild Kconfig options :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_USING_KMU` to use KMU for key storage and :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_GENERATE_DEFAULT_KMU_KEYFILE` to auto-provision the KMU when using the above west flash commands.
* The code for the UART MCUmgr application has now been refactored into a separate library to facilitate reuse in other applications.

Logging
=======

* Removed ``module-dep=LOG`` in Kconfig files.
  This is no longer defined.

Drivers
=======

No changes since the latest nRF Connect SDK Bare Metal release.

Libraries
=========

* Added the :ref:`lib_ble_radio_notification` library.

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

* :ref:`lib_ble_conn_params` library:

   * Added:

    * Missing Kconfig dependencies.
    * Error event.

   * Updated the name of the ``id`` member in the :c:struct:`ble_conn_params_evt` structure to :c:member:`ble_conn_params_evt.evt_type`.

* :ref:`lib_bm_zms` library:

   * Added the struct :c:struct:`bm_zms_fs_config` for configuring a Zephyr Memory Storage file system instance at initialization.

   * Updated:

     * The :c:func:`bm_zms_register` function to return ``-EFAULT`` instead of ``-EINVAL`` when the input parameters are ``NULL``.
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
       Now the library expects at most one callback for each instance of the struct :c:struct:`bm_zms_fs`.
     * The ``bm_zms_init_flags.cb_registred`` member as it was not used anymore.

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

* :ref:`lib_storage` library:

  * Added the struct :c:struct:`bm_storage_config` for configuring a storage instance at initialization.

  * Updated:

    * To use errno instead of nrf_errors.
    * The :c:func:`bm_storage_init` function to expect an additional input parameter of type pointer to struct :c:struct:`bm_storage_config` for configuring the storage instance that is being initialized.

Bluetooth LE Services
------------

* Updated how to configure the characteristic security for the following Bluetooth LE services:

  * :ref:`lib_ble_service_bas` service.
  * :ref:`lib_ble_service_cgms` service.
  * :ref:`lib_ble_service_dis` service.
  * :ref:`lib_ble_service_hids` service.
  * :ref:`lib_ble_service_hrs` service.
  * :ref:`lib_ble_service_lbs` service.
  * :ref:`lib_ble_service_mcumgr` service.
  * :ref:`lib_ble_service_nus` service.

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

  * Fixed an issue where the client context was shared between all instances.
  * Added the error event to align event handling with other services.
  * Updated the name of the ``type`` member in the :c:struct:`ble_nus_evt` structure to :c:member:`ble_nus_evt.evt_type`.

* BLE Gatt Queue library:

  * Updated the event handling to align with other libraries.
    The :c:struct:`ble_gq_req` now takes an :c:type:`ble_gq_evt_handler_t` event handler and the :c:member:`ble_gq_req.ctx` context.

* :ref:`lib_bm_scheduler` library:

  * Renamed library from ``event_scheduler`` to ``bm_scheduler``.

* :ref:`lib_bm_buttons` library:

  * Fixed the GPIO trigger configuration when the ``active_state`` is ``BM_BUTTONS_ACTIVE_HIGH``

Samples
=======

* Updated all sample Kconfig options to be prefixed with ``APP_``.

Bluetooth samples
-----------------

Added the :ref:`ble_radio_ntf_sample` sample.

Peripheral samples
------------------

* Added the :ref:`pwm_sample` sample.

DFU samples
-----------

* Moved the MCUmgr samples to the :file:`applications/firmware_loader` folder.

Known issues and limitations
============================

No changes since the latest nRF Connect SDK Bare Metal release.

Documentation
=============

* Added documentation for the :ref:`lib_bm_buttons` library.
* Added documentation for the :ref:`lib_bm_timer` library.
* Added documentation for the :ref:`lib_ble_adv` library.
* Added documentation for the :ref:`lib_ble_gatt_queue` library.
* Added documentation for the :ref:`lib_ble_queued_writes` library.
* Added documentation for the :ref:`lib_bm_scheduler` library.
* Added documentation for the :ref:`lib_sensorsim` library.
* Added documentation for the :ref:`lib_storage` library.
* Added documentation for the :ref:`lib_ble_queued_writes` library.

Interrupts
==========

* Interrupts in nRF Connect SDK Bare Metal now use the IRQ vector table directly instead of the
  software ISR table. This saves 8 bytes of memory per IRQ, which is approximately 2kB for the
  nRF54L05, nRF54L10 and nRF54L15 Application core. This change requires applications change
  from using :c:macro:`IRQ_CONNECT` to :c:macro:`IRQ_DIRECT_CONNECT` and
  :c:macro:`ISR_DIRECT_DECLARE` when defining an ISR.

  An ISR defined with :c:macro:`IRQ_CONNECT`, like:

  .. code-block:: c

     int main(void)
     {
             IRQ_CONNECT(
                     NRFX_IRQ_NUMBER_GET(NRF_GPIOTE_INST_GET(20)),
                     10,
                     NRFX_GPIOTE_INST_HANDLER_GET(20),
                     0,
                     0
             );

  Must be defined like this:

  .. code-block:: c

     ISR_DIRECT_DECLARE(gpiote_20_direct_isr)
     {
             NRFX_GPIOTE_INST_HANDLER_GET(20)();
             return 0;
     }

     int main(void)
     {
             IRQ_DIRECT_CONNECT(
                     NRFX_IRQ_NUMBER_GET(NRF_GPIOTE_INST_GET(20)),
                     10,
                     gpiote_20_direct_isr,
                     0
             );
