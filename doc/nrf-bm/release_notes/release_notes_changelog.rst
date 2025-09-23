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

SoftDevice Handler
==================

* Added:

  * The :option:`NRF_SDH_SOC_RAND_SEED` Kconfig option to control whether to automatically respond to SoftDevice random seed requests.
  * Priority levels for SoftDevice event observers: HIGHEST, HIGH, USER, USER_LOW, LOWEST.
  * The :c:func:`nrf_sdh_ble_evt_tostr` function to stringify a BLE event.
  * The :c:func:`nrf_sdh_soc_evt_tostr` function to stringify a SoC event.

* Changed:

  * The return type of the :c:type:`nrf_sdh_state_evt_handler_t` event handler to ``int``.

* Removed:

  * The ``NRF_SDH_BLE`` Kconfig option.
  * The ``NRF_SDH_STATE_REQ_OBSERVER`` macro and relative data types.
    Register a state event observer and return non-zero to ``NRF_SDH_STATE_EVT_ENABLE_PREPARE``
    or ``NRF_SDH_STATE_EVT_DISABLE_PREPARE`` to abort state changes instead.
  * The ``nrf_sdh_ble_app_ram_start_get`` function.
  * The ``nrf_sdh_request_continue`` function.

Boards
======

No changes since the latest nRF Connect SDK Bare Metal release.

DFU
===

No changes since the latest nRF Connect SDK Bare Metal release.

Logging
=======

* Removed ``module-dep=LOG`` in Kconfig files.
  This is no longer defined.

Drivers
=======

No changes since the latest nRF Connect SDK Bare Metal release.

Libraries
=========

No changes since the latest nRF Connect SDK Bare Metal release.

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

No changes since the latest nRF Connect SDK Bare Metal release.

Known issues and limitations
============================

No changes since the latest nRF Connect SDK Bare Metal release.

Documentation
=============

No changes since the latest nRF Connect SDK Bare Metal release.
