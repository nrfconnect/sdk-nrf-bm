.. _ug_bm_softperipheral:

Using Soft Peripherals
######################

Soft peripherals implement digital peripherals in firmware running on the `Fast Lightweight Peripheral Processor (FLPR)`_, a dedicated `VPR`_ CPU based on the RISC-V architecture in the fast domain.
This guide describes what you need to add to an application developed with |BMlong| to use a Soft Peripheral, using sQSPI as an example.
For the driver API, firmware versions, and the full porting guide of each Soft Peripheral, see `Soft Peripherals`_.
For a sample implementation, see the :ref:`sQSPI_sample` sample.

Prerequisites
*************

Before you integrate a Soft Peripheral, make sure of the following:

* The Soft Peripheral firmware supports your SoC.
  See `Soft Peripherals`_ for the list of supported SoCs and firmware versions.
* FLPR is not used by another component in your application.
* The pins you want to use can be controlled by FLPR.
  See `VPR pin mapping`_ for the pins available on your SoC.

To add the sQSPI driver to your application, enable the :kconfig:option:`CONFIG_BM_SOFTPERIPHERAL` and :kconfig:option:`CONFIG_BM_SQSPI` Kconfig options.

Reserving memory
****************

The Soft Peripheral firmware runs from RAM.
At startup, the driver copies the firmware into RAM and starts FLPR, which then executes it from there.
The application core and FLPR also share a small RAM region, the virtual register interface, which the driver uses to control the Soft Peripheral.

You must reserve these RAM regions in the devicetree, so that the application, the SoftDevice, and other components do not use them.
The following table shows the requirement for sQSPI on the nRF54L15 and nRF54LM20 SoCs:

.. list-table::
   :header-rows: 1
   :widths: auto

   * - Node label
     - Content
     - Size (sQSPI)
   * - ``flpr_firmware_ram``
     - Firmware
     - ``0x3740``
   * - ``flpr_execution_ram``
     - Working memory of the firmware
     - ``0x400``
   * - ``flpr_vri_ram``
     - Virtual register interface
     - ``0x200``

The three regions must be placed directly after each other, in this order, which is ``0x3D40`` bytes in total.
For the required sizes of other Soft Peripherals or firmware versions, see the porting guide of the selected Soft Peripheral in `Soft Peripherals`_.

Placing the regions in RAM
==========================

In the |BMshort| board targets, the ``softdevice_ram`` region starts at the beginning of SRAM, and the ``app_ram`` region uses the rest of SRAM up to the FLPR context-save area at the end.
The easiest place for the Soft Peripheral regions is directly after ``softdevice_ram``, at the start of ``app_ram``.

To make room for them, complete the following steps:

1. Delete the original ``app_ram`` node.
#. Add the three Soft Peripheral regions, starting at the original ``app_ram`` start address.
#. Add a new ``app_ram`` node directly after the Soft Peripheral regions.
   Move its start address up by ``0x3D40`` and reduce its size by ``0x3D40``, so that its end address stays the same.

For example, for the ``bm_nrf54lm20dk/nrf54lm20a/cpuapp/s115_softdevice`` board target, where ``app_ram`` originally starts at ``0x4380``:

.. code-block:: devicetree

	/delete-node/ &app_ram;

		&cpuapp_sram {
			flpr_firmware_ram: sram@4380 {
				label = "flpr_firmware_ram";
				reg = <0x4380 0x3740>;
			};

		flpr_execution_ram: sram@7ac0 {
			label = "flpr_execution_ram";
			reg = <0x7ac0 0x400>;
		};

		flpr_vri_ram: sram@7ec0 {
			label = "flpr_vri_ram";
			reg = <0x7ec0 0x200>;
		};

		app_ram: sram@80c0 {
			label = "app_ram";
			reg = <0x80c0 DT_SIZE_K(479)>;
		};
	};

.. note::
   Do not let ``app_ram`` grow past its original end address.
   The memory after it is used by FLPR to save its context, and must not be overwritten.

You can add these nodes in one of the following ways:

* As a devicetree overlay file in your application, for example :file:`boards/<board_target>.overlay`.
  This is how the :ref:`sQSPI_sample` sample does it, and it keeps the board files unchanged.
* Directly in the devicetree source of your own board.
  Use this approach if the Soft Peripheral is a permanent part of your design.

Using the regions in the application
====================================

The driver accesses the Soft Peripheral through the virtual register interface.
Use the address of the ``flpr_vri_ram`` node as the register address of the driver instance:

.. code-block:: c

   static nrf_sqspi_t qspi = {
    .p_reg = (void *)DT_REG_ADDR(DT_NODELABEL(flpr_vri_ram)),
    .drv_inst_idx = 0
   };

Initializing the Soft Peripheral
********************************

Before you initialize the driver, make sure of the following:

* FLPR has access to the hardware peripherals that the firmware uses.
  For example, sQSPI needs access to MEMCONF, which you must allow in the SPU.
* The pins are configured and their control is assigned to FLPR, either by the driver or by the application.
* The FLPR interrupt is connected to the driver interrupt handler.
  Use ``BM_IRQ_DIRECT_CONNECT`` with ``SP_VPR_IRQn``, and call the driver interrupt handler, for example ``nrf_sqspi_irq_handler()``, from it.
  Choose an interrupt priority that your SoftDevice configuration allows.

Then initialize the driver, configure the connected device, and activate the Soft Peripheral as described in the driver API.
