#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(unit_test_ble_nus_client)

set(SOFTDEVICE_VARIANT "s145")
set(SOFTDEVICE_INCLUDE_DIR "${ZEPHYR_NRF_BM_MODULE_DIR}/components/softdevice/nrf54l/\
${SOFTDEVICE_VARIANT}/${SOFTDEVICE_VARIANT}_API/include")

cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble.h)
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gatts.h)
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gattc.h)
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gap.h)
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/bluetooth/ble_gq.h)
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/softdevice_handler/nrf_sdh_ble.h)
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/bluetooth/ble_db_discovery.h)

zephyr_compile_definitions(
  NRF54L15_XXAA
  SVCALL_AS_NORMAL_FUNCTION
  SUPPRESS_INLINE_IMPLEMENTATION
)

zephyr_linker_sources(SECTIONS evt_obs.ld)

zephyr_include_directories(
  ${ZEPHYR_NRF_BM_MODULE_DIR}/include
  ${SOFTDEVICE_INCLUDE_DIR}
)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)
