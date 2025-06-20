#
# Copyright (c) 2018-2021 mcumgr authors
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0

# Provides MCUmgr services, group registration, event handling etc.
zephyr_library_named(mgmt_mcumgr_mgmt)
zephyr_library_sources(${ZEPHYR_BASE}/subsys/mgmt/mcumgr/mgmt/src/mgmt.c)
zephyr_library_include_directories(${ZEPHYR_BASE}/subsys/mgmt/mcumgr/mgmt/include)
