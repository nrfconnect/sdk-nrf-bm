#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

set(NRF_LITE_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE PATH "nrf-lite directory")

zephyr_include_directories(include)

add_subdirectory(lib)
add_subdirectory(subsys)
