#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(unit_test_ble_conn_state)

set(SOFTDEVICE_VARIANT "s115")
set(SOFTDEVICE_INCLUDE_DIR "${ZEPHYR_NRF_BM_MODULE_DIR}/components/softdevice/\
${SOFTDEVICE_VARIANT}/${SOFTDEVICE_VARIANT}_API/include")

target_compile_definitions(app PRIVATE
  SVCALL_AS_NORMAL_FUNCTION
  SUPPRESS_INLINE_IMPLEMENTATION
  NRF54L15_XXAA
  CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT=64
  CONFIG_BLE_CONN_STATE_USER_FLAG_COUNT=24
  CONFIG_BLE_CONN_STATE_DEFAULT_FLAG_COLLECTION_COUNT=6
)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)

target_include_directories(app PRIVATE
  ${ZEPHYR_NRF_BM_MODULE_DIR}/include
  ${SOFTDEVICE_INCLUDE_DIR}
  ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/mdk
)

target_sources(app PRIVATE
  ${ZEPHYR_NRF_BM_MODULE_DIR}/lib/ble_conn_state/ble_conn_state.c
)
