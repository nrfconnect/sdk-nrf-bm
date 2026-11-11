#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_include_directories("${ZEPHYR_NRFXLIB_MODULE_DIR}/softperipheral/include")

add_subdirectory_ifdef(CONFIG_BM_SOFTPERIPHERAL_SCAN sCAN)
add_subdirectory_ifdef(CONFIG_BM_SOFTPERIPHERAL_SEMMC sEMMC)
add_subdirectory_ifdef(CONFIG_BM_SOFTPERIPHERAL_SQSPI sQSPI)
