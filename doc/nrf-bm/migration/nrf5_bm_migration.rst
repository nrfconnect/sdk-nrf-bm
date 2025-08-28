.. _nrf5_bm_migration:

Migration notes - nRF5 SDK to nRF Connect SDK Bare Metal
########################################################

This document outlines the high-level differences between nRF5 SDK and the |BMlong|.

It is meant to provide support when migrating an application built on nRF5 SDK to |BMshort|.

.. note::

   This document is in development and being constantly updated.

Project files
*************

In nRF5, project files are tightly coupled with the SDK directory structure.

Typically, projects are placed inside the :file:`examples` folder within the SDK and include header files, source files, and pre-built libraries in defined locations within the SDK.
These projects rely on embedded IDEs project files, such Keil µVision or IAR, or GNU Makefiles, which all have very limited dependency management.
Developers have to manually handle their project dependencies, adding libraries and their relative headers to their project by navigating the IDE’s menu, which can be tedious for large projects.

In an nRF5 SDK project, project configuration is achieved through the :file:`sdk_config.h` file, which sets the configuration options present in the nRF5 SDK that are applicable to a given project.
When new dependencies are manually added to the project, their relative configuration entries in the project’s initial :file:`sdk_config.h` file are missing, and must also be added manually.

In |BMshort|, projects can be organized more freely and managed more easily thanks to the new build system based on CMake and west.
An application can reside in any folder, and includes a :file:`CMakeLists.txt` build system configuration file (for CMake) and a :file:`prj.conf` project configuration file (for Kconfig, which is a tool that is part of the build system).

The purpose of the :file:`CMakeLists.txt` file is to define the structure of the project, such as source files, libraries, dependencies, and build targets.
Specifically, this file is used to:

* Define the name of your project.
* Add source files:

  * Specify the source files for your application that need to be compiled.
    This includes main files, libraries, or any additional ``.c`` or ``.cpp`` code.

    .. code-block:: cmake

       target_sources(app PRIVATE src/main.c src/other_module.c)

* Include headers:

  * Add directories for custom include paths to ensure header files are correctly found.

    .. code-block:: cmake

       target_include_directories(app PRIVATE include)

* Link libraries:

  * Add external or custom libraries required for your project (such as math, custom vendor libraries).

    .. code-block:: cmake

       target_link_libraries(app PRIVATE libfoo)

* External modules and definitions:

  * If your project relies on external modules or additional functionality, include them.

    .. code-block:: cmake

       add_subdirectory("path_to_directory")

Dependencies are added to the project configuration and incorporated into the build process automatically (you do not have to manually add files from specific locations), with the aid of a tool called Kconfig, in conjunction with CMake.
That is done using the :file:`prj.conf` file that replaces the :file:`sdk_config.h` file from the nRF5 SDK.
The entries in the :file:`prj.conf` file are referred to as **Kconfig options**.

Unlike the :file:`sdk_config.h` file that lists all the configuration options relevant for an application, even those whose values are unchanged from defaults, the :file:`prj.conf` file only contains entries whose values must be manually set or to override the default.

The configuration options whose values are left as default are not present in that file, although when a project is built, a file containing all configuration options pertaining to the application (called :file:`autoconf.h`) is created in the background.
The build system and the |nRFVSC| extension both provide a way to conveniently browse and search all available project options and inspect their dependencies and read their help text (menuconfig/extension).

There is no distinction or taxonomy between Kconfig options that are applicable to regular |NCS| only and those that are only applicable to |BMlong|.
Kconfig options that are applicable to the current application are shown and are selectable, while others are not.

There is no consistent mapping of the :file:`sdk_config.h` entries to Kconfig options.
Some libraries that were ported from nRF5 have similar Kconfig options as the :file:`sdk_config.h` entries they had in nRF5, but it is not a consistent rule.

In nRF5, the same application/sample had a project file for each supported board and IDE/compiler.
In |BMshort|, there is a single project file (consisting of :file:`CMakeLists.txt` and :file:`prj.conf`) that can be built for different boards with a different command-line instruction or by selecting a different board target in the VS Code extension.
If necessary, Kconfig options can be specified in a different :file:`.conf` file that is then automatically appended to the default :file:`prj.conf` file, thus realizing a dedicated configuration for a specific board.
Kconfig options appended in this way are referred to as **Kconfig fragments**.

Memory partitioning
===================

In nRF5, memory partitioning was done using linker scripts.
In |BMshort|, there are a few ready-made partitioning schemes that can be selected by compiling for specific board targets that cover the most common use cases.
Partitioning can be tweaked by making simple changes to textual **Devicetree** files which define the layout of the memory.
These can be edited in the board files, or applied to existing boards as **overlays**.

Libraries and drivers
*********************

Whereas nRF5 supported different short range protocols such as Gazell, ESB, and Ant, those are not supported by |BMshort|.
In general, |BMshort| support focuses on Bluetooth Low Energy.

Bluetooth LE libraries
======================

Bluetooth LE features that are natively offered by the SoftDevice are mostly unchanged from the nRF5, and the SoftDevice documentation highlights any differences.
As for the collection of Bluetooth LE libraries that were available in the nRF5, |BMshort| offers a limited subset, where each service may have slightly different API and functionality compared to their respective nRF5 implementation.

The Bluetooth LE services currently offered in |BMshort| are the following:

* Peripheral services:

  * Heart Rate Monitor (peripheral)
  * Nordic UART (NUS) (peripheral)
  * Nordic LED Button (LBS) (peripheral)
  * Continuous Glucose Monitor (peripheral)
  * Battery (peripheral)

* MCUMgr service (DFU service)
* Bond Management
* Device Information

Utility libraries for Bluetooth LE are available in |BMshort|, though their collection may not be as complete, and their functionality and API may be slightly different than their respective nRF5 implementation.

See table below for a summary of supported libraries.

.. list-table:: Supported libraries
   :header-rows: 1

   * - Name
     - Supported
     - New name
     - Planned
     - Comment
   * - ``ble_advertising``
     - Yes
     - ``ble_adv``
     -
     -
   * - ``ble_advdata``
     - Yes
     - Merged with ``ble_adv``
     -
     -
   * - ``ble_db_discovery``
     - No
     -
     - Yes
     -
   * - ``ble_conn_params``
     - Yes
     - Name unchanged
     -
     -
   * - ``ble_conn_state``
     - No
     -
     - Yes
     -
   * - ``ble_dtm``
     - No
     -
     - No
     - Out of scope
   * - ``ble_racp``
     - Yes
     - Name unchanged
     -
     -
   * - ``ble_srv_common``
     - No
     -
     - No
     - Using SoftDevice native API directly
   * - ``nrf_ble_gatt``
     - Yes
     - Merged with ``ble_conn_params``
     -
     -
   * - ``nrf_ble_gq``
     - Yes
     - ``ble_gq``
     -
     -
   * - ``nrf_ble_qwr``
     - Yes
     - ``ble_qwr``
     -
     -
   * - ``nrf_ble_scan``
     - No
     -
     - Yes
     -
   * - ``ble_link_ctx_manager``
     - No
     -
     - No
     - Functionality implemented manually where needed
   * - ``ble_radio_notification``
     - No
     -
     - Yes
     -
   * - ``peer_manager``
     - No
     - Unchanged
     - Yes
     -

Other libraries
===============

Regarding other utility libraries unrelated to Bluetooth LE, like ``app_timer``, a limited selection is available, often with a different name and slightly different API than their nRF5 variant.

Although sometimes a pattern can emerge on how to port from one to the other, no general rule is available and this must be done on a case-by-case basis.
This is due to several factors, including:

* Large number of libraries, with a mix of naming schemes such as ``ble_`` , ``nrf_``, no prefix.
* Large set of API, developed over the course of several years with little overall consistency with regards to error spaces, asynchronous events.
* Different project configuration mechanism, inherently affecting how libraries are configured.
* Different coding standard in the |NCS| (for example, limited use of ``typedef``).

.. _nrf5_bm_migration_dfu:

DFU
***

The DFU mechanism has changed from nRF5.
Some of the core functionality remains, although implemented differently.

Firstly, |BMshort| supports single-bank DFU updates.
It also supports SoftDevice updates as well as buttonless DFU.

In nRF5, the two major DFU components were the ``MBR`` firmware, (delivered beside the SoftDevice), and the nRF5 bootloader.
The ``MBR`` acted as a very simple first-stage bootloader, only booting the application or the bootloader, and supporting basic copy functionality.
The nRF5 bootloader had the capability to download new firmware, or SoftDevice + Bootloader images, which would then be verified and copied by the ``MBR``.

The bootloader has changed - |BMshort| uses the open-source MCUboot project as first-stage (immutable) bootloader, instead of the MBR.
MCUboot is also used in the |NCS| and other open-source projects.

A major difference is that the nRF5 bootloader included the transport (such as BLE, UART), whereas MCUboot does not.
MCUboot just decides which firmware to boot and verifies it before booting.

The actual download of the new firmware image is done by a dedicated firmware image called the Firmware Loader.
This firmware is provided in |Bmshort|.
In case of an application update, the Firmware Loader copies the new firmware in the application bank (or slot).
MCUboot will then verify and boot the updated firmware.

In case of a SoftDevice or Firmware Loader application update, the Firmware Loader on the device receives an image called Installer which is bundled with the new SoftDevice and/or Firmware Loader application.
The Installer firmware is also provided in |BMshort|.
This image is saved in place of the application by the Firmware Loader.
The Installer boots next after being verified by MCUboot, and proceeds to overwrite the Firmware Loader and SoftDevice as necessary.
The new Firmware Loader boots next to receive a new application image.

The host and mobile tools for DFU have changed, and new versions of both are made available by Nordic.

Drivers
*******

For migration of nrfx drivers, see `nrfx migration guides`_.
