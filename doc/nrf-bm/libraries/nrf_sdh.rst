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

The SoftDevice handler has several options to control the clock configuration passed to the :c:func:`sd_softdevice_enable` function.
These options are used automatically when you enable the SoftDevice by calling the :c:func:`nrf_sdh_enable_request` function.

Bluetooth configuration
=======================

The SoftDevice Bluetooth stack can store a set of configuration options and associate them with a numerical tag called a connection configuration tag.

The SoftDevice handler can be used to automatically create a Bluetooth configuration and associate it with the connection configuration tag :kconfig:option:`CONFIG_NRF_SDH_BLE_CONN_TAG`.

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

A stack observer is a special kind of observer that receives all SoftDevice events.
Normally, the application registers Bluetooth and SoC observers to receive SoftDevice events.

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


Dependencies
************

This library depends on the :kconfig:option:`CONFIG_SOFTDEVICE` and :kconfig:option:`CONFIG_ZERO_LATENCY_IRQS` Kconfig options.

Additionally, the dispatch model :kconfig:option:`CONFIG_NRF_SDH_DISPATCH_MODEL_SCHED` option depends on :kconfig:option:`CONFIG_BM_SCHEDULER`.

API documentation
*****************

| Header file: :file:`include/bm/softdevice_handler/nrf_sdh.h`
| Header file: :file:`include/bm/softdevice_handler/nrf_sdh_ble.h`
| Header file: :file:`include/bm/softdevice_handler/nrf_sdh_soc.h`
| Source files: :file:`subsys/softdevice_handler/`

:ref:`SoftDevice handler API reference <api_nrf_sdh>`
