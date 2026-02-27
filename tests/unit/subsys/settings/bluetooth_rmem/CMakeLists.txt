#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(bluetooth_rmem)

target_include_directories(app PRIVATE ${ZEPHYR_NRF_BM_MODULE_DIR}/include)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)

# Create mock for bm_rmem module (needed for ble_name_value_get)
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/storage/bm_rmem.h bm/storage)

# Unit under test
target_sources(app PRIVATE ${ZEPHYR_NRF_BM_MODULE_DIR}/subsys/settings/src/bluetooth_rmem.c)
