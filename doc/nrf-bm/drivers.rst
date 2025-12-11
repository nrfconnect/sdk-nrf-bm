.. _drivers:

Drivers
#######

|BMlong| provides drivers for typical use cases with Nordic Semiconductor devices.
This will mostly be more higher-level drivers and should be seen as a supplement to nrfx.

.. note::

   Only the drivers located under :file:`nrf-bm/drivers` and the ones provided in nrfx are in the scope of the |BMshort|.
   All other drivers that are included in the distribution must be ignored when working with the Bare Metal option.

Some peripherals are pre-allocated by the system and must not be accessed directly by the application.

By default, |BMshort| initializes a system timer that is used by both the :ref:`lib_bm_timer` and logging timestamp.
This allocates the ``GRTC`` channel ``0`` and ``GRTC_2_IRQn``.
The initialization starts ``GRTC`` and enables ``SYSCOUNTER``, as required by the SoftDevice.

When the SoftDevice is enabled, it takes the ownership of the peripherals that it needs.
They will not be available for application to use.
The SoftDevice is reusing both the `SoftDevice Controller`_ and the `Multiprotocol Service Layer`_ from the |NCS|.
See the Integration Notes documentation for these libraries to get an overview of the resource it will be using.

For details on the SoftDevice behavior, see the `S115 SoftDevice Specification`_.

.. toctree::
   :maxdepth: 1
   :glob:
   :caption: Subpages:

   drivers/*
