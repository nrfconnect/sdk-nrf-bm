#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_NCS_BM)
  zephyr_include_directories(include)
  zephyr_include_directories(include/bm)
  zephyr_include_directories(include/bm/bluetooth)

  include(cmake/version.cmake)

  add_subdirectory(drivers)
  add_subdirectory(lib)
  add_subdirectory(subsys)
endif()
