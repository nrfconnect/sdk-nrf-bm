#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(unit_test_ble_hrs_client)

softdevice_unity_setup()

cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble.h)
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gattc.h)
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gap.h)
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/bluetooth/ble_db_discovery.h)
cmock_handle(${ZEPHYR_NRF_BM_MODULE_DIR}/include/bm/bluetooth/ble_gq.h)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)
