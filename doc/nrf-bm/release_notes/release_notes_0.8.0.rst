.. _nrf_bm_release_notes_080:

Release Notes for |BMlong| v0.8.0
#################################

.. contents::
   :local:
   :depth: 2

|BMshort| v0.8.0 is the public launch release of this solution, demonstrating a set of samples for the `nRF54L15 DK`_ with build targets supporting the following SoCs:

* `nRF54L15`_
* `nRF54L10`_
* `nRF54L05`_

The release includes a prototype version of the S115 SoftDevice - a proprietary Bluetooth® LE stack to be used on the nRF54L15 DK.

IDE and tool support
********************

`nRF Connect extension for Visual Studio Code <nRF Connect for Visual Studio Code_>`_ is the recommended IDE for |BMshort| v0.8.0.
See the :ref:`install_nrf_bm` section for more information about supported operating systems and toolchain.

Supported boards
****************

* PCA10156 (`nRF54L15 DK`_)

Changelog
*********

The following sections provide detailed lists of changes by component.

SDK installation
================

* The default way for installing |BMshort| is through the |nRFVSC| user interface, using either the :guilabel:`Pre-packaged SDK & Toolchain` option or a GitHub installation.
  For detailed steps, see :ref:`cloning_the_repositories_nrf_bm`.

S115 SoftDevice
===============

* Updated the S115 SoftDevice version to ``s115_9.0.0-3.prototype``.
  The SoftDevice comes in three variants to support different SoCs of the nRF54L family: nRF54L15, nRF54L10, and nRF54L05.

  * :file:`s115_9.0.0-3.prototype_nrf54l15_softdevice`
  * :file:`s115_9.0.0-3.prototype_nrf54l10_softdevice`
  * :file:`s115_9.0.0-3.prototype_nrf54l05_softdevice`

  For more details about this release of the SoftDevice, see :ref:`s115_docs`.

Softdevice Handler
==================

* Added support for seeding the SoftDevice RNG when requested.
* Fixed the maximum MTU Kconfig option.

Boards
======

* Renamed the ``bm_nrf54l15dk`` board variants:

  * ``bm_nrf54l15dk/nrf54l05/softdevice_s115`` to ``bm_nrf54l15dk/nrf54l05/s115_softdevice``
  * ``bm_nrf54l15dk/nrf54l10/softdevice_s115`` to ``bm_nrf54l15dk/nrf54l10/s115_softdevice``
  * ``bm_nrf54l15dk/nrf54l15/softdevice_s115`` to ``bm_nrf54l15dk/nrf54l15/s115_softdevice``

* Added the following ``bm_nrf54l15dk`` board variants for MCUboot (DFU-enabled board variants):

  * ``bm_nrf54l15dk/nrf54l05/softdevice_s115`` to ``bm_nrf54l15dk/nrf54l05/s115_softdevice/mcuboot``
  * ``bm_nrf54l15dk/nrf54l10/softdevice_s115`` to ``bm_nrf54l15dk/nrf54l10/s115_softdevice/mcuboot``
  * ``bm_nrf54l15dk/nrf54l15/softdevice_s115`` to ``bm_nrf54l15dk/nrf54l15/s115_softdevice/mcuboot``

* Removed the template board variants.

DFU
===

* The DFU subsystem is now introduced in the SDK.
  For more details, see :ref:`bm_dfu`.

Logging
=======

* Fixed an issue with the minimal log mode.

Drivers
=======

* Added the following drivers:

  * The :ref:`driver_lpuarte` driver.
  * The SoftDevice-based flash driver.

Libraries
=========

* Added the following libraries:

  * The ``bm_zms`` storage. TODO link when ready
  * The ``Storage`` library.
  * A Zephyr queue functionality.
..
* ``ble_adv`` library:

  * Changed the default advertising name to ``nRF_BM_device``.
  * Corrected the :kconfig:option:`BLE_ADV_DIRECTED_ADVERTISING_HIGH_DUTY` Kconfig option.
..
* ``ble_conn_params library`` library:

  * Added support for asymmetric data length.
  * Renamed slave latency to peripheral latency according to specification.

  * Fixed:

    * Data length timeout issue and the :c:func:`data_length_get` function returning wrong value.
    * Incorrect stated maximum ATT MTU value.
    * Removed unsupported bitmask from PHY update.
..
* ``bm_buttons`` library:

  * Added the use of nrfx critical section API.

Samples
=======

* General changes:

  * All samples now use the logging module instead of ``printk``.
  * Updated the main loop sleep instructions as the :c:func:`sd_app_evt_wait` function is deprecated.
  * Fixed processing of deferred logging before returning from main.

Bluetooth samples
-----------------

* Added the :ref:`ble_pwr_profiling_sample` sample.
..
* :ref:`ble_cgms_sample` sample:

  * Fixed an issue where a PHY update request was responded twice.
..
* :ref:`ble_hrs_sample` sample:

  * Removed the unused Coded PHY overlay.
..
* :ref:`ble_nus_sample` sample:

  * Added support for Low Power UARTE (LPUARTE).
  * Fixed the pin configuration to use the pins from the ``board_config.h`` board header.

Peripheral samples
------------------

* Added the following peripheral samples:

  * :ref:`bm_zms_sample` sample.
  * :ref:`bm_lpuarte_sample` sample.
  * :ref:`storage_sample` sample.

MCUboot samples
---------------

* Added the following samples:

  * :ref:`ble_mcuboot_recovery_entry_sample` sample.
  * :ref:`ble_mcuboot_recovery_retention_sample` sample.

Known issues and limitations
============================

* Some issues are observed when using iPhone as the peer during testing of the Bluetooth samples.
* Some issues are observed when using Linux with the Bluetooth Low Energy app in nRF Connect for Desktop as the peer during testing of the Bluetooth samples.
* Bluetooth LE pairing and bonding is not supported.
* The samples are not optimized for power consumption.

Documentation
=============

* First public release of product documentation.
* Added top-level sections for :ref:`drivers`, :ref:`libraries`, and :ref:`bm_dfu`.
* Added documentation for the :ref:`lib_ble_conn_params` library.
