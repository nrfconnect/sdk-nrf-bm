.. _lib_nfc_ndef:

NFC Data Exchange Format (NDEF)
###############################

NFC communication uses NFC Data Exchange Format (NDEF) messages to exchange data.

See the `nRF Connect SDK Near Field Communication`_ user guide for more information about `nRF Connect SDK NFC Data Exchange Format`_.

The |BMshort| provides modules for generating messages and records to make it easy to generate such messages.

.. note::
  If you want to implement your own polling device, refer to the `nRF Connect SDK Near Field Communication`_ user guide.

The NFC NDEF libraries are used in the following samples:

* :ref:`record_text_t2t_sample` (Type 2 Tag)
* :ref:`record_text_t4t_sample` (Type 4 Tag)

.. toctree::
   :maxdepth: 1
   :glob:
   :caption: Subpages:

   *
