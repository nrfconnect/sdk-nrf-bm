#
# Copyright (c) 2018-2021 mcumgr authors
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

# mgmt_mcumgr_transport covers interface API, allowing to implement transports.
# It is exposed with mgmt_mcumgr interface.
zephyr_library_named(mgmt_mcumgr_transport)
zephyr_library_sources(src/smp.c)

zephyr_library_sources_ifdef(CONFIG_MCUMGR_TRANSPORT_REASSEMBLY
  ${ZEPHYR_BASE}/subsys/mgmt/mcumgr/transport/src/smp_reassembly.c
)

#if(CONFIG_MCUMGR_TRANSPORT_SHELL OR CONFIG_MCUMGR_TRANSPORT_UART)
#  zephyr_library_sources(src/serial_util.c)
#endif()

zephyr_include_directories(${ZEPHYR_BASE}/subsys/mgmt/mcumgr/transport/include)
