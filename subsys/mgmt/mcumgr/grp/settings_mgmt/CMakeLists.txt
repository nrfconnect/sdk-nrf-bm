#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

# Settings management group public API is exposed by MCUmgr API
# interface, when settings management is enabled.
zephyr_library_named(mgmt_mcumgr_grp_settings)
zephyr_library_sources(${ZEPHYR_BASE}/subsys/mgmt/mcumgr/grp/settings_mgmt/src/settings_mgmt.c)

if(CONFIG_MCUMGR_GRP_SETTINGS AND NOT CONFIG_MCUMGR_GRP_SETTINGS_ACCESS_HOOK)
  message(WARNING "Note: MCUmgr settings management is enabled but settings access hooks are "
                  "disabled, this is an insecure configuration and not recommended for production "
                  "use, as all settings on the device can be manipulated by a remote device. See "
                  "https://docs.zephyrproject.org/latest/services/device_mgmt/mcumgr_callbacks.html "
                  "for details on enabling and using MCUmgr hooks.")
endif()
