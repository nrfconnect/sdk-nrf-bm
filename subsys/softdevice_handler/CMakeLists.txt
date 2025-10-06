#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
zephyr_library()
zephyr_linker_sources(SECTIONS sdh.ld)
zephyr_library_sources(
  nrf_sdh.c
  nrf_sdh_soc.c
  nrf_sdh_ble.c
  irq_connect.c
)

zephyr_library_sources_ifdef(CONFIG_NRF_SDH_SOC_RAND_SEED rand_seed.c)

if(CONFIG_SOFTDEVICE_S115)
  zephyr_library_sources(irq_forward.s)
  # Suppress the swap_helper.S file so that z_arm_svc can be defined manually
  set(cortex_m_dir ${ZEPHYR_BASE}/arch/arm/core/cortex_m)
  set_property(
    SOURCE ${cortex_m_dir}/swap_helper.S
    DIRECTORY ${cortex_m_dir}
    PROPERTY HEADER_FILE_ONLY ON
  )
  target_link_options(app PUBLIC "-Wl,--defsym=z_arm_svc=SVC_Handler")
endif()
