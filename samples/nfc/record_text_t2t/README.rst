.. _record_text_t2t_sample:

NFC: Text record for Type 2 Tag
###############################

.. contents::
   :local:
   :depth: 2

The NFC Text record for Type 2 Tag sample shows how to use the NFC tag to expose a text record on a Type 2 Tag to NFC polling devices.
It uses the NFC Data Exchange Format (NDEF).

Requirements
************

The sample supports the following development kits:

The following board variants do not have DFU capabilities.

.. list-table::
   :header-rows: 1

   * - Hardware platform
     - PCA
     - SoftDevice
     - Board target
   * - `nRF54L15 DK`_
     - PCA10156
     - S115
     - bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice
   * - `nRF54L15 DK`_ (emulating nRF54L10)
     - PCA10156
     - S115
     - bm_nrf54l15dk/nrf54l10/cpuapp/s115_softdevice
   * - `nRF54L15 DK`_ (emulating nRF54L05)
     - PCA10156
     - S115
     - bm_nrf54l15dk/nrf54l05/cpuapp/s115_softdevice

The sample also requires a smartphone or tablet.

Overview
********

When the sample starts, it initializes the NFC tag and generates an NDEF message with three text records that contain the text "Hello World!" in three languages.
Then it sets up the NFC library to use the generated message and sense the external NFC field.

The only events handled by the application are the NFC events.

User interface
**************

LED 0:
   Indicates if an NFC field is present.

Building and running
********************

This sample is located under :file:`samples/nfc/record_text/` in the |BMshort| folder structure.

.. include:: /includes/create_sample.txt

.. include:: /includes/configure_and_build_sample.txt

.. include:: /includes/program_sample.txt

.. note::
   |nfc_nfct_driver_note|

Testing
=======

After programming the sample to your development kit, complete the following steps to test it:

1. Touch the NFC antenna with the smartphone or tablet and observe that **LED 0** is lit.
#. Observe that the smartphone or tablet displays the encoded text (in the most suitable language).
#. Move the smartphone or tablet away from the NFC antenna and observe that **LED 0** turns off.

Dependencies
************

This sample uses the following |NCS| libraries:

* NDEF messages
* Text records

In addition, it uses the Type 2 Tag library from `sdk-nrfxlib`_.
