.. _lib_nfc:

NFC libraries
#############

The NFC libraries support the NFC-A listen mode, where the tag waits for polling devices but does not actively start a connection.
This makes them ideal for implementing NFC tag functionality in bare metal applications.

.. note::
   For background information about NFC technology and the NDEF format, see the `nRF Connect SDK NFC libraries`_.
   The bare metal context supports only NFC-A listen mode (tag emulation).

.. toctree::
   :maxdepth: 2
   :caption: Subpages:

   integration_notes
   t2t/index
   t4t/index
   ndef/index
