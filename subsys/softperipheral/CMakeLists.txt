#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_include_directories("${ZEPHYR_NRFXLIB_MODULE_DIR}/softperipheral/include")

zephyr_include_directories_ifdef(CONFIG_BM_SQSPI
  "${ZEPHYR_NRFXLIB_MODULE_DIR}/softperipheral/sQSPI/include"
  "${ZEPHYR_NRFXLIB_MODULE_DIR}/softperipheral/sQSPI/include/nrf54l"
)

zephyr_compile_definitions_ifdef(CONFIG_BM_SQSPI
  NRF_SQSPI_ENABLED=1
  NRF_SQSPI_MAX_NUM_DATA_LINES=4
)

zephyr_sources_ifdef(CONFIG_BM_SQSPI
  "${ZEPHYR_NRFXLIB_MODULE_DIR}/softperipheral/sQSPI/src/nrf_sqspi.c"
)
