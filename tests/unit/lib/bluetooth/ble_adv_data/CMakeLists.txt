#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(unit_test_ble_adv_data)

softdevice_unity_setup()

cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble.h)
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gap.h)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)
