#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(bm_storage_sd)

softdevice_unity_setup()

cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/nrf_sdm.h)
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/nrf_soc.h)

cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/softdevice_handler/nrf_sdh.h
  FUNC_EXCLUDE "_handler" # Do not generate a mock for the prototype of the observer's handlers
)

# Required by nrf_soc.h.
zephyr_compile_definitions(
  NRF54L15_XXAA
  NRF_APPLICATION
  SUPPRESS_INLINE_IMPLEMENTATION
)

zephyr_include_directories(
  ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/bsp/stable
  ${ZEPHYR_BASE}/modules/hal_nordic/nrfx
)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)
