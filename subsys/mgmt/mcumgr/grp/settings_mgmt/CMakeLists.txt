#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

# Settings management group public API is exposed by MCUmgr API
# interface, when settings management is enabled.
zephyr_library_named(mgmt_mcumgr_grp_settings)
zephyr_library_sources(${ZEPHYR_BASE}/subsys/mgmt/mcumgr/grp/settings_mgmt/src/settings_mgmt.c)
