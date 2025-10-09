.. _lib_sensorsim:

Sensor Simulator
################

.. contents::
   :local:
   :depth: 2

Overview
********

The sensor simulator library provides functionality for simulating sensor data.
It currently supports only a triangular waveform simulator.

Configuration
*************

The library is enabled using the Kconfig system.
Set the :kconfig:option:`CONFIG_SENSORSIM` Kconfig option to enable the library.

Initialization
==============

A sensor simulator instance is initialized by calling the :c:func:`sensorsim_init` function, providing a configuration for the minimal, maximum, and start value, plus the increment between each measurement.
For details on the configuration see the :c:struct:`sensorsim_cfg` structure.

Usage
*****

After initialization, the :c:func:`sensorsim_measure` function can be called to generate a triangular wave generated sensor measurement.

Dependencies
************

This library does not have any dependencies.

API documentation
*****************

| Header file: :file:`include/sensorsim.h`
| Source files: :file:`lib/sensorsim/`

:ref:`Sensorsim library API reference <api_sensorsim>`
