#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library()
zephyr_library_sources(bm_storage.c)

add_subdirectory_ifdef(CONFIG_BM_STORAGE_BACKEND_RRAM rram)
add_subdirectory_ifdef(CONFIG_BM_STORAGE_BACKEND_SD sd)
add_subdirectory_ifdef(CONFIG_BM_STORAGE_BACKEND_NATIVE_SIM native_sim)
