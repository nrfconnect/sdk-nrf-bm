#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library()
zephyr_library_include_directories(include)
zephyr_library_sources(peer_manager.c)
zephyr_library_sources(nrf_strerror.c)

add_subdirectory(modules)

if(CONFIG_PM_LESC_PRIVATE_KEY_EXPORT)
  message(WARNING "CONFIG_PM_LESC_PRIVATE_KEY_EXPORT must only be used for debugging purposes. "
                  "Not to be used in production!")
endif()
