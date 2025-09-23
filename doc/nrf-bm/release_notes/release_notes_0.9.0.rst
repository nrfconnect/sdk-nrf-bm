.. _nrf_bm_release_notes_090:

Release Notes for |BMlong| v0.9.0
#################################

.. contents::
   :local:
   :depth: 2

|BMshort| v0.9.0 is an early release of this product, demonstrating a set of samples for the `nRF54L15 DK`_ with build targets supporting the following SoCs:

* `nRF54L15`_
* `nRF54L10`_ (emulated on the nRF54L15 DK)
* `nRF54L05`_ (emulated on the nRF54L15 DK)

The supported DK version is `nRF54L15 DK v0.9.3`_.

The release includes a prototype version of the S115 SoftDevice - a Bluetooth® LE stack to be used on the nRF54L15 DK.

Product maturity
****************

As this is version 0.9.0 of |BMshort|, please note that the solution is still under development and not production ready.
Modules and libraries are subject to changes.
This includes potential modifications to the organization of the folder structure.
Although the memory partition structure is expected to remain consistent, the sizes of individual partitions might be refined as progress is made towards a stable release.

The documentation for |BMshort| is still in development and might not cover all libraries and modules available in the codebase.
Some sections are incomplete and lacking detailed information as content is being developed.

IDE and tool support
********************

`nRF Connect extension for Visual Studio Code <nRF Connect for Visual Studio Code_>`_ is the recommended IDE for |BMshort| v0.9.0.
See the :ref:`install_nrf_bm` section for more information about supported operating systems and toolchain.

Supported boards
****************

* PCA10156 (`nRF54L15 DK`_)

Migration from nRF5 SDK
***********************

For guidelines on how to migrate an application built on nRF5 SDK to |BMshort|, refer to `Migration notes - nRF5 SDK to nRF Connect SDK Bare Metal option`_.

S115 SoftDevice
***************

The S115 SoftDevice has not been updated since last release and its version is still ``s115_9.0.0-3.prototype``.
The SoftDevice comes in three variants to support different SoCs of the nRF54L Series: nRF54L15, nRF54L10, and nRF54L05.

* :file:`s115_9.0.0-3.prototype_nrf54l15_softdevice.hex`
* :file:`s115_9.0.0-3.prototype_nrf54l10_softdevice.hex`
* :file:`s115_9.0.0-3.prototype_nrf54l05_softdevice.hex`

For more details about the SoftDevice, see :ref:`s115_docs`.

For API documentation, see `S115 9.0.0-3.prototype API reference`_.

Changelog
*********

The following sections provide detailed lists of changes by component.

SoftDevice Handler
==================

* Added the :c:func:`nrf_sdh_ble_conn_handle_get` function.

* Updated:

  * The system initialization to initialize on application level.
  * The entropy source to use the CRACEN true random number generator instead of the CRACEN pseudorandom number generator.

Boards
======

* Boards must now select a SoftDevice sysbuild Kconfig in the `Kconfig.sysbuild` file e.g. :kconfig:option:`SB_CONFIG_SOFTDEVICE_S115`.
  The supported Nordic boards have been updated to reflect this change and no action is necessary.

Logging
=======

* Fixed an issue where builds would fail if the :kconfig:option:`CONFIG_LOG` Kconfig option was disabled and the :kconfig:option:`CONFIG_LOG_BACKEND_BM_UARTE` Kconfig option was enabled.

Libraries
=========

* Added the following libraries:

  * The :ref:`api_ble_conn_state` library.
  * The :ref:`lib_peer_manager` library.
  * The :ref:`lib_ble_service_hids`.

* :ref:`lib_bm_zms` library:

   * Added the :c:enumerator:`BM_ZMS_EVT_DELETE` event ID to distinguish :c:func:`bm_zms_delete` events.

   * Updated:

     * The :c:func:`bm_zms_register` function to return ``-EINVAL`` when passing ``NULL`` input parameters.
     * The name of the :c:struct:`bm_zms_evt_t` ``id`` field to :c:member:`bm_zms_evt_t.evt_id`.
     * The name of the :c:struct:`bm_zms_evt_t` ``ate_id`` field to :c:member:`bm_zms_evt_t.id`.
     * The type of :c:member:`bm_zms_evt_t.result` from ``uint32_t`` to ``int``.

   * Fixed an issue where some data was written incorrectly to storage if the data size was not a multiple of the program unit of 16 bytes.

* :ref:`lib_ble_conn_params` library:

   * Fixed an issue that caused the :kconfig:option:`CONFIG_BLE_CONN_PARAMS_INITIATE_DATA_LENGTH_UPDATE` Kconfig option to be always hidden.

Samples
=======

Bluetooth samples
-----------------

* Added the :ref:`ble_hids_keyboard_sample` and :ref:`ble_hids_mouse_sample` samples.

* :ref:`ble_hrs_sample` sample:

  * Added support for bonding and pairing.

* :ref:`ble_cgms_sample` sample:

  * Corrected the return type for the :c:func:`ble_bas_battery_level_update` function.

Known issues and limitations
============================

* Some issues are observed when using iPhone as the peer during testing of the Bluetooth samples.
* Some issues are observed when using Linux with the Bluetooth Low Energy app in nRF Connect for Desktop as the peer during testing of the Bluetooth samples.
* The samples are not optimized for power consumption unless explicitly stated.

..

* :ref:`ble_mcuboot_recovery_retention_sample` sample: the sample fails to build on Windows due to the path length being too long (more than 260 chars).
  As a workaround, use the VS Code :guilabel:`Create a new application -> Copy from sample` option and place it in a location where the path is shorter.

Documentation
=============

* Added documentation for :ref:`Bluetooth services <lib_bm_bluetooth_services>`.
* Added the :ref:`nrf5_bm_migration_nvm` section in the :ref:`nrf5_bm_migration`.
