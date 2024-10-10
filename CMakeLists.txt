#
# Copyright (c) 2024 Nordic Semiconductor ASA. All Rights Reserved.
#
# The information contained herein is confidential property of Nordic Semiconductor ASA.
# The use, copying, transfer or disclosure of such information is prohibited except by
# express written agreement with Nordic Semiconductor ASA.
#

set(NRF_LITE_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE PATH "nrf-lite directory")

zephyr_include_directories(include)

add_subdirectory(lib)
#add_subdirectory(samples)
add_subdirectory(subsys)
