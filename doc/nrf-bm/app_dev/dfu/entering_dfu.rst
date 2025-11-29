.. _entering_dfu_single_bank:

Entering single-Bank Device Firmware Update (DFU) mode
######################################################

|BMlong| supports a single-bank DFU mechanism.
The device normally executes the application that cannot update itself nor the firmware elements it depends on.
This means that the device running the application must be switched to the DFU mode so that it can run the firmware loader.
The firmware loader is a physically separated firmware entity that provides services for supporting the firmware download to the single bank.
Decision on entering the firmware loader is made by the MCUboot.
The available entering schemes are listed the following sections.

Entering on GPIO state
**********************

The firmware loader is entered when the active state of a GPIO pin is asserted during early device startup.
The SoC must be rebooted, for instance, by the board power toggling to perform this action.
Use the :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_FIRMWARE_LOADER_ENTRANCE_GPIO` sysbuild Kconfig option to enter this mode.
The GPIO PIN is configured by the defining ``BOARD_PIN_BTN_0`` of the board in its :file:`include/board-config.h` file.

Entering on reset by PIN
************************

The firmware loader is entered when the SoC reset is triggered by the reset pin.
Use the :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_FIRMWARE_LOADER_ENTRANCE_PIN_RESET` sysbuild Kconfig option to enter this mode.

Buttonless entering
*******************

The firmware loader is entered when the MCUboot detects such request delivered to the retained memory area by the application.
The application must trigger the SoC reset for making MCUboot to perform this action.
Use boot mode API: :c:func:`bootmode_set` and :c:enum:`BOOT_MODE_TYPE_BOOTLOADER` to set the retention boot mode value.
Use the :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_FIRMWARE_LOADER_ENTRANCE_BOOT_MODE` sysbuild Kconfig option to enter this mode.

The SoC's ``GPREGRET1`` register is used as the retained memory area for that purpose.
The board's DTS must describe this register and its assignment to the ``chosen/zephyr,boot-mode`` property.

This mode is integrated with the MCUmgr OS reset command when the :kconfig:option:`CONFIG_MCUMGR_GRP_OS_RESET_BOOT_MODE` Kconfig option is enabled.
