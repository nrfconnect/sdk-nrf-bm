.. _ug_dfu_name:

Setting up DFU Device Bluetooth name remotely
#############################################

.. note::
   The support for this feature is currently experimental.

The Device Firmware Update (DFU) over Bluetooth® Low Energy in the single bank solution uses the loader application for firmware download support.
The firmware loader is separate application to which the remote Bluetooth LE DFU client connects to perform the firmware update.
To identify the firmware loader application over Bluetooth LE, it uses a specific advertising name provided by the remote client to the application.

You can configure the name at application runtime using the MCUmgr settings command group.

Write the name using the MCUmgr write setting request for ``fw_loader/adv_name`` key and export the key value to retention memory using the MCUmgr save settings request.
Once the device is rebooted into the firmware loader application, the name can be read back from the retention memory and used as the advertising name of the firmware loader.

Enabling the feature
********************

To enable this feature for a sysbuild project, use the :kconfig:option:`SB_CONFIG_BM_APP_CAN_SETUP_FIRMWARE_LOADER_NAME` Kconfig option.
Otherwise, enable the following Kconfig options in both the application and the firmware loader configuration:

* :kconfig:option:`CONFIG_RETENTION` - Allows the retention of data across device reboots.
* :kconfig:option:`CONFIG_BM_FLAT_SETTINGS_BLUETOOTH_NAME` - Enables settings handlers for Bluetooth name sharing using the retention clipboard (key ``fw_loader/adv_name``).
  This selects the inter-application retention clipboard in SRAM.

Additionally, the application must enable the MCUmgr settings group:

* :kconfig:option:`CONFIG_MCUMGR_GRP_SETTINGS` - Enables the MCUmgr settings group (available when :kconfig:option:`CONFIG_BM_FLAT_SETTINGS_BLUETOOTH_NAME` is enabled).

This feature is used in the :ref:`ble_mcuboot_recovery_entry_sample` sample.

Board DTS setup for clipboard partition
***************************************

When using the :kconfig:option:`CONFIG_BM_FLAT_SETTINGS_BLUETOOTH_NAME` Kconfig option, the application and the firmware loader share the Bluetooth LE advertising name through an inter-application retention clipboard stored in SRAM.
This feature involves the clipboard subsystem that is enabled through the :kconfig:option:`CONFIG_BM_FLAT_SETTINGS_BLUETOOTH_NAME` Kconfig option.
The clipboard subsystem is assigned to a SRAM region using the devicetree chosen property ``ncsbm,clipboard-partition``.
A board target that enables this feature must define an SRAM memory region for the clipboard and expose it through that chosen property.

Add the following to your board devicetree (for example, in the MCUboot variant DTS such as :file:`bm_nrf54ls05dk_nrf54ls05b_cpuapp_s115_softdevice_mcuboot.dts`):

* A node with ``compatible`` set to ``"zephyr,memory-region"`` and ``"mmio-sram"``.
   The build system generates a linker section for any node with ``"zephyr,memory-region"``, so the region is reserved and excluded from the default SRAM allocation.

* The chosen property ``ncsbm,clipboard-partition`` set to a phandle of that node.

Example (adjust the ``reg`` address and size to match your SoC memory map and the space you reserve for the clipboard):

.. code-block:: devicetree

   / {
           clipboard_partition: sram@20017c00 {
                   compatible = "zephyr,memory-region", "mmio-sram";
                   reg = <0x20017c00 DT_SIZE_K(1)>;
                   zephyr,memory-region = "RetainedMem";
                   status = "okay";
           };

           chosen {
                   ncsbm,clipboard-partition = &clipboard_partition;
           };
   };

Ensure that the clipboard region does not overlap the application SRAM (for example, by reducing the main SRAM size in the same DTS so that it ends before the clipboard region).
For reference, see the |BMshort| board DTS files in the :file:`nrf-bm/boards/nordic/` folder.
