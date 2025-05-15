# Copyright (c) 2025 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

# No chances needed to this file

if(CONFIG_SOC_NRF54L10_CPUAPP)
  board_runner_args(jlink "--device=cortex-m33" "--speed=4000")
endif()

include(${ZEPHYR_BASE}/boards/common/nrfutil.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
