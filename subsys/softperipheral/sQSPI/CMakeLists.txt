#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_include_directories(
  "${ZEPHYR_NRFXLIB_MODULE_DIR}/softperipheral/sQSPI/include"
  "${ZEPHYR_NRFXLIB_MODULE_DIR}/softperipheral/sQSPI/include/nrf54l"
)

zephyr_compile_definitions(
  NRF_SQSPI_ENABLED=1
  NRF_SQSPI_MAX_NUM_DATA_LINES=${CONFIG_BM_SOFTPERIPHERAL_SQSPI_MAX_NUM_DATA_LINES}
  NRF_SQSPI_TRANSFERS_PER_REQUEST=${CONFIG_BM_SOFTPERIPHERAL_SQSPI_TRANSFERS_PER_REQUEST}
)

zephyr_sources(
  "${ZEPHYR_NRFXLIB_MODULE_DIR}/softperipheral/sQSPI/src/nrf_sqspi.c"
)
