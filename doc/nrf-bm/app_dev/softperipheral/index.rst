.. _bm_softperipheral:

Soft peripherals
################

Soft peripherals implement digital peripherals in firmware running on the `Fast Lightweight Peripheral Processor (FLPR)`_, a dedicated `VPR`_ CPU based on the RISC-V architecture in the fast domain.
The application core controls them through a driver.

For the driver API, limitations, and integration steps, see the `Soft Peripherals`_ documentation in nrfxlib.
For an example, see the :ref:`sQSPI_sample` sample, which runs on development kits with on-board external flash.

.. toctree::
   :maxdepth: 1
   :caption: Subpages:

   ug_softperipheral
