#
# Copyright (c) 2018-2021 mcumgr authors
# Copyright (c) 2022-2025 Nordic Semiconductor ASA
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

zephyr_library_sources_ifdef(CONFIG_MCUMGR_TRANSPORT_BM_UART
  src/smp_uart.c
  src/bm_uart_mcumgr.c
  ${ZEPHYR_BASE}/subsys/mgmt/mcumgr/transport/src/serial_util.c
)

zephyr_include_directories(${ZEPHYR_BASE}/subsys/mgmt/mcumgr/transport/include include)
zephyr_library_include_directories(${ZEPHYR_BASE}/subsys/mgmt/mcumgr/util/include)
zephyr_linker_sources(SECTIONS mcumgr_handler.ld)
