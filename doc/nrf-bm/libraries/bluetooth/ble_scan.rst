.. _lib_ble_scan:

Bluetooth: Scan
###############

.. contents::
   :local:
   :depth: 2

The Bluetooth® Low Energy Scan library handles scanning for advertising Bluetooth LE devices.
You can use it to find an advertising device and establish a connection with it.
You can narrow down the scan to a device of a specific type using scan filters or the allow list.

Overview
********

You can configure this library in one of the following modes:

* Simple mode without using filters or the allow list.
* Advanced mode that allows you to use advanced filters and the allow list.

The library can automatically establish a connection on a filter match or when identifying an allow listed device.

The library registers as a Bluetooth LE event observer using the :c:macro:`NRF_SDH_BLE_OBSERVER` macro and handles the relevant Bluetooth LE events from the SoftDevice.

.. note::

   This library requires a SoftDevice with central support.

Configuration
*************

To enable the library, set the :kconfig:option:`CONFIG_BLE_SCAN` Kconfig option.

The library provides the following Kconfig configuration options:

* :kconfig:option:`CONFIG_BLE_SCAN_BUFFER_SIZE` - Maximum size of an advertising event.
* :kconfig:option:`CONFIG_BLE_SCAN_NAME_MAX_LEN` - Maximum size of the name to search for in the advertisement report.
* :kconfig:option:`CONFIG_BLE_SCAN_SHORT_NAME_MAX_LEN` - Maximum size of the short name to search for in the advertisement report.
* :kconfig:option:`CONFIG_BLE_SCAN_FILTER` - Enables filters for the scan library.
* :kconfig:option:`CONFIG_BLE_SCAN_NAME_COUNT` - Maximum number of name filters.
* :kconfig:option:`CONFIG_BLE_SCAN_APPEARANCE_COUNT` - Maximum number of appearance filters.
* :kconfig:option:`CONFIG_BLE_SCAN_ADDRESS_COUNT` - Maximum number of address filters.
* :kconfig:option:`CONFIG_BLE_SCAN_SHORT_NAME_COUNT` - Maximum number of short name filters.
* :kconfig:option:`CONFIG_BLE_SCAN_UUID_COUNT` - Maximum number of filters for UUIDs.
* :kconfig:option:`CONFIG_BLE_SCAN_INTERVAL` - Determines the scan interval in units of 0.625 ms.
* :kconfig:option:`CONFIG_BLE_SCAN_DURATION` - Duration of a scanning session in units of 10 ms, if set to 0, the scanning continues until it is explicitly disabled.
* :kconfig:option:`CONFIG_BLE_SCAN_WINDOW` - Determines the scanning window in units of 0.625 ms.
* :kconfig:option:`CONFIG_BLE_SCAN_PERIPHERAL_LATENCY` - Determines the peripheral latency in counts of connection events.
* :kconfig:option:`CONFIG_BLE_SCAN_MIN_CONNECTION_INTERVAL` - Determines the minimum connection interval in units of 1.25 ms.
* :kconfig:option:`CONFIG_BLE_SCAN_MAX_CONNECTION_INTERVAL` - Determines the maximum connection interval in units of 1.25 ms.
* :kconfig:option:`CONFIG_BLE_SCAN_SUPERVISION_TIMEOUT` - Determines the supervision time-out in units of 10 ms.

Initialization
==============

To initialize the library, call the :c:func:`ble_scan_init` function.
The application can provide an event handler to receive events to inform about a filter or allow list match, or about a connection error during the automatic connection.

Simple initialization
=====================

You can use the simple initialization with the default scanning and connection parameters when you want the library to work in the simple mode:

.. code-block:: c

   BLE_SCAN_DEF(ble_scan);

   void scan_event_handler_func(const struct ble_scan_evt *scan_evt);

   uint32_t nrf_err;
   struct ble_scan_config scan_cfg = {
           .scan_params = BLE_SCAN_SCAN_PARAMS_DEFAULT,
           .conn_params = BLE_SCAN_CONN_PARAMS_DEFAULT,
           .evt_handler = scan_event_handler_func,
   };

   nrf_err = ble_scan_init(&ble_scan, &scan_cfg);
   if (nrf_err) {
           LOG_ERR("Failed to initialize scan library, nrf_error %#x", nrf_err);
   }

Advanced initialization
=======================

The advanced initialization provides a larger configuration set for the library.
It is required when using scan filters or the allow list.
Example code:

.. code-block:: c

   BLE_SCAN_DEF(ble_scan);

   void scan_event_handler_func(const struct ble_scan_evt *scan_evt);

   uint32_t nrf_err;
   struct ble_scan_config scan_cfg = {
           .scan_params = {
                   .active = 0x01,
                   .interval = NRF_BLE_SCAN_INTERVAL,
                   .window = NRF_BLE_SCAN_WINDOW,
                   .filter_policy = BLE_GAP_SCAN_FP_WHITELIST,
                   .timeout = SCAN_DURATION,
                   .scan_phys = BLE_GAP_PHY_1MBPS,
                   .extended = true,
           },
           .conn_params = BLE_SCAN_CONN_PARAMS_DEFAULT,
           .connect_if_match = true,
           .conn_cfg_tag = CONFIG_NRF_SDH_BLE_CONN_TAG,
           .evt_handler = scan_event_handler_func,
   };

   nrf_err = ble_scan_init(&ble_scan, &scan_cfg);
   if (nrf_err) {
           LOG_ERR("Failed to initialize scan library, nrf_error %#x", nrf_err);
   }

.. note::

   When setting connection-specific configurations with the :c:func:`sd_ble_cfg_set` function, you must create a tag for each configuration.
   If your application uses the library with automatic connection, this tag must be provided when calling the :c:func:`sd_ble_gap_scan_start` or the :c:func:`sd_ble_gap_connect` function.

Usage
*****

The application starts scanning when the :c:func:`ble_scan_start` function is called.

To update the scan parameters, call the :c:func:`ble_scan_params_set` function.

To stop scanning, call the :c:func:`ble_scan_stop` function.

The library resumes scanning after receiving advertising reports.
Scanning stops if the library establishes a connection automatically, or if the application calls the :c:func:`ble_scan_stop` or the :c:func:`sd_ble_gap_connect` function.

.. note::

   When you use the :c:func:`ble_scan_params_set` function during the ongoing scan, scanning is stopped.
   To resume scanning, use the :c:func:`ble_scan_start` function.

Allow list
==========

The allow list (formerly known as whitelist) stores information about all the device connections and bonding.
If you enable the allow list, the application receives advertising packets only from the devices that are on the allow list.
An advertising package from an allow-listed device generates an :c:macro:`BLE_SCAN_EVT_ALLOW_LIST_ADV_REPORT` event.

.. note::

   When using the allow list, filters are inactive.

.. caution::

   If you use the allow list, you must pass the event handler during the library initialization.
   The initial scanning with allow list generates an :c:macro:`BLE_SCAN_EVT_ALLOW_LIST_REQUEST` event.
   The application must react to this event by either setting up the allow list or switching off the allow list scan.
   Otherwise, an error is reported when the scan starts.

Filters
=======

The library can set scanning filters of different type and mode.
When a filter is matched, it generates an :c:macro:`NRF_BLE_SCAN_EVT_FILTER_MATCH` event to the main application.
If the filter matching is enabled and no filter is matched, an :c:macro:`NRF_BLE_SCAN_EVT_NOT_FOUND` event is generated.

The available filter types are:

* Name - Filter set to the target name.
   The maximum length of the name corresponds to :kconfig:option:`CONFIG_BLE_SCAN_NAME_MAX_LEN`.
   The maximum number of filters of this type corresponds to :kconfig:option:`CONFIG_BLE_SCAN_NAME_COUNT`.
* Short name - Filter set to the short target name.
   The maximum length of the name corresponds to :kconfig:option:`CONFIG_BLE_SCAN_SHORT_NAME_MAX_LEN`.
   The maximum number of filters of this type corresponds to :kconfig:option:`CONFIG_BLE_SCAN_SHORT_NAME_COUNT`.
* Address - Filter set to the target address.
   The maximum number of filters of this type corresponds to :kconfig:option:`CONFIG_BLE_SCAN_ADDRESS_COUNT`.
* UUID - Filter set to the target UUID.
   The maximum number of filters of this type corresponds to :kconfig:option:`CONFIG_BLE_SCAN_UUID_COUNT`.
* Appearance - Filter set to the target appearance.
   The maximum number of filters of this type corresponds to :kconfig:option:`CONFIG_BLE_SCAN_APPEARANCE_COUNT`.

The following two filter modes are available:

* Normal - Only one of the filters set, regardless of the type, must be matched to generate an event.
* Multifilter - At least one filter from each filter type you set must be matched to generate an event.
  For UUID filters, all specified UUIDs must match in this mode.
  To enabled multifilter, set the :c:macro:`match_all` argument to true when calling the :c:func:`ble_scan_filters_enable` function.

Multifilter example:

Several filters are set for name, address, UUID, and appearance.
To generate the :c:macro:`NRF_BLE_SCAN_EVT_FILTER_MATCH` event, the following types must match:

* One of the address filters.
* One of the name filters.
* One of the appearance filters.
* All UUID filters.

Otherwise, the :c:macro:`NRF_BLE_SCAN_EVT_NOT_FOUND` event is generated.

You can enable filters by calling the :c:func:`ble_scan_filters_enable` function after initialization.
You can activate filters for one filter type, or for a combination of several filter types.
Example code:

.. code-block:: c

   BLE_SCAN_DEF(ble_scan);

   uint8_t addr[BLE_GAP_ADDR_LEN] = {0xa, 0xd, 0xd, 0x4, 0xe, 0x5};
   char *device_name = "my_device";

   /* See above code snippet for initialization */

   /* Enable filter for name and address in normal mode */
   nrf_err = ble_scan_filters_enable(&ble_scan, BLE_SCAN_NAME_FILTER | BLE_SCAN_ADDR_FILTER, false);
   if (nrf_err) {
           LOG_ERR("Failed to enable scan filters, nrf_error %#x", nrf_err);
   }

   /* Add address to scan filter */
	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_ADDR_FILTER, addr);
   if (nrf_err) {
           LOG_ERR("Failed to add address scan filter, nrf_error %#x", nrf_err);
   }

   /* Add name to scan filter */
	nrf_err = ble_scan_filter_add(&ble_scan, BLE_SCAN_NAME_FILTER, device_name);
	if (nrf_err) {
           LOG_ERR("Failed to add name scan filter, nrf_error %#x", nrf_err);
   }

   /* Start scanning */
   nrf_err = ble_scan_start(&ble_scan);
   if (nrf_err) {
           LOG_ERR("Failed to start scan, nrf_error %#x", nrf_err);
   }

   /* Start scanning */
   ble_scan_stop(&ble_scan);

   /* Disable filters */
   nrf_err = ble_scan_filters_disable(&ble_scan);
   if (nrf_err) {
           LOG_ERR("Failed to disable scan filters, nrf_error %#x", nrf_err);
   }

   /* Remove all scan filters */
   nrf_err = ble_scan_all_filter_remove(&ble_scan);
   if (nrf_err) {
           LOG_ERR("Failed to remove scan filters, nrf_error %#x", nrf_err);
   }

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/bm/bluetooth/ble_scan.h`
| Source files: :file:`lib/bluetooth/ble_scan/`

:ref:`Bluetooth LE Scan library API reference <api_ble_scan>`
