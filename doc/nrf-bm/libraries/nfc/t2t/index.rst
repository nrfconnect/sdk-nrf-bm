.. _lib_nfc_t2t:

Type 2 Tag
##########

.. contents::
   :local:
   :depth: 2

The Type 2 Tag library implements NFC Type 2 Tag functionality based on the NFC Forum document *Type 2 Tag Technical Specification Version 1.0*.

Overview
********

A Type 2 Tag can be read and re-written, and the memory of the tag can be write protected.
The Type 2 Tag library implements a Type 2 Tag in read-only state, meaning the memory is write-protected and cannot be erased or re-written by the polling device.

For detailed information about the Type 2 Tag memory layout, data container structure, and TLV blocks format, see the `Type 2 Tag nrfxlib documentation`_.

The library provides functions to perform the following operations:

* Initialize the NFC front end and register event callbacks.
* Set NDEF message payload or raw TLV data.
* Configure NFC parameters such as NFCID1.
* Start and stop NFC field sensing.

Configuration
*************

To enable the library, set the :kconfig:option:`CONFIG_NFC_T2T_NRFXLIB` Kconfig option.

Usage
*****

Programming a Type 2 Tag involves the following steps:

1. Register callback and initialize.

   Implement a callback function to handle NFC events and register it using the :c:func:`nfc_t2t_setup` function:

   .. code-block:: c

      static void nfc_callback(void *context,
                               nfc_t2t_event_t event,
                               const uint8_t *data,
                               size_t data_length)
      {
          switch (event) {
          case NFC_T2T_EVENT_FIELD_ON:
              /* NFC field detected */
              break;
          case NFC_T2T_EVENT_FIELD_OFF:
              /* NFC field removed */
              break;
          case NFC_T2T_EVENT_DATA_READ:
              /* Tag data has been read */
              break;
          default:
              break;
          }
      }

      int err = nfc_t2t_setup(nfc_callback, NULL);
      if (err != 0) {
          /* Handle error */
      }

#. Set the NDEF payload.

   Use the :c:func:`nfc_t2t_payload_set` function to configure an NDEF message:

   .. code-block:: c

      uint8_t ndef_msg_buf[256];
      size_t len = sizeof(ndef_msg_buf);

      /* Encode your NDEF message into ndef_msg_buf */

      err = nfc_t2t_payload_set(ndef_msg_buf, len);
      if (err != 0) {
          /* Handle error */
      }

   Alternatively, use the :c:func:`nfc_t2t_payload_raw_set` function to set a raw TLV structure for advanced use cases.

#. Start NFC emulation.

   Activate the NFC front end to start sensing for NFC fields:

   .. code-block:: c

      err = nfc_t2t_emulation_start();
      if (err != 0) {
          /* Handle error */
      }

#. Stop emulation (optional).

   Stop the NFC operation as follows:

   .. code-block:: c

      err = nfc_t2t_emulation_stop();
      if (err != 0) {
          /* Handle error */
      }

Events
******

The library signals the following events through the registered callback:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Event
     - Description
   * - :c:enumerator:`NFC_T2T_EVENT_FIELD_ON`
     - NFC tag detected external NFC field and was selected by a polling device
   * - :c:enumerator:`NFC_T2T_EVENT_FIELD_OFF`
     - External NFC field has been removed
   * - :c:enumerator:`NFC_T2T_EVENT_DATA_READ`
     - NFC polling device has read all tag data
   * - :c:enumerator:`NFC_T2T_EVENT_STOPPED`
     - Reference to application callback released via :c:func:`nfc_t2t_done`

Parameters
**********

The following parameters can be configured using :c:func:`nfc_t2t_parameter_set`:

* :c:enumerator:`NFC_T2T_PARAM_FDT_MIN` - Frame Delay Time Min parameter controlling frame transmission timing during collision resolution.
* :c:enumerator:`NFC_T2T_PARAM_NFCID1` - NFCID1 value.
  Data can be 4, 7, or 10 bytes long.
  The default 7-byte NFCID1 from FICR registers is used if not set.

Dependencies
************

This library requires:

* NFCT driver from `nrfx`_
* :ref:`lib_nfc_platform`
* `NDEF library`_ (for encoding NDEF messages)

The :ref:`record_text_t2t_sample` sample demonstrates how to use this library.

API documentation
*****************

| Header file: :file:`nrfxlib/nfc/include/nfc_t2t_lib.h`
| Library: :file:`nrfxlib/nfc/lib/`

:ref:`Type 2 Tag library API reference <nfc_api_type2>`
