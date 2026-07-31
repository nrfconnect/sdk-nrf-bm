.. _nrf_bm_release_notes_201:

Release Notes for |BMlong| v2.0.1
#################################

The |BMshort| v2.0.1 is a patch release that fixes an issue in the S115 and S145 SoftDevice.
See the :ref:`SoftDevice release notes <softdevice_docs>` for details.

This release is based on the |NCS| v3.3.0.

Supported hardware
******************

The |BMshort| v2.0.1 provides a set of samples for the following development kits of the nRF54L Series:

* `nRF54L15 DK`_, with build targets supporting three SoCs: `nRF54L15`_, `nRF54L10`_, and `nRF54L05`_.
* `nRF54LM20 DK`_, supporting the `nRF54LM20A`_ SoC.
* `nRF54LS05 DK`_, supporting the `nRF54LS05B`_ SoC.
* `nRF54LV10 DK`_, supporting the `nRF54LV10A`_ SoC.

SoftDevice variants
*******************

This release includes two versions of the SoftDevice, the Nordic Semiconductor's Bluetooth LE protocol stack used on the nRF54L DKs.
Both SoftDevices have been updated to **v10.0.1** in this release:

* **S115** - Supports the peripheral role only.
  Recommended for memory-constrained peripheral-only applications.

* **S145** - Supports both peripheral and central roles, and allows a higher number of simultaneous connections.
  Recommended for applications that act as a central, or that need to handle more than one connection.

Separate documentation and API references are now provided for each SoC-specific variant of the S115 and S145 SoftDevices (nRF54L15, nRF54LM20, nRF54LS05, and nRF54LV10).
See :ref:`softdevice_docs` and :ref:`nrf_bm_api` for details.

Release tag
***********

The release tag for the |BMshort| manifest repository is **v2.0.1**.
Check the :file:`west.yml` file for the corresponding tags in the project repositories.

To use this release, install it using |nRFVSC| by following the instructions in :ref:`install_nrf_bm`.

Alternatively, check out the tag in the manifest repository, run ``west config manifest.path nrf-bm``, and then ``west update``.

This release of the |BMshort| is based on |NCS| **v3.3.0**.

IDE and tool support
********************

`nRF Connect for Visual Studio Code`_ is the recommended IDE for the |BMshort| v2.0.1.
See the :ref:`Installation <install_nrf_bm>` section for more information about supported operating systems and toolchain.

Changelog
*********

The following sections provide detailed lists of changes by component.

SDK installation
================

* Updated the steps to install prerequisites in the :ref:`install_nrf_bm` page.
  Nordic Semiconductor tools will inform you about the recommended version of SEGGER J-Link.

S115 SoftDevice
===============

* Updated the SoftDevice to v10.0.1.
  See the :ref:`SoftDevice release notes <softdevice_docs>` for details.

S145 SoftDevice
===============

* Updated the SoftDevice to v10.0.1.
  See the :ref:`SoftDevice release notes <softdevice_docs>` for details.
