#
# Copyright (c) 2018-2021 mcumgr authors
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

# The mgmt_mcumgr is interface library for MCUmgr services.
zephyr_interface_library_named(mgmt_mcumgr)

# zephyr-keep-sorted-start
add_subdirectory(grp)
add_subdirectory(mgmt)
add_subdirectory(smp)
add_subdirectory(transport)
add_subdirectory(util)
# zephyr-keep-sorted-stop

zephyr_library_link_libraries(mgmt_mcumgr)
