#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# zephyr-keep-sorted-start
add_subdirectory_ifdef(CONFIG_BM_STORAGE bm_storage)
add_subdirectory_ifdef(CONFIG_FLASH_MAP flash_map)
# zephyr-keep-sorted-stop
