#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(unit_test_ble_radio_notif)

softdevice_unity_setup()

cmock_handle(mocks/cmsis.h)
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/nrf_soc.h)

zephyr_compile_definitions(
  NRF54L15_XXAA
  SUPPRESS_INLINE_IMPLEMENTATION
  SWI02_IRQn=30
)

zephyr_include_directories(
  mocks
)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)
