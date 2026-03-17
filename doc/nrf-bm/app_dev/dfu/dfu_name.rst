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
* :kconfig:option:`CONFIG_BM_FLAT_SETTINGS_BLUETOOTH_NAME` - Enables settings handlers for Bluetooth name sharing using the retention clipboard (key ``fw_loader/adv_name``). Selects the inter-application retention clipboard in SRAM.

Additionally, the application must enable the MCUmgr settings group:

* :kconfig:option:`CONFIG_MCUMGR_GRP_SETTINGS` - Enables the MCUmgr settings group (available when :kconfig:option:`CONFIG_BM_FLAT_SETTINGS_BLUETOOTH_NAME` is enabled).

This feature is used in the :ref:`ble_mcuboot_recovery_entry_sample` sample.
