#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(unit_test_bm_spi_mngr)


cmock_handle(${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/drivers/include/nrfx_spim.h
  # Exclude the nRF52-only anomaly-58 workaround, irrelevant here and needs nrfx_gpiote.h types.
  FUNC_EXCLUDE ".*nrfx_spim_nrf52_anomaly_58_init"
)

zephyr_compile_definitions(
  NRF54L15_XXAA
  NRF_APPLICATION
  NRFX_DECLARE_ONLY
)

zephyr_include_directories(
  ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx
  ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/drivers/include
  ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/bsp/stable/templates
  ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/nrfx/bsp/stable
  ${ZEPHYR_BASE}/modules/hal_nordic/nrfx
  ${ZEPHYR_CMSIS_MODULE_DIR}/CMSIS/Core/Include
)

test_runner_generate(src/unity_test.c)
target_sources(app PRIVATE src/unity_test.c)
