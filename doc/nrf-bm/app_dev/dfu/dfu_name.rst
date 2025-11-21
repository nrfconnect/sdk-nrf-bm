.. _ug_dfu_name:

DFU BLE Device name remote setup
################################

The Device Firmware Update (DFU) over Bluetooth Low Energy (BLE) in the single bank solution uses dedicated firmware entity: the loader application for storage support.
The firmware loader is separate application to which the remote BLE DFU client connects to perform the firmware update.
To identify the firmware loader application over BLE, it can uses a specific advertising name provided by the remote client to the application.

Such BLE name can be configured at application runtime using the MCUmgr settings commands group.
The name has to be write using the MCUmgr write setting request `bluetooh_name/name`` key, then the key-value must be exported to retention memory using the MCUmgr save settings request.
Once the device will be rebooted into the firmware loader application, it can be read back from the retention memory and be use as the advertising name
of the firmware loader.

Enabling the feature
********************

To enable this feature for a sysbuild project, the :kconfig:option:`SB_CONFIG_BM_APP_CAN_SETUP_FIRMWARE_LOADER_NAME` sysbuild Kconfig option can be used
Otherwise the application and the firmware loader configuration should enable following Kconfig options must be enabled:

- :kconfig:option:`CONFIG_RETAINED_MEM`: Enables the use of retained memory.
- :kconfig:option:`CONFIG_RETENTION`: Allows the retention of data across device reboots.
- :kconfig:option:`CONFIG_SETTINGS`: Enables the settings management subsystem.
- :kconfig:option:`CONFIG_SETTINGS_RETENTION`: Enables retention backend implementation of settings subsystem.
- :kconfig:option:`CONFIG_NCS_BM_SETTINGS_BLUETOOTH_NAME`: Enables setting handlers required for bluetooth name sharing support.

Additionally, the application must enabled the MCumgr settings group by enabling the following Kconfig option:
- :kconfig:option:`CONFIG_MCUMGR_GRP_SETTINGS`: Enables the MCUmgr settings group.
- :kconfig:option:`CONFIG_SETTINGS_RUNTIME`: Allows runtime modification of settings.

Usage example
*************

This feature is used in the :ref:`ble_mcuboot_recovery_entry_sample` so in the BLE mcumgr firmware loader used by that project.
