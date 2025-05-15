.. _ncs_bm_release_notes_010:

Release Notes for |BMlong| v0.1.0
#################################

.. contents::
   :local:
   :depth: 2

Version 0.1.0 is an early release demonstrating a first set of samples on the nRF54L15 DK.

It also includes a prototype version of the S115 SoftDevice - a proprietary Bluetooth LE stack to be used on the nRF54L15 DK.

IDE and tool support
********************

`nRF Connect extension for Visual Studio Code <nRF Connect for Visual Studio Code_>`_ is the recommended IDE for |BMshort| v0.1.0.
See the :ref:`install_ncs_bm` section for more information about supported operating systems and toolchain.

Supported board
***************

* PCA10156 (`nRF54L15 DK`_)

Changelog
*********

The following sections provide detailed lists of changes by component.

Samples
=======

Added a set of first samples:

* :ref:`ble_hrs_sample`
* :ref:`ble_lbs_sample`
* :ref:`ble_nus_sample`
* :ref:`buttons_sample`
* :ref:`hello_softdevice_sample`
* :ref:`timer_sample`

S115 SoftDevice
===============

* A prototype binary of the S115 SoftDevice can be found in :file:`subsys/softdevice/hex/s115/s115_9.0.0-1.prototype_softdevice.hex`.

Documentation
=============

* Created first draft including documentation for :ref:`install_ncs_bm`.
