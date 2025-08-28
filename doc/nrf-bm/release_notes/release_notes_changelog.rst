.. _nrf_bm_release_notes_0899:

Release Notes for |BMlong| v0.8.99
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

* Added the :c:func:`nrf_sdh_ble_conn_handle_get` function.
* Updated the system initialization to initialize on application level.
* Updated the entropy source to use the CRACEN true random number generator instead of the CRACEN pseudorandom number generator.

Boards
======

No changes since the latest nRF Connect SDK Bare Metal release.

DFU
===

No changes since the latest nRF Connect SDK Bare Metal release.

Logging
=======

No changes since the latest nRF Connect SDK Bare Metal release.

Drivers
=======

No changes since the latest nRF Connect SDK Bare Metal release.

Libraries
=========

* :ref:`lib_bm_zms` library:

   * Updated:

     * The :c:func:`bm_zms_register` function to return ``-EINVAL`` when passing ``NULL`` input parameters.
     * The name of the :c:struct:`bm_zms_evt_t` ``id`` field to :c:member:`bm_zms_evt_t.evt_id`.
     * The name of the :c:struct:`bm_zms_evt_t` ``ate_id`` field to :c:member:`bm_zms_evt_t.id`.

   * Added the :c:enumerator:`BM_ZMS_EVT_DELETE` event ID to distinguish :c:func:`bm_zms_delete` events.

* :ref:`lib_ble_conn_params` library:

   * Fixed an issue that caused the :kconfig:option:`CONFIG_BLE_CONN_PARAMS_INITIATE_DATA_LENGTH_UPDATE` Kconfig option to always be hidden.

Samples
=======

Bluetooth samples
-----------------

No changes since the latest nRF Connect SDK Bare Metal release.

Peripheral samples
------------------

No changes since the latest nRF Connect SDK Bare Metal release.

DFU samples
-----------

No changes since the latest nRF Connect SDK Bare Metal release.

Known issues and limitations
============================

No changes since the latest nRF Connect SDK Bare Metal release.

Documentation
=============

No changes since the latest nRF Connect SDK Bare Metal release.
