.. _lib_ble_service_mds:

Memfault Diagnostic Service (MDS)
#################################

.. contents::
   :local:
   :depth: 2

This module implements the `Memfault Diagnostic GATT Service`_ (MDS) over Bluetooth LE on |BMshort|.
It lets an MDS gateway read the Memfault upload URI and authorization from the device, subscribe to data export notifications, and stream Memfault diagnostic chunks to the Memfault cloud.

Overview
********

During initialization, the module adds the vendor-specific MDS GATT service to the Bluetooth LE stack database.
The service and characteristic UUIDs follow the `Memfault Diagnostic GATT Service`_ specification and are defined in :file:`include/bm/bluetooth/services/ble_mds.h`.

The service uses the vendor-specific 128-bit UUID base :c:macro:`BLE_MDS_UUID_BASE` with a 16-bit offset for the service and for each characteristic.
The service uses offset ``0x0000`` (:c:macro:`BLE_UUID_MDS_SERVICE`), which resolves to the 128-bit UUID ``54220000-f6a5-4007-a371-722f4ebd8436``.
Each characteristic resolves to the same 128-bit UUID with its own offset in place of ``0000``.

Characteristics
===============

The service exposes the following characteristics:

Supported Features (read)
   16-bit UUID ``0x0001``, :c:macro:`BLE_UUID_MDS_SUPPORTED_FEATURES_CHAR`.
   Reserved for future use. Currently returns ``0x00``.

Device Identifier (read)
   16-bit UUID ``0x0002``, :c:macro:`BLE_UUID_MDS_DEVICE_IDENTIFIER_CHAR`.
   The Memfault device serial string from ``memfault_platform_get_device_info()``.

Data URI (read)
   16-bit UUID ``0x0003``, :c:macro:`BLE_UUID_MDS_DATA_URI_CHAR`.
   The Memfault chunks API endpoint for this device, ``https://chunks.memfault.com/api/v0/chunks/<device serial>`` with the default Memfault SDK HTTP configuration.

Authorization (read)
   16-bit UUID ``0x0004``, :c:macro:`BLE_UUID_MDS_AUTHORIZATION_CHAR`.
   The HTTP header that a gateway must set when forwarding chunks to the Data URI, in ``<header name>:<header value>`` form.
   The module builds it as ``Memfault-Project-Key:<project key>`` using the configured Memfault project key.

Data Export (write + notify)
   16-bit UUID ``0x0005``, :c:macro:`BLE_UUID_MDS_DATA_EXPORT_CHAR`.
   Used by the gateway to enable or disable streaming and to receive chunk notifications.

The service reads static upload metadata from the Memfault SDK at init time.
Chunk export is driven by the application: call :c:func:`ble_mds_process` from the main loop to pull data from the Memfault packetizer and send notifications when a gateway has subscribed and enabled streaming.

SoftDevice BLE events are handled through the observer registered by :c:macro:`BLE_MDS_DEF`.
Heavy Memfault work (packetizer reads and notifications) is intentionally deferred to :c:func:`ble_mds_process` so it runs in main-loop context.
See :ref:`memfault_bm` for Memfault usage rules on bare metal.

Gateway interaction
===================

A typical MDS gateway session follows these steps:

1. Connect to the peripheral.
2. Read the Device Identifier, Data URI, and Authorization characteristics.
3. Enable notifications on the Data Export Client Characteristic Configuration descriptor (CCCD).
4. Write ``0x01`` to the Data Export characteristic to enable streaming (``0x00`` disables it).
   This write is rejected with the ``Client Characteristic Configuration Descriptor Improperly Configured`` ATT error if the client has not completed step 3 first.
5. Receive chunk notifications until streaming is disabled or the connection drops.
6. Forward received chunks to Memfault using the URI and authorization values.

Only one active MDS subscriber is supported at a time.
If a second central enables notifications while another subscriber is active, the request is ignored.
Disabling notifications or losing the connection resets both the subscription and the streaming state, so a reconnecting gateway must repeat steps 3 and 4.

Configuration
*************

Set the :kconfig:option:`CONFIG_BLE_MDS` Kconfig option to enable the service.
This option depends on the :kconfig:option:`CONFIG_MEMFAULT` Kconfig option.
The build also requires a Memfault project key, set with the :kconfig:option:`CONFIG_MEMFAULT_NCS_PROJECT_KEY` Kconfig option.
See :ref:`Memfault prerequisites <memfault_bm_prerequisites>` for setup.

Additional options:

* :kconfig:option:`CONFIG_BLE_MDS_DATA_URI_MAX_LEN` - Maximum length of the Data URI characteristic value.
* :kconfig:option:`CONFIG_BLE_MDS_EMPTY_POLL_INTERVAL_MS` - Minimum interval between packetizer polls when no data is available.
* :kconfig:option:`CONFIG_BLE_MDS_LOG_COLLECTION` - Periodically snapshot the Memfault RAM log buffer into the packetizer while streaming is active.
  This has no effect unless the :kconfig:option:`CONFIG_MEMFAULT_LOGGING_ENABLE` Kconfig option is also enabled, because that option is what routes Zephyr log messages into the Memfault log buffer.
* :kconfig:option:`CONFIG_BLE_MDS_LOG_COLLECTION_INTERVAL_MS` - Interval for log collection when enabled.

Initialization
==============

The service instance is declared using the :c:macro:`BLE_MDS_DEF` macro, specifying the name of the instance.
The macro also registers a SoftDevice BLE observer for the instance.

The service is initialized by calling the :c:func:`ble_mds_init` function.
Use :c:macro:`BLE_MDS_CONFIG_SEC_MODE_DEFAULT` or populate :c:struct:`ble_mds_config` to set GATT security requirements for each characteristic.

.. note::

   The :c:macro:`BLE_MDS_CONFIG_SEC_MODE_DEFAULT` macro sets every characteristic to :c:macro:`BLE_GAP_CONN_SEC_MODE_OPEN`.
   Any connected central can then read the Memfault project key and the diagnostic data.
   In applications that support pairing, populate the :c:struct:`ble_mds_config` structure with a mode that requires encryption, such as :c:macro:`BLE_GAP_CONN_SEC_MODE_ENC_NO_MITM`, instead of using the default macro.

After initialization, include the MDS UUID type returned by :c:func:`ble_mds_service_uuid_type` in the advertising UUID list if the service should be discoverable before connection.

Usage
*****

Call the :c:func:`ble_mds_process` function regularly from the application main loop while a connection may be active.
The function is non-blocking: it returns immediately if no subscriber is streaming, if a notification is already in flight, or if the SoftDevice TX queue is full.

When streaming is enabled and Memfault data is available, the service:

1. Optionally triggers Memfault log collection (if the :kconfig:option:`CONFIG_BLE_MDS_LOG_COLLECTION` Kconfig option is enabled).
2. Reads the next chunk from the Memfault packetizer, sized to the current ATT MTU.
3. Sends the chunk in a Data Export notification prefixed with a sequence number.

The service only exports data that is already stored in the Memfault event storage.
The application is responsible for collecting that data, such as metrics, trace events, and coredumps.
For the rules on which Memfault APIs to call from the main loop and which to call from an ISR, see :ref:`memfault_bm`.

Dependencies
************

This service has the following |BMshort| dependencies:

* SoftDevice (peripheral role) - :kconfig:option:`CONFIG_SOFTDEVICE_PERIPHERAL`
* :ref:`lib_nrf_sdh` (Bluetooth LE) - :kconfig:option:`CONFIG_NRF_SDH_BLE`
* :ref:`lib_ble_conn_params` (ATT MTU handling) - :kconfig:option:`CONFIG_BLE_CONN_PARAMS`
* Memfault SDK - :kconfig:option:`CONFIG_MEMFAULT`

API documentation
*****************

| Header file: :file:`include/bm/bluetooth/services/ble_mds.h`
| Source files: :file:`subsys/bluetooth/services/ble_mds/`

:ref:`Memfault Diagnostic Service API reference <api_ble_mds>`
