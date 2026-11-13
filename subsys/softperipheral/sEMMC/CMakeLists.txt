#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_include_directories(
  "${ZEPHYR_NRFXLIB_MODULE_DIR}/softperipheral/sEMMC/include"
  "${ZEPHYR_NRFXLIB_MODULE_DIR}/softperipheral/sEMMC/include/nrf54l"
)

zephyr_compile_definitions(
  NRF_SEMMC_ENABLED=1
  NRF_SEMMC_RESPONSE_SIZE=${CONFIG_BM_SOFTPERIPHERAL_SEMMC_RESPONSE_SIZE}
  NRF_SEMMC_TRANSFERS_PER_REQUEST=${CONFIG_BM_SOFTPERIPHERAL_SEMMC_TRANSFERS_PER_REQUEST}
)

zephyr_sources(
  "${ZEPHYR_NRFXLIB_MODULE_DIR}/softperipheral/sEMMC/src/nrf_semmc.c"
)
