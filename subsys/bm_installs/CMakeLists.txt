#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_BM_INSTALL_IMAGES GREATER 0)
  zephyr_library()
  zephyr_include_directories(include)
  zephyr_library_sources(src/bm_installs.c)
endif()
