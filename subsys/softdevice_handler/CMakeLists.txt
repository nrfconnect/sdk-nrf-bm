#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
zephyr_library()
zephyr_linker_sources(SECTIONS sdh.ld)
zephyr_linker_sources(DATA_SECTIONS sdh_ram.ld)

zephyr_library_sources(
  nrf_sdh.c
  nrf_sdh_info.c
  nrf_sdh_soc.c
)

zephyr_library_sources_ifdef(CONFIG_NRF_SDH_BLE nrf_sdh_ble.c)
zephyr_library_sources_ifdef(CONFIG_NRF_SDH_SOC_RAND_SEED rand_seed.c)

# IRQ setup
if(CONFIG_SOFTDEVICE_BSIM)
  zephyr_library_sources(irq_connect_bsim.c)
else()
  # ARM-only: IRQ connection, SVC forwarding and swap_helper suppression
  # Suppress the swap_helper.S file so that z_arm_svc can be defined manually
  zephyr_library_sources(irq_connect.c)
  zephyr_library_sources(irq_forward.s)
  set(cortex_m_dir ${ZEPHYR_BASE}/arch/arm/core/cortex_m)
  set_property(
    SOURCE ${cortex_m_dir}/swap_helper.S
    DIRECTORY ${cortex_m_dir}
    PROPERTY HEADER_FILE_ONLY ON
  )
  target_link_options(app PUBLIC "-Wl,--defsym=z_arm_svc=SVC_Handler")
endif()
