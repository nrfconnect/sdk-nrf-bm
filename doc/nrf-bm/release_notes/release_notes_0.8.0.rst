.. _nrf_bm_release_notes_080:

Release Notes for |BMlong| v0.8.0
#################################

.. contents::
   :local:
   :depth: 2

|BMshort| v0.8.0 is the public launch release of this product, demonstrating a set of samples for the `nRF54L15 DK`_ with build targets supporting the following SoCs:

* `nRF54L15`_
* `nRF54L10`_ (emulated on the nRF54L15 DK)
* `nRF54L05`_ (emulated on the nRF54L15 DK)

The supported DK version is `nRF54L15 DK v0.9.3`_.

The release includes a prototype version of the S115 SoftDevice - a proprietary Bluetooth® LE stack to be used on the nRF54L15 DK.

Product maturity
****************

As this is version 0.8.0 of |BMshort|, please note that the solution is still under development and not production ready.
Modules and libraries are subject to changes.
This includes potential modifications to the organization of the folder structure.
Although the memory partition structure is expected to remain consistent, the sizes of individual partitions might be refined as progress is made towards a stable release.

The documentation for |BMshort| is still in development and might not cover all libraries and modules available in the codebase.
Some sections are incomplete and lacking detailed information as content is being developed.

IDE and tool support
********************

`nRF Connect extension for Visual Studio Code <nRF Connect for Visual Studio Code_>`_ is the recommended IDE for |BMshort| v0.8.0.
See the :ref:`install_nrf_bm` section for more information about supported operating systems and toolchain.

Supported boards
****************

* PCA10156 (`nRF54L15 DK`_)

Migration from nRF5 SDK
***********************

For guidelines on how to migrate an application built on nRF5 SDK to |BMshort|, refer to `Migration notes - nRF5 SDK to nRF Connect SDK Bare Metal option`_.

Changelog
*********

The following sections provide detailed lists of changes by component.

SDK installation
================

* The default way to install |BMshort| is through the |nRFVSC| user interface, using either the :guilabel:`Pre-packaged SDK & Toolchain` option or a GitHub installation.
  For detailed steps, see :ref:`cloning_the_repositories_nrf_bm`.

S115 SoftDevice
===============

* Updated the S115 SoftDevice version to ``s115_9.0.0-3.prototype``.
  The SoftDevice comes in three variants to support different SoCs of the nRF54L Series: nRF54L15, nRF54L10, and nRF54L05.

  * :file:`s115_9.0.0-3.prototype_nrf54l15_softdevice.hex`
  * :file:`s115_9.0.0-3.prototype_nrf54l10_softdevice.hex`
  * :file:`s115_9.0.0-3.prototype_nrf54l05_softdevice.hex`

  For more details about this release of the SoftDevice, see :ref:`s115_docs`.

  For API documentation, see `S115 9.0.0-3.prototype API reference`_.

SoftDevice Handler
==================

* Added support for seeding the SoftDevice RNG when requested by the SoftDevice.

* Fixed the maximum MTU Kconfig option.

Boards
======

* Added the following ``bm_nrf54l15dk`` board variants for MCUboot (DFU-enabled board variants):

  * ``bm_nrf54l15dk/nrf54l05/s115_softdevice/mcuboot``
  * ``bm_nrf54l15dk/nrf54l10/s115_softdevice/mcuboot``
  * ``bm_nrf54l15dk/nrf54l15/s115_softdevice/mcuboot``

..

* Renamed the ``bm_nrf54l15dk`` board variants:

  * ``bm_nrf54l15dk/nrf54l05/softdevice_s115`` to ``bm_nrf54l15dk/nrf54l05/s115_softdevice``
  * ``bm_nrf54l15dk/nrf54l10/softdevice_s115`` to ``bm_nrf54l15dk/nrf54l10/s115_softdevice``
  * ``bm_nrf54l15dk/nrf54l15/softdevice_s115`` to ``bm_nrf54l15dk/nrf54l15/s115_softdevice``

..

* Fixed:

  * Fixed wrong RRAM size on the nRF54L10 emulation target from 1022 to 1012 KiB.

..

* Removed:

  * The template board variants.
  * The ``no_softdevice`` variants of the ``bm_nRF54l15dk``.

DFU
===

* DFU using MCUboot firmware loader mode is now introduced in the SDK:

  * Added experimental support for single-bank DFU, using the MCUboot bootloader and a firmware loader which is tasked to receive the new firmware image.
    The DFU supports updating either the application or the SoftDevice with the firmware loader.

  For more details, see :ref:`bm_dfu`.

Logging
=======

* Fixed an issue with the minimal log mode.

Drivers
=======

* Added the following drivers:

  * The :ref:`driver_lpuarte` driver.
  * A SoftDevice-based flash driver implementing the Zephyr flash interface.
    This is added exclusively for DFU and is not expected to be used by other parts of the application.
    Instead, use the :ref:`lib_bm_zms` and ``bm_storage`` libraries.

Libraries
=========

* Added the following libraries:

  * The :ref:`lib_bm_zms` library.
  * The ``bm_storage`` library.
  * A queue library (modified library from Zephyr).
    This library has been added as a dependency for MCUmgr (used in DFU) offering support for threadless applications.

..

* ``ble_adv`` library:

  * Changed the default advertising name to ``nRF_BM_device``.
  * Corrected the :kconfig:option:`CONFIG_BLE_ADV_DIRECTED_ADVERTISING_HIGH_DUTY` Kconfig option.

..

* ``ble_conn_params library`` library:

  * Updated connection parameter values to the :c:enumerator:`BLE_CONN_PARAMS_EVT_UPDATED` event.
  * Renamed slave latency to peripheral latency according to specification.

  * Fixed:

    * An issue where the ATT MTU could be set to a lower value than the current value.
    * An issue where the list of PHYs supported by the SoftDevice was not respected by the library when doing PHY updates.
    * Data length timeout issue and the :c:func:`data_length_get` function returning wrong value.
    * Incorrectly stated maximum ATT MTU value.

Samples
=======

* General changes:

  * All samples now use the logging module instead of ``printk``.

  * Updated:

    * The main loop sleep instructions as the :c:func:`sd_app_evt_wait` function is deprecated.
    * Updated the main function flow to always end up in a loop with sleep instructions and deferred log processing instead of returning from the main function.
      This ensures that log messages are printed and that the device goes into sleep mode.
      Returning from main leaves the device in a state that prevents low power sleep and unhandled deferred log messages are not processed.

Bluetooth samples
-----------------

* Added the :ref:`ble_pwr_profiling_sample` sample.

..

* :ref:`ble_cgms_sample` sample:

  * Fixed an issue with responding twice to a PHY update request.

..

* :ref:`ble_hrs_sample` sample:

  * Removed the unused Coded PHY Kconfig fragment.

..

* :ref:`ble_nus_sample` sample:

  * Added support for :ref:`driver_lpuarte`.

..

  * Fixed:

    * The pin configuration to use the pins from the :file:`board_config.h` board header.
    * An issue in the :c:func:`ble_nus_data_send` function where passing NULL as the NUS instance could cause a segmentation fault.

Peripheral samples
------------------

* Added the following samples:

  * :ref:`bm_zms_sample` sample.
  * :ref:`bm_lpuarte_sample` sample.
  * :ref:`bm_storage_sample` sample.

DFU samples
-----------

* Added the following samples:

  * :ref:`ble_mcuboot_recovery_entry_sample` sample that demonstrates how to activate DFU mode on the device using Bluetooth LE on a mobile application.
  * :ref:`ble_mcuboot_recovery_retention_sample` sample that demonstrates how to switch a device into DFU mode on embedded application's request and execute DFU over UART or Bluetooth LE.

Known issues and limitations
============================

* Some issues are observed when using iPhone as the peer during testing of the Bluetooth samples.
* Some issues are observed when using Linux with the Bluetooth Low Energy app in nRF Connect for Desktop as the peer during testing of the Bluetooth samples.
* There are no samples or libraries available to demonstrate or support the Bluetooth LE Pairing and bonding functionality in the SoftDevice.
* The samples are not optimized for power consumption unless explicitly stated.

Documentation
=============

* First public release of product documentation.
* Added:

  * Top-level sections for :ref:`drivers`, :ref:`libraries`, :ref:`bm_dfu`, and :ref:`s115_docs`.
  * Documentation for the :ref:`lib_ble_conn_params` library.
  * :ref:`nrf_bm_api` section.

* :ref:`s115_docs` are now integrated with the |BMshort| documentation.
