#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(unit_test_peer_manager)

include(${ZEPHYR_NRF_BM_MODULE_DIR}/cmake/unity/unity_softdevice_setup.cmake)
unity_softdevice_header_setup(VARIANT "s145")
unity_softdevice_event_setup()

cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gap.h)
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gattc.h)
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gatts.h)
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/nrf_soc.h)
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/bm_timer.h)
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/fs/bm_zms.h)
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/softdevice_handler/nrf_sdh_ble.h)
cmock_handle(include/psa/crypto.h) # Local definitions for mocking of psa/crypto.h with CMock

# Increase the memory size for CMock. The default of 32768 is not sufficient for the tests.
zephyr_compile_definitions(CMOCK_MEM_SIZE=65536)

zephyr_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include # Include test specific headers
)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)
