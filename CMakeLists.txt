#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_NCS_BM)
  zephyr_include_directories(include)

  # SoftDevice headers are accesible without specifying the path.
  if(CONFIG_SOFTDEVICE_S115)
    zephyr_include_directories(include/bm/softdevice/s115)
  endif()

  include(cmake/version.cmake)

  add_subdirectory(drivers)
  add_subdirectory(lib)
  add_subdirectory(subsys)
endif()
