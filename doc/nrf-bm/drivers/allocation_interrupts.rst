.. _drivers_alloc_int:

Allocation and interrupts
#########################

When working with drivers in |BMshort|, keep in mind the following information about peripheral allocation and driver interrupts.

Allocation
**********

Some peripherals are pre-allocated by the system and must not be accessed directly by the application.

By default, |BMshort| initializes a system timer that is used by both the :ref:`lib_bm_timer` and logging timestamp.
This allocates the ``GRTC`` channel ``0`` and ``GRTC_2_IRQn``.
The initialization starts ``GRTC`` and enables ``SYSCOUNTER``, as required by the SoftDevice.

When the SoftDevice is enabled, it takes the ownership of the peripherals that it needs.
They will not be available for application to use.
The SoftDevice is reusing both the `SoftDevice Controller <SoftDevice Controller Integration_>`_ and the `Multiprotocol Service Layer`_ from the |NCS|.
See the Integration Notes documentation for these libraries to get an overview of the resources that the SoftDevice needs.

For details on the SoftDevice behavior, see the `S115 SoftDevice Specification`_.

Interrupts
**********

In |BMshort|, interrupt priority numbers are aligned directly with the NVIC priority numbers of the nRF device (0–7).
This differs from Zephyr when `Zero Latency Interrupts`_ (ZLIs) are enabled, as Zephyr will offset the priority values to exclude the reserved interrupt priorities.
For example, Zephyr priorities 0–5 map to NVIC priorities 2–7.

.. important::

   When using the SoftDevice, only interrupt priorities 2–7 are available to the application.
   Priorities 0 and 1 are reserved for the SoftDevice.
   For more information, see the `Interrupt priority levels`_ and `Application signals – software interrupts`_ sections of the SoftDevice Specification.

Two macros are provided to help manage interrupts:

* :c:macro:`BM_IRQ_DIRECT_CONNECT` — Connects an interrupt signal to an interrupt routine and adjusts the priority number to match the correct NVIC priority.
* :c:macro:`BM_IRQ_SET_PRIORITY` — Changes the interrupt priority of a given interrupt.

Both macros perform a build-time assertion to verify that the specified priority is within the valid range.
