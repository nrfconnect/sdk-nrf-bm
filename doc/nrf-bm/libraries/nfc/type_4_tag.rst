.. _lib_nfc_t4t:

NFC Type 4 Tag library
######################

.. contents::
   :local:
   :depth: 2

The Type 4 Tag library implements NFC Type 4 Tag functionality based on the NFC Forum document *Type 4 Tag Technical Specification Version 1.0*.

Overview
********

A Type 4 Tag provides a more sophisticated NFC tag implementation compared to Type 2 Tag.
It uses the ISO-DEP (ISO14443-4A) protocol for communication and provides a file system containing Elementary Files (EFs).

The library supports three different modes of emulation:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Mode
     - Description
   * - Raw ISO-DEP
     - All APDUs are signaled through the callback. Application handles responses.
   * - Read-only NDEF
     - NDEF file cannot be modified. Only field status and read events are signaled.
   * - Read-write NDEF
     - NDEF file can be modified. Changes to NDEF content are signaled through callback.

File structure
**************

A Type 4 Tag contains at least two Elementary Files:

**Capability Container (CC)**
   A read-only metafile containing the version of the implemented specification, communication parameters, and properties of all other EF files.

**NDEF File**
   Contains the NDEF message. The file format consists of:

   .. list-table::
      :header-rows: 1
      :widths: 15 15 70

      * - Field
        - Length
        - Description
      * - NLEN
        - 2 bytes
        - Length of the NDEF message in big-endian format
      * - NDEF Message
        - NLEN bytes
        - The actual NDEF message content

Configuration
*************

Enable the library by setting the :kconfig:option:`CONFIG_NFC_T4T_NRFXLIB` Kconfig option.

The library requires the NFCT driver from nrfx.
Configure the timer instance via ``NRFX_NFCT_CONFIG_TIMER_INSTANCE_ID`` in :file:`nrfx_config.h`.

Usage
*****

The sequence of initialization functions determines the emulation mode:

NDEF emulation mode
===================

For read-only or read-write NDEF tag emulation:

1. **Register callback and initialize**

   Implement a callback function to handle NFC events and register it using :c:func:`nfc_t4t_setup`:

   .. code-block:: c

      static void nfc_callback(void *context,
                               nfc_t4t_event_t event,
                               const uint8_t *data,
                               size_t data_length,
                               uint32_t flags)
      {
          switch (event) {
          case NFC_T4T_EVENT_FIELD_ON:
              /* NFC field detected */
              break;
          case NFC_T4T_EVENT_FIELD_OFF:
              /* NFC field removed */
              break;
          case NFC_T4T_EVENT_NDEF_READ:
              /* NDEF data has been read */
              break;
          case NFC_T4T_EVENT_NDEF_UPDATED:
              /* NDEF content was updated (read-write mode) */
              break;
          default:
              break;
          }
      }

      int err = nfc_t4t_setup(nfc_callback, NULL);
      if (err != 0) {
          /* Handle error */
      }

2. **Set NDEF payload**

   For read-only mode, use :c:func:`nfc_t4t_ndef_staticpayload_set`:

   .. code-block:: c

      static const uint8_t ndef_msg[] = { /* NDEF message data */ };

      err = nfc_t4t_ndef_staticpayload_set(ndef_msg, sizeof(ndef_msg));
      if (err != 0) {
          /* Handle error */
      }

   For read-write mode, use :c:func:`nfc_t4t_ndef_rwpayload_set`:

   .. code-block:: c

      static uint8_t ndef_buffer[1024];
      /* Initialize ndef_buffer with initial NDEF message */

      err = nfc_t4t_ndef_rwpayload_set(ndef_buffer, sizeof(ndef_buffer));
      if (err != 0) {
          /* Handle error */
      }

3. **Start emulation**

   .. code-block:: c

      err = nfc_t4t_emulation_start();
      if (err != 0) {
          /* Handle error */
      }

Raw ISO-DEP mode
================

For raw mode where the application handles all APDUs:

1. Call :c:func:`nfc_t4t_setup` with a callback
2. Call :c:func:`nfc_t4t_emulation_start` directly (without setting NDEF payload)
3. Handle :c:enumerator:`NFC_T4T_EVENT_DATA_IND` events and respond using :c:func:`nfc_t4t_response_pdu_send`

Events
******

The library signals the following events through the registered callback:

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Event
     - Description
   * - :c:enumerator:`NFC_T4T_EVENT_FIELD_ON`
     - External reader polling detected
   * - :c:enumerator:`NFC_T4T_EVENT_FIELD_OFF`
     - External reader polling ended
   * - :c:enumerator:`NFC_T4T_EVENT_NDEF_READ`
     - Reader has read static NDEF data (read-only mode)
   * - :c:enumerator:`NFC_T4T_EVENT_NDEF_UPDATED`
     - Reader has written to NDEF data (read-write mode)
   * - :c:enumerator:`NFC_T4T_EVENT_DATA_TRANSMITTED`
     - Response PDU sent in raw mode
   * - :c:enumerator:`NFC_T4T_EVENT_DATA_IND`
     - APDU fragment received in raw mode

Parameters
**********

The following parameters can be configured using :c:func:`nfc_t4t_parameter_set`:

:c:enumerator:`NFC_T4T_PARAM_FWI`
   Frame Wait Time parameter. Maximum allowed value is limited by :c:enumerator:`NFC_T4T_PARAM_FWI_MAX`.

:c:enumerator:`NFC_T4T_PARAM_FDT_MIN`
   Frame Delay Time Min parameter for collision resolution timing.

:c:enumerator:`NFC_T4T_PARAM_SELRES`
   Protocol bits for SEL_RES packet.

:c:enumerator:`NFC_T4T_PARAM_NFCID1`
   NFCID1 value. Data can be 4, 7, or 10 bytes long.

:c:enumerator:`NFC_T4T_PARAM_FWI_MAX`
   Maximum value for Frame Wait Time Integer (7 for EMV, 8 for NFC specification).

Dependencies
************

This library requires:

* NFCT driver from `nrfx`_
* NFC Platform module implementation
* NDEF message library (for encoding NDEF messages)

The :ref:`record_text_t4t_sample` demonstrates how to use this library.

API documentation
*****************

| Header file: :file:`nrfxlib/nfc/include/nfc_t4t_lib.h`
| Library: :file:`nrfxlib/nfc/lib/`

:ref:`Type 4 Tag library API reference <nfc_api_type4>`
