#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(bm_storage_rram)

zephyr_compile_definitions(
  NRF54L15_XXAA
  NRF_APPLICATION
  NRFX_DECLARE_ONLY
)

cmock_handle(${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/drivers/include/nrfx_rramc.h
  WORD_EXCLUDE "NRFX_STATIC_INLINE"
  FUNC_EXCLUDE "nrfy_rramc.*"
)

zephyr_include_directories(
  ${ZEPHYR_NRF_BM_MODULE_DIR}/include
  ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx
  ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/drivers/include
  ${ZEPHYR_BASE}/modules/hal_nordic/nrfx
  ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/bsp/stable/templates
  ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/bsp/stable
  ${ZEPHYR_BASE}/../modules/hal/cmsis/CMSIS/Core/Include
)

# Generate and add test file
test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)

# Unit under test
zephyr_sources(${ZEPHYR_NRF_BM_MODULE_DIR}/subsys/storage/bm_storage/bm_storage.c)
zephyr_sources(${ZEPHYR_NRF_BM_MODULE_DIR}/subsys/storage/bm_storage/rram/bm_storage_rram.c)
