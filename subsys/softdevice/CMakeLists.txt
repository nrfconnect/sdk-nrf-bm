#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
if (CONFIG_SOFTDEVICE_S140)
  zephyr_include_directories(${NRF_LITE_DIR}/include/s140/)
endif()

if (CONFIG_SOFTDEVICE_S115)
  zephyr_include_directories(${NRF_LITE_DIR}/include/s115/)
endif()

zephyr_sources(nvic.c)
zephyr_sources(tables.c)
