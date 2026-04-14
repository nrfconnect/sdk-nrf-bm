#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(unit_test_ble_bas_client)

softdevice_unity_setup(VARIANT s145)

cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/bluetooth/ble_gq.h)
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/bluetooth/ble_db_discovery.h)

zephyr_compile_definitions(
  NRF54L15_XXAA
  SUPPRESS_INLINE_IMPLEMENTATION
)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)
