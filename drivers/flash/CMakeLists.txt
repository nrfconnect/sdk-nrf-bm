# SPDX-License-Identifier: Apache-2.0

zephyr_syscall_header(${ZEPHYR_BASE}/include/zephyr/drivers/flash.h)

zephyr_library_amend()

# zephyr-keep-sorted-start
zephyr_library_sources_ifdef(CONFIG_SOC_FLASH_NRF_RRAM_BM soc_flash_nrf_rram.c)
# zephyr-keep-sorted-stop
