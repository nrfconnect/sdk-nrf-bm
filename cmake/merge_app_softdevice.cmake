#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_PROGRAM_SOFTDEVICE_WITH_APP)
  add_custom_target(app_softdevice_merged
    ALL
    DEPENDS ${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}_softdevice_merged.hex
  )

  add_custom_command(OUTPUT ${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}_softdevice_merged.hex
    COMMAND
    ${PYTHON_EXECUTABLE}
    ${ZEPHYR_BASE}/scripts/build/mergehex.py
    -o ${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}_softdevice_merged.hex
    ${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}.hex
    ${CONFIG_SOFTDEVICE_FILE}
    DEPENDS
    ${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}.hex
    ${CONFIG_SOFTDEVICE_FILE}
  )

  set_target_properties(runners_yaml_props_target PROPERTIES "hex_file"
    "${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}_softdevice_merged.hex"
  )
  set(BYPRODUCT_KERNEL_APP_SD_HEX_NAME
    "${ZEPHYR_BINARY_DIR}/${KERNEL_NAME}_softdevice_merged.hex"
    CACHE FILEPATH "Application with SoftDevice hex file" FORCE
  )

endif()
