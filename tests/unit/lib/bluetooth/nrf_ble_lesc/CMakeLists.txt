#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(unit_test_peer_manager_nrf_ble_lesc)

include(${ZEPHYR_NRF_BM_MODULE_DIR}/cmake/unity/unity_softdevice_setup.cmake)
unity_softdevice_header_setup(VARIANT "s145")

cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gap.h)
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/softdevice_handler/nrf_sdh_ble.h)
cmock_handle(include/psa/crypto.h) # Local definitions for mocking of psa/crypto.h with CMock

zephyr_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include # Include test specific headers
)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)

# Unit under test
target_sources(app PRIVATE
  ${ZEPHYR_NRF_BM_MODULE_DIR}/lib/bluetooth/peer_manager/modules/nrf_ble_lesc.c
)
