.. _nrf_bm_release_notes_070:

Release Notes for |BMlong| v0.7.0
#################################

.. contents::
   :local:
   :depth: 2

|BMshort| v0.7.0 is an early release demonstrating a first set of samples for the `nRF54L15 DK`_ with build targets supporting the following SoCs:

* `nRF54L15`_
* `nRF54L10`_
* `nRF54L05`_

It also includes a prototype version of the S115 SoftDevice - a proprietary Bluetooth® LE stack to be used on the nRF54L15 DK.

IDE and tool support
********************

`nRF Connect extension for Visual Studio Code <nRF Connect for Visual Studio Code_>`_ is the recommended IDE for |BMshort| v0.7.0.
See the :ref:`install_nrf_bm` section for more information about supported operating systems and toolchain.

Supported boards
****************

* PCA10156 (`nRF54L15 DK`_)

Changelog
*********

The following sections provide detailed lists of changes by component.

SDK installation
================

* The default way for installing |BMshort| is now through an archive file.
  The file contains a source mirror of the Git repositories required to get started with the SDK.
  For more details, see :ref:`cloning_the_repositories_nrf_bm`.

S115 SoftDevice
===============

* Updated the S115 SoftDevice version to ``s115_9.0.0-2.prototype``.
  The SoftDevice now comes in three variants to support different SoCs of the nRF54L family: nRF54L15, nRF54L10, and nRF54L05.

  See the `s115_9.0.0-2.prototype release notes`_ document for more detailed information.

Samples
=======

Bluetooth samples
-----------------

* Added the :ref:`ble_cgms_sample` sample.

Peripheral samples
------------------

* Added the following samples:

  * :ref:`uarte_sample` sample.
  * :ref:`led_sample` sample.

Documentation
=============

* Refactored sample documentation to make it more user friendly.
* Introduced major updates to the :ref:`Installation instructions <install_nrf_bm>`.
