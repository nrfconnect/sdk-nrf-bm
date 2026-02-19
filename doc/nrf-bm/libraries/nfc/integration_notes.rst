.. _lib_nfc_integration:

Integration notes
#################

When integrating the NFC libraries in your application, consider the following notes.

.. contents::
   :local:
   :depth: 2

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
  See :ref:`nrfx driver <driver_nrfx>` for more information.

  The NFCT driver pre-allocates one Timer peripheral that it needs to work correctly.
  On nRF54L Series devices, this timer is set to TIMER24.
  It is not recommended to change the timer.
  To override the default when necessary, set the ``NRFX_NFCT_CONFIG_TIMER_INSTANCE_ID`` macro in the :file:`nrfx_config.h` file.
  The following is an example configuration for :file:`nrfx_config.h` (valid for all nRF54L Series devices):

  .. code-block:: c

   #ifndef NRFX_CONFIG_H__
   #define NRFX_CONFIG_H__

   /* Set the Timer TIMER22 instance for the NFCT driver. */
   #define NRFX_NFCT_CONFIG_TIMER_INSTANCE_ID 22

   /* Use defaults for undefined symbols. */
   #include "nrfx_templates_config.h"
   #endif // NRFX_CONFIG_H__

* Each library must be the only user of the NFCT peripheral and of the chosen TIMER instance.
* The libraries use an NFC Platform software module that handles clock control and interrupt routing.
  See the :ref:`lib_nfc_platform` section for details.

.. _lib_nfc_platform:

NFC Platform module
*******************

The NFC Platform module allows the NFC libraries to operate in different runtime environments.

To enable this module, set the :kconfig:option:`CONFIG_BM_NFC_PLATFORM` Kconfig option.
This option is automatically enabled when using :kconfig:option:`CONFIG_NFC_T2T_NRFXLIB` or :kconfig:option:`CONFIG_NFC_T4T_NRFXLIB`.

This module is responsible for activating the NFCT driver when the following conditions are fulfilled:

* NFC field is present
* HFCLK is running

The module is implemented in the :file:`subsys/nfc/lib/nfc_bm_platform.c` file and provides the following functions:

:c:func:`nfc_platform_setup`
   Called by the NFC libraries at initialization.
   Sets up the clock interface and connects NFCT and the chosen TIMER IRQs with their respective IRQ handler functions from nrfx.

:c:func:`nfc_platform_nfcid1_default_bytes_get`
   Used to fetch default bytes for NFCID1 stored in FICR registers.

:c:func:`nfc_platform_event_handler`
   Called by the NFC libraries to forward NFC events received from the NFCT driver.
   This is necessary to track when HFCLK must be running.

The implementation requests HFCLK asynchronously with a notification when the clock has been started, and activates the NFCT peripheral after receiving this notification.
