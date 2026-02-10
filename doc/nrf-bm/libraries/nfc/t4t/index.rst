.. _lib_nfc_t4t:

Type 4 Tag
##########

.. contents::
   :local:
   :depth: 2

The Type 4 Tag library implements NFC Type 4 Tag functionality based on the NFC Forum document *Type 4 Tag Technical Specification Version 1.0*.

Overview
********

A Type 4 Tag provides a more sophisticated NFC tag implementation compared to Type 2 Tag.
It uses the ISO-DEP (ISO14443-4A) protocol for communication and provides a file system containing Elementary Files (EFs).

For detailed information about the Type 4 Tag protocol, data exchange, and APDU handling, see the `Type 4 Tag nrfxlib documentation`_.

The library supports three different modes of emulation:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Mode
     - Description
   * - Read-only NDEF
     - NDEF file cannot be modified. Only field status and read events are signaled.
   * - Read-write NDEF
     - NDEF file can be modified. Changes to NDEF content are signaled through callback.
   * - Raw ISO-DEP
     - All APDUs are signaled through the callback. Application handles responses.

.. note::
   Raw ISO-DEP mode is supported by the nrfxlib library but is not implemented in |BMshort| (no sample or application support).

Configuration
*************

To enable the library, set the :kconfig:option:`CONFIG_NFC_T4T_NRFXLIB` Kconfig option.

Usage
*****

The sequence of initialization functions determines the emulation mode.

NDEF emulation mode
===================

For read-only or read-write NDEF tag emulation:

1. Register callback and initialize.

   Implement a callback function to handle NFC events and register it using the :c:func:`nfc_t4t_setup` function:

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

#. Set NDEF payload.

   First encode the NDEF file (see :ref:`nfc_t4t_ndef_file_readme`) for use as the tag payload.
   Then pass the resulting buffer to one of the following:

   a. For read-only mode, use the :c:func:`nfc_t4t_ndef_staticpayload_set` function:

      .. code-block:: c

         static const uint8_t ndef_msg[] = { /* NDEF message data */ };

         err = nfc_t4t_ndef_staticpayload_set(ndef_msg, sizeof(ndef_msg));
         if (err != 0) {
             /* Handle error */
         }

   #. For read-write mode, use the :c:func:`nfc_t4t_ndef_rwpayload_set` function:

      .. code-block:: c

         static uint8_t ndef_buffer[1024];

         /* Initialize ndef_buffer with initial NDEF message */
         err = nfc_t4t_ndef_rwpayload_set(ndef_buffer, sizeof(ndef_buffer));
         if (err != 0) {
             /* Handle error */
         }

#. Start emulation.

   .. code-block:: c

      err = nfc_t4t_emulation_start();
      if (err != 0) {
          /* Handle error */
      }

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

You can configure the following parameters using the :c:func:`nfc_t4t_parameter_set` function:

* :c:enumerator:`NFC_T4T_PARAM_FWI` - Frame Wait Time parameter.
  Maximum allowed value is limited by the :c:enumerator:`NFC_T4T_PARAM_FWI_MAX` parameter.

* :c:enumerator:`NFC_T4T_PARAM_FDT_MIN` - Frame Delay Time Min parameter for collision resolution timing.

* :c:enumerator:`NFC_T4T_PARAM_SELRES` - Protocol bits for SEL_RES packet.

* :c:enumerator:`NFC_T4T_PARAM_NFCID1` - NFCID1 value.
  Data can be 4, 7, or 10 bytes long.

* :c:enumerator:`NFC_T4T_PARAM_FWI_MAX` - Maximum value for Frame Wait Time Integer (7 for EMV, 8 for NFC specification).

Dependencies
************

This library requires the following drivers and libraries:

* NFCT driver from `nrfx`_
* :ref:`lib_nfc_platform`
* NDEF encoding (for example, :ref:`nfc_ndef_msg` and :ref:`nfc_t4t_ndef_file_readme`)

The :ref:`record_text_t4t_sample` sample demonstrates how to use this library.

API reference
*************

| Header file: :file:`nrfxlib/nfc/include/nfc_t4t_lib.h`
| Library: :file:`nrfxlib/nfc/lib/`

:ref:`Type 4 Tag library API reference <nfc_api_type4>`
