#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_NCS_BM)
  zephyr_include_directories(include)
  if(CONFIG_NCS_BM_INCLUDE_PATH_LEGACY)
    zephyr_include_directories(include/bm)
    zephyr_include_directories(include/bm/bluetooth)
    zephyr_include_directories(include/bm/drivers)
    zephyr_include_directories(include/bm/fs)
    zephyr_include_directories(include/bm/softdevice_handler)
    zephyr_include_directories(include/bm/storage)
  endif()

  include(cmake/version.cmake)

  add_subdirectory(drivers)
  add_subdirectory(lib)
  add_subdirectory(subsys)
endif()
