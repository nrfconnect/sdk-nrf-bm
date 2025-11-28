.. _index:
.. _nrf_bm_introduction:

Introduction
############

The |BMlong| is a streamlined enhancement of the |NCS|, tailored for developers who are working on Bluetooth® LE applications that do not require the complexities of an RTOS or the full suite of advanced features.
This option is particularly beneficial for applications that require a lightweight, responsive solution.

A major component of the |BMshort| configuration is the SoftDevice.
To facilitate Bluetooth LE operations without an RTOS, a new SoftDevice variant supporting the latest nRF54L devices has been released.
This new variant maintains a similar functionality and API to those found in the existing nRF52 SoftDevices, ensuring a smooth transition and familiarity for developers.

The |BMshort| codebase is structured to include libraries and modules that are specifically designed to operate without an RTOS.
This ensures that all included components are optimized for threadless, bare metal environments.

The |BMshort| option is ideal for:

* Developing simple, threadless applications without the need for Zephyr RTOS.
* Migrating applications initially developed for nRF52 devices using the nRF5 SDK.

Integration and structure
*************************

The |BMlong| is a distinct repository that incorporates elements from the existing |NCS|:

* **Repository-Level Filtering:** |BMshort| utilizes a repository-level filtering mechanism designed to include components beneficial for bare metal development.
  However, this approach is based on the best possible method available, which means that some components that are not suited for bare metal applications will also be included.
  Developers should be aware of these limitations and exercise discretion when utilizing the repository.
* **Reuse of nRF Connect SDK components:** The |BMshort| solution reuses some components (such as modules/libraries) from the |NCS| repositories.
  These components are included and reused because they do not rely on Zephyr RTOS primitives, making them suitable for the bare metal SDK.
  Examples of modules/libraries reused in |BMshort| are logging, crypto functionality, and smaller libraries like CRC calculation, signing scripts, and ring buffer.
* **nrfx:** The |BMshort| solution does not use Zephyr drivers and the Devicetree configuration for peripherals.
  Instead, nrfx drivers are used directly.
  |BMShort| uses the nrfx drivers that come with |NCS|.
* **Tools:** Tools such as the compiler and linker from the original |NCS| are reused in |BMshort|.
  The existing nRF Connect extension in |VSC| is the recommended IDE for working with Bare Metal.

.. toctree::
   :maxdepth: 2
   :caption: Contents

   install_nrf_bm.rst
   drivers.rst
   libraries/index.rst
   samples.rst
   app_dev/dfu/index.rst
   release_notes.rst
   migration/nrf5_bm_migration.rst
   softdevice_docs.rst
   api/api.rst
