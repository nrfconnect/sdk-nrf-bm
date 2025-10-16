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

* Added the :kconfig:option:`NRF_SDH_CLOCK_HFINT_CALIBRATION_INTERVAL` Kconfig option to control the HFINT calibration interval.

Boards
======

* Added nrf54l15DK board variants for the S145 SoftDevice.

* MCUboot partition size has been reduced from 36 KiB to 31 KiB for the following board targets:

  * `bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice/mcuboot`
  * `bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice/mcuboot`
  * `bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice/mcuboot`

* Removed unused peripheral nodes from Devicetree.

DFU
===

* Support for KMU usage for MCUboot keys has been added, along with west auto-provisioning support (`west flash --erase` or `west flash --recover` must be used during first programming of a board to program the KMU with the keys).
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

* :ref:`lib_ble_conn_params` library:

   * Added missing Kconfig dependencies.

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

Samples
=======

Bluetooth samples
-----------------

No changes since the latest nRF Connect SDK Bare Metal release.

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

No changes since the latest nRF Connect SDK Bare Metal release.
