.. _lib_peer_manager:

Bluetooth: Peer Manager
#######################

.. contents::
   :local:
   :depth: 2

The Peer Manager library handles Bluetooth LE link security procedures and can be used to initiate pairing, bonding, and encryption, as well as to manage device whitelists.

Overview
********

The Peer Manager provides functionality for:

* Configuring and enforcing link security parameters
* Establishing encrypted connections and reusing stored keys
* Creating and storing bonds and GATT data in non-volatile storage
* Managing application-specific peer data
* Notifying bonded peers when the GATT database changes
* Building whitelists based on peer information

Managing link security
======================

After a Bluetooth LE connection is established by the SoftDevice, the Peer Manager is responsible for the pairing procedure and, if requested, for creating a bond.
Before a pairing procedure can be initiated, the application must configure the security parameters that are used.
For example, these parameters contain the security level of the link, information if bonding should be performed, and if so, what data should be shared during bonding.
See the ``ble_gap_sec_params_t`` structure for detailed information about the security parameters.
You can also retrieve information about the level of security that is set on a specific link.
To encrypt traffic on a link, the application must call the :c:func:`pm_conn_secure` function.
Depending on the relationship with the peer and the configured security parameters, this function will establish an encrypted link.
If a bond is established already, the stored encryption key is used. Otherwise, pairing is initiated.

Managing peers
==============

After a bond to a new peer is established, the Peer Manager assigns a unique peer ID to the peer and stores the bonding and GATT data in non-volatile storage.
The application can later read or update this data, if required, but in most cases, it should be handled exclusively by the Peer Manager.
In addition to the bonding and GATT data, the application can store application-specific data for each peer.
The content, format, and size of this data is determined by the application.
The Peer Manager also provides functions to query the number of valid peer IDs and to iterate through all used peer IDs.
Using this mechanism can be convenient, for example, to write application data for all peers.
If an application's GATT database changes, all peers must be informed of this change.
The Peer Manager provides a function that the application should call to distribute the service changed indications.

Managing whitelists
===================

The Peer Manager can be used to create a whitelist that restricts which peers are allowed to connect.
To construct a whitelist, you must provide a list of peer IDs.
The whitelist will then contain the addresses and Identity Resolving Keys (IRKs) of the specified peers.

.. note::

   If you include the Peer Manager in an application and want to use whitelisting, the whitelist must be created by the Peer Manager.

Architecture
************

The Peer Manager consists of the following modules:

Security Manager & Dispatcher
=============================

When the application or a connected peer requests a secured link, the Security Manager & Dispatcher is responsible for handling the required procedure.
It interfaces with the SoftDevice in creating the secured connection, stores and retrieves the exchanged keys, and manages the pairing procedure.
The module consists of two parts: the Security Manager and the Security Dispatcher.
The Security Manager stores security parameters, keeps track of the current state, and coordinates the pairing procedure.
The Security Dispatcher interfaces with the SoftDevice and the non-volatile storage to do the actual pairing.

LE Secure Connections support
=============================

You can enable support for LE Secure Connections (LESC) pairing by setting the :kconfig:option:`CONFIG_PM_LESC` Kconfig option.
This functionality is disabled by default.
In this mode, the Peer Manager handles internally all requests for Diffie-Hellman keys from the SoftDevice.
When enabled, it is necessary to call the :c:func:`nrf_ble_lesc_request_handler` function in the main context of the application.
If there is any pending DH key request, the function will calculate the requested key and provide it to the SoftDevice.

Repeated pairing attempts protection
====================================

You can enable protection against repeated pairing attempts by setting the :kconfig:option:`CONFIG_PM_RA_PROTECTION` Kconfig option.
This functionality is disabled by default.
In this mode, the Peer Manager uses the timing module to keep track of peers that failed at the pairing procedure.
Future pairing attempts from these peers are rejected for a certain period of time.
More detailed description of peer tracking policy can be found in `Bluetooth Core Specification`_ v5.0, Vol 3, Part H, Section 2.3.6.

ID Manager
==========

The ID Manager keeps track of connected peers and identifies them based on different kinds of IDs: the static device address, master ID, Identity Resolving Key (IRK), IRK whitelist index, and peer ID.
It detects if different IDs refer to the same peer and determines which of the connected peers are bonded.
When a bonded device is connected, the application can ask for the connection handle associated with the peer ID (or the other way around).
In addition, the ID Manager creates and maintains whitelists.

GATT Cache Manager
==================

The GATT Cache Manager is composed by the submodules GATT Server Cache Manager and GATT Client Cache Manager.

This module has three main tasks:

* Store CCCD values - As required by the Bluetooth Specification, the GATT Server Cache Manager persistently stores the CCCD values for all bonded peers across connections.
* Distribute service changed indications - When the application notifies the Peer Manager that its database has changed, the GATT Server Cache Manager sends a service changed indication to all connected peers.
  The service changed indications are also sent to all bonded peers when they reconnect.
* Store remote ATT databases in non-volatile storage - If requested by the application, the GATT Client Cache Manager stores the remote database for all peers.
  This attribute caching is optional, but it reduces the required packet exchange and therefore conserves energy.

Peer Database
=============

The Peer Database holds the stored data for all peer IDs.
It provides functions to create unique peer IDs, write and read data for a specific peer ID, free a peer ID, and enumerate all existing peer IDs.

Peer Data Storage
=================

The Peer Data Storage module is an interface between the Peer Database and the :ref:`lib_bm_zms` library.
It is responsible for storing the peer data in non-volatile storage.
To do so, it uses the Bare Metal Zephyr Memory Storage library with a dedicated storage partition in the board Device Tree.
In addition, the Peer Data Storage module assigns peer IDs.

Configuration
*************

The library is enabled and configured entirely using the Kconfig system.
Set the :kconfig:option:`CONFIG_PEER_MANAGER` Kconfig option to enable the library.

Some features are disabled by default and can be optionally enabled:

* :kconfig:option:`CONFIG_PM_LESC` - Enables LESC support in Peer Manager.
* :kconfig:option:`CONFIG_PM_RA_PROTECTION` - Enables protection against repeated pairing attempts in Peer Manager.

Initialization
==============

Initializing the Peer Manager typically consists of three steps:

* Call the :c:func:`pm_init` function once to initialize the module.
* Optionally, call the :c:func:`pm_sec_params_set` function to set the security parameters.
  If you do not call this function, pairing and bonding is not supported.
  See `Security parameters`_.
* Subscribe to the Peer Manager events by calling the :c:func:`pm_register` function.

The following code example shows how the Peer Manager is initialized:

.. code-block:: c

   static int peer_manager_init(bool erase_bonds)
   {
      uint32_t err;
      ble_gap_sec_params_t sec_param;

      err = pm_init();
      if (err) {
         return -EFAULT;
      }

      if (erase_bonds) {
         pm_peers_delete();
      }

      /* Security parameters to be used for all security procedures. */
      sec_param = (ble_gap_sec_params_t) {
         .bond = SEC_PARAM_BOND,
         .mitm = SEC_PARAM_MITM,
         .lesc = SEC_PARAM_LESC,
         .keypress = SEC_PARAM_KEYPRESS,
         .io_caps = SEC_PARAM_IO_CAPABILITIES,
         .oob = SEC_PARAM_OOB,
         .min_key_size = SEC_PARAM_MIN_KEY_SIZE,
         .max_key_size = SEC_PARAM_MAX_KEY_SIZE,
         .kdist_own.enc = 1,
         .kdist_own.id = 1,
         .kdist_peer.enc = 1,
         .kdist_peer.id = 1,
      };

      err = pm_sec_params_set(&sec_param);
      if (err) {
         LOG_ERR("pm_sec_params_set() failed, err: 0x%x", err);
         return -EFAULT;
      }

      err = pm_register(pm_evt_handler);
      if (err) {
         LOG_ERR("pm_register() failed, err: 0x%x", err);
         return -EFAULT;
      }
   }

Usage
*****

Security parameters
===================

The :c:func:`pm_sec_params_set` function configures how the Peer Manager behaves when securing the link, thus it configures bonding, pairing, and encryption.
The configuration is given by security parameters (``ble_gap_sec_params_t``).
These security parameters are also used directly in the SoftDevice security API and contain the parameters that are sent over-the-air during the bonding procedure.
See the `Bluetooth Core Specification`_ (sections 3.H.3.5.1 and 3.H.3.5.2) for more information.

The :c:func:`pm_sec_params_set` function rejects invalid security parameters.
See the mentioned Bluetooth specification sections or the verification function in the Peer Manager source code for the constraints on the parameters.

The following list shows the required security parameters for common use cases:

* No pairing/bonding:

  The :c:func:`pm_sec_params_set` function can be called with ``NULL`` as input parameter.
  In this case, the Peer Manager will start rejecting all procedures.

* Pairing, no bonding:

   .. code-block:: c

      ble_gap_sec_params_t sec_param = (ble_gap_sec_params_t) {
         .bond = false,
         .mitm = false,
         .lesc = 0,
         .keypress = 0,
         .io_caps = BLE_GAP_IO_CAPS_NONE,
         .oob = false,
         .min_key_size = 7,
         .max_key_size = 16,
         .kdist_own.enc = 0,
         .kdist_own.id = 0,
         .kdist_peer.enc = 0,
         .kdist_peer.id = 0,
      };

* Just Works bonding:

   .. code-block:: c

      ble_gap_sec_params_t sec_param = (ble_gap_sec_params_t) {
         .bond = true,
         .mitm = false,
         .lesc = 0,
         .keypress = 0,
         .io_caps = BLE_GAP_IO_CAPS_NONE
         .oob = false,
         .min_key_size = 7,
         .max_key_size = 16,
         .kdist_own.enc = 1,
         .kdist_own.id = 1,
         .kdist_peer.enc = 1,
         .kdist_peer.id = 1,
      };

* Passkey bonding with keyboard capabilities:

   .. code-block:: c

      ble_gap_sec_params_t sec_param = (ble_gap_sec_params_t) {
         .bond = true,
         .mitm = true,
         .lesc = 0,
         .keypress = 0,
         .io_caps = BLE_GAP_IO_CAPS_KEYBOARD_ONLY,
         .oob = false,
         .min_key_size = 7,
         .max_key_size = 16,
         .kdist_own.enc = 1,
         .kdist_own.id = 1,
         .kdist_peer.enc = 1,
         .kdist_peer.id = 1,
      };

* OOB bonding:

   .. code-block:: c

      ble_gap_sec_params_t sec_param = (ble_gap_sec_params_t) {
         .bond = true,
         .mitm = true,
         .lesc = 0,
         .keypress = 0;
         .io_caps = BLE_GAP_IO_CAPS_NONE,
         .oob = true,
         .min_key_size = 7,
         .max_key_size = 16,
         .kdist_own.enc = 1,
         .kdist_own.id = 1,
         .kdist_peer.enc = 1,
         .kdist_peer.id = 1,
      };

* Disallow IRKs:

   .. code-block:: c

      ble_gap_sec_params_t sec_param = (ble_gap_sec_params_t) {
         .bond = true,
         .kdist_own.enc = 1,
         .kdist_own.id = 0,
         .kdist_peer.enc = 1,
         .kdist_peer.id = 0,
      };

Event handling
==============

The library provides a set of event handlers that can be used by the application.
For more information, see the :file:`peer_manager_handler.h` header.
The :c:func:`pm_handler_on_pm_evt` function provides basic event handling needed for Peer Manager operation, while the other handlers provide additional functionality.
The handlers can be provided to the :c:func:`pm_register` function directly, or called from the application event handler, as in the following example where the application needs to also start scanning as a result of a Peer Manager event.

.. code-block:: c

   static void pm_evt_handler(const struct pm_evt *p_evt)
   {
      pm_handler_on_pm_evt(p_evt);
      pm_handler_disconnect_on_sec_failure(p_evt);
      pm_handler_flash_clean(p_evt);

      switch (p_evt->evt_id) {
      case PM_EVT_PEERS_DELETE_SUCCEEDED:
         scan_start();
         break;
      default:
         break;
      }
   }

In central applications, to ensure authentication of the peer, it is strongly recommended to use the :c:func:`pm_handler_on_pm_evt` and :c:func:`pm_handler_disconnect_on_sec_failure` functions event handlers (as seen above).

See the :file:`peer_manager_handler.c` source file for more examples of Peer Manager event handling.

Storing data
============

The Peer Manager stores and retrieves data autonomously and does not require you to manually store any data.
However, if you want to manually change, add, or remove data, the Peer Manager provides API functions to manipulate all data that is associated with its bonded peers.

The data is stored in chunks. For example, all bonding data (keys and identities) is stored together.
A chunk cannot be partially stored or updated, but each chunk can be stored or updated independently of the other chunks.
The only restrictions are that there must always be valid bonding data for a peer in non-volatile storage and that there is only one instance of each chunk for each bonded peer.
Two of the chunks, the remote GATT database and the application data, are not used internally by the Peer Manager.
They are solely meant to be used through the Peer Manager API.

The following code example shows how to store a remote GATT database, for example inside a :c:var:`array_of_services` array.
Note that the :c:var:`array_of_services` array must be available for the duration of the (asynchronous) store operation.
The store operation is finished when either the :c:enum:`PM_EVT_PEER_DATA_UPDATE_SUCCEEDED` event or the :c:enum:`PM_EVT_PEER_DATA_UPDATE_FAILED` event is received.

.. code-block:: c

   uint32_t err;
   uint32_t store_token;

   err = pm_peer_data_remote_db_store(peer_id, array_of_services, number_of_services, &store_token);
   if (err != NRF_ERROR_BUSY) {
      return err;
   }

The :c:func:`pm_peer_data_remote_db_store`, :c:func:`pm_peer_data_bonding_store`, and :c:func:`pm_peer_data_app_data_store` functions call the :c:func:`pm_peer_data_store` function.
The :c:func:`pm_peer_data_store` function can also be used directly, as in the following example:

.. code-block:: c

   uint32_t err;
   uint32_t store_token;

   err = pm_peer_data_store(peer_id, PM_PEER_DATA_ID_GATT_REMOTE, array_of_services, number_of_services, &store_token);
   if (err != NRF_ERROR_BUSY) {
      return err;
   }

Using a whitelist
=================

The Peer Manager can be used to set and retrieve a whitelist that can be provided to the Bluetooth LE advertising module and used during advertising.
When a whitelist is needed, call the :c:func:`pm_whitelist_set` function to whitelist peers based on their peer IDs.

The following example shows how to use the :c:func:`pm_whitelist_set` function to whitelist a number of peers and the :c:func:`pm_whitelist_get` function to retrieve such a list and provide it to the Bluetooth LE advertising module for use during advertising:

.. code-block:: c

   {
      /* Fetch a list of peer IDs from Peer Manager and whitelist them. */

      uint16_t peer_ids[8] = {PM_PEER_ID_INVALID};
      uint32_t n_peer_ids = 0;
      uint16_t peer_id = pm_next_peer_id_get(PM_PEER_ID_INVALID);

      while((peer_id != PM_PEER_ID_INVALID) && (n_peer_ids < 8)) {
         peer_ids[n_peer_ids++] = peer_id;
         peer_id = pm_next_peer_id_get(peer_id);
      }

      /* Whitelist peers. */
      err = pm_whitelist_set(peer_ids, n_peer_ids);
      if (err != NRF_SUCCESS) {
         return err;
      }
   }

   static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
   {
      switch (ble_adv_evt) {
      ...
      case BLE_ADV_EVT_WHITELIST_REQUEST:
            /* When the Advertising module is about to advertise, an event
             * will be received by the application. In this event, the application
             * retrieves a whitelist from the Peer Manager, based on the peers
             * previously whitelisted using pm_whitelist_set().
             */

            uint32_t err;

            /* Storage for the whitelist. */
            ble_gap_irk_t irks[8] = {0};
            ble_gap_addr_t addrs[8] = {0};

            uint32_t irk_cnt = 8;
            uint32_t addr_cnt = 8;

            err = pm_whitelist_get(addrs, &addr_cnt, irks, &irk_cnt);
            if (err != NRF_SUCCESS) {
               return err;
            }

            /* Apply the whitelist. */
            err = ble_advertising_whitelist_reply(addrs, addr_cnt, irks, irk_cnt);
            if (err != NRF_SUCCESS) {
               return err;
            }

            break;
         ...
      }
   }

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`
* BLE Connection State - :kconfig:option:`CONFIG_BLE_CONN_STATE`
* Timer library - :kconfig:option:`CONFIG_BM_TIMER`
* Bare Metal Zephyr Memory Storage - :kconfig:option:`CONFIG_BM_ZMS`

LESC Dependencies
=================

LE Secure Connections is an optional functionality of the Peer Manager and is disabled by default.
However, if you want to use it, keep in mind that it depends on nRF Security to generate the Diffie-Helman key pair.
The following Kconfig options must be enabled to support LE Secure Connections:

* :kconfig:option:`CONFIG_NRF_SECURITY`
* :kconfig:option:`CONFIG_PSA_WANT_ALG_ECDH`
* :kconfig:option:`CONFIG_PSA_WANT_GENERATE_RANDOM`
* :kconfig:option:`CONFIG_PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_GENERATE`
* :kconfig:option:`CONFIG_PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_IMPORT`
* :kconfig:option:`CONFIG_PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_EXPORT`
* :kconfig:option:`CONFIG_PSA_WANT_ECC_SECP_R1_256`

Additionally, static key slots or heap memory must be enabled for holding the key material.
Enable the :kconfig:option:`CONFIG_MBEDTLS_PSA_STATIC_KEY_SLOTS` Kconfig option to use static key slots. Set the number of static key slots required by the application using the :kconfig:option:`CONFIG_MBEDTLS_PSA_KEY_SLOT_COUNT` Kconfig option. One slot is required for storing the DH key pair used by the LESC module. Ensure that the :kconfig:option:`CONFIG_MBEDTLS_PSA_STATIC_KEY_SLOT_BUFFER_SIZE` Kconfig option is set large enough to hold the largest key required by the application. A key slot buffer size of at least 65 bytes is required for the DH key pair used by the LESC module.
Enable the :kconfig:option:`CONFIG_MBEDTLS_ENABLE_HEAP` Kconfig option to use heap memory to hold the key material.

API documentation
*****************

| Header files: :file:`include/bm/bluetooth/peer_manager/`
| Source files: :file:`lib/peer_manager/`

:ref:`Peer Manager API reference <api_peer_manager>`
