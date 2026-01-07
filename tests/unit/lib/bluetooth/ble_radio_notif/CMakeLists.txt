#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(test_ble_radio_notif)

set(SOFTDEVICE_VARIANT "s115")
set(SOFTDEVICE_INCLUDE_DIR "${ZEPHYR_NRF_BM_MODULE_DIR}/components/softdevice/\
${SOFTDEVICE_VARIANT}/${SOFTDEVICE_VARIANT}_API/include")

zephyr_compile_definitions(
  SVCALL_AS_NORMAL_FUNCTION
  SUPPRESS_INLINE_IMPLEMENTATION
  NRF54L15_XXAA
  SWI02_IRQn=30
)

cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/nrf_soc.h)
cmock_handle(mocks/cmsis.h)

zephyr_include_directories(
  mocks
  ${ZEPHYR_NRF_BM_MODULE_DIR}/include
  ${SOFTDEVICE_INCLUDE_DIR}
)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)
