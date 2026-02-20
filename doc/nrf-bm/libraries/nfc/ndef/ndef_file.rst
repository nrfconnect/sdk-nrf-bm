.. _nfc_t4t_ndef_file_readme:

NDEF file
#########

.. contents::
   :local:
   :depth: 2

The NDEF file library stores the length and content of the NDEF message.
Use this library to encode standardized data for the NFC Type 4 Tag.
To generate an NDEF message, you can use the :ref:`nfc_ndef_msg` and :ref:`nfc_ndef_record` libraries.

The following code sample demonstrates how to encode the NDEF file for NFC Type 4 Tag:

.. code-block:: c

   int ndef_file_encode(uint8_t *buff, uint32_t *size)
   {
           int err;
           uint32_t ndef_size = nfc_t4t_ndef_file_msg_size_get(*size);

           /* Encode NDEF message into buffer. */
           err = nfc_ndef_msg_encode(&NFC_NDEF_MSG(my_message),
                                     nfc_t4t_ndef_file_msg_get(buff),
                                     &ndef_size);
           if (err) {
                   return err;
           }

           err = nfc_t4t_ndef_file_encode(buff, &ndef_size);
           if (err) {
                   return err;
           }

           *size = ndef_size;
           return 0;
   }

The :ref:`record_text_t4t_sample` sample demonstrates full usage.

API documentation
*****************

| Header file: :file:`bm/nfc/t4t/ndef_file.h`
| Source file: :file:`subsys/nfc/t4t/ndef_file.c`

:ref:`NDEF file API reference <nfc_t4t_ndef_file>`
