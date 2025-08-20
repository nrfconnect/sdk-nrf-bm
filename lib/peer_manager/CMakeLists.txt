#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library()
zephyr_library_include_directories(include)
zephyr_library_sources(peer_manager.c)

add_subdirectory(modules)
