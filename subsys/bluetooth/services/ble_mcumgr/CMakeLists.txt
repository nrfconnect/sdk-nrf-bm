#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_sources(mcumgr.c)
zephyr_include_directories(${ZEPHYR_BASE}/subsys/mgmt/mcumgr/util/include)
zephyr_include_directories(${ZEPHYR_BASE}/subsys/mgmt/mcumgr/grp/img_mgmt/include)
zephyr_include_directories(${ZEPHYR_BASE}/subsys/mgmt/mcumgr/transport/include)
zephyr_linker_sources(SECTIONS mcumgr_handler.ld)
zephyr_link_libraries(MCUBOOT_BOOTUTIL)
