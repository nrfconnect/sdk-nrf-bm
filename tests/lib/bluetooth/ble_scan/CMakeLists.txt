#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(unit_test_ble_scan)

add_compile_definitions(
  SVCALL_AS_NORMAL_FUNCTION
  SUPPRESS_INLINE_IMPLEMENTATION
  NRF54L15_XXAA
  CONFIG_BLE_SCAN_BUFFER_SIZE=31
  CONFIG_BLE_SCAN_NAME_MAX_LEN=32
  CONFIG_BLE_SCAN_SHORT_NAME_MAX_LEN=32
  CONFIG_BLE_SCAN_NAME_COUNT=1
  CONFIG_BLE_SCAN_APPEARANCE_COUNT=1
  CONFIG_BLE_SCAN_ADDRESS_COUNT=1
  CONFIG_BLE_SCAN_SHORT_NAME_COUNT=2
  CONFIG_BLE_SCAN_UUID_COUNT=1
  CONFIG_BLE_SCAN_SCAN_INTERVAL=160
  CONFIG_BLE_SCAN_SCAN_DURATION=0x0000
  CONFIG_BLE_SCAN_SCAN_WINDOW=80
  CONFIG_BLE_SCAN_SLAVE_LATENCY=0
  CONFIG_BLE_SCAN_MIN_CONNECTION_INTERVAL=0x0006
  CONFIG_BLE_SCAN_MAX_CONNECTION_INTERVAL=0x0018
  CONFIG_BLE_SCAN_SUPERVISION_TIMEOUT=0x0C80
  CONFIG_BLE_SCAN_FILTER=1
)

set(SOFTDEVICE_VARIANT "s145")
set(SOFTDEVICE_INCLUDE_DIR "${ZEPHYR_NRF_BM_MODULE_DIR}/components/softdevice/${SOFTDEVICE_VARIANT}/${SOFTDEVICE_VARIANT}_API/include")

cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble.h)
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gap.h
  WORD_EXCLUDE
  "__STATIC_INLINE")
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gatts.h
  WORD_EXCLUDE
  "__STATIC_INLINE")
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gattc.h
  WORD_EXCLUDE
  "__STATIC_INLINE")
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/softdevice_handler/nrf_sdh_ble.h)
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/bluetooth/ble_adv_data.h)

zephyr_linker_sources(SECTIONS evt_obs.ld)

target_include_directories(app PRIVATE ${ZEPHYR_NRF_BM_MODULE_DIR}/include)
target_include_directories(app PRIVATE ${SOFTDEVICE_INCLUDE_DIR})
target_include_directories(app PRIVATE ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/mdk)
target_include_directories(app PRIVATE ${ZEPHYR_CMSIS_MODULE_DIR}/CMSIS/Core/Include)

test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)

target_sources(app
  PRIVATE
  ${ZEPHYR_NRF_BM_MODULE_DIR}/lib/bluetooth/ble_scan/ble_scan.c
)
