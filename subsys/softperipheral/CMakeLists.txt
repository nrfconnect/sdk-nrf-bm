#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

add_subdirectory_ifdef(CONFIG_BM_SOFTPERIPHERAL_SCAN scan)
add_subdirectory_ifdef(CONFIG_BM_SOFTPERIPHERAL_SEMMC semmc)
add_subdirectory_ifdef(CONFIG_BM_SOFTPERIPHERAL_SQSPI sqspi)
