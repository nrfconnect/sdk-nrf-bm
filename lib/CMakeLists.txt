#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

add_subdirectory(bluetooth)
add_subdirectory_ifdef(CONFIG_BM_SCHEDULER bm_scheduler)
add_subdirectory_ifdef(CONFIG_BM_BUTTONS bm_buttons)
add_subdirectory_ifdef(CONFIG_BM_TIMER bm_timer)
add_subdirectory_ifdef(CONFIG_SENSORSIM sensorsim)
add_subdirectory_ifdef(CONFIG_NCS_BARE_METAL_BOOT_BANNER boot_banner)
add_subdirectory_ifdef(CONFIG_ZEPHYR_QUEUE zephyr_queue)
