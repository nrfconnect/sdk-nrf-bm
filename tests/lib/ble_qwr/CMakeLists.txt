#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(unit_test_ble_qwr)

add_compile_definitions(
        SVCALL_AS_NORMAL_FUNCTION=1
        NRF54L15_XXAA
        CONFIG_BLE_QWR_MAX_ATTR=2
)

cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/s115/ble.h)
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/s115/ble_gatts.h
        WORD_EXCLUDE
        "__STATIC_INLINE")
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/s115/ble_gattc.h
        WORD_EXCLUDE
        "__STATIC_INLINE")

target_include_directories(app PRIVATE ${ZEPHYR_NRF_BM_MODULE_DIR}/include)
target_include_directories(app PRIVATE ${ZEPHYR_NRF_BM_MODULE_DIR}/include/s115)
target_include_directories(app PRIVATE ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/mdk)
target_include_directories(app PRIVATE ${ZEPHYR_CMSIS_MODULE_DIR}/CMSIS/Core/Include)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)

# Unit under test
target_sources(app PRIVATE ${ZEPHYR_NRF_BM_MODULE_DIR}/lib/ble_qwr/ble_qwr.c)
