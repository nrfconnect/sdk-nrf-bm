#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr
  COMPONENTS zephyr_default:kconfig
  REQUIRED HINTS $ENV{ZEPHYR_BASE}
)

project(softdevice)

# Manually include board configuration to enable automatic runners.yaml generation
include(${BOARD_DIR}/board.cmake OPTIONAL)

# Create the runners_yaml_props_target that flash system expects
add_custom_target(runners_yaml_props_target)

# Set hex_file property to point to zephyr.hex in this directory
set_target_properties(runners_yaml_props_target PROPERTIES
  hex_file "${CONFIG_SOFTDEVICE_FILE}"
)

# Override the runners.yaml path to use CMAKE_CURRENT_BINARY_DIR/zephyr instead of
# PROJECT_BINARY_DIR, this ensures runners.yaml is generated at <build>/softdevice/zephyr where
# west expects it
set(PROJECT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/zephyr)

# Include flash support to automatically generate runners.yaml
include(${ZEPHYR_BASE}/cmake/flash/CMakeLists.txt)
