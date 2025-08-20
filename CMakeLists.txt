#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_NCS_BM)
  zephyr_include_directories(include)

  include(cmake/merge_app_softdevice.cmake)
  include(cmake/version.cmake)

  add_subdirectory(drivers)
  add_subdirectory(lib)
  add_subdirectory(subsys)
endif()
