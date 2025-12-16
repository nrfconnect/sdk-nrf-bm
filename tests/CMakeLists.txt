#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
if(CONFIG_UNITY)
  zephyr_linker_sources(SECTIONS observers.ld)
  zephyr_include_directories(.)
  zephyr_sources(observers.c)
endif()
