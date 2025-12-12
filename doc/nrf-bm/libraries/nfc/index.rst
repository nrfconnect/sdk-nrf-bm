.. _lib_nfc:

NFC libraries
#############

.. contents::
   :local:
   :depth: 2

The NFC libraries support NFC-A listen mode, where the tag waits for polling devices but does not actively start a connection.
This makes them ideal for implementing NFC tag functionality in bare metal applications.

Supported NFC tag types
***********************

The following NFC tag types are supported:

.. list-table::
   :header-rows: 1
   :widths: auto

   * - Tag Type
     - Description
     - Library
   * - Type 2 Tag (T2T)
     - Read-only NDEF tag with 992 bytes of data
     - :ref:`lib_nfc_t2t`
   * - Type 4 Tag (T4T)
     - Read-only or read-write NDEF tag with up to 64 KB of data
     - :ref:`lib_nfc_t4t`

Integration requirements
************************

When integrating the NFC libraries in your application, be aware of the following:

* The libraries require the NFCT driver from the `nrfx`_ repository.
* The NFCT driver uses one Timer peripheral.
  You can configure the timer instance by changing ``NRFX_NFCT_CONFIG_TIMER_INSTANCE_ID`` in the :file:`nrfx_config.h` header.
* Each library must be the only user of the NFCT peripheral and the chosen TIMER instance.
* To use the libraries, you must implement an NFC Platform software module that handles clock control and interrupt routing.
  See the :ref:`lib_nfc_platform` section for details.

.. _lib_nfc_platform:

NFC Platform module
*******************

The NFC Platform module allows the NFC libraries to operate in different runtime environments.
This module is responsible for activating the NFCT driver when the following conditions are fulfilled:

* NFC field is present
* HFCLK is running

The module must implement the following functions:

:c:func:`nfc_platform_setup`
   Called by the NFC libraries at initialization.
   Sets up the clock interface and connects NFCT and the chosen TIMER IRQs with their respective IRQ handler functions from nrfx.

:c:func:`nfc_platform_nfcid1_default_bytes_get`
   Used to fetch default bytes for NFCID1 stored in FICR registers.

:c:func:`nfc_platform_event_handler`
   Called by the NFC libraries to forward NFC events received from the NFCT driver.
   This is necessary to track when HFCLK must be running.

It is recommended to request HFCLK asynchronously with a notification when the clock has been started, and activate the NFCT peripheral after receiving this notification.

.. toctree::
   :maxdepth: 1
   :caption: NFC Tag libraries:

   type_2_tag
   type_4_tag
