#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_BOOT_FIRMWARE_LOADER)
  set(sysbuild_dir ${CMAKE_BINARY_DIR}/../)

  # Flash metadata
  add_custom_target(flash_metadata_generation
    ALL
    DEPENDS ${sysbuild_dir}/flash_metadata.hex
  )

  add_custom_command(OUTPUT ${sysbuild_dir}/flash_metadata.hex
    COMMAND
    ${PYTHON_EXECUTABLE}
    ${ZEPHYR_NRF_BM_MODULE_DIR}/scripts/generate_flash_metadata.py
    --build-dir ${sysbuild_dir}
    DEPENDS ${sysbuild_dir}/zephyr/.config
  )

  # MCUboot, signed softdevice and flash metadata merged
  add_custom_target(signed_mcuboot_softdevice_flash_metadata
    ALL
    DEPENDS ${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}_signed_softdevice_flash_metadata.hex
  )

  add_custom_command(OUTPUT ${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}_signed_softdevice_flash_metadata.hex
    COMMAND
    ${PYTHON_EXECUTABLE}
    ${ZEPHYR_BASE}/scripts/build/mergehex.py
    -o ${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}_signed_softdevice_flash_metadata.hex
    ${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}.hex
    ${CMAKE_BINARY_DIR}/../softdevice.signed.hex
    ${sysbuild_dir}/flash_metadata.hex
    DEPENDS
    flash_metadata_generation
    signed_softdevice_target
    ${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}.hex
    ${CMAKE_BINARY_DIR}/../softdevice.signed.hex
    ${sysbuild_dir}/flash_metadata.hex
  )

  # Set flash file to merged hex file
  set_target_properties(runners_yaml_props_target PROPERTIES "hex_file"
    "${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}_signed_softdevice_flash_metadata.hex"
  )
  set(BYPRODUCT_KERNEL_SIGNED_HEX_NAME
    "${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}_signed_softdevice_flash_metadata.hex"
    CACHE FILEPATH "MCUboot with signed softdevice and flash metadata hex file" FORCE
  )
endif()
