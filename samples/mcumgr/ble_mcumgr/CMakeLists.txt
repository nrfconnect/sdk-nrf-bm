#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ble_mcumgr)

zephyr_include_directories(${ZEPHYR_BASE}/subsys/mgmt/mcumgr/util/include)
zephyr_include_directories(${ZEPHYR_BASE}/subsys/mgmt/mcumgr/grp/img_mgmt/include)
zephyr_include_directories(${ZEPHYR_BASE}/subsys/mgmt/mcumgr/transport/include)
target_sources(app PRIVATE
  src/main.c
)
zephyr_linker_sources(SECTIONS mcumgr_handler.ld)
zephyr_link_libraries(MCUBOOT_BOOTUTIL)
