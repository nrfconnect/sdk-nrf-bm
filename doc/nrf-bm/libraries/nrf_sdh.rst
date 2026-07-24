.. _lib_nrf_sdh:

SoftDevice handler
##################

Overview
********

The SoftDevice handler is a library that integrates the SoftDevice into the application.

It provides core functionality such as:

* Defining ISRs for interrupts belonging to peripherals used by the SoftDevice.
* Automatically forwarding IRQs for SoftDevice-owned peripherals to the SoftDevice as necessary.
* Managing SoftDevice state changes (enabling/disabling).
* Retrieving and forwarding SoftDevice events to different parts of the application.
* Simplifying SoftDevice and Bluetooth LE configuration and initialization.
* Provide a default behavior on SoftDevice fault.

Interrupt configuration
=======================

The SoftDevice handler defines ISRs for interrupts belonging to peripherals used by the SoftDevice.
It automatically configures and enables those interrupts, and determines whether they should be forwarded to the SoftDevice or not, based on whether the SoftDevice is enabled.

The interrupts forwarded to the SoftDevice are declared as Zero Latency interrupts, meaning they cannot be masked using :c:func:`irq_lock`.

Events and observers
====================

The SoftDevice handler lets the application register functions to receive various events.
A function that receives SoftDevice handler events is called an observer.

There are four types of observers, each receiving different types of events:

* State observers - Events pertaining to the SoftDevice state (enabled/disabled)
* Bluetooth observers - Bluetooth events coming from the SoftDevice
* SoC observers - SoC events coming from the SoftDevice
* Stack observers - All events coming from the SoftDevice

An observer has a priority relative to other observers of the same type.
The priority of an observer determines the order with which the observers receive the same event.

The following five priority levels are defined:

* ``HIGHEST``
* ``HIGH``
* ``USER``
* ``USER_LOW``
* ``LOWEST``

An observer can optionally have a context, defined as a pointer that is passed as a parameter to the observer function.

Fault handling
==============

The SoftDevice handler provides a weak implementation of the SoftDevice fault handler function.
This implementation prints the fault on the terminal and enters an endless loop.

Configuration
*************

The library is enabled by default in all projects using the SoftDevice.

The SoftDevice handler main configuration options are related to how it dispatches events, the clock and the Bluetooth stack.

Dispatch model
==============

The SoftDevice handler implements the SoftDevice event ISR, receiving events from the SoftDevice and dispatching them to the registered observers.

You can configure the SoftDevice handler to dispatch those events in three different ways, using the following Kconfig options:

* :kconfig:option:`CONFIG_NRF_SDH_DISPATCH_MODEL_IRQ` - Events are dispatched from the SoftDevice event ISR (default).
* :kconfig:option:`CONFIG_NRF_SDH_DISPATCH_MODEL_SCHED` - Events are dispatched using the event scheduler library, from the main application loop.
* :kconfig:option:`CONFIG_NRF_SDH_DISPATCH_MODEL_POLL` - Events are dispatched from the same context as the function calling :c:func:`nrf_sdh_evts_poll`.

Clock configuration
===================

The SoftDevice handler has several Kconfig options to control the clock configuration passed to the :c:func:`sd_softdevice_enable` function.
These Kconfig options are used automatically when you enable the SoftDevice by calling the :c:func:`nrf_sdh_enable_request` function.

.. note::
   The clock source Kconfig options must match the oscillator hardware available on your board, as described in its Devicetree.
   For example, do not select the :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_SRC_XO` Kconfig option if your board has no external low-frequency crystal (LFXO).

Low-frequency clock source
--------------------------

The following Kconfig options set the source of SoftDevice's low-frequency clock:

* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_SRC_XO` - Use an external 32.768 kHz crystal (LFXO).
* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_SRC_RC` - Use the internal RC oscillator (LFRC).

Calibration (LFRC only)
-----------------------

When using the internal RC oscillator as the low-frequency clock source, the following Kconfig options control the frequency and interval of the clock calibration:

* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_RC_CTIV` - Calibration timer interval, in 1/4 second units (range 1-32, default 16).
  To avoid excessive clock drift, 0.5 degrees Celsius is the maximum temperature change allowed in one calibration timer interval.
  Select the interval to ensure this.
* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_RC_TEMP_CTIV` - The calibration frequency of the RC oscillator if the temperature has not changed, in number of calibration intervals (range 0-33, default 2).

  * ``0``: Always calibrate even if the temperature has not changed.
  * ``1``: Invalid.
  * ``2`` - ``33``: Check the temperature and only calibrate if it has changed.
    Always calibrate when this many intervals have passed.

Clock accuracy
--------------

You must set the accuracy of the low-frequency clock source to one of the following Kconfig options to match the accuracy of the clock source.
The SoftDevice uses this value to size its Bluetooth LE connection timing windows.
A less accurate clock requires wider windows to avoid missing connection events, at the cost of additional radio-on time.

* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_ACCURACY_500_PPM` - 500 ppm.
* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_ACCURACY_250_PPM` - 250 ppm.
* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_ACCURACY_150_PPM` - 150 ppm.
* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_ACCURACY_100_PPM` - 100 ppm.
* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_ACCURACY_75_PPM` - 75 ppm.
* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_ACCURACY_50_PPM` - 50 ppm.
* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_ACCURACY_30_PPM` - 30 ppm.
* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_ACCURACY_20_PPM` - 20 ppm.
* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_ACCURACY_10_PPM` - 10 ppm.
* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_ACCURACY_5_PPM` - 5 ppm.
* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_ACCURACY_2_PPM` - 2 ppm.
* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_LF_ACCURACY_1_PPM` - 1 ppm.

High-frequency clock
--------------------

The high-frequency clock (HFCLK) can be sourced from the internal oscillator (HFINT) or the crystal oscillator (HFXO).
HFXO must be running to achieve the clock accuracy the radio requires.
If HFXO is not running already, the SoftDevice starts it before radio activity.
When radio activity ends, the SoftDevice switches back to HFINT, unless the application has requested HFXO itself.
The following Kconfig options control how often the internal oscillator is calibrated, and how long the crystal oscillator takes to stabilize (ramp up) when requested:

* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_HFINT_CALIBRATION_INTERVAL` - Calibration interval of the high-frequency internal oscillator, in seconds (range 1-255, default 60).
* :kconfig:option:`CONFIG_NRF_SDH_CLOCK_HFCLK_LATENCY` - Ramp-up time of the high-frequency crystal oscillator, in microseconds (default 1500).
  This lets the SoftDevice start the crystal oscillator early enough to be ready before a radio event.

Bluetooth configuration
========================

The SoftDevice handler has several Kconfig options to control the Bluetooth LE configuration passed to the :c:func:`sd_ble_cfg_set` function.
These Kconfig options are used automatically when you enable the Bluetooth LE stack by calling the :c:func:`nrf_sdh_ble_enable` function, which applies each configuration with the :c:func:`sd_ble_cfg_set` function and then enables the stack by calling the :c:func:`sd_ble_enable` function.
Once the stack is enabled, the SoftDevice handler sends the :c:enumerator:`NRF_SDH_STATE_EVT_BLE_ENABLED` event to the registered state observers.

The SoftDevice Bluetooth stack stores this set of configuration options and associates them with a numerical identifier called a connection configuration tag.
The SoftDevice handler builds the configuration from the Kconfig options and associates it with the tag defined by the :kconfig:option:`CONFIG_NRF_SDH_BLE_CONN_TAG` Kconfig option.

These Kconfig options are only available when the :kconfig:option:`CONFIG_NRF_SDH_BLE` Kconfig option is enabled (default).

.. note::
   Several of these options increase the SoftDevice's dynamic RAM usage.
   Enable the :kconfig:option:`CONFIG_NRF_SDH_BLE_LOG_SD_RAM_USAGE` Kconfig option to observe the effect.
   Some options are only available on selected SoftDevice variants.

You can configure the Bluetooth LE stack using the Kconfig options listed in the following sections.

Connection tag
--------------

Identifies the set of connection configuration options that the SoftDevice handler builds and applies to the stack.
The application refers to this tag when advertising or establishing connections so that the SoftDevice knows which configuration to use.

* :kconfig:option:`CONFIG_NRF_SDH_BLE_CONN_TAG` - The numerical tag identifying the connection configuration applied by the SoftDevice handler in the :c:func:`nrf_sdh_ble_enable` function (range 1-255, default 99).
  It is the shared reference used by the handler and the application, which passes it on to the |BMshort| service libraries.

Connection counts
-----------------

Controls how many concurrent connections the SoftDevice reserves resources for.
These Kconfig options are interrelated.
The number of concurrent peripheral or central connections is limited by the total connection count, and the maximum depends on the SoftDevice variant in use.
Reserving more connections increases the amount of RAM required by the SoftDevice.

* :kconfig:option:`CONFIG_NRF_SDH_BLE_PERIPHERAL_LINK_COUNT` - Maximum number of concurrent peripheral connections (default 1).
* :kconfig:option:`CONFIG_NRF_SDH_BLE_CENTRAL_LINK_COUNT` - Maximum number of concurrent central connections (default 0).
* :kconfig:option:`CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT` - Maximum number of total concurrent connections (default 1).
  The allowed value range is 0 to 2 for the S115 and 0 to 5 for the S145, as stated in the `S115 SoftDevice Specification`_ and `S145 SoftDevice Specification`_.
  See `S115 Bluetooth LE role configuration`_ and `S145 Bluetooth LE role configuration`_ for how connections are allocated between central and peripheral roles.

Connection and GATT parameters
------------------------------

Controls the timing, packet size, and attribute storage that the SoftDevice uses for each connection and for the GATT server.
Larger values generally increase the throughput or capacity at the cost of additional RAM and radio-on time.

* :kconfig:option:`CONFIG_NRF_SDH_BLE_GAP_EVENT_LENGTH` - Time reserved for each connection event, in 1.25 ms units (range 2-3200, default 3).
  A connection event is the window in which the radio exchanges packets during one connection interval, so this value must be smaller than the connection interval negotiated for the link.
  A longer event length allows more data for each interval but keeps the radio on longer.
* :kconfig:option:`CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE` - Maximum size of an ATT packet the SoftDevice can send or receive (range 23-65535, default 23).
  A larger value allows more data to be sent in a single ATT PDU, but increases the size of the SoftDevice's internal buffers and its RAM usage.
* :kconfig:option:`CONFIG_NRF_SDH_BLE_GATTS_ATTR_TAB_SIZE` - Size of the Attribute Table in bytes (default 1408).
  Increase this value to fit more services and characteristics in the Attribute Table.
  The value must be a multiple of 4.
  Increasing this value will increase the SoftDevice's RAM usage.
* :kconfig:option:`CONFIG_NRF_SDH_BLE_VS_UUID_COUNT` - Number of vendor-specific (custom 128-bit) UUIDs (default 0).
  Increase this when your application defines custom services or characteristics.
  Increasing this value will increase the SoftDevice's RAM usage.
* :kconfig:option:`CONFIG_NRF_SDH_BLE_SERVICE_CHANGED` - Include the Service Changed characteristic in the Attribute Table.
  This standard GATT characteristic lets the application send a Service Changed indication to a connected peer, to inform it that the Attribute Table layout has changed.
  Use the :c:func:`sd_ble_gatts_service_changed` function to send a Service Changed indication, documented in the :ref:`api_softdevice`.
  Enable it only if your services can change.

Random seeding
==============

The SoftDevice requires the internal random number generator to be seeded for the underlying security procedures in Bluetooth LE to function.
Without it, Bluetooth LE cannot be enabled.

* :kconfig:option:`CONFIG_NRF_SDH_SOC_RAND_SEED` - Automatically seed the SoftDevice random number generator (enabled by default).
  Automatic seeding relies on the `PSA Crypto API`_ to generate the seed.
  Leave this enabled unless your application must provide the random seed in another way.

.. _nrf_sdh_logging_diagnostics:

Logging and diagnostics
=======================

Controls the diagnostic output the SoftDevice handler produces, trading a small amount of memory for more readable logs and startup information.

* :kconfig:option:`CONFIG_NRF_SDH_STR_TABLES` - Build string tables so that the :c:func:`nrf_sdh_ble_evt_to_str` and :c:func:`nrf_sdh_soc_evt_to_str` functions can return human-readable event names (enabled by default).
  These strings are also used when logging SoftDevice state changes.
  Disable this option to save non-volatile memory when you do not need readable event names.
* :kconfig:option:`CONFIG_NRF_SDH_LOG_SD_INFO` - Log SoftDevice information when the SoftDevice is enabled, including its ID, version, firmware ID, and unique string, as well as the Link Layer version when Bluetooth LE is enabled.
  This option also logs a warning if the SoftDevice found on the device differs from the one the application was compiled against, which is useful for catching version mismatches during development.
* :kconfig:option:`CONFIG_NRF_SDH_BLE_LOG_SD_RAM_USAGE` - Log the amount of RAM the SoftDevice requires and where its RAM region ends when Bluetooth is enabled.
  Enable this when tuning the Bluetooth Kconfig options above to confirm the application reserves enough RAM for the SoftDevice and to reclaim any RAM left unused.

Usage
*****

You can use the SoftDevice handler API to declare observers, change the SoftDevice state, and enable the SoftDevice Bluetooth stack.

Declaring observers
===================

You can declare an observer in any part of your application by using a macro and specifying an event handler function, an optional parameter, and a priority.

State observers
---------------

A state observer receives the following events related to the SoftDevice state:

* :c:enumerator:`NRF_SDH_STATE_EVT_ENABLE_PREPARE` - SoftDevice is going to be enabled.
* :c:enumerator:`NRF_SDH_STATE_EVT_ENABLED` - SoftDevice is enabled.
* :c:enumerator:`NRF_SDH_STATE_EVT_BLE_ENABLED` -  Bluetooth is enabled.
* :c:enumerator:`NRF_SDH_STATE_EVT_DISABLE_PREPARE` - SoftDevice is going to be disabled.
* :c:enumerator:`NRF_SDH_STATE_EVT_DISABLED` - SoftDevice is disabled.

A state observer is declared using the :c:macro:`NRF_SDH_STATE_EVT_OBSERVER` macro.

The following snippet shows how to declare a state observer:

.. code-block:: c

   #include <bm/softdevice_handler/nrf_sdh.h>

   static int on_state_change(enum nrf_sdh_state_evt state, void *ctx)
   {
       LOG_INF("SoftDevice state %d", state);
       return 0;
   }
   NRF_SDH_STATE_EVT_OBSERVER(sdh_state, on_state_change, NULL, USER_LOW);


Bluetooth observers
-------------------

A Bluetooth observer receives Bluetooth events coming from the SoftDevice and is declared using the :c:macro:`NRF_SDH_BLE_OBSERVER` macro.

The following snippet shows how to declare a Bluetooth observer:

.. code-block:: c

   #include <bm/softdevice_handler/nrf_sdh_ble.h>

   static void on_ble_evt(const ble_evt_t *evt, void *ctx)
   {
       LOG_INF("BLE event %d", evt->header.evt_id);
   }
   NRF_SDH_BLE_OBSERVER(sdh_ble, on_ble_evt, NULL, USER_LOW);


SoC observers
-------------

An SoC observer receives SoC events coming from the SoftDevice and is declared using the :c:macro:`NRF_SDH_SOC_OBSERVER` macro.

The following snippet shows how to declare an SoC observer:

.. code-block:: c

   #include <bm/softdevice_handler/nrf_sdh_soc.h>

   static void on_soc_evt(uint32_t evt, void *ctx)
   {
       LOG_INF("SoC event %d", evt);
   }
   NRF_SDH_SOC_OBSERVER(sdh_soc, on_soc_evt, NULL, USER_LOW);


Stack observers
---------------

A stack observer is a special kind of observer that receives all SoftDevice events, both Bluetooth and SoC.
Normally, the application registers Bluetooth and SoC observers instead, and uses a stack observer only when it needs to handle every SoftDevice event in one place.

A stack observer is declared using the :c:macro:`NRF_SDH_STACK_EVT_OBSERVER` macro.

The following snippet shows how to declare a stack observer:

.. code-block:: c

   #include <bm/softdevice_handler/nrf_sdh.h>

   static void on_stack_evt(void *ctx)
   {
       LOG_INF("SoftDevice stack event");
   }
   NRF_SDH_STACK_EVT_OBSERVER(sdh_stack, on_stack_evt, NULL, USER_LOW);

Changing the SoftDevice state
=============================

The SoftDevice handler is used by the application to change the SoftDevice state.
When changing the state, it notifies the state observers.

You can enable the SoftDevice by calling the :c:func:`nrf_sdh_enable_request` function, sending the :c:enumerator:`NRF_SDH_STATE_EVT_ENABLE_PREPARE` event to the observers.
The observers can return ``0`` from the event handler to let the SoftDevice change the state, or return a non-zero value to halt the SoftDevice state change.
When an observer returns non-zero to the :c:enumerator:`NRF_SDH_STATE_EVT_ENABLE_PREPARE` event, it must call the :c:func:`nrf_sdh_observer_ready` function when it becomes ready for the SoftDevice state change.

When the last state observer is ready for the SoftDevice state change, the state is changed and the :c:enumerator:`NRF_SDH_STATE_EVT_ENABLED` event is sent.

Similarly, you can disable the SoftDevice by calling the :c:func:`nrf_sdh_disable_request` function, sending the :c:enumerator:`NRF_SDH_STATE_EVT_DISABLE_PREPARE` event to the observers.
The observers can return ``0`` from the event handler to let the SoftDevice change the state, or return a non-zero value to halt the SoftDevice state change.
When an observer returns non-zero to the :c:enumerator:`NRF_SDH_STATE_EVT_DISABLE_PREPARE` event, it must call the :c:func:`nrf_sdh_observer_ready` function when it becomes ready for the SoftDevice state change.

When the last state observer is ready for the SoftDevice state change, the state is changed and the :c:enumerator:`NRF_SDH_STATE_EVT_DISABLED` event is sent.

Fault handling
==============

You can override the SoftDevice fault handler function by defining the following function:

.. code-block:: c

   #include <stdint.h>
   #include <nrf_sdm.h>

   void softdevice_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
   {
       /* your code */
   }

Debugging
=========

The SoftDevice handler can produce diagnostic output to help you inspect events and debug issues.
The Kconfig options controlling this output are described in :ref:`nrf_sdh_logging_diagnostics` section.

Converting events to readable strings
--------------------------------------

Requires :kconfig:option:`CONFIG_NRF_SDH_STR_TABLES` (enabled by default).
Use the :c:func:`nrf_sdh_ble_evt_to_str` and :c:func:`nrf_sdh_soc_evt_to_str` functions inside your Bluetooth and SoC observers to log human-readable event names:

.. code-block:: c

   static void on_ble_evt(const ble_evt_t *evt, void *ctx)
   {
       LOG_INF("BLE event: %s", nrf_sdh_ble_evt_to_str(evt->header.evt_id));
   }

   static void on_soc_evt(uint32_t evt, void *ctx)
   {
       LOG_INF("SoC event: %s", nrf_sdh_soc_evt_to_str(evt));
   }

Adjusting the log level
------------------------

The SoftDevice handler uses the standard logging module, so you can control how much it logs using the :kconfig:option:`CONFIG_NRF_SDH_LOG_LEVEL` Kconfig option.

Dependencies
************

This library depends on the :kconfig:option:`CONFIG_SOFTDEVICE` and :kconfig:option:`CONFIG_ZERO_LATENCY_IRQS` Kconfig options.

Additionally, the :kconfig:option:`CONFIG_NRF_SDH_DISPATCH_MODEL_SCHED` Kconfig option depends on the :kconfig:option:`CONFIG_BM_SCHEDULER` Kconfig option.

API documentation
*****************

| Header file: :file:`include/bm/softdevice_handler/nrf_sdh.h`
| Header file: :file:`include/bm/softdevice_handler/nrf_sdh_ble.h`
| Header file: :file:`include/bm/softdevice_handler/nrf_sdh_soc.h`
| Source files: :file:`subsys/softdevice_handler/`

:ref:`SoftDevice handler API reference <api_nrf_sdh>`
