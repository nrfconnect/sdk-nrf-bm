# Copyright (c) 2025 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

# This script defines a CMake target 'generate_kmu_keyfile_json' to create keyfile.json
# using 'west ncs-provision upload --dry-run'.

# --- Construct the list of commands and dependencies ---
set(kmu_json_commands "")
set(kmu_json_dependencies "")

# Update keyfile for BL_PUBKEY
string(CONFIGURE "${SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE}" mcuboot_signature_key_file)
list(APPEND kmu_json_commands
  COMMAND ${Python3_EXECUTABLE} -m west ncs-provision upload
    --keyname BL_PUBKEY
    --key ${mcuboot_signature_key_file}
    --build-dir ${CMAKE_BINARY_DIR}
    --dry-run
)
list(APPEND kmu_json_dependencies ${mcuboot_signature_key_file})

# --- Add custom command to generate/update keyfile.json ---
if(NOT kmu_json_commands STREQUAL "")
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/keyfile.json
    ${kmu_json_commands} # Expands to one or more COMMAND clauses
    DEPENDS ${kmu_json_dependencies}
    COMMENT "Generating/Updating KMU keyfile JSON (${CMAKE_BINARY_DIR}/keyfile.json)"
    VERBATIM
  )

  # --- Add custom target to trigger the generation ---
  add_custom_target(
    generate_kmu_keyfile_json ALL
    DEPENDS ${CMAKE_BINARY_DIR}/keyfile.json
  )
endif()
