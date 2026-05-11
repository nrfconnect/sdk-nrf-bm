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

No changes since the latest nRF Connect SDK Bare Metal release.

Boards
======

No changes since the latest nRF Connect SDK Bare Metal release.

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

   * Added support for selecting more than one PHY mode (1M, 2M, and Coded) when setting the PHY mode preference with Kconfig.
   * Updated the :c:func:`ble_conn_params_phy_radio_mode_set` function to return :c:macro:`NRF_ERROR_INVALID_PARAM` if the ``phy_pref`` parameter contains PHY modes not supported by the SoftDevice.

   * Fixed:

      * An issue where the :c:func:`ble_conn_params_phy_radio_mode_get` function would incorrectly return the PHY mode mask of a pending update rather than the currently active PHY mode if a PHY update initiated by the :c:func:`ble_conn_params_phy_radio_mode_set` function was still in progress.
      * An issue where the SoftDevice define :c:macro:`BLE_GAP_PHYS_SUPPORTED` was used instead of the PHY preferences set with Kconfig when initiating or responding to a PHY update procedure.

Bluetooth LE Services
---------------------

No changes since the latest nRF Connect SDK Bare Metal release.

Libraries for NFC
-----------------

No changes since the latest nRF Connect SDK Bare Metal release.

Utils
-----

No changes since the latest nRF Connect SDK Bare Metal release.

Samples
=======

No changes since the latest nRF Connect SDK Bare Metal release.

Bluetooth LE samples
--------------------

No changes since the latest nRF Connect SDK Bare Metal release.

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
