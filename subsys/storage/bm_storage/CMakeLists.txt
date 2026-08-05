#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library()
zephyr_library_sources(bm_storage.c)

# zephyr-keep-sorted-start
add_subdirectory_ifdef(CONFIG_BM_STORAGE_BACKEND_NATIVE_SIM native_sim)
add_subdirectory_ifdef(CONFIG_BM_STORAGE_BACKEND_RRAM rram)
add_subdirectory_ifdef(CONFIG_BM_STORAGE_BACKEND_SD sd)
# zephyr-keep-sorted-stop
