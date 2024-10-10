#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
zephyr_library()
zephyr_linker_sources(SECTIONS sdh.ld)
zephyr_library_sources(
  nrf_sdh.c
  irq_connect.c
)

if (CONFIG_NRF_SDH_SOC)
  zephyr_library_sources(nrf_sdh_soc.c)
endif()
if (CONFIG_NRF_SDH_BLE)
  zephyr_library_sources(nrf_sdh_ble.c)
endif()
