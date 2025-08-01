#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(unit_test_ble_adv)

set(SOFTDEVICE_VARIANT "s115")
set(SOFTDEVICE_INCLUDE_DIR "${ZEPHYR_NRF_BM_MODULE_DIR}/components/softdevice/${SOFTDEVICE_VARIANT}/${SOFTDEVICE_VARIANT}_API/include")

cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble.h)
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gatts.h
        WORD_EXCLUDE
        "__STATIC_INLINE")
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gattc.h
        WORD_EXCLUDE
        "__STATIC_INLINE")
cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/ble_gap.h)

add_compile_definitions(
        SVCALL_AS_NORMAL_FUNCTION=1
        NRF54L15_XXAA)

target_compile_definitions( app PRIVATE
        CONFIG_BLE_QWR_MAX_ATTR=2
        CONFIG_NRF_SDH_BLE_TOTAL_LINK_COUNT=1
        CONFIG_BLE_ADV_PRIMARY_PHY=0x05
        CONFIG_BLE_ADV_SECONDARY_PHY=0x05
        CONFIG_BLE_ADV_NAME="nRF_BM_CGMS"
        CONFIG_BLE_ADV_DIRECTED_ADVERTISING=1
        CONFIG_BLE_ADV_USE_WHITELIST=1
        CONFIG_BLE_ADV_DIRECTED_ADVERTISING_HIGH_DUTY=1
        CONFIG_BLE_ADV_DIRECTED_ADVERTISING=1
        CONFIG_BLE_ADV_FAST_ADVERTISING=1
        CONFIG_BLE_ADV_FAST_ADVERTISING_INTERVAL=40
        CONFIG_BLE_ADV_FAST_ADVERTISING_TIMEOUT=3000
        CONFIG_BLE_ADV_SLOW_ADVERTISING=1
        CONFIG_BLE_ADV_SLOW_ADVERTISING_INTERVAL=3200
        CONFIG_BLE_ADV_SLOW_ADVERTISING_TIMEOUT=18000
        CONFIG_BLE_ADV_DIRECTED_ADVERTISING_TIMEOUT=100
        CONFIG_BLE_ADV_DIRECTED_ADVERTISING_INTERVAL=32
)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)

target_include_directories(app PRIVATE ${ZEPHYR_NRF_BM_MODULE_DIR}/include)
target_include_directories(app PRIVATE ${SOFTDEVICE_INCLUDE_DIR})
target_include_directories(app PRIVATE ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/mdk)
target_include_directories(app PRIVATE ${ZEPHYR_CMSIS_MODULE_DIR}/CMSIS/Core/Include)

target_sources(app
  PRIVATE
  ${ZEPHYR_NRF_BM_MODULE_DIR}/lib/ble_adv/ble_adv.c
)
target_sources(app
  PRIVATE
  ${ZEPHYR_NRF_BM_MODULE_DIR}/lib/ble_adv/ble_adv_data.c
)
zephyr_include_directories(${ZEPHYR_NRF_BM_MODULE_DIR}/include)
