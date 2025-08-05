#
# Copyright (c) 2025 Nordic Semiconductor ASA. All Rights Reserved.
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(unittest_test_bm_timer)

add_compile_definitions(
    NRF54L15_XXAA
    NRF_APPLICATION
    NRFX_DECLARE_ONLY
    CONFIG_BM_TIMER_IRQ_PRIO=3
    CONFIG_SOC_SERIES_NRF54LX=1
)

cmock_handle(
    ./include/zephyr/kernel.h
)

target_include_directories(app PRIVATE ${ZEPHYR_NRF_BM_MODULE_DIR}/include)
target_include_directories(app PRIVATE ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx)
target_include_directories(app PRIVATE ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/mdk)
target_include_directories(app PRIVATE ${ZEPHYR_CMSIS_MODULE_DIR}/CMSIS/Core/Include)
target_include_directories(app PRIVATE ${ZEPHYR_BASE}/modules/hal_nordic/nrfx)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)

# Unit under test
target_sources(app PRIVATE ${ZEPHYR_NRF_BM_MODULE_DIR}/lib/bm_timer/bm_timer.c)
