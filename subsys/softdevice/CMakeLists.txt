#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
if (CONFIG_SOFTDEVICE_S115)
  zephyr_include_directories(${ZEPHYR_NRF_BM_MODULE_DIR}/include/s115/)
endif()

if (CONFIG_SOFTDEVICE_S145)
  zephyr_include_directories(${ZEPHYR_NRF_BM_MODULE_DIR}/include/s145/)
endif()

zephyr_sources(tables.c)
