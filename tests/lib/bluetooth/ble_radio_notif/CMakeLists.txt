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

cmock_handle(${SOFTDEVICE_INCLUDE_DIR}/nrf_soc.h)
cmock_handle(mocks/cmsis.h)

target_compile_definitions(app PRIVATE
  SVCALL_AS_NORMAL_FUNCTION
  SUPPRESS_INLINE_IMPLEMENTATION
  NRF54L15_XXAA
  CONFIG_BLE_RADIO_NOTIFICATION_ON_BOTH=1
  CONFIG_BLE_RADIO_NOTIFICATION_IRQ_PRIO=5
  SWI02_IRQn=30
)

target_include_directories(app PRIVATE
  ${ZEPHYR_NRF_BM_MODULE_DIR}/include
  ${SOFTDEVICE_INCLUDE_DIR}
  ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/mdk
)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)

# Unit under test
target_sources(app PRIVATE
  ${ZEPHYR_NRF_BM_MODULE_DIR}/lib/bluetooth/ble_radio_notification/ble_radio_notification.c
)
