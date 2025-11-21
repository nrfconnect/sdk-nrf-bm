.. _lib_sensorsim:

Sensor Simulator
################

.. contents::
   :local:
   :depth: 2

The sensor simulator library provides functionality for simulating sensor data for testing and development purposes.

It currently supports simulating data using a triangular waveform.

Configuration
*************

The library is enabled using the Kconfig system.
Set the :kconfig:option:`CONFIG_SENSORSIM` Kconfig option to enable the library.

Initialization
==============

To initialize a sensor simulator instance, use the :c:func:`sensorsim_init` function.
The function requires providing a configuration for the minimal, maximum, and starting values, as well as the increment between each measurement.

For details on the configuration parameters, see the :c:struct:`sensorsim_cfg` structure.

Usage
*****

After initialization, you can call the :c:func:`sensorsim_measure` function to generate measurements based on a triangular waveform.

Dependencies
************

This library does not have any dependencies.

API documentation
*****************

| Header file: :file:`include/bm/sensorsim.h`
| Source files: :file:`lib/sensorsim/`

:ref:`Sensor data simulator library API reference <api_sensorsim>`
