#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# zephyr-keep-sorted-start
add_subdirectory(ble)
add_subdirectory(bm_installs)
add_subdirectory(logging)
add_subdirectory_ifdef(CONFIG_NCS_BM_MCUMGR mgmt/mcumgr)
add_subdirectory_ifdef(CONFIG_NRF_SDH nrf_sdh)
add_subdirectory_ifdef(CONFIG_SOFTDEVICE softdevice)
add_subdirectory_ifdef(CONFIG_FLASH_MAP storage/flash_map)
# zephyr-keep-sorted-stop
