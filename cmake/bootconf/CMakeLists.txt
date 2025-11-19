#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr
  COMPONENTS zephyr_default:dts,kconfig
  REQUIRED HINTS $ENV{ZEPHYR_BASE}
)

project(bootconf)

# Manually include board configuration to enable automatic runners.yaml generation
include(${BOARD_DIR}/board.cmake OPTIONAL)

# Create the runners_yaml_props_target that flash system expects
add_custom_target(runners_yaml_props_target)

# Set hex_file property to point to zephyr.hex in this directory
set_target_properties(runners_yaml_props_target PROPERTIES
  hex_file "${CMAKE_BINARY_DIR}/bootconf.hex"
)

# Override the runners.yaml path to use CMAKE_CURRENT_BINARY_DIR/zephyr instead of
# PROJECT_BINARY_DIR, this ensures runners.yaml is generated at <build>/softdevice/zephyr where
# west expects it
set(PROJECT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/zephyr)

# Include flash support to automatically generate runners.yaml
include(${ZEPHYR_BASE}/cmake/flash/CMakeLists.txt)

# Generate bootconf file
add_custom_target(bootconf_target
  ALL
  DEPENDS ${CMAKE_BINARY_DIR}/bootconf.hex
)

dt_nodelabel(boot_partition_node_full_path NODELABEL "boot_partition")
dt_reg_size(boot_partition_node_size PATH "${boot_partition_node_full_path}")

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/bootconf.hex
  COMMAND ${Python3_EXECUTABLE}
    ${ZEPHYR_NRF_MODULE_DIR}/scripts/reglock.py
    --output ${CMAKE_BINARY_DIR}/bootconf.hex
    --size ${boot_partition_node_size}
    --soc ${CONFIG_SOC}
  VERBATIM
  DEPENDS ${dts_files}
)

if(${boot_partition_node_size} GREATER 0x7c00)
  message(WARNING
    "boot_partition doesn't fit into protection region. "
    "Protection will be applied over maximum allowed span."
  )
endif()

set(BYPRODUCT_HEX_NAME
  "${CMAKE_BINARY_DIR}/bootconf.hex"
  CACHE FILEPATH "MCUboot with signed softdevice and flash metadata hex file" FORCE
)
