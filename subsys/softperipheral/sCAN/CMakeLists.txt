#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_include_directories(
  "${ZEPHYR_NRFXLIB_MODULE_DIR}/softperipheral/sCAN/include"
  "${ZEPHYR_NRFXLIB_MODULE_DIR}/softperipheral/sCAN/include/nrf54l"
)

zephyr_compile_definitions(
  NRF_SCAN_ENABLED=1
  NRF_SCAN_RXFILTER_MAX_BUFFER_SIZE=${CONFIG_BM_SOFTPERIPHERAL_SCAN_RXFILTER_MAX_BUFFER_SIZE}
  NRF_SCAN_MAX_DATA_LENGTH=${CONFIG_BM_SOFTPERIPHERAL_SCAN_MAX_DATA_LENGTH}
)

zephyr_sources(
  "${ZEPHYR_NRFXLIB_MODULE_DIR}/softperipheral/sCAN/src/nrf_scan.c"
)
