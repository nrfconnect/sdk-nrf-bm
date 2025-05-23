#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_SOC_NRF54L05_CPUAPP OR CONFIG_SOC_NRF54L10_CPUAPP OR CONFIG_SOC_NRF54L15_CPUAPP)
  board_runner_args(jlink "--device=cortex-m33" "--speed=4000")
endif()

include(${ZEPHYR_BASE}/boards/common/nrfutil.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
